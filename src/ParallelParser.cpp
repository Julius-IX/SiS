#include <Logging.h>
#include <Parser.h>

#include <future>
#include <unordered_map>

void panic(std::string_view msg);

namespace par {

  // lexAndParseFile: lex and parse a single source file on a worker thread.
  // Takes hooks by value so the worker owns its own Parser instance with no
  // shared state. Never touches m_states, m_load_order, or m_include_stack.
  Parser::ParseResult Parser::lexAndParseFile(const std::string& source, const Path& path, ParserHooks hooks) {
    lex::Lexer lexer(source, path);
    State state;
    state.tokens = lexer.tokenize();
    state.line_cache = lexer.takeLineCache();

    Parser worker;
    worker.m_hooks = std::move(hooks);
    worker.parse(&state);

    return ParseResult{
      .block = std::move(state.block),
      .line_cache = std::move(state.line_cache),
    };
  }

  std::optional<Program> Parser::parseRootParallel() {
    using Future = std::future<ParseResult>;
    std::unordered_map<Path, Future> futures;

    // Include discovery stays serial -- checkForInclude advances the cursor
    // and must not be called concurrently.
    while (!m_include_stack.empty()) {
      Path current_path = m_include_stack.back();
      m_include_stack.pop_back();

      auto ext = current_path.extension();
      if (ext == ".so" || ext == ".dll" || ext == ".dylib") {
        m_load_order.push_back(current_path);
        continue;
      }

      // If a future is in flight for this path, resolve it into m_states now
      // before checkForInclude tries to read the token stream.
      if (auto it = futures.find(current_path); it != futures.end()) {
        ParseResult result = it->second.get();
        futures.erase(it);
        m_states[current_path] = State{
          .tokens = {},
          .line_cache = std::move(result.line_cache),
          .block = std::move(result.block),
          .last_token = {},
        };
      }

      std::expected<std::optional<Path>, std::string> include_path = checkForInclude(current_path);
      if (!include_path.has_value()) {
        panic(include_path.error());
        break;
      }

      if (include_path.value() == std::nullopt) {
        // This file has no more includes and is not yet parsed -- it was
        // loaded by initRootState or loadIncludeSource before parallel mode
        // took over, so parse it now on the main thread.
        parseCurrentFile(current_path);
        continue;
      }

      const Path& resolved = include_path.value().value();

      bool mid_flight = std::ranges::find(m_include_stack, resolved) != m_include_stack.end();
      if (mid_flight) {
        panic(fmt::format("Circular include detected at '{}'", resolved.string()));
        break;
      }

      if (m_states.contains(resolved) || futures.contains(resolved)) {
        m_include_stack.push_back(current_path);
        continue;
      }

      // New include: read source on the main thread, then fire a worker to
      // lex and parse it. Hooks are copied by value so the worker is isolated.
      std::optional<std::string> source = m_hooks.read_file(resolved);
      if (!source) {
        panic(fmt::format("Could not open included source file '{}'", resolved.string()));
        break;
      }
      futures[resolved] = std::async(std::launch::async, lexAndParseFile, std::move(*source), resolved, m_hooks);

      m_include_stack.push_back(current_path);
      m_include_stack.push_back(resolved);
    }

    Program program;
    for (const Path& p : m_load_order) {
      State& state = m_states[p];
      auto ext = p.extension();
      bool is_dynamic = (ext == ".so" || ext == ".dll" || ext == ".dylib");
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
