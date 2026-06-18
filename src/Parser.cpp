#include <Parser.h>

namespace par {
  void Parser::parse(lex::Lexer* lexer) {
    m_lexer = lexer;
    lex::Token current_token = m_lexer->nextToken();
    while (current_token.type != lex::TokenType::SIS_EOF) {
      switch (current_token.type) {
        case lex::TokenType::ILLEGAL: break;;
        case lex::TokenType::IDENT: break;

        case lex::TokenType::NUM:
        case lex::TokenType::STRING:
        case lex::TokenType::TRUE:
        case lex::TokenType::FALSE:
        case lex::TokenType::SIS_NULL: parseLiteral(current_token); break;

        case lex::TokenType::IF: break;
        case lex::TokenType::ELSE: break;
        case lex::TokenType::FOR: break;
        case lex::TokenType::WHILE: break;
        case lex::TokenType::SWITCH: break;
        case lex::TokenType::CASE: break;
        case lex::TokenType::RETURN: break;
        case lex::TokenType::BREAK: break;
        case lex::TokenType::CONTINUE: break;

        case lex::TokenType::FN: break;
        case lex::TokenType::PIN: break;
        case lex::TokenType::CLASS: break;
        case lex::TokenType::INCLUDE: break;

        case lex::TokenType::PLUS: break;
        case lex::TokenType::PLUS_PLUS: break;
        case lex::TokenType::MINUS: break;
        case lex::TokenType::MINUS_MINUS: break;
        case lex::TokenType::STAR: break;
        case lex::TokenType::SLASH: break;
        case lex::TokenType::PERCENT: break;
        case lex::TokenType::ASSIGN: break;
        case lex::TokenType::PLUS_ASSIGN: break;
        case lex::TokenType::MINUS_ASSIGN: break;
        case lex::TokenType::STAR_ASSIGN: break;
        case lex::TokenType::SLASH_ASSIGN: break;
        case lex::TokenType::PERCENT_ASSIGN: break;

        case lex::TokenType::EQUALS: break;
        case lex::TokenType::NOT_EQUALS: break;
        case lex::TokenType::LESS_THAN: break;
        case lex::TokenType::LESS_THAN_EQUALS: break;
        case lex::TokenType::GREATER_THAN: break;
        case lex::TokenType::GREATER_THAN_EQUALS: break;
        case lex::TokenType::AND: break;
        case lex::TokenType::OR:
        case lex::TokenType::NOT: break;

        case lex::TokenType::L_PAREN: break;
        case lex::TokenType::R_PAREN: break;
        case lex::TokenType::L_BRACK: break;
        case lex::TokenType::R_BRACK: break;
        case lex::TokenType::L_BRACE: break;
        case lex::TokenType::R_BRACE: break;

        case lex::TokenType::COMMA: break;
        case lex::TokenType::DOT: break;
        case lex::TokenType::COLON: break;
        case lex::TokenType::SCOPE_RES: break;
        case lex::TokenType::SEMICOLON: break;
        case lex::TokenType::HASH: break;
        case lex::TokenType::ARROW: break;

        case lex::TokenType::COMMENT: break;
        default: break;
      }
    }
    m_lexer = nullptr;
  }
} // namespace par
