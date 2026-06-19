#pragma once

#include <Lexer.h>
#include <ParserNodeTypes.h>
#include <set>
#include <string>

namespace par {
  class Parser {
    public:
    ~Parser() = default;

    // Parses an entire input as a sequence of top level statements.
    // Returns true on success, false if any parseX call failed along the way.
    // The resulting tree is stored in m_root and can be read with getRoot()
    // or printed with printTree().
    //
    // Before any tokenizing happens, the raw source behind `lexer` is run
    // through preprocessIncludes() (see Parser.cpp), which textually splices
    // in the contents of every `include "path";` statement, recursively,
    // skipping any path already included earlier in the chain (so it behaves
    // like #pragma once rather than raw C-style #include). The Lexer this
    // function is handed should be constructed with that expanded source, or
    // pass the original source via parseFile()/parseSource() helpers below
    // which do the expansion for you.
    bool parse(lex::Lexer* lexer);

    // Convenience entry point: reads `path`, expands includes, lexes and
    // parses the result. Prefer this over parse() when starting from a file
    // on disk, since parse() alone assumes include-expansion already
    // happened to whatever string the Lexer was built from.
    bool parseFile(const std::string& path);

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

    // return [expr] ;   |   break ;   |   continue ;
    std::unique_ptr<Node> parseReturn(lex::Lexer* lexer);
    std::unique_ptr<Node> parseBreak(lex::Lexer* lexer);
    std::unique_ptr<Node> parseContinue(lex::Lexer* lexer);

    // class Name [extends Parent] { (pin field [= init];)* (fn name(...) {...})* }
    std::unique_ptr<Node> parseClassDecl(lex::Lexer* lexer);

    // new ClassName(args)
    std::unique_ptr<Node> parseNewExpr(lex::Lexer* lexer);

    // Handles a leading `this` or `super` primary token. Builds a ThisExpr
    // (bare `this`) or, when followed by ARROW, a SuperAccess/MemberAccess
    // for `this->field` / `super->field`. `is_super` tells it which keyword
    // was consumed. ARROW chains after the first access (this->a->b) recurse
    // through parsePostfix's normal '.'/'(' handling on top of the node this
    // returns, ARROW is only special for the FIRST hop off this/super.
    std::unique_ptr<Node> parseThisOrSuper(lex::Lexer* lexer, bool is_super);
  };

  // Reads the file at `path`, returns its contents, or an empty optional if
  // it couldn't be opened. Exposed (not static-in-cpp) so it can be reused
  // by tooling/tests that want the same file-reading behavior as the parser.
  [[nodiscard]] std::optional<std::string> readFileToString(const std::string& path);

  // Expands every `include "relative/path";` statement found in `source`
  // (which was itself read from `source_path`, used to resolve relative
  // includes against the including file's own directory) by replacing the
  // statement with the target file's fully expanded contents, recursively.
  // `already_included` tracks resolved absolute paths seen so far in this
  // expansion chain so a file is never spliced in twice (#pragma once
  // semantics), whether that's a diamond include or an accidental cycle.
  [[nodiscard]] std::string preprocessIncludes(const std::string& source, const std::string& source_path, std::set<std::string>& already_included);

} // namespace par
