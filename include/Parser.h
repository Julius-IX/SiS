#pragma once

#include <Lexer.h>

namespace par {
  class Parser {
    public:
    Parser() = default;
    ~Parser() = default;

    void parse(lex::Lexer* lexer);

    private:
    lex::Lexer* m_lexer;

    void parseLiteral(lex::Token& token);
  };

} // namespace par
