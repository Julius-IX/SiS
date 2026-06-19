#pragma once

#include <Lexer.h>
#include <ParserNodeTypes.h>

namespace par {
  class Parser {
    public:
    ~Parser() = default;

    // Parses an entire input as a sequence of top level statements.
    // Returns true on success, false if any parseX call failed along the way.
    // The resulting tree is stored in m_root and can be read with getRoot()
    // or printed with printTree().
    bool parse(lex::Lexer* lexer);

    [[nodiscard]] const Block* const getRoot() const { return m_root.get(); }

    // Prints the parsed tree to stdout. Only useful after a successful parse() call.
    void printTree() const;

    private:
    std::vector<lex::Token> m_tokens;
    std::unique_ptr<Block> m_root = nullptr;

    static std::string formatIllegalTokenMessage(lex::Lexer* lexer, const lex::Token& token, std::string_view msg = "");

    static void printNode(const Node* node, const std::string& prefix, bool is_last, std::string_view label = "");

    static bool check(lex::Lexer* lexer, lex::TokenType type);
    static bool isAtEnd(lex::Lexer* lexer);
    bool expect(lex::Lexer* lexer, lex::TokenType type, std::string_view err_msg);
    bool match(lex::Lexer* lexer, lex::TokenType type);
    bool matchAny(lex::Lexer* lexer, std::initializer_list<lex::TokenType> types);
    lex::Token advance(lex::Lexer* lexer);

    std::unique_ptr<Node> parseLiteral(lex::Lexer* lexer);
    std::unique_ptr<Node> parseIdentifier(lex::Lexer* lexer);
    std::unique_ptr<Node> parseUnary(lex::Lexer* lexer);
    std::unique_ptr<Node> parseBinary(lex::Lexer* lexer, int min_prec);
    std::unique_ptr<Node> parseAssignment(lex::Lexer* lexer);
    std::unique_ptr<Node> parsePostfix(lex::Lexer* lexer);
    std::unique_ptr<Node> parsePrimary(lex::Lexer* lexer);
    std::unique_ptr<Node> parseBlock(lex::Lexer* lexer);
    std::unique_ptr<Node> parseStatement(lex::Lexer* lexer);
    std::unique_ptr<Node> parseIf(lex::Lexer* lexer);
    std::unique_ptr<Node> parseWhile(lex::Lexer* lexer);
    std::unique_ptr<Node> parseVarDecl(lex::Lexer* lexer);
    std::unique_ptr<Node> parseExprStmt(lex::Lexer* lexer);
    std::unique_ptr<Node> parseCall(lex::Lexer* lexer, std::unique_ptr<Node> callee);
    std::unique_ptr<Node> parseFnLiteral(lex::Lexer* lexer);
    std::unique_ptr<Node> parseExpression(lex::Lexer* lexer);
    std::unique_ptr<Node> parseArrayLiteral(lex::Lexer* lexer);
  };

} // namespace par
