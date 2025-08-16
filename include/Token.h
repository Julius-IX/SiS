#ifndef SIS_TOKEN_H
#define SIS_TOKEN_H

#include <unordered_map>
#include <string>
#include <variant>

namespace lex {
    typedef enum TokenTypes {
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
        ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN,

        EQUALS, NOT_EQUALS,
        LESS_THAN, LESS_THAN_EQUALS,
        GREATER_THAN, GREATER_THAN_EQUALS,
        AND, OR, NOT,

        L_PAREN, R_PAREN,
        L_BRACK, R_BRACK,
        L_BRACE, R_BRACE,
        L_ANGLE, R_ANGLE,

        COMMA,
        DOT,
        COLON,
        SCOPE_RES,
        SEMICOLON,
        HASH,
        ARROW,

        COMMENT_LINE, COMMENT_BLOCK,
    } TokenType;

    typedef struct Token {
        TokenType type;
        std::variant<int, double, bool, std::string> value;
        size_t line;
        size_t colmn;
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
        } else {
            return IDENT;
        }
    }

    inline std::string tokenToString(Token token) {
        auto t_val = token.value;

        switch (token.type) {
        case SIS_EOF: return "\0"; 
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
        case L_ANGLE: return "<"; 
        case R_ANGLE: return ">";

        case COMMA: return ",";
        case DOT: return ".";
        case COLON: return ":";
        case SCOPE_RES: return "::";
        case SEMICOLON: return ";";
        case HASH: return "#";
        case ARROW: return "->";

        case COMMENT_LINE: return std::get<std::string>(t_val); 
        case COMMENT_BLOCK: return std::get<std::string>(t_val);
        default: return "";
        }
    }
}

#endif // SIS_TOKEN_H
