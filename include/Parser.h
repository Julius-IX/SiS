#pragma once

#include <Lexer.h>
#include <vector>
#include <ParserNodeTypes.h>

namespace par {
  class Parser {
    public:
    Parser() = delete;
    ~Parser() = default;

    private:
    lex::Lexer* m_lexer;
    std::vector<lex::Token> m_tokens;
    Block m_root;

    std::string formatIllegalTokenMessage(const lex::Token& token, std::string_view msg = "");

    std::unique_ptr<Node> parseLiteral(lex::Lexer* lexer);
    std::unique_ptr<Node> parseIdentifier(lex::Lexer* lexer);
    std::unique_ptr<Node> parseUnary(lex::Lexer* lexer);
    std::unique_ptr<Node> parseBinary(lex::Lexer* lexer);
    std::unique_ptr<Node> parseBlock(lex::Lexer* lexer);
    std::unique_ptr<Node> parseIf(lex::Lexer* lexer);
    std::unique_ptr<Node> parseWhile(lex::Lexer* lexer);
    std::unique_ptr<Node> parseVarDecl(lex::Lexer* lexer);
    std::unique_ptr<Node> parseExprStmt(lex::Lexer* lexer);
    std::unique_ptr<Node> parseCall(lex::Lexer* lexer);
  };

} // namespace par
