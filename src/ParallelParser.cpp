#include <Logging.h>
#include <Parser.h>

#include <future>
#include <unordered_map>

void panic(std::string_view msg);

namespace par {

  // lexFile: lex a single source file and return its token stream and line cache.
  // Called on worker threads -- reads from disk and produces a TokenStream.
  // Never touches m_states, m_load_order, or m_include_stack.
  static std::pair<lex::TokenStream, std::unordered_map<size_t, std::string>>
  lexFile(const std::string& source, const Path& path) {
    lex::Lexer lexer(source, path);
    lex::TokenStream tokens = lexer.tokenize();
    auto line_cache = lexer.takeLineCache();
    return {std::move(tokens), std::move(line_cache)};
  }

  std::optional<Program> Parser::parseRootParallel() {
    // futures: keyed by resolved path, holds the async lex result for each
    // discovered include. Workers only read source text and produce tokens --
    // they never touch shared parser state.
    using LexResult = std::pair<lex::TokenStream, std::unordered_map<size_t, std::string>>;
    std::unordered_map<Path, std::future<LexResult>> futures;

    // The root file was already lexed by initRootState before this method is
    // called. The include discovery loop below is intentionally serial: cursor
    // advancement in checkForInclude is not thread-safe.
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
        auto [tokens, line_cache] = it->second.get();
        futures.erase(it);
        m_states[current_path] = State{
          .tokens = std::move(tokens),
          .line_cache = std::move(line_cache),
          .last_token = {},
        };
      }

      std::expected<std::optional<Path>, std::string> include_path = checkForInclude(current_path);
      if (!include_path.has_value()) {
        panic(include_path.error());
        break;
      }

      if (include_path.value() == std::nullopt) {
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

      // New include: read source on the main thread (fast, avoids concurrent
      // filesystem access), then fire a worker thread to lex it.
      std::optional<std::string> source = m_hooks.read_file(resolved);
      if (!source) {
        panic(fmt::format("Could not open included source file '{}'", resolved.string()));
        break;
      }
      futures[resolved] = std::async(std::launch::async, lexFile, std::move(*source), resolved);

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
