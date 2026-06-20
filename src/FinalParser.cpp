#include <FinalParser.h>

#include <algorithm>
#include <fstream>
#include <print>
#include <Logging.h>

// NOTE: placeholder
static void panic(const std::string_view msg) {
  std::print("PANIC: {}\n", msg.data());
  std::exit(1);
}

namespace fpar {                     // NOTE: separate namespace stuff that is not implemented yet
  void Parser::parse(State* state) { // NOLINT
    throw std::logic_error("Not implemented");
  }

  // NOLINTBEGIN
  // clangd-format off
  std::unique_ptr<Node> Parser::parseLiteral(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseIdentifier(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseUnary(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseBinary(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseBlock(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseIf(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseWhile(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseVarDecl(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseExprStmt(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseCall(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseFnLiteral(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseMemberAccess(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseArrayLiteral(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseReturn(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseBreak(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseContinue(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseClassDecl(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseNewExpr(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseThisExpr(State* state) { throw std::logic_error("Not implemented"); }
  std::unique_ptr<Node> Parser::parseSuperAccess(State* state) { throw std::logic_error("Not implemented"); }
  // clangd-format on
  // NOLINTEND

} // namespace fpar

namespace fpar { // NOTE: separate namespace block for readability
  static std::optional<std::string> readFileToString(const Path& path) {
    LOG_DEBUG_FLUSH("Reading file {}", path.string());
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  std::string formatIllegalTokenMessage(State* state, const lex::Token& token, const std::string_view msg) {
    std::string fmted_msg = fmt::format("Illegal token received {}:{}\n", token.line, token.column);
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

namespace fpar {

  template <typename T>
  static std::optional<T> getFromVariant(const lex::Token& token) {
    if (const auto* val = std::get_if<T>(&token.value)) {
      return *val;
    }
    return std::nullopt;
  }

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

  void Parser::parseRoot(const Path& path) {
    std::optional<std::string> source = m_hooks.read_file(path);
    if (source == std::nullopt) {
      panic(fmt::format("Could not open root source file '{}'", path.string()));
      return;
    }
    m_states[path] = State{.lexer = std::make_unique<lex::Lexer>(source.value(), path), .tokens = {}, .token_idex = 0};
    m_include_stack.push_back(path);

    while (!m_include_stack.empty()) { // NOLINT
      Path current_path = m_include_stack.back();
      m_include_stack.pop_back();

      std::expected<std::optional<Path>, std::string> include_path = checkForInclude(current_path);
      if (!include_path.has_value()) {
        panic(include_path.error());
        break;
      }

      if (include_path.value() == std::nullopt) {
        parse(&m_states[current_path]);
        continue;
      }

      if (std::ranges::contains(m_include_stack, include_path.value().value())) {
        panic(fmt::format("Circular include detected at '{}'", include_path.value().value().string()));
        break;
      }

      source = m_hooks.read_file(include_path.value().value()); // TODO: expand path
      if (source == std::nullopt) {
        panic(fmt::format("Could not open included source file '{}'", include_path.value().value().string()));
        break;
      }

      m_states[include_path.value().value()] = State{.lexer = std::make_unique<lex::Lexer>(source.value(), include_path.value().value()), .tokens = {}, .token_idex = 0};
      m_include_stack.push_back(current_path);
    }
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
} // namespace fpar

namespace fpar { // NOTE: separate namespace block for grouping stuff that will be broken

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
