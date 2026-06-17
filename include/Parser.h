#pragma once

#include <Lexer.h>

namespace par {
  class Parser {
    public:
    Parser() = default;
    ~Parser() { lexer = nullptr; }

    void parse(lex::Lexer* lexer);

    private:
    lex::Lexer* lexer;
  };

} // namespace par
