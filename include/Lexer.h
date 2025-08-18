#ifndef SIS_LEXER_H
#define SIS_LEXER_H

#include "Token.h"

#include <string>

namespace lex {
    typedef struct Lexer {
        std::string input;
        std::string current_file;
        std::string current_char;

        size_t line{1};
        size_t begin_of_line{0};

        size_t cursor_pos{0};
        size_t next_pos{0};
    } Lexer;

    Lexer* newLexer(std::string input);
    Token nextToken(Lexer& lexer);
} // namespace lex

#endif // SIS_LEXER_H
