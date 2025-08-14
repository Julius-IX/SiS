#ifndef SIS_TOKEN_H
#define SIS_TOKEN_H

#include <string>
#include <variant>

namespace lex {
    typedef enum TokenTypes {
        SIS_EOF,
        ILLEGAL,
        IDENT,

        NUM,
        STRING,
        BOOL,
        SIS_NULL,

        IF, ELSE,
        FOR, WHILE,
        RETURN, BREAK, CONTINUE,

        FN,
        PIN,
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
        SEMICOLON,
        HASH,
        ARROW,

        COMMENT_LINE, COMMENT_BLOCK,
    } TokenType;

    typedef struct Token {
        TokenType type;
        std::variant<int, double, bool, std::string> value;
        int line;
        const char* file;
    } Token;
}

#endif
