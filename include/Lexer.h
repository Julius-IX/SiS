#pragma once

#include <Logging.h>
#include <Token.h>
#include <cassert>
#include <string>

namespace lex {
  typedef std::pair<TokenType, TokenVariant> TypeValuePair;
  typedef std::unordered_map<char, std::unordered_map<char, TokenType>> DoubleSymbolTable;

  typedef struct TokenBuffer {
    static constexpr uint8_t CAPACITY = 5;
    Token slots[CAPACITY];
    uint8_t head{0};
    uint8_t count{0};

    void append(const Token& token) {
      if (count > CAPACITY) {
        LOG_ERROR("TokenBuffer overflow");
        throw std::logic_error("TokenBuffer overflow");
      }

      slots[(head + count) % CAPACITY] = token;
      count++;
    }

    Token pop() {
      if (count < 0) {
        LOG_ERROR("TokenBuffer overflow");
        throw std::logic_error("TokenBuffer underflow");
      }

      Token token = slots[head];
      head = (head + 1) % CAPACITY;
      --count;
      return token;
    }

    [[nodiscard]] const Token& peek(const unsigned short index = 1) const noexcept {
      assert(index <= count && "TokenBuffer peek out of range");
      return slots[(head + index - 1) % CAPACITY];
    }
  } TokenBuffer;

  typedef struct LexState {
    char current_char{'\0'};

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
    Lexer(std::string input, std::optional<Path> source_path = std::nullopt)
      : m_input(std::move(input)), // NOLINT
        m_source_path(std::move(source_path)),
        m_state({}),
        m_buffer({}) {
      if (!this->m_input.empty()) this->m_state.current_char = this->m_input[0];

      m_state.current_char = m_input.size() > 0 ? m_input[0] : '\0';
      if (m_input.size() >= 2 && m_input[0] == '#' && m_input[1] == '!') {
        while (m_state.pos < m_input.size() && m_input[m_state.pos] != '\n') {
          advanceState();
        }
      }
      for (size_t i = 0; i < TokenBuffer::CAPACITY; ++i) {
        fillBuffer();
      }
    }
    ~Lexer() = default;

    [[nodiscard]] Token nextToken();
    [[nodiscard]] const Token& peekToken(const unsigned short index = 1) const noexcept { return m_buffer.peek(index); }

    [[nodiscard]] TokenStream tokenize();

    void newInput(std::string input) {
      reset();

      if (m_input.size() >= 2 && m_input[0] == '#' && m_input[1] == '!') {
        while (m_state.pos < m_input.size() && m_input[m_state.pos] != '\n') {
          advanceState();
        }
      }

      this->m_input = std::move(input);
      if (!this->m_input.empty()) this->m_state.current_char = this->m_input[0];

      for (size_t i = 0; i < TokenBuffer::CAPACITY; ++i) {
        fillBuffer();
      }
    }

    void reset() noexcept {
      this->m_input = {};
      this->m_state = {};
      this->m_buffer = {};
      m_state.current_char = m_input.size() > 0 ? m_input[0] : '\0';
    }

    std::string getLineContent(size_t line) {
      if (auto it = m_line_cache.find(line); it != m_line_cache.end()) return it->second;

      // fallback: scan input
      size_t current_line = 1;
      size_t start = 0;

      for (size_t i = 0; i < m_input.size(); ++i) {
        if (current_line == line && m_input[i] == '\n') {
          return m_input.substr(start, i - start);
        }

        if (m_input[i] == '\n') {
          ++current_line;
          start = i + 1;
        }
      }

      if (current_line == line) return m_input.substr(start);

      return {};
    }

    private:
    std::string m_input;
    std::optional<Path> m_source_path;
    std::unordered_map<size_t, std::string> m_line_cache;
    State m_state;
    TokenBuffer m_buffer;
    static DoubleSymbolTable s_symbol_table;

    [[nodiscard]] static DoubleSymbolTable initSymbolTable();

    [[nodiscard]] static Token newToken(const TokenType type, const TokenVariant& value, const size_t line, const size_t column) {
      return Token{.type = type, .value = value, .line = line, .column = column};
    }

    void fillBuffer();
    void advanceState();
    void consumeSpace();
    void skipComment(const char& current_char, const char& next_char);

    [[nodiscard]] bool stateIsNotAtEof() const { return (this->m_state.pos < this->m_input.size()); }

    [[nodiscard]] char peekChar(uint32_t offset = 0) const noexcept;

    [[nodiscard]] TypeValuePair parsePossiblePair(const char& table_id);

    [[nodiscard]] TypeValuePair parseNum();
    [[nodiscard]] TypeValuePair processValidNumLiteral(const size_t& index_start);

    [[nodiscard]] TypeValuePair parseIdent();
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
    case '\n': case '\t': case '\r': case '\v': case '\0': case ':':
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
      case '(': case '[': case '{': case ':': case '.': is_valid = true; break;
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
