#include <Logging.h>
#include <Parser.h>
#include <ParserNodeTypes.h>
#include <Token.h>
#include <print>

// NOTE: placeholder
static void panic(const std::string_view msg) { std::print("PANIC: {}\n", msg.data()); }

namespace par {
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

  std::unique_ptr<Node> Parser::parseLiteral(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseIdentifier(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseUnary(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseBinary(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseBlock(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseIf(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseWhile(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseVarDecl(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseExprStmt(lex::Lexer* lexer) { return nullptr; }
  std::unique_ptr<Node> Parser::parseCall(lex::Lexer* lexer) { return nullptr; }

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
    }
  }

} // namespace par
