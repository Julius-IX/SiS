#include <Logging.h>
#include <Parser.h>

#include <algorithm>
#include <future>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

void panic(std::string_view msg);

namespace par {

  // Consumes include directives from the front of the token stream and returns
  // their raw string paths in source order. Advances state.cursor past each
  // consumed INCLUDE STRING [AS IDENT] SEMICOLON sequence. Throws on malformed
  // syntax or an include appearing after non-include statements.
  static std::vector<std::string> drainIncludes(State& state, std::unordered_map<std::string, std::string>& aliases_out) {
    std::vector<std::string> raw_includes;
    bool past_include_zone = false;

    while (state.cursor < state.tokens.size()) {
      const lex::Token& tok = state.tokens[state.cursor];

      if (tok.type == lex::TokenType::SIS_EOF) break;

      if (tok.type != lex::TokenType::INCLUDE) {
        past_include_zone = true;
        break;
      }

      if (past_include_zone) {
        throw std::runtime_error(
          fmt::format("{}:{}: includes must appear at the top of the file", tok.line, tok.column));
      }

      ++state.cursor; // consume INCLUDE

      if (state.cursor >= state.tokens.size() ||
          state.tokens[state.cursor].type != lex::TokenType::STRING) {
        const lex::Token& bad =
          state.cursor < state.tokens.size() ? state.tokens[state.cursor] : state.tokens.back();
        throw std::runtime_error(
          fmt::format("{}:{}: expected string literal path after 'include'", bad.line, bad.column));
      }

      const lex::Token& path_tok = state.tokens[state.cursor];
      const std::string* raw_path = std::get_if<std::string>(&path_tok.value);
      if (raw_path == nullptr) {
        throw std::runtime_error(
          fmt::format("{}:{}: failed to read include path", path_tok.line, path_tok.column));
      }
      std::string raw = *raw_path;
      ++state.cursor; // consume STRING

      // Optional: as <name>
      if (state.cursor < state.tokens.size() &&
          state.tokens[state.cursor].type == lex::TokenType::AS) {
        ++state.cursor; // consume AS
        if (state.cursor >= state.tokens.size() ||
            state.tokens[state.cursor].type != lex::TokenType::IDENT) {
          const lex::Token& bad =
            state.cursor < state.tokens.size() ? state.tokens[state.cursor] : state.tokens.back();
          throw std::runtime_error(
            fmt::format("{}:{}: expected identifier after 'as'", bad.line, bad.column));
        }
        const std::string* alias = std::get_if<std::string>(&state.tokens[state.cursor].value);
        if (alias != nullptr) {
          aliases_out[raw] = *alias;
        }
        ++state.cursor; // consume IDENT
      }

      if (state.cursor >= state.tokens.size() ||
          state.tokens[state.cursor].type != lex::TokenType::SEMICOLON) {
        const lex::Token& bad =
          state.cursor < state.tokens.size() ? state.tokens[state.cursor] : state.tokens.back();
        throw std::runtime_error(
          fmt::format("{}:{}: expected ';' after include path", bad.line, bad.column));
      }
      ++state.cursor; // consume SEMICOLON

      raw_includes.push_back(std::move(raw));
    }

    return raw_includes;
  }

  // Resolves raw string include paths to real filesystem paths and checks for
  // circular includes. Returns resolved paths in source order, or nullopt on
  // error (panic is called before returning).
  static std::optional<std::vector<Path>> resolveRawIncludes(
    const std::vector<std::string>& raw,
    const Path& current_path,
    const ParserHooks& hooks,
    const std::deque<Path>& include_stack) {
    std::vector<Path> resolved;
    resolved.reserve(raw.size());

    for (const std::string& r : raw) {
      std::optional<Path> p = hooks.resolve_file(current_path.parent_path(), Path(r));
      if (!p) {
        panic(fmt::format("Could not resolve include '{}' from '{}'", r, current_path.string()));
        return std::nullopt;
      }

      if (std::ranges::find(include_stack, *p) != include_stack.end()) {
        panic(fmt::format("Circular include detected at '{}'", p->string()));
        return std::nullopt;
      }

      resolved.push_back(std::move(*p));
    }

    return resolved;
  }

  // lex a source file, strip its include directives into
  // raw_includes (unresolved string paths, in source order), then parse the
  // remaining statements into a Block AST. Runs entirely on a worker thread
  // with no access to the main parser's state.
  //
  // The main thread receives raw_includes, resolves them to real paths, runs
  // cycle detection, and schedules any unseen files as new futures.
  Parser::ParseResult Parser::lexAndParseFile(const std::string& source, const Path& path) {
    lex::Lexer lexer(source, path);
    State state;
    state.tokens     = lexer.tokenize();
    state.line_cache = lexer.takeLineCache();

    ParseResult result;
    result.line_cache   = state.line_cache;
    result.raw_includes = drainIncludes(state, result.raw_aliases);

    Parser worker;
    if (!worker.parse(&state)) {
      throw std::runtime_error(fmt::format("Failed to parse '{}'", path.string()));
    }

    result.block = std::move(state.block);
    return result;
  }

  // Schedules a file for async lex+parse and marks it as seen.
  void Parser::pScheduleFile(ParallelContext& ctx, const Path& path, std::string source) {
    ctx.scheduled.insert(path);
    ctx.futures[path] = std::async(std::launch::async, lexAndParseFile, std::move(source), path);
  }

  // Registers a native module: writes a sentinel state, commits it to
  // load_order, and marks it ordered. Native modules are always immediately
  // ready -- they have no source to parse and no `.sis` deps of their own.
  void Parser::pCommitNative(ParallelContext& ctx, const Path& dep) {
    lex::TokenStream sentinel;
    sentinel.push_back(
      lex::Token{.type = lex::TokenType::SIS_EOF, .value = {}, .line = 0, .column = 0});
    m_states[dep] = State{
      .tokens     = std::move(sentinel),
      .block      = std::make_unique<Block>(std::vector<std::unique_ptr<Node>>()),
      .last_token = {},
    };
    m_load_order.push_back(dep);
    ctx.ordered.insert(dep);
  }

  // Blocks on current_path's future, resolves its raw includes, schedules any
  // unseen deps, and writes the parsed result into m_states. Returns false if
  // an error occurred.
  bool Parser::pProcessFuture(ParallelContext& ctx, const Path& current_path) {
    auto fut_it = ctx.futures.find(current_path);
    if (fut_it == ctx.futures.end()) return true;

    ParseResult result = fut_it->second.get();
    ctx.futures.erase(fut_it);

    auto resolved = resolveRawIncludes(result.raw_includes, current_path, m_hooks, m_include_stack);
    if (!resolved) return false;

    // Translate raw string aliases to resolved Path keys now that we have
    // the resolved paths parallel to the raw include list.
    for (size_t i = 0; i < result.raw_includes.size(); ++i) {
      auto alias_it = result.raw_aliases.find(result.raw_includes[i]);
      if (alias_it != result.raw_aliases.end()) {
        m_aliases[(*resolved)[i]] = alias_it->second;
      }
    }

    // Iterate in reverse source order so deps pop off the stack in forward
    // source declaration order (stack is LIFO).
    for (auto& dep : std::views::reverse(*resolved)) {
      if (ctx.scheduled.contains(dep) || m_states.contains(dep)) continue;

      auto dep_ext = dep.extension();
      if (dep_ext == ".so" || dep_ext == ".dll" || dep_ext == ".dylib") {
        pCommitNative(ctx, dep);
      } else {
        std::optional<std::string> source = m_hooks.read_file(dep);
        if (!source) {
          panic(fmt::format("Could not open included source file '{}'", dep.string()));
          return false;
        }
        pScheduleFile(ctx, dep, std::move(*source));
        m_include_stack.push_back(dep);
      }
    }

    m_states[current_path] = State{
      .tokens     = {},
      .line_cache = std::move(result.line_cache),
      .block      = std::move(result.block),
      .includes   = std::move(*resolved),
      .last_token = {},
    };
    return true;
  }

  // Commits current_path to load_order if all its direct deps are already
  // ordered. Returns false if any dep is still pending (caller re-queues).
  bool Parser::pTryCommit(ParallelContext& ctx, const Path& current_path) {
    const std::vector<Path>& deps = m_states[current_path].includes;
    if (!std::ranges::all_of(deps, [&](const Path& d) { return ctx.ordered.contains(d); })) {
      return false;
    }
    m_load_order.push_back(current_path);
    ctx.ordered.insert(current_path);
    return true;
  }

  // drives the include graph walk using async workers.
  //
  // Include discovery and ordering stay serial on the main thread. Workers
  // only lex+parse their assigned file and return a ParseResult.
  //
  // Load order guarantee: a file is committed to m_load_order only after all
  // its direct dependencies are already committed (tracked via ctx.ordered).
  // Native modules are committed immediately on discovery since they have no
  // deps of their own.
  std::optional<Program> Parser::parseRootParallel() {
    ParallelContext ctx;

    Path root = m_include_stack.back();
    std::optional<std::string> source = m_hooks.read_file(root);
    if (!source) {
      panic(fmt::format("Could not open root source file '{}'", root.string()));
      return std::nullopt;
    }
    pScheduleFile(ctx, root, std::move(*source));

    while (!m_include_stack.empty()) {
      Path current = m_include_stack.back();
      m_include_stack.pop_back();

      auto ext = current.extension();
      if (ext == ".so" || ext == ".dll" || ext == ".dylib") {
        m_load_order.push_back(current);
        ctx.ordered.insert(current);
        continue;
      }

      if (!pProcessFuture(ctx, current)) return std::nullopt;
      if (!pTryCommit(ctx, current))     m_include_stack.push_front(current);
    }

    Program program;
    program.root_path = root;
    for (const Path& p : m_load_order) {
      State& state    = m_states[p];
      auto   p_ext    = p.extension();
      bool is_dynamic = (p_ext == ".so" || p_ext == ".dll" || p_ext == ".dylib");
      if (!state.block && !is_dynamic) return std::nullopt;
      program.files[p] = ParsedFile{
        .tokens     = {},
        .ast        = std::move(state.block),
        .includes   = std::move(state.includes),
        .is_dynamic = is_dynamic,
        .alias      = m_aliases.count(p) ? m_aliases.at(p) : "",
      };
      program.load_order.push_back(p);
    }
    return program;
  }

} // namespace par
