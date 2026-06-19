#include <Logging.h>
#include <Parser.h>
#include <ParserNodeTypes.h>
#include <Token.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#include <sstream>

// NOTE: placeholder
static void panic(const std::string_view msg) { std::print("PANIC: {}\n", msg.data()); }

namespace par {
  // ---------------------------------------------------------------------
  // #include preprocessing
  //
  // This runs entirely on raw source TEXT, before the Lexer ever sees it.
  // Conceptually identical to what a C preprocessor does with #include:
  // scan for the directive, read the target file, recursively expand IT,
  // and splice the result in verbatim where the directive was. By the time
  // parse()/the Lexer run, includes simply don't exist anymore, it's all
  // one flat string.
  //
  // Why text instead of tokens or AST nodes: the Lexer in this codebase is
  // a single streaming pass with a small read-ahead buffer, it has no
  // "pause, lex this other file, then resume" capability, and patching that
  // in would be far more invasive than doing the splice a layer below it.
  // ---------------------------------------------------------------------

  std::optional<std::string> readFileToString(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  namespace {
    // Finds the byte offset of the next `include "..."` (or `include '...'`)
    // directive at or after `from`. An "include" is only recognized as the
    // directive (not, say, part of a longer identifier like "includes" or a
    // string literal containing the word) if it's preceded by nothing but
    // whitespace/start-of-line and followed by whitespace then a quote.
    // Returns std::string::npos if there are no more include directives.
    struct IncludeMatch {
      size_t directive_start; // index of the 'i' in "include"
      size_t directive_end; // index just past the closing ';' (or EOL if missing)
      std::string target_path;
    };

    [[nodiscard]] bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

    [[nodiscard]] std::optional<IncludeMatch> findNextInclude(const std::string& source, size_t from) {
      static const std::string kKeyword = "include";

      size_t search_pos = from;
      while (true) {
        size_t pos = source.find(kKeyword, search_pos);
        if (pos == std::string::npos) return std::nullopt;

        // Reject matches that are part of a longer identifier, e.g. "include_path".
        bool left_ok = (pos == 0) || !isIdentChar(source[pos - 1]);
        size_t after_keyword = pos + kKeyword.size();
        bool right_ok = (after_keyword >= source.size()) || !isIdentChar(source[after_keyword]);

        if (!left_ok || !right_ok) {
          search_pos = pos + kKeyword.size();
          continue;
        }

        // Skip whitespace between 'include' and the expected quote.
        size_t cursor = after_keyword;
        while (cursor < source.size() && std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;

        if (cursor >= source.size() || (source[cursor] != '"' && source[cursor] != '\'')) {
          // Not actually a directive (e.g. a variable named "include" used
          // as a regular identifier elsewhere), keep scanning past it.
          search_pos = pos + kKeyword.size();
          continue;
        }

        char quote = source[cursor];
        size_t path_start = cursor + 1;
        size_t path_end = source.find(quote, path_start);
        if (path_end == std::string::npos) {
          // Unterminated string, nothing sane to do, stop looking.
          return std::nullopt;
        }

        std::string target_path = source.substr(path_start, path_end - path_start);

        // Consume an optional trailing ';' and the rest of the line's
        // trailing whitespace so we don't leave a dangling stray semicolon
        // or blank-looking line behind after splicing.
        size_t directive_end = path_end + 1;
        size_t semi_scan = directive_end;
        while (semi_scan < source.size() && std::isspace(static_cast<unsigned char>(source[semi_scan])) && source[semi_scan] != '\n') ++semi_scan;
        if (semi_scan < source.size() && source[semi_scan] == ';') {
          directive_end = semi_scan + 1;
        }

        return IncludeMatch{.directive_start = pos, .directive_end = directive_end, .target_path = std::move(target_path)};
      }
    }
  } // namespace

  std::string preprocessIncludes(const std::string& source, const std::string& source_path, std::set<std::string>& already_included) {
    std::filesystem::path base_dir = std::filesystem::path(source_path).has_parent_path() ? std::filesystem::path(source_path).parent_path() : std::filesystem::path(".");

    std::string result;
    result.reserve(source.size());

    size_t cursor = 0;
    while (true) {
      auto match = findNextInclude(source, cursor);
      if (!match) {
        result.append(source, cursor, std::string::npos);
        break;
      }

      // Copy everything before the directive untouched.
      result.append(source, cursor, match->directive_start - cursor);

      std::filesystem::path resolved = std::filesystem::weakly_canonical(base_dir / match->target_path);
      std::string resolved_str = resolved.string();

      if (already_included.contains(resolved_str)) {
        // #pragma once semantics: silently drop this include, the file's
        // contents are already in the output from an earlier directive.
      } else {
        already_included.insert(resolved_str);

        auto included_source = readFileToString(resolved_str);
        if (!included_source) {
          panic(fmt::format("Could not open included file '{}' (resolved to '{}')", match->target_path, resolved_str));
        } else {
          // Recursively expand the included file's own includes BEFORE
          // splicing, so nested includes resolve relative to where they're
          // declared, not relative to the root file.
          result.append(preprocessIncludes(*included_source, resolved_str, already_included));
        }
      }

      cursor = match->directive_end;
    }

    return result;
  }

  std::string Parser::formatIllegalTokenMessage(lex::Lexer* lexer, const lex::Token& token, const std::string_view msg) {
    std::string fmted_msg = fmt::format("Illegal token received {}:{}\n", token.line, token.column);
    if (!msg.empty()) {
      fmted_msg += msg;
      fmted_msg += "\n";
    }

    std::string line_content = lexer->getLineContent(token.line);
    while (!line_content.empty() && (line_content.back() == '\n' || line_content.back() == '\r')) {
      line_content.pop_back();
    }

    return fmted_msg + fmt::format("{}\n{}", line_content, std::string(token.column - 1, ' ') + std::string(std::max<size_t>(token.length, 1), '^'));
  }

  static bool isAssignmentOperator(lex::TokenType type) {
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

  // Binding power table for binary operators, higher number binds tighter.
  // 0 means "not a binary operator" so parseBinary's loop stops there.
  static int getBinaryPrecedence(lex::TokenType type) {
    switch (type) {
      case lex::TokenType::OR: return 1;
      case lex::TokenType::AND: return 2;
      case lex::TokenType::EQUALS:
      case lex::TokenType::NOT_EQUALS: return 3;
      case lex::TokenType::LESS_THAN:
      case lex::TokenType::LESS_THAN_EQUALS:
      case lex::TokenType::GREATER_THAN:
      case lex::TokenType::GREATER_THAN_EQUALS: return 4;
      case lex::TokenType::PLUS:
      case lex::TokenType::MINUS: return 5;
      case lex::TokenType::STAR:
      case lex::TokenType::SLASH:
      case lex::TokenType::PERCENT: return 6;
      default: return 0;
    }
  }

  // check: does the next token match this type? No consuming, just looking.
  bool Parser::check(lex::Lexer* lexer, lex::TokenType type) { return lexer->peekToken().type == type; }

  // expect: consumes the next token if it matches `type`, panics + returns false otherwise.
  bool Parser::expect(lex::Lexer* lexer, lex::TokenType type, std::string_view err_msg) {
    if (lexer->peekToken().type != type) {
      panic(formatIllegalTokenMessage(lexer, lexer->peekToken(), err_msg));
      return false;
    }
    m_tokens.push_back(lexer->nextToken());
    return true;
  }

  // match: if the next token is `type`, consume it and return true. Otherwise leave
  // it alone and return false. This is "expect" without the panic for OPTIONAL
  // tokens, like checking for an else branch, or a trailing comma in a call.
  bool Parser::match(lex::Lexer* lexer, lex::TokenType type) {
    if (!check(lexer, type)) return false;
    m_tokens.push_back(lexer->nextToken());
    return true;
  }

  // matchAny: same as match, but against a list of acceptable types, useful for
  // things like "is this any kind of assignment operator" or "is this any
  // comparison operator" where several token types are valid here.
  bool Parser::matchAny(lex::Lexer* lexer, std::initializer_list<lex::TokenType> types) {
    // Replace loop by 'std::ranges::any_of()' 65:5:3 clang-tidy readability-use-anyofallof
    return std::ranges::any_of(types, [&](auto type) { return match(lexer, type); });
  }

  // advance: unconditionally consume and return the next token. For when you
  // already know what it is and just need to move past it.
  lex::Token Parser::advance(lex::Lexer* lexer) {
    lex::Token token = lexer->nextToken();
    m_tokens.push_back(token);
    return token;
  }

  // isAtEnd: are we out of tokens? Stops infinite loops in any while() that
  // parses a list of things (block statements, call args, etc.)
  bool Parser::isAtEnd(lex::Lexer* lexer) { return check(lexer, lex::TokenType::SIS_EOF); }

  template <typename T>
  static std::optional<T> getFromVariant(const lex::Token& token) {
    if (const auto* val = std::get_if<T>(&token.value)) {
      return *val;
    }
    return std::nullopt;
  }

  // Public entry point. Parses the whole input as a sequence of top level
  // statements and stores the result in m_root. Stops early and returns false
  // if any statement fails to parse, since there's nothing sensible to
  // recover into yet (no synchronize() logic).
  //
  // NOTE: this assumes `lexer` was already constructed from fully
  // include-expanded source. If you're starting from a file on disk, call
  // parseFile() instead, which does the expansion for you first.
  bool Parser::parse(lex::Lexer* lexer) {
    std::vector<std::unique_ptr<Node>> statements;

    while (!isAtEnd(lexer)) {
      std::unique_ptr<Node> stmt = parseStatement(lexer);
      if (!stmt) return false;
      statements.push_back(std::move(stmt));
    }

    m_root = std::make_unique<Block>(std::move(statements));
    return true;
  }

  bool Parser::parseFile(const std::string& path) {
    auto source = readFileToString(path);
    if (!source) {
      panic(fmt::format("Could not open source file '{}'", path));
      return false;
    }

    std::set<std::string> already_included;
    already_included.insert(std::filesystem::weakly_canonical(path).string());
    std::string expanded = preprocessIncludes(*source, path, already_included);

    lex::Lexer lexer(std::move(expanded));
    return parse(&lexer);
  }

  // Precedence-climbing: parses everything from `min_prec` upward in one loop
  // instead of a chain of parseAdd/parseMul/etc functions, same correctness,
  // fewer functions to maintain.
  std::unique_ptr<Node> Parser::parseBinary(lex::Lexer* lexer, int min_prec) {
    std::unique_ptr<Node> left = parseUnary(lexer);
    if (!left) return nullptr;

    while (true) {
      int prec = getBinaryPrecedence(lexer->peekToken().type);
      if (prec == 0 || prec < min_prec) break;

      lex::Token op = advance(lexer);
      std::unique_ptr<Node> right = parseBinary(lexer, prec + 1); // +1 = left-associative
      if (!right) return nullptr;

      left = std::make_unique<Binary>(op.type, std::move(left), std::move(right));
    }

    return left;
  }

  std::unique_ptr<Node> Parser::parseExpression(lex::Lexer* lexer) { return parseAssignment(lexer); }

  // Assignment sits below the binary operator chain: parses the left side as a
  // full expression first, then checks if an assignment operator follows.
  // Right-associative (a = b = c means a = (b = c)), unlike binary ops.
  std::unique_ptr<Node> Parser::parseAssignment(lex::Lexer* lexer) {
    std::unique_ptr<Node> left = parseBinary(lexer, 1);
    if (!left) return nullptr;

    if (isAssignmentOperator(lexer->peekToken().type)) {
      if (left->type != NodeType::IDENTIFIER && left->type != NodeType::MEMBER_ACCESS && left->type != NodeType::SUPER_ACCESS) {
        panic(formatIllegalTokenMessage(lexer, lexer->peekToken(), "Invalid assignment target"));
        return nullptr;
      }
      lex::Token op = advance(lexer);
      std::unique_ptr<Node> right = parseAssignment(lexer); // recurse for right-associativity
      if (!right) return nullptr;
      return std::make_unique<Binary>(op.type, std::move(left), std::move(right));
    }

    return left;
  }

  std::unique_ptr<Node> Parser::parseUnary(lex::Lexer* lexer) {
    if (check(lexer, lex::TokenType::NOT) || check(lexer, lex::TokenType::MINUS)) {
      lex::Token op = advance(lexer);
      std::unique_ptr<Node> operand = parseUnary(lexer); // right-recursive: -!x works
      if (!operand) return nullptr;
      return std::make_unique<Unary>(op.type, operand.release());
    }
    return parsePostfix(lexer);
  }

  // Handles chains of '.' and '(...)' on a base expression. A loop, not a fixed
  // lookahead check, because the chain can go arbitrarily deep
  // (e.g. ident.something().else.again()).
  //
  // ARROW ('->') is NOT handled here: it's only legal as the very first hop
  // off a bare `this`/`super`, which parsePrimary -> parseThisOrSuper already
  // consumes before parsePostfix gets a chance to run. By the time we're in
  // this loop, any ARROW use has already happened (or the source is invalid
  // and the dangling '->' will fail to match anything below, surfacing as a
  // normal "unexpected token" error from whatever called us next).
  std::unique_ptr<Node> Parser::parsePostfix(lex::Lexer* lexer) {
    std::unique_ptr<Node> expr = parsePrimary(lexer);
    if (!expr) return nullptr;

    while (true) {
      if (check(lexer, lex::TokenType::DOT)) {
        advance(lexer); // consume '.'
        if (!expect(lexer, lex::TokenType::IDENT, "Expected identifier after '.'")) {
          return nullptr;
        }
        auto field_name = getFromVariant<std::string>(m_tokens.back());
        if (!field_name) {
          panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty identifier after '.'"));
          return nullptr;
        }
        expr = std::make_unique<MemberAccess>(std::move(expr), std::move(*field_name));
      } else if (check(lexer, lex::TokenType::L_PAREN)) {
        expr = parseCall(lexer, std::move(expr));
        if (!expr) return nullptr;
      } else {
        break;
      }
    }

    return expr;
  }

  std::unique_ptr<Node> Parser::parsePrimary(lex::Lexer* lexer) {
    switch (lexer->peekToken().type) {
      case lex::TokenType::NUM:
      case lex::TokenType::STRING:
      case lex::TokenType::TRUE:
      case lex::TokenType::FALSE:
      case lex::TokenType::SIS_NULL: return parseLiteral(lexer);

      case lex::TokenType::IDENT: return parseIdentifier(lexer);

      case lex::TokenType::FN: return parseFnLiteral(lexer);

      case lex::TokenType::L_BRACK: return parseArrayLiteral(lexer);

      case lex::TokenType::NEW: return parseNewExpr(lexer);

      case lex::TokenType::THIS: return parseThisOrSuper(lexer, /*is_super=*/false);
      case lex::TokenType::SUPER: return parseThisOrSuper(lexer, /*is_super=*/true);

      case lex::TokenType::L_PAREN: {
        advance(lexer); // consume '('
        std::unique_ptr<Node> expr = parseExpression(lexer);
        if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after expression")) {
          return nullptr;
        }
        return expr;
      }

      default: panic(formatIllegalTokenMessage(lexer, lexer->peekToken(), "Unexpected token in expression")); return nullptr;
    }
  }

  std::unique_ptr<Node> Parser::parseLiteral(lex::Lexer* lexer) {
    lex::Token tok = advance(lexer);
    LiteralType value;

    switch (tok.type) {
      case lex::TokenType::NUM: value = std::get<double>(tok.value); break;
      case lex::TokenType::STRING: value = std::get<std::string>(tok.value); break;
      case lex::TokenType::TRUE: value = true; break;
      case lex::TokenType::FALSE: value = false; break;
      case lex::TokenType::SIS_NULL: value = std::monostate{}; break;
      default: panic(formatIllegalTokenMessage(lexer, tok, "Expected a literal")); return nullptr;
    }

    return std::make_unique<Literal>(std::move(value));
  }

  std::unique_ptr<Node> Parser::parseIdentifier(lex::Lexer* lexer) {
    if (!expect(lexer, lex::TokenType::IDENT, "Expected identifier")) {
      return nullptr;
    }
    auto name = getFromVariant<std::string>(m_tokens.back());
    if (!name) {
      panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty identifier name"));
      return nullptr;
    }
    return std::make_unique<Identifier>(std::move(*name));
  }

  // Takes an already-built callee (e.g. from parsePostfix) so chains like
  // ident.something() work. This only parses the '(' args ')' part.
  std::unique_ptr<Node> Parser::parseCall(lex::Lexer* lexer, std::unique_ptr<Node> callee) {
    advance(lexer); // consume '('
    std::vector<std::unique_ptr<Node>> args;

    if (!check(lexer, lex::TokenType::R_PAREN)) {
      do {
        std::unique_ptr<Node> arg = parseExpression(lexer);
        if (!arg) return nullptr;
        args.push_back(std::move(arg));
      } while (match(lexer, lex::TokenType::COMMA));
    }

    if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after call arguments")) {
      return nullptr;
    }

    return std::make_unique<Call>(std::move(callee), std::move(args));
  }

  std::unique_ptr<Node> Parser::parseArrayLiteral(lex::Lexer* lexer) {
    advance(lexer); // consume '['
    std::vector<std::unique_ptr<Node>> elements;

    if (!check(lexer, lex::TokenType::R_BRACK)) {
      do {
        std::unique_ptr<Node> elem = parseExpression(lexer);
        if (!elem) return nullptr;
        elements.push_back(std::move(elem));
      } while (match(lexer, lex::TokenType::COMMA));
    }

    if (!expect(lexer, lex::TokenType::R_BRACK, "Expected ']' after array elements")) {
      return nullptr;
    }

    return std::make_unique<ArrayLiteral>(std::move(elements));
  }

  std::unique_ptr<Node> Parser::parseFnLiteral(lex::Lexer* lexer) {
    advance(lexer); // consume 'fn'
    if (!expect(lexer, lex::TokenType::L_PAREN, "Expected '(' after 'fn'")) {
      return nullptr;
    }

    std::vector<std::string> params;
    if (!check(lexer, lex::TokenType::R_PAREN)) {
      do {
        if (!expect(lexer, lex::TokenType::IDENT, "Expected parameter name")) {
          return nullptr;
        }
        auto param_name = getFromVariant<std::string>(m_tokens.back());
        if (!param_name) {
          panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty parameter name"));
          return nullptr;
        }
        params.push_back(std::move(*param_name));
      } while (match(lexer, lex::TokenType::COMMA));
    }

    if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after parameters")) {
      return nullptr;
    }

    std::unique_ptr<Node> body = parseBlock(lexer);
    if (!body) return nullptr;

    return std::make_unique<FnLiteral>(std::move(params), std::move(body));
  }

  std::unique_ptr<Node> Parser::parseBlock(lex::Lexer* lexer) {
    if (!expect(lexer, lex::TokenType::L_BRACE, "Expected '{' to start block")) {
      return nullptr;
    }

    std::vector<std::unique_ptr<Node>> statements;
    while (!check(lexer, lex::TokenType::R_BRACE) && !isAtEnd(lexer)) {
      std::unique_ptr<Node> stmt = parseStatement(lexer);
      if (!stmt) return nullptr; // bail rather than loop forever on a bad statement
      statements.push_back(std::move(stmt));
    }

    if (!expect(lexer, lex::TokenType::R_BRACE, "Expected '}' to close block")) {
      return nullptr;
    }

    return std::make_unique<Block>(std::move(statements));
  }

  // Dispatches on what starts a statement. Used by parse() at the top level
  // and by parseBlock's loop for anything nested.
  std::unique_ptr<Node> Parser::parseStatement(lex::Lexer* lexer) {
    switch (lexer->peekToken().type) {
      case lex::TokenType::PIN: return parseVarDecl(lexer);
      case lex::TokenType::IF: return parseIf(lexer);
      case lex::TokenType::WHILE: return parseWhile(lexer);
      case lex::TokenType::L_BRACE: return parseBlock(lexer);
      case lex::TokenType::CLASS: return parseClassDecl(lexer);
      case lex::TokenType::RETURN: return parseReturn(lexer);
      case lex::TokenType::BREAK: return parseBreak(lexer);
      case lex::TokenType::CONTINUE: return parseContinue(lexer);
      default: return parseExprStmt(lexer);
    }
  }

  std::unique_ptr<Node> Parser::parseIf(lex::Lexer* lexer) {
    advance(lexer); // consume 'if'
    if (!expect(lexer, lex::TokenType::L_PAREN, "Expected '(' after 'if'")) return nullptr;
    std::unique_ptr<Node> condition = parseExpression(lexer);
    if (!condition) return nullptr;
    if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after condition")) return nullptr;

    std::unique_ptr<Node> then_branch = parseBlock(lexer);
    if (!then_branch) return nullptr;

    std::unique_ptr<Node> else_branch = nullptr;
    if (match(lexer, lex::TokenType::ELSE)) {
      else_branch = check(lexer, lex::TokenType::IF) ? parseIf(lexer) : parseBlock(lexer);
      if (!else_branch) return nullptr;
    }

    return std::make_unique<If>(std::move(condition), std::move(then_branch), std::move(else_branch));
  }

  std::unique_ptr<Node> Parser::parseWhile(lex::Lexer* lexer) {
    advance(lexer); // consume 'while'
    if (!expect(lexer, lex::TokenType::L_PAREN, "Expected '(' after 'while'")) return nullptr;
    std::unique_ptr<Node> condition = parseExpression(lexer);
    if (!condition) return nullptr;
    if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after condition")) return nullptr;

    std::unique_ptr<Node> body = parseBlock(lexer);
    if (!body) return nullptr;

    return std::make_unique<While>(std::move(condition), std::move(body));
  }

  std::unique_ptr<Node> Parser::parseExprStmt(lex::Lexer* lexer) {
    std::unique_ptr<Node> expr = parseExpression(lexer);
    if (!expr) return nullptr;
    if (!expect(lexer, lex::TokenType::SEMICOLON, "Expected ';' after expression")) {
      return nullptr;
    }
    return std::make_unique<ExprStmt>(std::move(expr));
  }

  // We enter this function while lexer->peekToken()::type == lex::TokenType::PIN
  std::unique_ptr<Node> Parser::parseVarDecl(lex::Lexer* lexer) {
    advance(lexer); // consume PIN
    if (!expect(lexer, lex::TokenType::IDENT, "Expected identifier after 'pin'")) {
      // TODO: check what token it is and return useful error message (e.g. reserved keyword)
      return nullptr;
    }

    std::string name;
    if (auto result = getFromVariant<std::string>(m_tokens.back())) {
      name = result.value();
    } else {
      panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty identifier name received"));
      return nullptr; // was missing, fell through to use an empty `name` otherwise
    }

    if (match(lexer, lex::TokenType::SEMICOLON)) {
      return std::make_unique<VarDecl>(std::move(name));
    }

    if (!expect(lexer, lex::TokenType::ASSIGN, "Expected '=' in variable declaration")) {
      return nullptr;
    }

    // parseExpression already covers literals, fn literals, array literals,
    // parenthesized expressions, and identifier/call/member-access chains
    // (via parsePrimary + parsePostfix), so no separate switch is needed here.
    std::unique_ptr<Node> init = parseExpression(lexer);
    if (!init) return nullptr;

    if (!expect(lexer, lex::TokenType::SEMICOLON, "Expected ';' after variable declaration")) {
      return nullptr;
    }

    return std::make_unique<VarDecl>(std::move(name), std::move(init));
  }

  // We enter this function while lexer->peekToken()::type == lex::TokenType::RETURN
  // Bare `return;` (no expression before the semicolon) returns null, same
  // idea as a C function falling off the end of a void/value-returning path.
  std::unique_ptr<Node> Parser::parseReturn(lex::Lexer* lexer) {
    advance(lexer); // consume 'return'

    if (match(lexer, lex::TokenType::SEMICOLON)) {
      return std::make_unique<Return>(nullptr);
    }

    std::unique_ptr<Node> value = parseExpression(lexer);
    if (!value) return nullptr;

    if (!expect(lexer, lex::TokenType::SEMICOLON, "Expected ';' after return value")) {
      return nullptr;
    }

    return std::make_unique<Return>(std::move(value));
  }

  std::unique_ptr<Node> Parser::parseBreak(lex::Lexer* lexer) {
    advance(lexer); // consume 'break'
    if (!expect(lexer, lex::TokenType::SEMICOLON, "Expected ';' after 'break'")) {
      return nullptr;
    }
    return std::make_unique<Break>();
  }

  std::unique_ptr<Node> Parser::parseContinue(lex::Lexer* lexer) {
    advance(lexer); // consume 'continue'
    if (!expect(lexer, lex::TokenType::SEMICOLON, "Expected ';' after 'continue'")) {
      return nullptr;
    }
    return std::make_unique<Continue>();
  }

  // We enter this function while lexer->peekToken()::type is THIS or SUPER.
  // Consumes the keyword, then:
  //   - bare `this` (not followed by ARROW): returns a ThisExpr, parsePostfix
  //     can still chain '.'/'(' off of it normally afterward.
  //   - bare `super` with no ARROW is invalid, `super` only makes sense as
  //     the left side of `super->something`, there's no standalone value for
  //     "the parent class" to hand back.
  //   - `this->field` / `super->field`: returns a SuperAccess node. Further
  //     '.'/'(' chaining off of THAT happens normally in parsePostfix since
  //     SuperAccess is just another expression node as far as it's concerned
  //     (e.g. `super->getList().length` works: ARROW only governs the very
  //     first hop off this/super, everything after is regular '.'/call syntax).
  std::unique_ptr<Node> Parser::parseThisOrSuper(lex::Lexer* lexer, bool is_super) {
    advance(lexer); // consume 'this' or 'super'

    if (!check(lexer, lex::TokenType::ARROW)) {
      if (is_super) {
        panic(formatIllegalTokenMessage(lexer, lexer->peekToken(), "'super' must be followed by '->member' or '->method(...)'"));
        return nullptr;
      }
      return std::make_unique<ThisExpr>();
    }

    advance(lexer); // consume '->'
    if (!expect(lexer, lex::TokenType::IDENT, "Expected identifier after '->'")) {
      return nullptr;
    }
    auto field_name = getFromVariant<std::string>(m_tokens.back());
    if (!field_name) {
      panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty identifier after '->'"));
      return nullptr;
    }

    return std::make_unique<SuperAccess>(is_super, std::move(*field_name));
  }

  // We enter this function while lexer->peekToken()::type == lex::TokenType::CLASS
  // class Name [extends Parent] {
  //   pin field [= init];   // any number; pin/fn statements can appear in any
  //                         // order in the source, they're sorted into the
  //                         // right bucket (fields vs methods) below
  //   fn name(...) { ... }  // any number
  // }
  std::unique_ptr<Node> Parser::parseClassDecl(lex::Lexer* lexer) {
    advance(lexer); // consume 'class'

    if (!expect(lexer, lex::TokenType::IDENT, "Expected class name after 'class'")) {
      return nullptr;
    }
    auto class_name = getFromVariant<std::string>(m_tokens.back());
    if (!class_name) {
      panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty class name"));
      return nullptr;
    }

    std::string parent_name;
    if (match(lexer, lex::TokenType::EXTENDS)) {
      if (!expect(lexer, lex::TokenType::IDENT, "Expected parent class name after 'extends'")) {
        return nullptr;
      }
      auto name = getFromVariant<std::string>(m_tokens.back());
      if (!name) {
        panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty parent class name"));
        return nullptr;
      }
      parent_name = std::move(*name);
    }

    if (!expect(lexer, lex::TokenType::L_BRACE, "Expected '{' to start class body")) {
      return nullptr;
    }

    std::vector<std::unique_ptr<VarDecl>> fields;
    std::vector<std::unique_ptr<FnLiteral>> methods;
    std::vector<std::string> method_names;

    while (!check(lexer, lex::TokenType::R_BRACE) && !isAtEnd(lexer)) {
      if (check(lexer, lex::TokenType::PIN)) {
        std::unique_ptr<Node> field_node = parseVarDecl(lexer);
        if (!field_node) return nullptr;
        // parseVarDecl always returns a VarDecl, this cast is just to get
        // back the concrete type so it can go in the typed `fields` vector.
        fields.push_back(std::unique_ptr<VarDecl>(static_cast<VarDecl*>(field_node.release())));
      } else if (check(lexer, lex::TokenType::FN)) {
        if (!expect(lexer, lex::TokenType::FN, "Expected 'fn'")) return nullptr;
        if (!expect(lexer, lex::TokenType::IDENT, "Expected method name after 'fn'")) {
          return nullptr;
        }
        auto method_name = getFromVariant<std::string>(m_tokens.back());
        if (!method_name) {
          panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty method name"));
          return nullptr;
        }

        if (!expect(lexer, lex::TokenType::L_PAREN, "Expected '(' after method name")) {
          return nullptr;
        }
        std::vector<std::string> params;
        if (!check(lexer, lex::TokenType::R_PAREN)) {
          do {
            if (!expect(lexer, lex::TokenType::IDENT, "Expected parameter name")) {
              return nullptr;
            }
            auto param_name = getFromVariant<std::string>(m_tokens.back());
            if (!param_name) {
              panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty parameter name"));
              return nullptr;
            }
            params.push_back(std::move(*param_name));
          } while (match(lexer, lex::TokenType::COMMA));
        }
        if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after parameters")) {
          return nullptr;
        }

        std::unique_ptr<Node> body = parseBlock(lexer);
        if (!body) return nullptr;

        methods.push_back(std::make_unique<FnLiteral>(std::move(params), std::move(body)));
        method_names.push_back(std::move(*method_name));
      } else {
        panic(formatIllegalTokenMessage(lexer, lexer->peekToken(), "Expected 'pin' field or 'fn' method in class body"));
        return nullptr;
      }
    }

    if (!expect(lexer, lex::TokenType::R_BRACE, "Expected '}' to close class body")) {
      return nullptr;
    }

    return std::make_unique<ClassDecl>(std::move(*class_name), std::move(parent_name), std::move(fields), std::move(methods), std::move(method_names));
  }

  // We enter this function while lexer->peekToken()::type == lex::TokenType::NEW
  std::unique_ptr<Node> Parser::parseNewExpr(lex::Lexer* lexer) {
    advance(lexer); // consume 'new'

    if (!expect(lexer, lex::TokenType::IDENT, "Expected class name after 'new'")) {
      return nullptr;
    }
    auto class_name = getFromVariant<std::string>(m_tokens.back());
    if (!class_name) {
      panic(formatIllegalTokenMessage(lexer, m_tokens.back(), "Empty class name after 'new'"));
      return nullptr;
    }

    if (!expect(lexer, lex::TokenType::L_PAREN, "Expected '(' after class name in 'new' expression")) {
      return nullptr;
    }

    std::vector<std::unique_ptr<Node>> args;
    if (!check(lexer, lex::TokenType::R_PAREN)) {
      do {
        std::unique_ptr<Node> arg = parseExpression(lexer);
        if (!arg) return nullptr;
        args.push_back(std::move(arg));
      } while (match(lexer, lex::TokenType::COMMA));
    }

    if (!expect(lexer, lex::TokenType::R_PAREN, "Expected ')' after constructor arguments")) {
      return nullptr;
    }

    return std::make_unique<NewExpr>(std::move(*class_name), std::move(args));
  }

  // printTree: public entry point, walks m_root and prints the parsed
  // statements with indentation showing nesting depth.
  void Parser::printTree() const {
    std::print("Program\n");
    const auto& statements = m_root->statements; // adjust if your unique_ptr<Block> access differs
    for (size_t i = 0; i < statements.size(); ++i) {
      printNode(statements[i].get(), "", i + 1 == statements.size());
    }
  }

  // printNode: recursive walk over a Node*, one branch per NodeType.
  // Indentation depth is just depth * 2 spaces, nothing fancy.
  void Parser::printNode(const Node* node, const std::string& prefix, bool is_last, std::string_view label) {
    std::string connector = prefix + (is_last ? "\u2514\u2500\u2500 " : "\u251c\u2500\u2500 ");
    std::string child_prefix = prefix + (is_last ? "    " : "\u2502   ");

    if (node == nullptr) {
      std::print("{}{}<null>\n", connector, label);
      return;
    }

    switch (node->type) {
      case NodeType::LITERAL: {
        const auto* lit = static_cast<const Literal*>(node);
        std::visit(
          [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
              std::print("{}{}Literal(null)\n", connector, label);
            } else if constexpr (std::is_same_v<T, bool>) {
              std::print("{}{}Literal({})\n", connector, label, v ? "true" : "false");
            } else {
              std::print("{}{}Literal({})\n", connector, label, v);
            }
          },
          lit->value);
        break;
      }

      case NodeType::IDENTIFIER: {
        const auto* ident = static_cast<const Identifier*>(node);
        std::print("{}{}Identifier({})\n", connector, label, ident->name);
        break;
      }

      case NodeType::UNARY: {
        const auto* unary = static_cast<const Unary*>(node);
        std::print("{}{}Unary({})\n", connector, label, lex::literalTokenToString(unary->operation));
        printNode(unary->operand.get(), child_prefix, true);
        break;
      }

      case NodeType::BINARY: {
        const auto* binary = static_cast<const Binary*>(node);
        std::print("{}{}Binary({})\n", connector, label, lex::literalTokenToString(binary->operation));
        printNode(binary->left.get(), child_prefix, false);
        printNode(binary->right.get(), child_prefix, true);
        break;
      }

      case NodeType::BLOCK: {
        const auto* block = static_cast<const Block*>(node);
        std::print("{}{}Block\n", connector, label);
        for (size_t i = 0; i < block->statements.size(); ++i) {
          printNode(block->statements[i].get(), child_prefix, i + 1 == block->statements.size());
        }
        break;
      }

      case NodeType::IF: {
        const auto* if_node = static_cast<const If*>(node);
        std::print("{}{}If\n", connector, label);
        bool has_else = if_node->else_branch != nullptr;
        printNode(if_node->condition.get(), child_prefix, false, "condition: ");
        printNode(if_node->then_branch.get(), child_prefix, !has_else, "then: ");
        if (has_else) {
          printNode(if_node->else_branch.get(), child_prefix, true, "else: ");
        }
        break;
      }

      case NodeType::WHILE: {
        const auto* while_node = static_cast<const While*>(node);
        std::print("{}{}While\n", connector, label);
        printNode(while_node->condition.get(), child_prefix, false, "condition: ");
        printNode(while_node->body.get(), child_prefix, true, "body: ");
        break;
      }

      case NodeType::VAR_DECL: {
        const auto* var_decl = static_cast<const VarDecl*>(node);
        std::print("{}{}VarDecl({})\n", connector, label, var_decl->name);
        if (var_decl->initializer) {
          printNode(var_decl->initializer.get(), child_prefix, true);
        }
        break;
      }

      case NodeType::EXPR_STMT: {
        const auto* expr_stmt = static_cast<const ExprStmt*>(node);
        std::print("{}{}ExprStmt\n", connector, label);
        printNode(expr_stmt->expr.get(), child_prefix, true);
        break;
      }

      case NodeType::CALL: {
        const auto* call = static_cast<const Call*>(node);
        std::print("{}{}Call\n", connector, label);
        bool has_args = !call->args.empty();
        printNode(call->callee.get(), child_prefix, !has_args, "callee: ");
        for (size_t i = 0; i < call->args.size(); ++i) {
          printNode(call->args[i].get(), child_prefix, i + 1 == call->args.size());
        }
        break;
      }

      case NodeType::FN_LITERAL: {
        const auto* fn = static_cast<const FnLiteral*>(node);
        std::string params;
        for (size_t i = 0; i < fn->params.size(); ++i) {
          params += fn->params[i];
          if (i + 1 < fn->params.size()) params += ", ";
        }
        std::print("{}{}FnLiteral({})\n", connector, label, params);
        printNode(fn->body.get(), child_prefix, true);
        break;
      }

      case NodeType::MEMBER_ACCESS: {
        const auto* member = static_cast<const MemberAccess*>(node);
        std::print("{}{}MemberAccess(.{})\n", connector, label, member->field);
        printNode(member->object.get(), child_prefix, true);
        break;
      }

      case NodeType::ARRAY_LITERAL: {
        const auto* array = static_cast<const ArrayLiteral*>(node);
        std::print("{}{}ArrayLiteral\n", connector, label);
        for (size_t i = 0; i < array->elements.size(); ++i) {
          printNode(array->elements[i].get(), child_prefix, i + 1 == array->elements.size());
        }
        break;
      }

      case NodeType::RETURN: {
        const auto* ret = static_cast<const Return*>(node);
        std::print("{}{}Return\n", connector, label);
        if (ret->value) {
          printNode(ret->value.get(), child_prefix, true);
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
        std::print(
          "{}{}Class({}{})\n", connector, label, class_decl->name, class_decl->parent_name.empty() ? "" : (" extends " + class_decl->parent_name));
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

} // namespace par
