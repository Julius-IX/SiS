#include <Logging.h>
#include <Parser.h>

#include <algorithm>
#include <future>
#include <unordered_map>
#include <unordered_set>

void panic(std::string_view msg);

namespace par {

  // lexAndParseFile: lex a source file, strip its include directives into
  // raw_includes (unresolved string paths, in source order), then parse the
  // remaining statements into a Block AST. Runs entirely on a worker thread
  // with no access to the main parser's state.
  //
  // Include directives are consumed directly from the token stream rather than
  // going through checkForInclude, because checkForInclude touches m_states
  // and m_include_stack which are main-thread-only. The token-level logic is
  // straightforward: INCLUDE STRING SEMICOLON. Any violation (missing string,
  // missing semicolon, include after non-include) is an error.
  //
  // The main thread receives raw_includes, resolves them to real paths, runs
  // cycle detection, and schedules any unseen files as new futures.
  Parser::ParseResult Parser::lexAndParseFile(const std::string& source, const Path& path) {
    lex::Lexer lexer(source, path);
    State state;
    state.tokens = lexer.tokenize();
    state.line_cache = lexer.takeLineCache();

    ParseResult result;
    result.line_cache = state.line_cache; // copy: needed for error context below

    // Drain include directives from the front of the token stream. Includes
    // must appear before any other statement (mirrors past_include_zone logic
    // in checkForInclude on the main thread).
    bool past_include_zone = false;
    while (state.cursor < state.tokens.size()) {
      const lex::Token& tok = state.tokens[state.cursor];

      if (tok.type == lex::TokenType::SIS_EOF) break;

      if (tok.type != lex::TokenType::INCLUDE) {
        past_include_zone = true;
        break;
      }

      if (past_include_zone) {
        throw std::runtime_error(fmt::format("{}:{}: includes must appear at the top of the file", tok.line, tok.column));
      }

      ++state.cursor; // consume INCLUDE

      if (state.cursor >= state.tokens.size() || state.tokens[state.cursor].type != lex::TokenType::STRING) {
        const lex::Token& bad = state.cursor < state.tokens.size() ? state.tokens[state.cursor] : state.tokens.back();
        throw std::runtime_error(fmt::format("{}:{}: expected string literal path after 'include'", bad.line, bad.column));
      }

      const lex::Token& path_tok = state.tokens[state.cursor];
      const std::string* raw_path = std::get_if<std::string>(&path_tok.value);
      if (!raw_path) {
        throw std::runtime_error(fmt::format("{}:{}: failed to read include path", path_tok.line, path_tok.column));
      }
      result.raw_includes.push_back(*raw_path);
      ++state.cursor; // consume STRING

      if (state.cursor >= state.tokens.size() || state.tokens[state.cursor].type != lex::TokenType::SEMICOLON) {
        const lex::Token& bad = state.cursor < state.tokens.size() ? state.tokens[state.cursor] : state.tokens.back();
        throw std::runtime_error(fmt::format("{}:{}: expected ';' after include path", bad.line, bad.column));
      }
      ++state.cursor; // consume SEMICOLON
    }

    // Parse the remainder (everything after the include zone) into a Block.
    // A temporary Parser is used purely for its parse() method; it has no
    // shared state with the main parser instance.
    Parser worker;
    if (!worker.parse(&state)) {
      throw std::runtime_error(fmt::format("Failed to parse '{}'", path.string()));
    }

    result.block = std::move(state.block);
    return result;
  }

  std::optional<Program> Parser::parseRootParallel() {
    using Future = std::future<ParseResult>;

    std::unordered_map<Path, Future> futures;
    std::unordered_set<Path> scheduled;
    std::unordered_set<Path> ordered; // paths fully committed to m_load_order

    {
      Path root = m_include_stack.back();
      scheduled.insert(root);
      std::optional<std::string> source = m_hooks.read_file(root);
      if (!source) {
        panic(fmt::format("Could not open root source file '{}'", root.string()));
        return std::nullopt;
      }
      futures[root] = std::async(std::launch::async, lexAndParseFile, std::move(*source), root);
    }

    while (!m_include_stack.empty()) {
      Path current_path = m_include_stack.back();
      m_include_stack.pop_back();

      auto ext = current_path.extension();
      if (ext == ".so" || ext == ".dll" || ext == ".dylib") {
        m_load_order.push_back(current_path);
        ordered.insert(current_path);
        continue;
      }

      // If this file still has an unresolved future, block on it now.
      if (auto fut_it = futures.find(current_path); fut_it != futures.end()) {
        ParseResult result = fut_it->second.get();
        futures.erase(fut_it);

        std::vector<Path> resolved_includes;
        for (const std::string& raw : result.raw_includes) {
          std::optional<Path> resolved = m_hooks.resolve_file(current_path.parent_path(), Path(raw));
          if (!resolved) {
            panic(fmt::format("Could not resolve include '{}' from '{}'", raw, current_path.string()));
            return std::nullopt;
          }

          bool mid_flight = std::ranges::find(m_include_stack, *resolved) != m_include_stack.end();
          if (mid_flight) {
            panic(fmt::format("Circular include detected at '{}'", resolved->string()));
            return std::nullopt;
          }

          resolved_includes.push_back(*resolved);
        }

        // Push new non-native includes onto the stack in reverse source order
        // so they pop off (and get processed) in source declaration order.
        for (auto it = resolved_includes.rbegin(); it != resolved_includes.rend(); ++it) {
          const Path& dep = *it;
          if (scheduled.contains(dep) || m_states.contains(dep)) continue;
          scheduled.insert(dep);

          auto inc_ext = dep.extension();
          if (inc_ext == ".so" || inc_ext == ".dll" || inc_ext == ".dylib") {
            lex::TokenStream sentinel;
            sentinel.push_back(lex::Token{.type = lex::TokenType::SIS_EOF, .value = {}, .line = 0, .column = 0});
            m_states[dep] = State{
              .tokens = std::move(sentinel),
              .block = std::make_unique<Block>(std::vector<std::unique_ptr<Node>>()),
              .last_token = {},
            };
            m_load_order.push_back(dep);
            ordered.insert(dep);
          } else {
            std::optional<std::string> source = m_hooks.read_file(dep);
            if (!source) {
              panic(fmt::format("Could not open included source file '{}'", dep.string()));
              return std::nullopt;
            }
            futures[dep] = std::async(std::launch::async, lexAndParseFile, std::move(*source), dep);
            m_include_stack.push_back(dep);
          }
        }

        m_states[current_path] = State{
          .tokens = {},
          .line_cache = std::move(result.line_cache),
          .block = std::move(result.block),
          .includes = resolved_includes,
          .last_token = {},
        };
      }

      // All deps must be in load_order before this file can be committed.
      // Use `ordered` not `futures` -- a dep may be resolved but not yet ordered.
      const std::vector<Path>& deps = m_states[current_path].includes;
      bool deps_ready = std::ranges::all_of(deps, [&](const Path& dep) { return ordered.contains(dep); });

      if (!deps_ready) {
        m_include_stack.push_front(current_path); // push to FRONT so deps resolve first
        continue;
      }

      m_load_order.push_back(current_path);
      ordered.insert(current_path);
    }

    Program program;
    for (const Path& p : m_load_order) {
      State& state = m_states[p];
      auto p_ext = p.extension();
      bool is_dynamic = (p_ext == ".so" || p_ext == ".dll" || p_ext == ".dylib");
      if (!state.block && !is_dynamic) return std::nullopt;
      program.files[p] = ParsedFile{
        .tokens = {},
        .ast = std::move(state.block),
        .includes = std::move(state.includes),
        .is_dynamic = is_dynamic,
      };
      program.load_order.push_back(p);
    }
    return program;
  }

} // namespace par
