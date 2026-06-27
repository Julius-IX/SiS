#pragma once

#include <Lexer.h>
#include <ParserNodeTypes.h>
#include <Program.h>

#include <deque>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>

namespace par {
  typedef struct ParserState {
    lex::TokenStream tokens;
    size_t cursor = 0;
    std::unordered_map<size_t, std::string> line_cache;
    std::unique_ptr<Block> block;
    std::vector<Path> includes;
    bool past_include_zone = false;
    lex::Token last_token;
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

    void setParallel(bool parallel) { m_parallel = parallel; }
    std::optional<Program> parseRoot(const Path& path);
    bool parse(State* state);

    const Block& peekRoot() const { return *m_states.at(m_load_order.front()).block; }
    const std::vector<Path>& loadOrder() const { return m_load_order; }
    const State& getState(const Path& path) const { return m_states.at(path); }

    protected:
    ParserHooks m_hooks; // NOLINT

    void registerTestState(const Path& path, State state) {
      m_states[path] = std::move(state);
      m_load_order.push_back(path);
    }

    State& getStateMut(const Path& path) { return m_states.at(path); }

    private:
    std::vector<Path> m_load_order;
    std::deque<Path> m_include_stack;
    std::unordered_map<Path, State> m_states;
    bool m_parallel = false;

    struct ParseResult {
      std::unique_ptr<Block> block;
      std::unordered_map<size_t, std::string> line_cache;
    };

    static ParseResult lexAndParseFile(const std::string& source, const Path& path, ParserHooks hooks);
    std::optional<Program> parseRootParallel();

    void initRootState(const Path& full_root_path, const Path& original_path);
    void parseCurrentFile(const Path& current_path);
    bool loadIncludeSource(const Path& include_path);

    static lex::Token advance(State* state);
    static bool match(State* state, lex::TokenType type);
    static bool check(const State* state, lex::TokenType type);
    static bool isAtEnd(const State* state);
    bool expect(State* state, lex::TokenType type, std::string_view err_msg) const;

    std::expected<std::optional<Path>, std::string> checkForInclude(const Path& path);

    // Expression parsing Pratt core
    static int bindingPower(const lex::TokenType& type);
    std::unique_ptr<Node> parseAtom(State* state);                                                                  // nud: things that START an expression
    std::unique_ptr<Node> parseContinuation(State* state, std::unique_ptr<Node> left);                              // led: things that EXTEND an expression
    std::unique_ptr<Node> parseExpression(State* state, int min_prec = 1);                                          // driver loop
    std::optional<std::vector<std::unique_ptr<Node>>> parseExpressionList(State* state, lex::TokenType terminator); // comma-sep until terminator
    std::optional<std::vector<std::string>> parseParamList(State* state);

    // Statement parsing one function per distinct statement shape
    std::unique_ptr<Node> parseStatement(State* state);
    std::unique_ptr<Node> parseBlock(State* state);
    std::unique_ptr<Node> parseIf(State* state);
    std::unique_ptr<Node> parseWhile(State* state);
    std::unique_ptr<Node> parseVarDecl(State* state);
    std::unique_ptr<Node> parseVarDeclNoSemicolon(State* state);
    std::unique_ptr<Node> parseReturn(State* state);
    std::unique_ptr<Node> parseFnLiteral(State* state);
    std::unique_ptr<Node> parseTopLevelFn(State* state);
    std::unique_ptr<Node> parseNewExpr(State* state);
    std::unique_ptr<Node> parseThisOrSuper(State* state, bool is_super);
    std::unique_ptr<Node> parseFor(State* state);
    std::unique_ptr<Node> parseSwitch(State* state);

    // dedicated section for classes
    std::unique_ptr<Node> parseClassDecl(State* state);
    bool parseClassHeader(State* state, std::string* out_name, std::string* out_parent_name) const;
    bool parseClassBody(State* state,
                        std::vector<std::unique_ptr<VarDecl>>* out_fields,
                        std::vector<std::unique_ptr<FnLiteral>>* out_methods,
                        std::vector<std::string>* out_method_names);
    std::unique_ptr<VarDecl> parseClassField(State* state);
    std::unique_ptr<FnLiteral> parseClassMethod(State* state, std::string* out_name);
    std::optional<std::vector<std::string>> parseClassMethodParams(State* state);
  };
} // namespace par
