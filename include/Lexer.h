#pragma once

#include <Token.h>
#include <string>

namespace lex {
  typedef std::pair<TokenType, TokenVariant> TypeValuePair;
  typedef std::unordered_map<char, std::unordered_map<char, TokenType>> DoubleSymbolTable;

  typedef struct LexState {
    char current_char{' '};

    size_t pos{0};
    size_t next_pos{1};

    size_t line{1};
    size_t bol{0};
  } State;

  typedef struct Position {
    size_t line{1};
    size_t column{1};
  } Position;

  class Lexer {
    public:
    explicit Lexer(std::string input)
      : m_input(std::move(input)),
        m_live_pos({}),
        m_state({}),
        m_buffer({}) {
      if (!this->m_input.empty()) this->m_state.current_char = this->m_input[0];
      const Token token = nextToken();
    }
    ~Lexer() = default;

    [[nodiscard]] const Token nextToken();
    [[nodiscard]] const Token& peekToken() const noexcept { return this->m_buffer; }

    void newInput(std::string input) {
      this->m_input = std::move(input);
      if (!this->m_input.empty()) this->m_state.current_char = this->m_input[0];
      const Token token = nextToken();
    }

    void reset() noexcept {
      this->m_input = {};
      this->m_live_pos = {};
      this->m_state = {};
      this->m_buffer = {};
    }

    [[nodiscard]] size_t getLine() const noexcept { return this->m_live_pos.line; }

    [[nodiscard]] size_t getColumn() const noexcept { return this->m_live_pos.column; }

    private:
    std::string m_input;
    Position m_live_pos;
    State m_state;
    Token m_buffer;
    static DoubleSymbolTable s_symbol_table;

    [[nodiscard]] static DoubleSymbolTable initSymbolTable();

    [[nodiscard]] size_t getAheadLine() const noexcept { return this->m_state.line; }
    [[nodiscard]] size_t getAheadColumn() const noexcept { return this->m_state.pos - this->m_state.bol + 1; }

    [[nodiscard]] static Token newToken(const TokenType type, const TokenVariant& value, const size_t line, const size_t column) {
      return Token{.type = type, .value = value, .line = line, .column = column};
    }

    void advanceState();
    void consumeSpace();
    void skipComment(const char& current_char, const char& next_char);

    [[nodiscard]] bool stateIsNotAtEof() const { return (this->m_state.pos < this->m_input.size()); }

    [[nodiscard]] const char& peekChar() const noexcept;

    [[nodiscard]] TypeValuePair parsePossiblePair(const char& table_id);

    [[nodiscard]] TypeValuePair parseNum();
    [[nodiscard]] TypeValuePair processValidNumLiteral(const size_t& index_start);
    void skipInvalidNumSequence(const char* next_char);

    [[nodiscard]] TypeValuePair parseIdent();
    void skipInvalidIdentSequence(const char* next_char);
    void escapeChars(std::string& str);

    [[nodiscard]] TypeValuePair parseComment(const char* next_char);
    [[nodiscard]] TypeValuePair parseString();
  };

  [[nodiscard]] static bool isNum(const char& c /* NOLINT */) noexcept { return (c >= '0' && c <= '9'); }

  [[nodiscard]] static bool isValidNumFollower(const char& c /* NOLINT */) noexcept {
    // clang-format off
    switch (c) {
    case '+': case '-': case '*': case '/': case '%': case '=':
    case '<': case '>': case '&': case '|': case '^': case '!':
    case ')': case ']': case '}': case ';': case ',': case ' ':
    case '\n': case '\t': case '\r': case '\v': case '\0':
    return true;
    default: return false;
    }
    // clang-format on
  }

  [[nodiscard]] static bool isValidWordFollower(const char& c /* NOLINT */) noexcept {
    bool is_valid = isValidNumFollower(c);
    if (!is_valid) {
      // clang-format off
      switch (c) {
      case '(': case '[': case '{': case ':': is_valid = true; break;
      default: break;
      }
    }
    // clang-format on

    return is_valid;
  }

  [[nodiscard]] static bool isAlpha(const char& c /* NOLINT */) noexcept {
    bool is_letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    bool is_underscore = (c == '_');
    return (is_letter || is_underscore);
  }

  [[nodiscard]] static bool isSpace(const char& c /* NOLINT */) noexcept { return (c == ' ' || c == '\t' || c == '\r' || c == '\n'); }
} // namespace lex
