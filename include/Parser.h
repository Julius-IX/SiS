#pragma once

#include <Lexer.h>
#include <ParserNodeTypes.h>

namespace par {
  class Parser {
    public:
    ~Parser() = default;

    private:
    std::vector<lex::Token> m_tokens;
    std::unique_ptr<Block> m_root = nullptr;

    std::string formatIllegalTokenMessage(lex::Lexer* lexer, const lex::Token& token, std::string_view msg = "");

    static bool check(lex::Lexer* lexer, lex::TokenType type);
    static bool isAtEnd(lex::Lexer* lexer);
    bool expect(lex::Lexer* lexer, lex::TokenType type, std::string_view err_msg);
    bool match(lex::Lexer* lexer, lex::TokenType type);
    bool matchAny(lex::Lexer* lexer, std::initializer_list<lex::TokenType> types);
    lex::Token advance(lex::Lexer* lexer);

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
