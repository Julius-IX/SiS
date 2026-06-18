#pragma once

#include <Lexer.h>
#include <vector>

namespace par {
  class Parser {
    public:
    Parser() = default;
    ~Parser() = default;

    void parse(lex::Lexer* lexer);

    private:
    lex::Lexer* m_lexer;
    std::vector<lex::Token> m_tokens;

    void parseLiteral(const lex::Token& token);
    std::string formatIllegalTokenMessage(const lex::Token& token);
  };

} // namespace par
