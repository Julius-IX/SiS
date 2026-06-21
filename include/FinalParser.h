#pragma once

#include <Lexer.h>
#include <ParserNodeTypes.h>

#include <deque>
#include <expected>
#include <filesystem>
#include <unordered_map>

using namespace par; // TODO: remove after fpar completion

namespace fpar {
  typedef struct ParserState {
    std::unique_ptr<lex::Lexer> lexer;
    std::vector<lex::Token> tokens;
    size_t token_idex;
  } State;

  struct ParserHooks {
    std::function<std::optional<std::string>(const Path&)> read_file;
    std::function<std::string(State*, const lex::Token&, std::string_view)> format_error;
    std::function<std::optional<Path>(const Path& root, const Path& file)> resolve_file;
  };

  class Parser {
    public:
    Parser();
    ~Parser() = default;

    void parseRoot(const Path& path);
    bool parse(State* state);
    void printTree() const;
    static void printNode(const Node* node, const std::string& prefix, bool is_last, std::string_view label = "");

    protected:
    ParserHooks m_hooks; // NOLINT

    private:
    std::unique_ptr<Block> m_root;
    std::deque<Path> m_include_stack;
    std::unordered_map<Path, State> m_states;

    void initRootState(const Path& full_root_path, const Path& original_path);
    void parseCurrentFile(const Path& current_path);
    bool loadIncludeSource(const Path& include_path);

    static lex::Token advance(State* state);
    static bool match(State* state, lex::TokenType type);
    static bool check(lex::Lexer* lexer, lex::TokenType type);
    static bool isAtEnd(lex::Lexer* lexer);
    bool expect(State* state, lex::TokenType type, std::string_view err_msg) const;

    std::expected<std::optional<Path>, std::string> checkForInclude(const Path& path);

    std::unique_ptr<Node> parseStatement(State* state);

    std::unique_ptr<Node> parseLiteral(State* state);
    std::unique_ptr<Node> parseIdentifier(State* state);
    std::unique_ptr<Node> parseUnary(State* state);
    std::unique_ptr<Node> parseBinary(State* state);
    std::unique_ptr<Node> parseBlock(State* state);
    std::unique_ptr<Node> parseIf(State* state);
    std::unique_ptr<Node> parseWhile(State* state);
    std::unique_ptr<Node> parseVarDecl(State* state);
    std::unique_ptr<Node> parseExprStmt(State* state);
    std::unique_ptr<Node> parseCall(State* state);
    std::unique_ptr<Node> parseFnLiteral(State* state);
    std::unique_ptr<Node> parseMemberAccess(State* state);
    std::unique_ptr<Node> parseArrayLiteral(State* state);
    std::unique_ptr<Node> parseReturn(State* state);
    std::unique_ptr<Node> parseBreak(State* state);
    std::unique_ptr<Node> parseContinue(State* state);
    std::unique_ptr<Node> parseClassDecl(State* state);
    std::unique_ptr<Node> parseNewExpr(State* state);
    std::unique_ptr<Node> parseThisExpr(State* state);
    std::unique_ptr<Node> parseSuperAccess(State* state);
  };
} // namespace fpar
