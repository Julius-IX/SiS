#include <Logging.h>
#include <Parser.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#ifdef _WIN32
#include <windows.h>
#undef TRUE
#undef FALSE
#undef THIS
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// WARN: May change at any time, temporary solution is the best permanent solution
void panic(const std::string_view msg) { throw std::runtime_error(msg.data()); }

namespace par { // Hooks
  static std::optional<std::string> readFileToString(const Path& path) {
    LOG_DEBUG_FLUSH("Reading file {}", path.string());
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  std::string formatIllegalTokenMessage(State* state, const lex::Token& token, const std::string_view msg) {
    std::string fmted_msg = token.source.has_value() ? fmt::format("Illegal token received {}:{}\nFile: {}\n", token.line, token.column, token.source.value().string())
                                                     : fmt::format("Illegal token received {}:{}", token.line, token.column);
    if (!msg.empty()) {
      fmted_msg += msg;
      fmted_msg += "\n";
    }

    std::string line_content;
    if (auto it = state->line_cache.find(token.line); it != state->line_cache.end()) {
      line_content = it->second;
    }
    while (!line_content.empty() && (line_content.back() == '\n' || line_content.back() == '\r')) {
      line_content.pop_back();
    }

    return fmted_msg + fmt::format("{}\n{}", line_content, std::string(token.column - 1, ' ') + std::string(std::max<size_t>(token.length, 1), '^'));
  }

  static std::optional<Path> resolveFileOnDisk(const Path& full) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(full, ec);
    if (ec || !std::filesystem::is_regular_file(canonical)) return std::nullopt;
    LOG_DEBUG_FLUSH("resolved file path: {}", canonical.string());
    return canonical;
  }

  std::optional<Path> resolveFile(const Path& root, const Path& relative) {
    LOG_DEBUG_FLUSH("resolving file path: {}", relative.string());
    if (relative.has_extension()) {
      return resolveFileOnDisk(root / relative);
    }

    const char* sis_path_env = std::getenv("SIS_PATH");
    LOG_DEBUG_FLUSH("resolving with env var: {}", sis_path_env ? sis_path_env : "not set");
#ifdef _WIN32
    const char sep = ';';
    const std::string dyn_subdir = "dynamic/";
    const std::string dyn_ext = ".dll";

    auto getFallbackDir = []() -> std::string {
      char exe_path[MAX_PATH];
      GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
      return Path(exe_path).parent_path().string();
    };
#elif __APPLE__
    const char sep = ':';
    const std::string dyn_subdir = "dynamic/";
    const std::string dyn_ext = ".dylib";

    auto getFallbackDir = []() -> std::string {
      char exe_path[PATH_MAX];
      uint32_t size = sizeof(exe_path);
      if (_NSGetExecutablePath(exe_path, &size) != 0) return "";
      return Path(exe_path).parent_path().string();
    };
#else
    const char sep = ':';
    const std::string dyn_subdir = "lib/dynamic/";
    const std::string dyn_ext = ".so";

    auto getFallbackDir = []() -> std::string { return ""; };
#endif
    std::vector<std::string> search_dirs;
    if (sis_path_env != nullptr) {
      std::stringstream ss(sis_path_env);
      std::string dir;
      while (std::getline(ss, dir, sep)) {
        search_dirs.push_back(dir);
      }
    }
    if (auto fallback = getFallbackDir(); !fallback.empty()) search_dirs.push_back(fallback);

    for (const auto& dir : search_dirs) {
      Path b(dir);
      if (auto p = resolveFileOnDisk(b / "lib" / "managed" / (relative.string() + ".sis"))) { // NOTE: ill deff change this and then forget to edit the build script but whatever
        LOG_DEBUG_FLUSH("checking native dir: {}", p->string());
        return p;
      }
      if (auto p = resolveFileOnDisk(b / dyn_subdir / (relative.string() + dyn_ext))) {
        LOG_DEBUG_FLUSH("checking dynamic dir");
        return p;
      }
    }
    LOG_DEBUG_FLUSH("failed to resolve file path: {}", relative.string());
    return std::nullopt;
  }

  Parser::Parser() {
    m_hooks.read_file = readFileToString;
    m_hooks.format_error = formatIllegalTokenMessage;
    m_hooks.resolve_file = resolveFile;
  }
} // namespace par

namespace par { // Helpers

  template <typename T>
  static std::optional<T> getFromVariant(const lex::Token& token) {
    if (const auto* val = std::get_if<T>(&token.value)) {
      return *val;
    }
    return std::nullopt;
  }

  // makeNode: construct a Node subtype and stamp it with a source position
  // in one expression, so node creation can stay a single `return` statement.
  template <typename T, typename... Args>
  static std::unique_ptr<T> makeNode(size_t line, size_t column, Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    node->line = line;
    node->column = column;
    return node;
  }

  static lex::Token makeSyntheticEof() { return lex::Token{.type = lex::TokenType::SIS_EOF, .value = {}, .line = 0, .column = 0}; }

  static const lex::Token& peekAt(const State* state) {
    if (state->cursor >= state->tokens.size()) {
      static const lex::Token eof = makeSyntheticEof();
      return eof;
    }
    return state->tokens[state->cursor];
  }

  bool Parser::isAtEnd(const State* state) { return check(state, lex::TokenType::SIS_EOF); }

  bool Parser::check(const State* state, lex::TokenType type) { return peekAt(state).type == type; }

  // advance: unconditionally consume and return the next token. For when you
  // already know what it is and just need to move past it.
  lex::Token Parser::advance(State* state) {
    lex::Token token = peekAt(state);
    if (state->cursor < state->tokens.size()) ++state->cursor;
    state->last_token = token;
    return token;
  }

  // match: if the next token is `type`, consume it and return true.
  // Otherwise leave it alone and return false.
  bool Parser::match(State* state, lex::TokenType type) {
    if (!check(state, type)) return false;
    state->last_token = peekAt(state);
    if (state->cursor < state->tokens.size()) ++state->cursor;
    return true;
  }

  // expect: consumes the next token if it matches `type`, panics + returns false otherwise.
  bool Parser::expect(State* state, lex::TokenType type, std::string_view err_msg) const {
    if (peekAt(state).type != type) {
      panic(m_hooks.format_error(state, peekAt(state), err_msg));
      return false;
    }
    state->last_token = peekAt(state);
    if (state->cursor < state->tokens.size()) ++state->cursor;
    return true;
  }

} // namespace par

namespace par { // Include resolving

  static Path resolveRootDirectory(const Path& path) {
    std::error_code err;
    Path full_root_path = std::filesystem::weakly_canonical(path, err);
    if (err) {
      panic(fmt::format("Could not resolve path '{}'\nError: {}", path.string(), err.message()));
    }
    return full_root_path;
  }

  void Parser::initRootState(const Path& full_root_path, const Path& original_path) {
    std::optional<std::string> source = m_hooks.read_file(full_root_path);
    if (source == std::nullopt) {
      panic(fmt::format("Could not open root source file '{}'", original_path.string()));
      return;
    }
    lex::Lexer lexer(source.value(), full_root_path);
    lex::TokenStream tokens = lexer.tokenize();
    auto line_cache = lexer.takeLineCache();
    m_states[full_root_path] = State{.tokens = std::move(tokens), .line_cache = std::move(line_cache), .last_token = {}};
  }

  void Parser::parseCurrentFile(const Path& current_path) {
    LOG_DEBUG_FLUSH("Parsing full file {}", current_path.string());
    try {
      if (!parse(&m_states[current_path])) {
        panic(fmt::format("Failed to parse '{}'", current_path.string()));
      }
      m_load_order.push_back(current_path);
    } catch (const std::exception& e) {
      panic(fmt::format("Error parsing '{}': {}", current_path.string(), e.what()));
    }
  }

  bool Parser::loadIncludeSource(const Path& include_path) {
    LOG_DEBUG_FLUSH("Resolving include path: {}", include_path.string());

    // Native modules (.so/.dll/.dylib) have no source to lex give them a
    // sentinel empty block so the final validity check in parseRoot passes,
    // and let the evaluator handle them via loadDynamicLib.
    auto ext = include_path.extension();
    if (ext == ".so" || ext == ".dll" || ext == ".dylib") {
      lex::TokenStream sentinel;
      sentinel.push_back(makeSyntheticEof());
      m_states[include_path] = State{.tokens = std::move(sentinel), .block = std::make_unique<Block>(std::vector<std::unique_ptr<Node>>()), .last_token = {}};
      return true;
    }

    std::optional<std::string> source = m_hooks.read_file(include_path);
    if (source == std::nullopt) {
      panic(fmt::format("Could not open included source file '{}'", include_path.string()));
      return false;
    }
    lex::Lexer lexer(source.value(), include_path);
    lex::TokenStream tokens = lexer.tokenize();
    auto line_cache = lexer.takeLineCache();
    m_states[include_path] = State{.tokens = std::move(tokens), .line_cache = std::move(line_cache), .last_token = {}};
    return true;
  }

  std::optional<Program> Parser::parseRoot(const Path& path) {
    Path full_root_path = resolveRootDirectory(path);
    LOG_DEBUG_FLUSH("Full root path: {}", full_root_path.string());

    initRootState(full_root_path, path);

    m_include_stack.push_back(full_root_path);

    if (m_parallel) return parseRootParallel();

    while (!m_include_stack.empty()) {
      Path current_path = m_include_stack.back();
      m_include_stack.pop_back();

      auto ext = current_path.extension();
      if (ext == ".so" || ext == ".dll" || ext == ".dylib") {
        m_load_order.push_back(current_path);
        continue;
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

      if (m_states.contains(resolved)) {
        m_include_stack.push_back(current_path);
        continue;
      }

      if (!loadIncludeSource(resolved)) break;
      m_include_stack.push_back(current_path);
      m_include_stack.push_back(resolved);
    }

    Program program;
    program.root_path = full_root_path;
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

  std::expected<std::optional<Path>, std::string> Parser::checkForInclude(const Path& path) {
    State& state = m_states[path];

    if (!check(&state, lex::TokenType::INCLUDE)) {
      state.past_include_zone = true;
      return std::nullopt;
    }

    if (state.past_include_zone) {
      return std::unexpected(m_hooks.format_error(&state, state.last_token, "includes must appear at the top of the file"));
    }

    advance(&state);

    if (!match(&state, lex::TokenType::STRING)) {
      return std::unexpected(m_hooks.format_error(&state, state.last_token, "Expected string literal path after 'include'"));
    }

    std::optional<Path> include_path = getFromVariant<std::string>(state.last_token).value();
    if (!include_path) {
      return std::unexpected(m_hooks.format_error(&state, state.last_token, "Failed to get path from 'include'"));
    }

    if (!match(&state, lex::TokenType::SEMICOLON)) {
      return std::unexpected(m_hooks.format_error(&state, state.last_token, "Expected ';' after 'include' expression"));
    }

    include_path = m_hooks.resolve_file(path.parent_path(), include_path.value());
    if (!include_path) {
      return std::unexpected(m_hooks.format_error(&state, state.last_token, "Failed to resolve include path"));
    }

    state.includes.push_back(include_path.value());
    return include_path;
  }

} // namespace par

namespace par { // Base parsing loop functions
  bool Parser::parse(State* state) {
    std::vector<std::unique_ptr<Node>> statements;
    while (!isAtEnd(state)) {
      std::unique_ptr<Node> stmt = parseStatement(state);
      if (stmt == nullptr) return false;
      statements.push_back(std::move(stmt));
    }
    state->block = std::make_unique<Block>(std::move(statements));
    return true;
  }

  static bool isAssignmentOp(lex::TokenType type) {
    switch (type) {
      case lex::TokenType::ASSIGN:
      case lex::TokenType::PLUS_ASSIGN:
      case lex::TokenType::MINUS_ASSIGN:
      case lex::TokenType::STAR_ASSIGN:
      case lex::TokenType::SLASH_ASSIGN:
      case lex::TokenType::PERCENT_ASSIGN: return true;
      default: return false;
    }
  }

  int Parser::bindingPower(const lex::TokenType& type) {
    switch (type) {
      case lex::TokenType::QUESTION_MARK:
      case lex::TokenType::ASSIGN:
      case lex::TokenType::PLUS_ASSIGN:
      case lex::TokenType::MINUS_ASSIGN:
      case lex::TokenType::STAR_ASSIGN:
      case lex::TokenType::SLASH_ASSIGN:
      case lex::TokenType::PERCENT_ASSIGN: return 1;

      case lex::TokenType::OR: return 2;
      case lex::TokenType::AND: return 3;
      case lex::TokenType::EQUALS:
      case lex::TokenType::NOT_EQUALS: return 4;
      case lex::TokenType::LESS_THAN:
      case lex::TokenType::LESS_THAN_EQUALS:
      case lex::TokenType::GREATER_THAN:
      case lex::TokenType::GREATER_THAN_EQUALS: return 5;
      case lex::TokenType::PLUS:
      case lex::TokenType::MINUS: return 6;
      case lex::TokenType::STAR:
      case lex::TokenType::SLASH:
      case lex::TokenType::PERCENT: return 7;
      case lex::TokenType::DOT:
      case lex::TokenType::L_BRACK:
      case lex::TokenType::L_PAREN: return 8;

      case lex::TokenType::SIS_EOF:
      case lex::TokenType::ILLEGAL:
      case lex::TokenType::IDENT:
      case lex::TokenType::NUM:
      case lex::TokenType::STRING:
      case lex::TokenType::TRUE:
      case lex::TokenType::FALSE:
      case lex::TokenType::SIS_NULL:
      case lex::TokenType::IF:
      case lex::TokenType::ELSE:
      case lex::TokenType::FOR:
      case lex::TokenType::WHILE:
      case lex::TokenType::SWITCH:
      case lex::TokenType::CASE:
      case lex::TokenType::RETURN:
      case lex::TokenType::BREAK:
      case lex::TokenType::CONTINUE:
      case lex::TokenType::DEFAULT:
      case lex::TokenType::FN:
      case lex::TokenType::PIN:
      case lex::TokenType::CLASS:
      case lex::TokenType::EXTENDS:
      case lex::TokenType::NEW:
      case lex::TokenType::THIS:
      case lex::TokenType::SUPER:
      case lex::TokenType::INCLUDE:
      case lex::TokenType::NOT:
      case lex::TokenType::R_PAREN:
      case lex::TokenType::L_BRACE:
      case lex::TokenType::R_BRACE:
      case lex::TokenType::R_BRACK:
      case lex::TokenType::COMMA:
      case lex::TokenType::COLON:
      case lex::TokenType::SEMICOLON:
      case lex::TokenType::ARROW:
      case lex::TokenType::COMMENT: return 0;
    }
    assert(false && "bindingPower: unnamed TokenType value");
    return 0;
  }

  std::unique_ptr<Node> Parser::parseAtom(State* state) {
    lex::Token tok = peekAt(state);

    switch (tok.type) {
      case lex::TokenType::NUM: {
        advance(state);
        std::optional<double> value = getFromVariant<double>(tok);
        if (value == std::nullopt) return nullptr;
        return makeNode<Literal>(tok.line, tok.column, value.value());
      }
      case lex::TokenType::STRING: {
        advance(state);
        std::optional<std::string> value = getFromVariant<std::string>(tok);
        if (value == std::nullopt) return nullptr;
        return makeNode<Literal>(tok.line, tok.column, value.value());
      }
      case lex::TokenType::TRUE: {
        advance(state);
        return makeNode<Literal>(tok.line, tok.column, true);
      }
      case lex::TokenType::FALSE: {
        advance(state);
        return makeNode<Literal>(tok.line, tok.column, false);
      }
      case lex::TokenType::SIS_NULL: {
        advance(state);
        return makeNode<Literal>(tok.line, tok.column, std::monostate{});
      }

      case lex::TokenType::IDENT: {
        advance(state);
        auto name = getFromVariant<std::string>(state->last_token);
        if (!name) {
          panic(m_hooks.format_error(state, state->last_token, "Empty identifier name"));
          return nullptr;
        }
        return makeNode<Identifier>(tok.line, tok.column, std::move(*name));
      }

      case lex::TokenType::NOT:
      case lex::TokenType::MINUS: {
        advance(state);
        std::unique_ptr<Node> operand = parseExpression(state, 8);
        if (operand == nullptr) return nullptr;
        return makeNode<Unary>(tok.line, tok.column, tok.type, operand.release());
      }

      case lex::TokenType::L_PAREN: {
        advance(state);
        std::unique_ptr<Node> inner = parseExpression(state, 1);
        if (inner == nullptr) return nullptr;
        if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after expression")) return nullptr;
        return inner;
      }

      case lex::TokenType::L_BRACK: {
        advance(state);
        std::vector<ArrayElement> elements;

        // Track the next auto-assigned numeric key for unkeyed entries.
        // Keyed entries don't advance this counter, matching Lua semantics:
        // [10: "x", "y", "z"] gives "y" key 0 and "z" key 1, not 11 and 12.
        while (!check(state, lex::TokenType::R_BRACK)) {
          if (check(state, lex::TokenType::SIS_EOF)) {
            panic("Unterminated array literal, expected ']'");
            return nullptr;
          }

          // Parse the first expression. It's either a standalone value, or the
          // key in a `key: value` pair we don't know which until we peek at
          // what follows it.
          std::unique_ptr<Node> first = parseExpression(state, 1);
          if (first == nullptr) return nullptr;

          ArrayElement elem;
          if (match(state, lex::TokenType::COLON)) {
            // key: value first expression was the key
            elem.key = std::move(first);
            elem.value = parseExpression(state, 1);
            if (elem.value == nullptr) return nullptr;
          } else {
            // bare value no explicit key
            elem.key = nullptr;
            elem.value = std::move(first);
          }

          elements.push_back(std::move(elem));
          if (!match(state, lex::TokenType::COMMA)) break;
        }

        if (!expect(state, lex::TokenType::R_BRACK, "Expected ']' after array elements")) return nullptr;
        return makeNode<ArrayLiteral>(tok.line, tok.column, std::move(elements));
      }

      case lex::TokenType::FN: return parseFnLiteral(state);
      case lex::TokenType::NEW: return parseNewExpr(state);
      case lex::TokenType::THIS: return parseThisOrSuper(state, false);
      case lex::TokenType::SUPER: return parseThisOrSuper(state, true);

      case lex::TokenType::SIS_EOF:
      case lex::TokenType::ILLEGAL:
      case lex::TokenType::IF:
      case lex::TokenType::ELSE:
      // case lex::TokenType::FOR:
      case lex::TokenType::WHILE:
      // case lex::TokenType::SWITCH:
      case lex::TokenType::CASE:
      case lex::TokenType::RETURN:
      case lex::TokenType::BREAK:
      case lex::TokenType::CONTINUE:
      case lex::TokenType::DEFAULT:
      case lex::TokenType::PIN:
      case lex::TokenType::CLASS:
      case lex::TokenType::EXTENDS:
      case lex::TokenType::INCLUDE:
      case lex::TokenType::PLUS:
      case lex::TokenType::STAR:
      case lex::TokenType::SLASH:
      case lex::TokenType::PERCENT:
      case lex::TokenType::QUESTION_MARK:
      case lex::TokenType::ASSIGN:
      case lex::TokenType::PLUS_ASSIGN:
      case lex::TokenType::MINUS_ASSIGN:
      case lex::TokenType::STAR_ASSIGN:
      case lex::TokenType::SLASH_ASSIGN:
      case lex::TokenType::PERCENT_ASSIGN:
      case lex::TokenType::EQUALS:
      case lex::TokenType::NOT_EQUALS:
      case lex::TokenType::LESS_THAN:
      case lex::TokenType::LESS_THAN_EQUALS:
      case lex::TokenType::GREATER_THAN:
      case lex::TokenType::GREATER_THAN_EQUALS:
      case lex::TokenType::AND:
      case lex::TokenType::OR:
      case lex::TokenType::R_PAREN:
      case lex::TokenType::L_BRACE:
      case lex::TokenType::R_BRACE:
      case lex::TokenType::R_BRACK:
      case lex::TokenType::COMMA:
      case lex::TokenType::DOT:
      case lex::TokenType::COLON:
      case lex::TokenType::SEMICOLON:
      case lex::TokenType::ARROW:
      case lex::TokenType::COMMENT: panic(m_hooks.format_error(state, tok, "Unexpected token in expression")); return nullptr;
    }
    assert(false && "parseAtom: unnamed TokenType value");
    return nullptr;
  }

  // parseContinuation "led" half of Pratt.
  // Called when we already have a left-hand expression and the next token can
  // extend it. The token's glueStrength told the driver loop it was worth
  // consuming; now we decide what node to build.
  std::unique_ptr<Node> Parser::parseContinuation(State* state, std::unique_ptr<Node> left) {
    lex::Token op = advance(state); // consume the operator / punctuation

    switch (op.type) {
      // Binary ops: left-associative, so recurse at prec+1.
      // Assignment ops: right-associative, so recurse at same prec (1).
      case lex::TokenType::PLUS:
      case lex::TokenType::MINUS:
      case lex::TokenType::STAR:
      case lex::TokenType::SLASH:
      case lex::TokenType::PERCENT:
      case lex::TokenType::EQUALS:
      case lex::TokenType::NOT_EQUALS:
      case lex::TokenType::LESS_THAN:
      case lex::TokenType::LESS_THAN_EQUALS:
      case lex::TokenType::GREATER_THAN:
      case lex::TokenType::GREATER_THAN_EQUALS:
      case lex::TokenType::AND:
      case lex::TokenType::OR: {
        int prec = bindingPower(op.type);
        size_t left_line = left->line;
        size_t left_column = left->column;
        std::unique_ptr<Node> right = parseExpression(state, prec + 1); // left-assoc
        if (right == nullptr) return nullptr;
        return makeNode<Binary>(left_line, left_column, op.type, std::move(left), std::move(right));
      }

      case lex::TokenType::ASSIGN:
      case lex::TokenType::PLUS_ASSIGN:
      case lex::TokenType::MINUS_ASSIGN:
      case lex::TokenType::STAR_ASSIGN:
      case lex::TokenType::SLASH_ASSIGN:
      case lex::TokenType::PERCENT_ASSIGN: {
        size_t left_line = left->line;
        size_t left_column = left->column;
        std::unique_ptr<Node> right = parseExpression(state, 1); // right-assoc: same prec
        if (right == nullptr) return nullptr;
        return makeNode<Binary>(left_line, left_column, op.type, std::move(left), std::move(right));
      }

      case lex::TokenType::QUESTION_MARK: {
        size_t left_line = left->line;
        size_t left_column = left->column;
        std::unique_ptr<Node> then_expr = parseExpression(state, 1);
        if (then_expr == nullptr) return nullptr;
        if (!expect(state, lex::TokenType::COLON, "Expected ':' in ternary expression")) return nullptr;
        std::unique_ptr<Node> else_expr = parseExpression(state, 1);
        if (else_expr == nullptr) return nullptr;
        return makeNode<Ternary>(left_line, left_column, std::move(left), std::move(then_expr), std::move(else_expr));
      }

      // obj.field consume the dot, then read the field name as an identifier
      case lex::TokenType::DOT: {
        size_t left_line = left->line;
        size_t left_column = left->column;
        lex::Token field_tok = advance(state);
        if (field_tok.type != lex::TokenType::IDENT) {
          panic(m_hooks.format_error(state, field_tok, "Expected field name after '.'"));
          return nullptr;
        }
        auto field = getFromVariant<std::string>(state->last_token);
        if (!field) {
          panic(m_hooks.format_error(state, field_tok, "Empty field name"));
          return nullptr;
        }
        return makeNode<MemberAccess>(left_line, left_column, std::move(left), std::move(*field));
      }

      // obj[index]
      case lex::TokenType::L_BRACK: {
        size_t left_line = left->line;
        size_t left_column = left->column;
        std::unique_ptr<Node> index = parseExpression(state, 1);
        if (index == nullptr) return nullptr;
        if (!expect(state, lex::TokenType::R_BRACK, "Expected ']' after subscript index")) return nullptr;
        return makeNode<Subscript>(left_line, left_column, std::move(left), std::move(index));
      }

      // callee(args) we already consumed '(', so just parse the arg list
      case lex::TokenType::L_PAREN: {
        size_t left_line = left->line;
        size_t left_column = left->column;
        std::optional<std::vector<std::unique_ptr<Node>>> args = parseExpressionList(state, lex::TokenType::R_PAREN);
        if (args == std::nullopt) return nullptr;
        if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after call arguments")) return nullptr;
        return makeNode<Call>(left_line, left_column, std::move(left), std::move(args.value()));
      }

      default: panic(m_hooks.format_error(state, op, "Unexpected token in infix position")); return nullptr;
    }
  }

  // parseExpression the Pratt driver loop.
  // Start with an atom (nud), then keep extending it (led) as long as what
  // follows binds tightly enough. min_prec controls how tightly: call with 1
  // to parse a full expression, with 8 for unary operands (so -a.b = -(a.b)).
  std::unique_ptr<Node> Parser::parseExpression(State* state, int min_prec) {
    std::unique_ptr<Node> left = parseAtom(state);
    if (left == nullptr) return nullptr;

    while (bindingPower(peekAt(state).type) >= min_prec) {
      left = parseContinuation(state, std::move(left));
      if (left == nullptr) return nullptr;
    }

    return left;
  }

  // parseExpressionList shared helper for comma-separated expression lists.
  // Used by array literals, call args, and new() args. Stops when it sees
  // `terminator` (doesn't consume, it's caller is responsible for that).
  std::optional<std::vector<std::unique_ptr<Node>>> Parser::parseExpressionList(State* state, lex::TokenType terminator) {
    std::vector<std::unique_ptr<Node>> list;
    if (check(state, terminator)) return list; // empty list

    while (true) {
      std::unique_ptr<Node> expr = parseExpression(state, 1);
      if (expr == nullptr) return std::nullopt;
      list.push_back(std::move(expr));
      if (!match(state, lex::TokenType::COMMA)) break;
    }
    return list;
  }

} // namespace par

namespace par { // Complex parsing structures

  // parseStatement dispatch table for statement-level grammar.
  // Keyword-led statements get their own function. Anything else
  // is an expression used as a statement (x = 5; / foo();).
  std::unique_ptr<Node> Parser::parseStatement(State* state) {
    lex::TokenType next = peekAt(state).type;

    switch (next) {
      case lex::TokenType::L_BRACE: return parseBlock(state);
      case lex::TokenType::IF: return parseIf(state);
      case lex::TokenType::WHILE: return parseWhile(state);
      case lex::TokenType::FOR: return parseFor(state);
      case lex::TokenType::SWITCH: return parseSwitch(state);
      case lex::TokenType::PIN: return parseVarDecl(state);
      case lex::TokenType::RETURN: return parseReturn(state);
      case lex::TokenType::FN: return parseTopLevelFn(state);
      case lex::TokenType::CLASS: return parseClassDecl(state);
      case lex::TokenType::BREAK: {
        lex::Token break_tok = advance(state);
        if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after 'break'")) return nullptr;
        return makeNode<Jump>(break_tok.line, break_tok.column, JumpKind::BREAK);
      }
      case lex::TokenType::CONTINUE: {
        lex::Token continue_tok = advance(state);
        if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after 'continue'")) return nullptr;
        return makeNode<Jump>(continue_tok.line, continue_tok.column, JumpKind::CONTINUE);
      }
      default: {
        // expression statement: parse an expression, demand a semicolon
        auto expr = parseExpression(state, 1);
        if (expr == nullptr) return nullptr;
        if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after expression")) return nullptr;
        size_t expr_line = expr->line;
        size_t expr_column = expr->column;
        return makeNode<ExprStmt>(expr_line, expr_column, std::move(expr));
      }
    }
  }

  std::unique_ptr<Node> Parser::parseBlock(State* state) {
    lex::Token brace_tok = peekAt(state);
    if (!expect(state, lex::TokenType::L_BRACE, "Expected '{'")) return nullptr;
    std::vector<std::unique_ptr<Node>> stmts;
    while (!check(state, lex::TokenType::R_BRACE) && !isAtEnd(state)) {
      auto stmt = parseStatement(state);
      if (stmt == nullptr) return nullptr;
      stmts.push_back(std::move(stmt));
    }
    if (!expect(state, lex::TokenType::R_BRACE, "Expected '}'")) return nullptr;
    auto node = std::make_unique<Block>(std::move(stmts));
    node->line = brace_tok.line;
    node->column = brace_tok.column;
    return node;
  }

  std::unique_ptr<Node> Parser::parseIf(State* state) {
    lex::Token if_tok = advance(state); // consume 'if'
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after 'if'")) return nullptr;
    auto condition = parseExpression(state, 1);
    if (condition == nullptr) return nullptr;
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after if condition")) return nullptr;
    auto then_branch = parseBlock(state);
    if (then_branch == nullptr) return nullptr;
    std::unique_ptr<Node> else_branch;
    if (match(state, lex::TokenType::ELSE)) {
      // else if or else block
      if (check(state, lex::TokenType::IF)) {
        else_branch = parseIf(state);
      } else {
        else_branch = parseBlock(state);
      }
      if (else_branch == nullptr) return nullptr;
    }
    auto node = std::make_unique<If>(std::move(condition), std::move(then_branch), std::move(else_branch));
    node->line = if_tok.line;
    node->column = if_tok.column;
    return node;
  }

  std::unique_ptr<Node> Parser::parseWhile(State* state) {
    lex::Token while_tok = advance(state); // consume 'while'
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after 'while'")) return nullptr;
    auto condition = parseExpression(state, 1);
    if (condition == nullptr) return nullptr;
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after while condition")) return nullptr;
    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;
    auto node = std::make_unique<While>(std::move(condition), std::move(body));
    node->line = while_tok.line;
    node->column = while_tok.column;
    return node;
  }

  // Shared by parseVarDecl (which demands a trailing ';') and parseFor's
  // init clause (which doesn't, the for-loop's own ';' covers it).
  std::unique_ptr<Node> Parser::parseVarDeclNoSemicolon(State* state) {
    lex::Token pin_tok = advance(state); // consume 'pin'
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected variable name after 'pin'"));
      return nullptr;
    }
    auto name = getFromVariant<std::string>(state->last_token);
    if (!name) {
      panic(m_hooks.format_error(state, name_tok, "Empty variable name"));
      return nullptr;
    }
    std::unique_ptr<Node> initializer;
    if (match(state, lex::TokenType::ASSIGN)) {
      initializer = parseExpression(state, 1);
      if (initializer == nullptr) return nullptr;
    }
    auto node = std::make_unique<VarDecl>(std::move(*name), std::move(initializer));
    node->line = pin_tok.line;
    node->column = pin_tok.column;
    return node;
  }

  std::unique_ptr<Node> Parser::parseVarDecl(State* state) {
    auto node = parseVarDeclNoSemicolon(state);
    if (node == nullptr) return nullptr;
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after variable declaration")) return nullptr;
    return node;
  }

  std::unique_ptr<Node> Parser::parseReturn(State* state) {
    lex::Token return_tok = advance(state); // consume 'return'
    std::unique_ptr<Node> value;
    if (!check(state, lex::TokenType::SEMICOLON)) {
      value = parseExpression(state, 1);
      if (value == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after return")) return nullptr;
    auto node = std::make_unique<Return>(std::move(value));
    node->line = return_tok.line;
    node->column = return_tok.column;
    return node;
  }

  // shared helper: parses '(' param, param, ... ')' and returns the param name list.
  // Called by both parseFnLiteral and parseTopLevelFn.
  std::optional<std::vector<std::string>> Parser::parseParamList(State* state) {
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after 'fn'")) return std::nullopt;
    std::vector<std::string> params;
    while (!check(state, lex::TokenType::R_PAREN) && !isAtEnd(state)) {
      lex::Token param_tok = advance(state);
      if (param_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, param_tok, "Expected parameter name"));
        return std::nullopt;
      }
      auto param = getFromVariant<std::string>(state->last_token);
      if (!param) {
        panic(m_hooks.format_error(state, param_tok, "Empty parameter name"));
        return std::nullopt;
      }
      params.push_back(std::move(*param));
      if (!match(state, lex::TokenType::COMMA)) break;
    }
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after parameters")) return std::nullopt;
    return params;
  }

  // parseFnLiteral called from parseAtom when 'fn' appears in expression
  // position (i.e. anonymous fn, no name). Named fns at statement level are
  // handled in parseStatement as sugar for pin name = fn(...){...};
  std::unique_ptr<Node> Parser::parseFnLiteral(State* state) {
    lex::Token fn_tok = advance(state); // consume 'fn'
    auto params = parseParamList(state);
    if (!params) return nullptr;
    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;
    auto node = std::make_unique<FnLiteral>(std::move(*params), std::move(body));
    node->line = fn_tok.line;
    node->column = fn_tok.column;
    return node;
  }

  // fn at statement level = named fn decl stored as pin
  // fn name(...) {} is syntactic sugar for: pin name = fn(...) {};
  // We parse it here as a VarDecl whose initializer is a FnLiteral.
  // This keeps the AST uniform the evaluator never needs to distinguish
  // "function statement" from "pinned function expression."
  std::unique_ptr<Node> Parser::parseTopLevelFn(State* state) {
    advance(state); // consume 'fn'
    lex::Token fn_tok = state->last_token;
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected function name after 'fn'"));
      return nullptr;
    }
    auto name = getFromVariant<std::string>(state->last_token);
    if (!name) {
      panic(m_hooks.format_error(state, name_tok, "Empty function name"));
      return nullptr;
    }
    auto params = parseParamList(state);
    if (!params) return nullptr;
    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;
    auto fn = makeNode<FnLiteral>(fn_tok.line, fn_tok.column, std::move(*params), std::move(body));
    return makeNode<VarDecl>(fn_tok.line, fn_tok.column, std::move(*name), std::move(fn));
  }

  std::unique_ptr<Node> Parser::parseNewExpr(State* state) {
    lex::Token new_tok = advance(state); // consume 'new'
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected class name after 'new'"));
      return nullptr;
    }
    auto class_name = getFromVariant<std::string>(state->last_token);
    if (!class_name) {
      panic(m_hooks.format_error(state, name_tok, "Empty class name"));
      return nullptr;
    }

    // Support qualified names: new ns.ClassName()
    while (check(state, lex::TokenType::DOT)) {
      advance(state); // consume '.'
      lex::Token part_tok = advance(state);
      if (part_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, part_tok, "Expected class name after '.' in 'new'"));
        return nullptr;
      }
      auto part = getFromVariant<std::string>(state->last_token);
      if (!part) {
        panic(m_hooks.format_error(state, part_tok, "Empty class name after '.'"));
        return nullptr;
      }
      *class_name += '.';
      *class_name += *part;
    }

    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after class name in 'new'")) return nullptr;
    std::optional<std::vector<std::unique_ptr<Node>>> args = parseExpressionList(state, lex::TokenType::R_PAREN);
    if (args == std::nullopt) return nullptr;
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after constructor arguments")) return nullptr;
    auto node = std::make_unique<NewExpr>(std::move(*class_name), std::move(args.value()));
    node->line = new_tok.line;
    node->column = new_tok.column;
    return node;
  }

  // parseThisOrSuper bare `this` or `super` in expression position.
  // The following ->field or ->method() becomes a MemberAccess/Call wrapped
  // around this Self node, handled automatically by parseContinuation's DOT case.
  // NOTE: grammar uses `->` for this/super access and `.` for regular objects.
  // accessing a field or method on a class REQUIRES '->' usage, `.` is not allowed.
  //
  // `this` alone is valid as a standalone expression (e.g. `return this;` for
  // method chaining). `super` alone is not, it has no value without a member
  // access, so `->` remains mandatory for super.
  std::unique_ptr<Node> Parser::parseThisOrSuper(State* state, bool is_super) {
    lex::Token self_tok = advance(state); // consume this/super

    if (!check(state, lex::TokenType::ARROW)) {
      if (is_super) {
        panic(m_hooks.format_error(state, peekAt(state), "Expected '->' after 'super'"));
        return nullptr;
      }
      // Bare `this` with no following `->`: return a Self node directly so
      // `return this;` and similar expressions evaluate to the instance.
      auto self = std::make_unique<Self>(false);
      self->line = self_tok.line;
      self->column = self_tok.column;
      return self;
    }

    advance(state); // consume '->'

    lex::Token member_tok = advance(state);
    if (member_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, member_tok, "Expected member name after '->'"));
      return nullptr;
    }

    auto member = getFromVariant<std::string>(state->last_token);
    if (!member) {
      panic(m_hooks.format_error(state, member_tok, "Empty member name"));
      return nullptr;
    }

    auto self = std::make_unique<Self>(is_super);
    self->line = self_tok.line;
    self->column = self_tok.column;
    auto node = std::make_unique<MemberAccess>(std::move(self), std::move(*member));
    node->line = self_tok.line;
    node->column = self_tok.column;
    return node;
  }

  std::unique_ptr<Node> Parser::parseFor(State* state) {
    lex::Token for_tok = advance(state); // consume 'for'
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after 'for'")) return nullptr;

    std::unique_ptr<Node> init;
    if (!check(state, lex::TokenType::SEMICOLON)) {
      init = check(state, lex::TokenType::PIN) ? parseVarDeclNoSemicolon(state) : parseExpression(state, 1);
      if (init == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after for-loop initializer")) return nullptr;

    std::unique_ptr<Node> condition;
    if (!check(state, lex::TokenType::SEMICOLON)) {
      condition = parseExpression(state, 1);
      if (condition == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after for-loop condition")) return nullptr;

    std::unique_ptr<Node> increment;
    if (!check(state, lex::TokenType::R_PAREN)) {
      increment = parseExpression(state, 1);
      if (increment == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after for-loop clauses")) return nullptr;

    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;

    return makeNode<For>(for_tok.line, for_tok.column, std::move(init), std::move(condition), std::move(increment), std::move(body));
  }

  std::unique_ptr<Node> Parser::parseSwitch(State* state) {
    lex::Token switch_tok = advance(state); // consume 'switch'
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after 'switch'")) return nullptr;
    auto subject = parseExpression(state, 1);
    if (subject == nullptr) return nullptr;
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after switch subject")) return nullptr;
    if (!expect(state, lex::TokenType::L_BRACE, "Expected '{' after switch")) return nullptr;

    std::vector<SwitchCase> cases;
    bool seen_default = false;
    while (!check(state, lex::TokenType::R_BRACE) && !isAtEnd(state)) {
      SwitchCase c;
      if (match(state, lex::TokenType::CASE)) {
        c.value = parseExpression(state, 1);
        if (c.value == nullptr) return nullptr;
      } else if (match(state, lex::TokenType::DEFAULT)) {
        if (seen_default) {
          panic(m_hooks.format_error(state, state->last_token, "Duplicate 'default' in switch"));
          return nullptr;
        }
        seen_default = true;
      } else {
        panic(m_hooks.format_error(state, peekAt(state), "Expected 'case' or 'default' in switch body"));
        return nullptr;
      }
      if (!expect(state, lex::TokenType::COLON, "Expected ':' after case label")) return nullptr;

      while (!check(state, lex::TokenType::CASE) && !check(state, lex::TokenType::DEFAULT) && !check(state, lex::TokenType::R_BRACE) && !isAtEnd(state)) {
        auto stmt = parseStatement(state);
        if (stmt == nullptr) return nullptr;
        c.body.push_back(std::move(stmt));
      }
      cases.push_back(std::move(c));
    }
    if (!expect(state, lex::TokenType::R_BRACE, "Expected '}' after switch body")) return nullptr;

    return makeNode<Switch>(switch_tok.line, switch_tok.column, std::move(subject), std::move(cases));
  }

  std::unique_ptr<VarDecl> Parser::parseClassField(State* state) {
    // field declaration
    lex::Token field_pin_tok = advance(state); // consume 'pin'
    lex::Token field_tok = advance(state);
    if (field_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, field_tok, "Expected field name"));
      return nullptr;
    }
    auto field_name = getFromVariant<std::string>(state->last_token);
    if (!field_name) {
      panic(m_hooks.format_error(state, field_tok, "Empty field name"));
      return nullptr;
    }
    std::unique_ptr<Node> field_init;
    if (match(state, lex::TokenType::ASSIGN)) {
      field_init = parseExpression(state, 1);
      if (field_init == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after field declaration")) return nullptr;
    auto field_node = std::make_unique<VarDecl>(std::move(*field_name), std::move(field_init));
    field_node->line = field_pin_tok.line;
    field_node->column = field_pin_tok.column;
    return field_node;
  }

  std::optional<std::vector<std::string>> Parser::parseClassMethodParams(State* state) {
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after method name")) return std::nullopt;
    std::vector<std::string> params;
    while (!check(state, lex::TokenType::R_PAREN) && !isAtEnd(state)) {
      lex::Token param_tok = advance(state);
      if (param_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, param_tok, "Expected parameter name"));
        return std::nullopt;
      }
      auto param = getFromVariant<std::string>(state->last_token);
      if (!param) {
        panic(m_hooks.format_error(state, param_tok, "Empty parameter name"));
        return std::nullopt;
      }
      params.push_back(std::move(*param));
      if (!match(state, lex::TokenType::COMMA)) break;
    }
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after method parameters")) return std::nullopt;
    return params;
  }

  std::unique_ptr<FnLiteral> Parser::parseClassMethod(State* state, std::string* out_name) {
    // method declaration
    lex::Token method_fn_tok = advance(state); // consume 'fn'
    lex::Token method_tok = advance(state);
    if (method_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, method_tok, "Expected method name"));
      return nullptr;
    }
    auto method_name = getFromVariant<std::string>(state->last_token);
    if (!method_name) {
      panic(m_hooks.format_error(state, method_tok, "Empty method name"));
      return nullptr;
    }
    auto params = parseClassMethodParams(state);
    if (!params) return nullptr;
    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;
    *out_name = std::move(*method_name);
    auto method_node = std::make_unique<FnLiteral>(std::move(*params), std::move(body));
    method_node->line = method_fn_tok.line;
    method_node->column = method_fn_tok.column;
    return method_node;
  }

  bool Parser::parseClassHeader(State* state, std::string* out_name, std::string* out_parent_name) const {
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected class name"));
      return false;
    }
    auto name = getFromVariant<std::string>(state->last_token);
    if (!name) {
      panic(m_hooks.format_error(state, name_tok, "Empty class name"));
      return false;
    }

    std::string parent_name;
    if (match(state, lex::TokenType::EXTENDS)) {
      lex::Token parent_tok = advance(state);
      if (parent_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, parent_tok, "Expected parent class name after 'extends'"));
        return false;
      }
      auto pname = getFromVariant<std::string>(state->last_token);
      if (!pname) {
        panic(m_hooks.format_error(state, parent_tok, "Empty parent class name"));
        return false;
      }
      parent_name = std::move(*pname);
    }

    *out_name = std::move(*name);
    *out_parent_name = std::move(parent_name);
    return true;
  }

  bool Parser::parseClassBody(State* state,
                              std::vector<std::unique_ptr<VarDecl>>* out_fields,
                              std::vector<std::unique_ptr<FnLiteral>>* out_methods,
                              std::vector<std::string>* out_method_names) {
    while (!check(state, lex::TokenType::R_BRACE) && !isAtEnd(state)) {
      if (check(state, lex::TokenType::PIN)) {
        auto field_node = parseClassField(state);
        if (field_node == nullptr) return false;
        out_fields->push_back(std::move(field_node));

      } else if (check(state, lex::TokenType::FN)) {
        std::string method_name;
        auto method_node = parseClassMethod(state, &method_name);
        if (method_node == nullptr) return false;
        out_method_names->push_back(std::move(method_name));
        out_methods->push_back(std::move(method_node));
      } else {
        panic(m_hooks.format_error(state, peekAt(state), "Expected 'pin' or 'fn' in class body"));
        return false;
      }
    }
    return true;
  }

  std::unique_ptr<Node> Parser::parseClassDecl(State* state) {
    lex::Token class_tok = advance(state); // consume 'class'

    std::string name;
    std::string parent_name;
    if (!parseClassHeader(state, &name, &parent_name)) return nullptr;

    if (!expect(state, lex::TokenType::L_BRACE, "Expected '{' after class declaration")) return nullptr;

    std::vector<std::unique_ptr<VarDecl>> fields;
    std::vector<std::unique_ptr<FnLiteral>> methods;
    std::vector<std::string> method_names;
    if (!parseClassBody(state, &fields, &methods, &method_names)) return nullptr;

    if (!expect(state, lex::TokenType::R_BRACE, "Expected '}' after class body")) return nullptr;
    auto node = std::make_unique<ClassDecl>(std::move(name), std::move(parent_name), std::move(fields), std::move(methods), std::move(method_names));
    node->line = class_tok.line;
    node->column = class_tok.column;
    return node;
  }

} // namespace par
