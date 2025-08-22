#ifndef SIS_TOKEN_H
#define SIS_TOKEN_H

#include <unordered_map>
#include <string>
#include <variant>
#include <cstdint>

namespace lex {
  typedef enum TokenType: std::uint8_t {
    SIS_EOF = 0,
    ILLEGAL,
    IDENT,

    NUM,
    STRING,
    TRUE,
    FALSE,
    SIS_NULL,

    IF, ELSE,
    FOR, WHILE,
    SWITCH, CASE, RETURN, BREAK, CONTINUE,

    FN,
    PIN,
    CLASS,
    INCLUDE,

    PLUS, PLUS_PLUS,
    MINUS, MINUS_MINUS,
    STAR,
    SLASH,
    PERCENT,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, PERCENT_ASSIGN,

    EQUALS, NOT_EQUALS,
    LESS_THAN, LESS_THAN_EQUALS,
    GREATER_THAN, GREATER_THAN_EQUALS,
    AND, OR, NOT,

    L_PAREN, R_PAREN,
    L_BRACK, R_BRACK,
    L_BRACE, R_BRACE,

    COMMA,
    DOT,
    COLON,
    SCOPE_RES,
    SEMICOLON,
    HASH,
    ARROW,

    COMMENT,
  } TokenType;

  // Against accidental mangling of variants types order
  typedef std::variant<std::monostate, double, bool, std::string> TokenVariant;

  typedef struct Token {
    TokenType type;
    TokenVariant value;
    size_t line;
    size_t column;
  } Token;

  inline TokenType lookupIdentifier(const std::string& identifier) {
    static std::unordered_map<std::string, TokenType> keywords = {
      {"true"    , TRUE    },
      {"false"   , FALSE   },
      {"null"    , SIS_NULL},

      {"if"      , IF      },
      {"else"    , ELSE    },
      {"for"     , FOR     },
      {"while"   , WHILE   },
      {"switch"  , SWITCH  },
      {"case"    , CASE    },
      {"return"  , RETURN  },
      {"break"   , BREAK   },
      {"continue", CONTINUE},

      {"fn"      , FN      },
      {"pin"     , PIN     },
      {"class"   , CLASS   },
      {"include" , INCLUDE },
    };

    auto keyword_it = keywords.find(identifier);
    if (keyword_it != keywords.end()) {
      return keyword_it->second;
    }
    return IDENT;
  }

  inline std::string tokenToString(const Token& token) {
    const auto& t_val = token.value;

    switch (token.type) {
    case SIS_EOF: return "SIS_EOF";
    case ILLEGAL: return "ILLEGAL";
    case IDENT: return std::get<std::string>(t_val);

    case NUM: return std::to_string(std::get<double>(t_val));
    case STRING: return std::get<std::string>(t_val);
    case TRUE: return "TRUE";
    case FALSE: return "FALSE";
    case SIS_NULL: return "SIS_NULL";

    case IF: return "IF";
    case ELSE: return "ELSE";
    case FOR: return "FOR";
    case WHILE: return "WHILE";
    case SWITCH: return "SWITCH";
    case CASE: return "CASE";
    case RETURN: return "RETURN";
    case BREAK: return "BREAK";
    case CONTINUE: return "CONTINUE";

    case FN: return "FN";
    case PIN: return "PIN";
    case CLASS: return "CLASS";
    case INCLUDE: return "INCLUDE";

    case PLUS: return "PLUS";
    case PLUS_PLUS: return "PLUS_PLUS";
    case MINUS: return "MINUS";
    case MINUS_MINUS: return "MINUS_MINUS";
    case STAR: return "STAR";
    case SLASH: return "SLASH";
    case PERCENT: return "PERCENT";
    case ASSIGN: return "ASSIGN";
    case PLUS_ASSIGN: return "PLUS_ASSIGN";
    case MINUS_ASSIGN: return "MINUS_ASSIGN";
    case STAR_ASSIGN: return "STAR_ASSIGN";
    case SLASH_ASSIGN: return "SLASH_ASSIGN";
    case PERCENT_ASSIGN: return "PERCENT_ASSIGN";

    case EQUALS: return "EQUALS";
    case NOT_EQUALS: return "NOT_EQUALS";
    case LESS_THAN: return "LESS_THAN";
    case LESS_THAN_EQUALS: return "LESS_THAN_EQUALS";
    case GREATER_THAN: return "GREATER_THAN";
    case GREATER_THAN_EQUALS: return "GREATER_THAN_EQUALS";
    case AND: return "AND";
    case OR: return "OR";
    case NOT: return "NOT";

    case L_PAREN: return "L_PAREN";
    case R_PAREN: return "R_PAREN";
    case L_BRACK: return "L_BRACK";
    case R_BRACK: return "R_BRACK";
    case L_BRACE: return "L_BRACE";
    case R_BRACE: return "R_BRACE";

    case COMMA: return "COMMA";
    case DOT: return "DOT";
    case COLON: return "COLON";
    case SCOPE_RES: return "SCOPE_RES";
    case SEMICOLON: return "SEMICOLON";
    case HASH: return "HASH";
    case ARROW: return "ARROW";

    case COMMENT: return std::get<std::string>(t_val);
    default: return "";
    }
  }

  inline std::string literalTokenToString(const Token& token) {
    switch (token.type) {
    case IDENT: return "IDENT";
    case NUM: return "NUM";
    case STRING: return "STRING";
    case COMMENT: return "COMMENT";
    default: return tokenToString(token);
    }
  }

  inline std::string literalTokenToString(const TokenType& token_type) {
    return literalTokenToString(Token{.type = token_type, .value = "", .line = 0, .column = 0});
  }
} // namespace lex

#endif // SIS_TOKEN_H
