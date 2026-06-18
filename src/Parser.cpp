#include <Logging.h>
#include <Parser.h>
#include <ParserNodeTypes.h>
#include <Token.h>
#include <print>

// NOTE: placeholder
static void panic(const std::string_view msg) { std::print("PANIC: {}\n", msg.data()); }

namespace par {
  std::string Parser::formatIllegalTokenMessage(const lex::Token& token, const std::string_view msg) {
    std::string fmted_msg = fmt::format("Illegal token received {}:{}\n", token.line, token.column);
    if (!msg.empty()) {
      fmted_msg += msg;
      fmted_msg += "\n";
    }
    return fmted_msg +
           fmt::format("{}\n{}\n", m_lexer->getLineContent(token.line), std::string(token.column - 1, ' ') + std::string(m_lexer->peekToken().column - token.column, '^'));
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

} // namespace par
