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

        COMMENT_LINE, COMMENT_BLOCK,
    } TokenType;

    // Against accidental mangling of variants types order
    typedef std::variant<std::monostate, int, double, bool, std::string> TokenVariant;

    typedef struct Token {
        TokenType type;
        TokenVariant value;
        size_t line;
        size_t column;
        size_t len;
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
        case SIS_EOF: return ""; 
        case ILLEGAL: return "ILLEGAL_SIS_TOKEN"; 
        case IDENT: return std::get<std::string>(t_val);

        case NUM: return std::to_string(( // Why not just double? idk either
                      std::holds_alternative<int>(t_val) 
                      ? std::get<int>(t_val) 
                      : std::get<double>(t_val)));

        case STRING: return std::get<std::string>(t_val);
        case TRUE: return "true";
        case FALSE: return "false";
        case SIS_NULL: return "null";

        case IF: return "if"; 
        case ELSE: return "else";
        case FOR: return "for"; 
        case WHILE: return "while";
        case SWITCH: return "switch"; 
        case CASE: return "case"; 
        case RETURN: return "return"; 
        case BREAK: return "break"; 
        case CONTINUE: return "continue";

        case FN: return "fn";
        case PIN: return "pin";
        case CLASS: return "class";
        case INCLUDE: return "include";

        case PLUS: return "+";
        case PLUS_PLUS: return "++";
        case MINUS: return "-"; 
        case MINUS_MINUS: return "--";
        case STAR: return "*";
        case SLASH: return "/";
        case PERCENT: return "%";
        case ASSIGN: return "="; 
        case PLUS_ASSIGN: return "+="; 
        case MINUS_ASSIGN: return "-="; 
        case STAR_ASSIGN: return "*="; 
        case SLASH_ASSIGN: return "/=";
        case PERCENT_ASSIGN: return "%=";

        case EQUALS: return "=="; 
        case NOT_EQUALS: return "!=";
        case LESS_THAN: return "<"; 
        case LESS_THAN_EQUALS: return "<=";
        case GREATER_THAN: return ">"; 
        case GREATER_THAN_EQUALS: return ">=";
        case AND: return "&&"; 
        case OR: return "||"; 
        case NOT: return "!";

        case L_PAREN: return "("; 
        case R_PAREN: return ")";
        case L_BRACK: return "["; 
        case R_BRACK: return "]";
        case L_BRACE: return "{"; 
        case R_BRACE: return "}";

        case COMMA: return ",";
        case DOT: return ".";
        case COLON: return ":";
        case SCOPE_RES: return "::";
        case SEMICOLON: return ";";
        case HASH: return "#";
        case ARROW: return "->";

        case COMMENT_LINE:
        case COMMENT_BLOCK: return std::get<std::string>(t_val);
        default: return "";
        }
    }

    inline std::string literalTokenToString(const Token& token) {
        switch (token.type) {
        case SIS_EOF: return "SIS_EOF";
        case ILLEGAL: return "ILLEGAL";
        case IDENT: return "IDENT";
        case NUM: return "NUM";
        case STRING: return "STRING";
        case COMMENT_LINE: return "COMMENT_LINE";
        case COMMENT_BLOCK: return "COMMENT_BLOCK";
        default: return tokenToString(token);
        }
    }

    inline std::string literalTokenToString(const TokenType& token_type) {
        return literalTokenToString(Token{.type = token_type, .value = "", .line = 0, .column = 0});
    }
} // namespace lex

#endif // SIS_TOKEN_H
