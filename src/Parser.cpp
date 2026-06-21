#include <Logging.h>
#include <Parser.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>

// NOTE: placeholder
static void panic(const std::string_view msg) {
  std::print("PANIC: {}\n", msg.data());
  std::exit(1);
}

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

} // namespace par

namespace par { // Base parsing loop functions
  bool Parser::parse(State* state) {
    std::vector<std::unique_ptr<Node>> statements;

    while (!isAtEnd(state->lexer.get())) {
      std::unique_ptr<Node> stmt = parseStatement(state);
      if (stmt == nullptr) return false;
      statements.push_back(std::move(stmt));
    }

    m_root->statements.reserve(m_root->statements.size() + statements.size());
    m_root->statements.insert(m_root->statements.end(), std::make_move_iterator(statements.begin()), std::make_move_iterator(statements.end()));

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
        return makeNode<Literal>(tok.line, tok.column, std::get<double>(tok.value));
      }
      case lex::TokenType::STRING: {
        advance(state);
        return makeNode<Literal>(tok.line, tok.column, std::get<std::string>(tok.value));
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
        auto name = getFromVariant<std::string>(state->tokens.back());
        if (!name) {
          panic(m_hooks.format_error(state, state->tokens.back(), "Empty identifier name"));
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
        std::vector<std::unique_ptr<Node>> elements = parseExpressionList(state, lex::TokenType::R_BRACK);
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

      // obj.field consume the dot, then read the field name as an identifier
      case lex::TokenType::DOT: {
        size_t left_line = left->line;
        size_t left_column = left->column;
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
        std::vector<std::unique_ptr<Node>> args = parseExpressionList(state, lex::TokenType::R_PAREN);
        if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after call arguments")) return nullptr;
        return makeNode<Call>(left_line, left_column, std::move(left), std::move(args));
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

    while (bindingPower(state->lexer->peekToken().type) >= min_prec) {
      left = parseContinuation(state, std::move(left));
      if (left == nullptr) return nullptr;
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

} // namespace par

namespace par { // Complex parsing structures

  // parseStatement dispatch table for statement-level grammar.
  // Keyword-led statements get their own function. Anything else
  // is an expression used as a statement (x = 5; / foo();).
  std::unique_ptr<Node> Parser::parseStatement(State* state) {
    lex::TokenType next = state->lexer->peekToken().type;

    switch (next) {
      case lex::TokenType::L_BRACE: return parseBlock(state);
      case lex::TokenType::IF: return parseIf(state);
      case lex::TokenType::WHILE: return parseWhile(state);
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
    lex::Token brace_tok = state->lexer->peekToken();
    if (!expect(state, lex::TokenType::L_BRACE, "Expected '{'")) return nullptr;
    std::vector<std::unique_ptr<Node>> stmts;
    while (!check(state->lexer.get(), lex::TokenType::R_BRACE) && !isAtEnd(state->lexer.get())) {
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
      if (check(state->lexer.get(), lex::TokenType::IF)) {
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

  std::unique_ptr<Node> Parser::parseVarDecl(State* state) {
    lex::Token pin_tok = advance(state); // consume 'pin'
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected variable name after 'pin'"));
      return nullptr;
    }
    auto name = getFromVariant<std::string>(state->tokens.back());
    if (!name) {
      panic(m_hooks.format_error(state, name_tok, "Empty variable name"));
      return nullptr;
    }
    std::unique_ptr<Node> initializer;
    if (match(state, lex::TokenType::ASSIGN)) {
      initializer = parseExpression(state, 1);
      if (initializer == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after variable declaration")) return nullptr;
    auto node = std::make_unique<VarDecl>(std::move(*name), std::move(initializer));
    node->line = pin_tok.line;
    node->column = pin_tok.column;
    return node;
  }

  std::unique_ptr<Node> Parser::parseReturn(State* state) {
    lex::Token return_tok = advance(state); // consume 'return'
    std::unique_ptr<Node> value;
    if (!check(state->lexer.get(), lex::TokenType::SEMICOLON)) {
      value = parseExpression(state, 1);
      if (value == nullptr) return nullptr;
    }
    if (!expect(state, lex::TokenType::SEMICOLON, "Expected ';' after return")) return nullptr;
    auto node = std::make_unique<Return>(std::move(value));
    node->line = return_tok.line;
    node->column = return_tok.column;
    return node;
  }

  // parseFnLiteral called from parseAtom when 'fn' appears in expression
  // position (i.e. anonymous fn, no name). Named fns at statement level are
  // handled in parseStatement as sugar for pin name = fn(...){...};
  std::unique_ptr<Node> Parser::parseFnLiteral(State* state) {
    lex::Token fn_tok = advance(state); // consume 'fn'
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after 'fn'")) return nullptr;
    std::vector<std::string> params;
    while (!check(state->lexer.get(), lex::TokenType::R_PAREN) && !isAtEnd(state->lexer.get())) {
      lex::Token param_tok = advance(state);
      if (param_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, param_tok, "Expected parameter name"));
        return nullptr;
      }
      auto param = getFromVariant<std::string>(state->tokens.back());
      if (!param) {
        panic(m_hooks.format_error(state, param_tok, "Empty parameter name"));
        return nullptr;
      }
      params.push_back(std::move(*param));
      if (!match(state, lex::TokenType::COMMA)) break;
    }
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after parameters")) return nullptr;
    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;
    auto node = std::make_unique<FnLiteral>(std::move(params), std::move(body));
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
    lex::Token fn_tok = state->tokens.back();
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected function name after 'fn'"));
      return nullptr;
    }
    auto name = getFromVariant<std::string>(state->tokens.back());
    if (!name) {
      panic(m_hooks.format_error(state, name_tok, "Empty function name"));
      return nullptr;
    }
    // Now parse the rest as if we saw fn without consuming the name
    // We need the '(' next
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after function name")) return nullptr;
    std::vector<std::string> params;
    while (!check(state->lexer.get(), lex::TokenType::R_PAREN) && !isAtEnd(state->lexer.get())) {
      lex::Token param_tok = advance(state);
      if (param_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, param_tok, "Expected parameter name"));
        return nullptr;
      }
      auto param = getFromVariant<std::string>(state->tokens.back());
      if (!param) {
        panic(m_hooks.format_error(state, param_tok, "Empty parameter name"));
        return nullptr;
      }
      params.push_back(std::move(*param));
      if (!match(state, lex::TokenType::COMMA)) break;
    }
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after parameters")) return nullptr;
    auto body = parseBlock(state);
    if (body == nullptr) return nullptr;
    auto fn = makeNode<FnLiteral>(fn_tok.line, fn_tok.column, std::move(params), std::move(body));
    return makeNode<VarDecl>(fn_tok.line, fn_tok.column, std::move(*name), std::move(fn));
  }

  std::unique_ptr<Node> Parser::parseNewExpr(State* state) {
    lex::Token new_tok = advance(state); // consume 'new'
    lex::Token name_tok = advance(state);
    if (name_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, name_tok, "Expected class name after 'new'"));
      return nullptr;
    }
    auto class_name = getFromVariant<std::string>(state->tokens.back());
    if (!class_name) {
      panic(m_hooks.format_error(state, name_tok, "Empty class name"));
      return nullptr;
    }
    if (!expect(state, lex::TokenType::L_PAREN, "Expected '(' after class name in 'new'")) return nullptr;
    auto args = parseExpressionList(state, lex::TokenType::R_PAREN);
    if (!expect(state, lex::TokenType::R_PAREN, "Expected ')' after constructor arguments")) return nullptr;
    auto node = std::make_unique<NewExpr>(std::move(*class_name), std::move(args));
    node->line = new_tok.line;
    node->column = new_tok.column;
    return node;
  }

  // parseThisOrSuper bare `this` or `super` in expression position.
  // The following ->field or ->method() becomes a MemberAccess/Call wrapped
  // around this Self node, handled automatically by parseContinuation's DOT case.
  // NOTE: grammar uses `->` for this/super access and `.` for regular objects.
  // accessing a field method on a class REQUIRES '->' usage, `.` is not allowed.
  std::unique_ptr<Node> Parser::parseThisOrSuper(State* state, bool is_super) {
    lex::Token self_tok = advance(state); // consume this/super

    if (!expect(state, lex::TokenType::ARROW, "Expected '->' after 'this' or 'super'")) {
      return nullptr;
    }

    lex::Token member_tok = advance(state);
    if (member_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, member_tok, "Expected member name after '->'"));
      return nullptr;
    }

    auto member = getFromVariant<std::string>(state->tokens.back());
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

  std::unique_ptr<VarDecl> Parser::parseClassField(State* state) {
    // field declaration
    lex::Token field_pin_tok = advance(state); // consume 'pin'
    lex::Token field_tok = advance(state);
    if (field_tok.type != lex::TokenType::IDENT) {
      panic(m_hooks.format_error(state, field_tok, "Expected field name"));
      return nullptr;
    }
    auto field_name = getFromVariant<std::string>(state->tokens.back());
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
    while (!check(state->lexer.get(), lex::TokenType::R_PAREN) && !isAtEnd(state->lexer.get())) {
      lex::Token param_tok = advance(state);
      if (param_tok.type != lex::TokenType::IDENT) {
        panic(m_hooks.format_error(state, param_tok, "Expected parameter name"));
        return std::nullopt;
      }
      auto param = getFromVariant<std::string>(state->tokens.back());
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
    auto method_name = getFromVariant<std::string>(state->tokens.back());
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
    auto name = getFromVariant<std::string>(state->tokens.back());
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
      auto pname = getFromVariant<std::string>(state->tokens.back());
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

  bool Parser::parseClassBody(State* state, std::vector<std::unique_ptr<VarDecl>>* out_fields, std::vector<std::unique_ptr<FnLiteral>>* out_methods, std::vector<std::string>* out_method_names) {
    while (!check(state->lexer.get(), lex::TokenType::R_BRACE) && !isAtEnd(state->lexer.get())) {
      if (check(state->lexer.get(), lex::TokenType::PIN)) {
        auto field_node = parseClassField(state);
        if (field_node == nullptr) return false;
        out_fields->push_back(std::move(field_node));

      } else if (check(state->lexer.get(), lex::TokenType::FN)) {
        std::string method_name;
        auto method_node = parseClassMethod(state, &method_name);
        if (method_node == nullptr) return false;
        out_method_names->push_back(std::move(method_name));
        out_methods->push_back(std::move(method_node));
      } else {
        panic(m_hooks.format_error(state, state->lexer->peekToken(), "Expected 'pin' or 'fn' in class body"));
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
