#include <FinalParser.h>
#include <Logging.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>

// NOTE: placeholder
static void panic(const std::string_view msg) {
  std::print("PANIC: {}\n", msg.data());
  std::exit(1);
}

namespace fpar { // Hooks
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

    std::string line_content = state->lexer->getLineContent(token.line);
    while (!line_content.empty() && (line_content.back() == '\n' || line_content.back() == '\r')) {
      line_content.pop_back();
    }

    return fmted_msg + fmt::format("{}\n{}", line_content, std::string(token.column - 1, ' ') + std::string(std::max<size_t>(token.length, 1), '^'));
  }

  std::optional<Path> resolveFile(const Path& root, const Path& relative_include_path) {
    std::filesystem::path full = root / relative_include_path;

    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(full, ec);

    if (ec) {
      return std::nullopt;
    }

    if (!std::filesystem::is_regular_file(canonical)) {
      return std::nullopt;
    }

    return canonical;
  }

  Parser::Parser() {
    m_hooks.read_file = readFileToString;
    m_hooks.format_error = formatIllegalTokenMessage;
    m_hooks.resolve_file = resolveFile;
  }
} // namespace fpar

namespace fpar { // Helpers

  template <typename T>
  static std::optional<T> getFromVariant(const lex::Token& token) {
    if (const auto* val = std::get_if<T>(&token.value)) {
      return *val;
    }
    return std::nullopt;
  }

  bool Parser::isAtEnd(lex::Lexer* lexer) { return check(lexer, lex::TokenType::SIS_EOF); }

  bool Parser::check(lex::Lexer* lexer, lex::TokenType type) { return lexer->peekToken().type == type; }

  // advance: unconditionally consume and return the next token. For when you
  // already know what it is and just need to move past it.
  lex::Token Parser::advance(State* state) {
    lex::Token token = state->lexer->nextToken();
    state->tokens.push_back(token);
    return token;
  }

  // match: if the next token is `type`, consume it and return true.
  // Otherwise leave it alone and return false.
  bool Parser::match(State* state, lex::TokenType type) {
    if (!check(state->lexer.get(), type)) return false;
    state->tokens.push_back(state->lexer->nextToken());
    return true;
  }

  // expect: consumes the next token if it matches `type`, panics + returns false otherwise.
  bool Parser::expect(State* state, lex::TokenType type, std::string_view err_msg) const {
    if (state->lexer->peekToken().type != type) {
      panic(m_hooks.format_error(state, state->lexer->peekToken(), err_msg));
      return false;
    }
    state->tokens.push_back(state->lexer->nextToken());
    return true;
  }

} // namespace fpar

namespace fpar { // Include resolving

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
    m_states[full_root_path] = State{.lexer = std::make_unique<lex::Lexer>(source.value(), full_root_path), .tokens = {}, .token_idex = 0};
  }

  void Parser::parseCurrentFile(const Path& current_path) {
    LOG_DEBUG_FLUSH("Parsing full file {}", current_path.string());
    try { // TODO: remove try/catch after full refactor
      parse(&m_states[current_path]);
    } catch (const std::exception& e) {
      panic(fmt::format("Error parsing '{}': {}", current_path.string(), e.what()));
    }
  }

  bool Parser::loadIncludeSource(const Path& include_path) {
    LOG_DEBUG_FLUSH("Resolving include path: {}", include_path.string());
    std::optional<std::string> source = m_hooks.read_file(include_path);
    if (source == std::nullopt) {
      panic(fmt::format("Could not open included source file '{}'", include_path.string()));
      return false;
    }
    m_states[include_path] = State{.lexer = std::make_unique<lex::Lexer>(source.value(), include_path), .tokens = {}, .token_idex = 0};
    return true;
  }

  void Parser::parseRoot(const Path& path) {
    Path full_root_path = resolveRootDirectory(path);
    LOG_DEBUG_FLUSH("Full root path: {}", full_root_path.string());

    initRootState(full_root_path, path);

    m_include_stack.push_back(full_root_path);
    while (!m_include_stack.empty()) { // NOLINT
      Path current_path = m_include_stack.back();
      m_include_stack.pop_back();

      std::expected<std::optional<Path>, std::string> include_path = checkForInclude(current_path);
      if (!include_path.has_value()) {
        panic(include_path.error());
        break;
      }

      if (include_path.value() == std::nullopt) {
        parseCurrentFile(current_path);
        continue;
      }

      if (std::ranges::contains(m_include_stack, include_path.value().value())) {
        panic(fmt::format("Circular include detected at '{}'", include_path.value().value().string()));
        break;
      }

      if (!loadIncludeSource(include_path.value().value())) {
        break;
      }
      m_include_stack.push_back(current_path);
      m_include_stack.push_back(include_path.value().value());
    }
  }

  std::expected<std::optional<Path>, std::string> Parser::checkForInclude(const Path& path) {
    lex::Lexer* lexer = m_states[path].lexer.get();
    if (!check(lexer, lex::TokenType::INCLUDE)) {
      return std::nullopt;
    }

    advance(&m_states[path]);

    if (!match(&m_states[path], lex::TokenType::STRING)) {
      return std::unexpected(m_hooks.format_error(&m_states[path], m_states[path].tokens.back(), "Expected string literal path after 'include'"));
    }

    std::optional<Path> include_path = getFromVariant<std::string>(m_states[path].tokens.back()).value();

    if (!include_path) {
      return std::unexpected(m_hooks.format_error(&m_states[path], m_states[path].tokens.back(), "Failed to get path from 'include'"));
    }

    if (!match(&m_states[path], lex::TokenType::SEMICOLON)) {
      return std::unexpected(m_hooks.format_error(&m_states[path], m_states[path].tokens.back(), "Expected ';' after 'include' expression"));
    }

    include_path = m_hooks.resolve_file(path.parent_path(), include_path.value());
    if (!include_path) {
      return std::unexpected(m_hooks.format_error(&m_states[path], m_states[path].tokens.back(), "Failed to resolve include path"));
    }

    return include_path;
  }

  bool Parser::parse(State* state) {
    std::vector<std::unique_ptr<Node>> statements;
    lex::Lexer* lexer = state->lexer.get();
    while (!isAtEnd(lexer)) {
      std::unique_ptr<Node> stmt = parseStatement(state);
      if (!stmt) return false;
      statements.push_back(std::move(stmt));
    }

    m_root->statements.reserve(m_root->statements.size() + statements.size());
    m_root->statements.insert(m_root->statements.end(), std::make_move_iterator(statements.begin()), std::make_move_iterator(statements.end()));

    return true;
  }

} // namespace fpar

namespace fpar { // Base parsing loop
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

  int Parser::glueStrength(const lex::TokenType& type) {
    switch (type) {
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
      case lex::TokenType::QUESTION_MARK:
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
    assert(false && "glueStrength: unnamed TokenType value");
    return 0;
  }

  std::unique_ptr<Node> Parser::parseAtom(State* state) {
    lex::Lexer* lexer = state->lexer.get();
    lex::Token tok = lexer->peekToken();

    switch (tok.type) {
      case lex::TokenType::NUM: {
        advance(state);
        return std::make_unique<Literal>(std::get<double>(tok.value));
      }
      case lex::TokenType::STRING: {
        advance(state);
        return std::make_unique<Literal>(std::get<std::string>(tok.value));
      }
      case lex::TokenType::TRUE: {
        advance(state);
        return std::make_unique<Literal>(true);
      }
      case lex::TokenType::FALSE: {
        advance(state);
        return std::make_unique<Literal>(false);
      }
      case lex::TokenType::SIS_NULL: {
        advance(state);
        return std::make_unique<Literal>(std::monostate{});
      }

      case lex::TokenType::IDENT: {
        advance(state);
        auto name = getFromVariant<std::string>(state->tokens.back());
        if (!name) {
          panic(m_hooks.format_error(state, state->tokens.back(), "Empty identifier name"));
          return nullptr;
        }
        return std::make_unique<Identifier>(std::move(*name));
      }

      case lex::TokenType::NOT:
      case lex::TokenType::MINUS: {
        advance(state);
        std::unique_ptr<Node> operand = parseExpression(state, 8);
        if (!operand) return nullptr;
        return std::make_unique<Unary>(tok.type, operand.release());
      }

      case lex::TokenType::L_PAREN: {
        advance(state);
        std::unique_ptr<Node> inner = parseExpression(state, 1);
        if (!inner) return nullptr;
        if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after expression")) return nullptr;
        return inner;
      }

      case lex::TokenType::L_BRACK: {
        advance(state);
        std::vector<std::unique_ptr<Node>> elements = parseExpressionList(state, lex::TokenType::R_BRACK);
        if (!expect(state, lex::TokenType::R_BRACK, "Expected ']' after array elements")) return nullptr;
        return std::make_unique<ArrayLiteral>(std::move(elements));
      }

      case lex::TokenType::FN: return parseFnLiteral(state);
      case lex::TokenType::NEW: return parseNewExpr(state);
      case lex::TokenType::THIS: return parseThisOrSuper(state, false);
      case lex::TokenType::SUPER: return parseThisOrSuper(state, true);

      case lex::TokenType::SIS_EOF:
      case lex::TokenType::ILLEGAL:
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
        int prec = glueStrength(op.type);
        std::unique_ptr<Node> right = parseExpression(state, prec + 1); // left-assoc
        if (!right) return nullptr;
        return std::make_unique<Binary>(op.type, std::move(left), std::move(right));
      }

      case lex::TokenType::ASSIGN:
      case lex::TokenType::PLUS_ASSIGN:
      case lex::TokenType::MINUS_ASSIGN:
      case lex::TokenType::STAR_ASSIGN:
      case lex::TokenType::SLASH_ASSIGN:
      case lex::TokenType::PERCENT_ASSIGN: {
        std::unique_ptr<Node> right = parseExpression(state, 1); // right-assoc: same prec
        if (!right) return nullptr;
        return std::make_unique<Binary>(op.type, std::move(left), std::move(right));
      }

      // obj.field consume the dot, then read the field name as an identifier
      case lex::TokenType::DOT: {
        lex::Token field_tok = advance(state);
        if (field_tok.type != lex::TokenType::IDENT) {
          panic(m_hooks.format_error(state, field_tok, "Expected field name after '.'"));
          return nullptr;
        }
        auto field = getFromVariant<std::string>(state->tokens.back());
        if (!field) {
          panic(m_hooks.format_error(state, field_tok, "Empty field name"));
          return nullptr;
        }
        return std::make_unique<MemberAccess>(std::move(left), std::move(*field));
      }

      // obj[index]
      case lex::TokenType::L_BRACK: {
        std::unique_ptr<Node> index = parseExpression(state, 1);
        if (!index) return nullptr;
        if (!expect(state, lex::TokenType::R_BRACK, "Expected ']' after subscript index")) return nullptr;
        return std::make_unique<Subscript>(std::move(left), std::move(index));
      }

      // callee(args) we already consumed '(', so just parse the arg list
      case lex::TokenType::L_PAREN: {
        std::vector<std::unique_ptr<Node>> args = parseExpressionList(state, lex::TokenType::R_PAREN);
        if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after call arguments")) return nullptr;
        return std::make_unique<Call>(std::move(left), std::move(args));
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
    if (!left) return nullptr;

    while (glueStrength(state->lexer->peekToken().type) >= min_prec) {
      left = parseContinuation(state, std::move(left));
      if (!left) return nullptr;
    }

    return left;
  }

  // parseExpressionList shared helper for comma-separated expression lists.
  // Used by array literals, call args, and new() args. Stops when it sees
  // `terminator` (doesn't consume, it's caller is responsible for that).
  std::vector<std::unique_ptr<Node>> Parser::parseExpressionList(State* state, lex::TokenType terminator) {
    std::vector<std::unique_ptr<Node>> list;
    if (check(state->lexer.get(), terminator)) return list; // empty list

    while (true) {
      auto expr = parseExpression(state, 1);
      if (!expr) return {};
      list.push_back(std::move(expr));
      if (!match(state, lex::TokenType::COMMA)) break;
    }
    return list;
  }

} // namespace fpar
        }
        break;
      }

      case NodeType::BREAK: {
        std::print("{}{}Break\n", connector, label);
        break;
      }

      case NodeType::CONTINUE: {
        std::print("{}{}Continue\n", connector, label);
        break;
      }

      case NodeType::THIS_EXPR: {
        std::print("{}{}This\n", connector, label);
        break;
      }

      case NodeType::SUPER_ACCESS: {
        const auto* super_access = static_cast<const SuperAccess*>(node);
        std::print("{}{}{}(->{})\n", connector, label, super_access->is_super ? "Super" : "This", super_access->field);
        break;
      }

      case NodeType::NEW_EXPR: {
        const auto* new_expr = static_cast<const NewExpr*>(node);
        std::print("{}{}New({})\n", connector, label, new_expr->class_name);
        for (size_t i = 0; i < new_expr->args.size(); ++i) {
          printNode(new_expr->args[i].get(), child_prefix, i + 1 == new_expr->args.size());
        }
        break;
      }

      case NodeType::CLASS_DECL: {
        const auto* class_decl = static_cast<const ClassDecl*>(node);
        std::print("{}{}Class({}{})\n", connector, label, class_decl->name, class_decl->parent_name.empty() ? "" : (" extends " + class_decl->parent_name));
        size_t total = class_decl->fields.size() + class_decl->methods.size();
        size_t i = 0;
        for (const auto& field : class_decl->fields) {
          ++i;
          printNode(field.get(), child_prefix, i == total, "field: ");
        }
        for (size_t m = 0; m < class_decl->methods.size(); ++m) {
          ++i;
          printNode(class_decl->methods[m].get(), child_prefix, i == total, "method " + class_decl->method_names[m] + ": ");
        }
        break;
      }
    }
  }

} // namespace fpar
