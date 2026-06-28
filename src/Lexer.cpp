#include <Lexer.h>

#include <limits>

namespace lex {
  DoubleSymbolTable Lexer::s_symbol_table = initSymbolTable();

  // the keys in 'table' can either consist of one or two chars
  // '\0' will represent the key to the default TokenType of the first char
  // if said char is not followed by a second character that could be part of the token
  DoubleSymbolTable Lexer::initSymbolTable() {
    DoubleSymbolTable table;

    table['+'] = {{'\0', PLUS}, {'=', PLUS_ASSIGN}};
    table['-'] = {{'\0', MINUS}, {'=', MINUS_ASSIGN}, {'>', ARROW}};
    table['*'] = {{'\0', STAR}, {'=', STAR_ASSIGN}};
    table['%'] = {{'\0', PERCENT}, {'=', PERCENT_ASSIGN}};
    table['='] = {{'\0', ASSIGN}, {'=', EQUALS}};
    table['<'] = {{'\0', LESS_THAN}, {'=', LESS_THAN_EQUALS}};
    table['>'] = {{'\0', GREATER_THAN}, {'=', GREATER_THAN_EQUALS}};
    table['|'] = {{'\0', ILLEGAL}, {'|', OR}};
    table['&'] = {{'\0', ILLEGAL}, {'&', AND}};
    table['!'] = {{'\0', NOT}, {'=', NOT_EQUALS}};
    table['/'] = {{'\0', SLASH}, {'=', SLASH_ASSIGN}};

    return table;
  }

  TypeValuePair Lexer::parsePossiblePair(const char& table_id) {
    const char& next_char = peekChar();
    const std::unordered_map<char, TokenType>& table = s_symbol_table.at(table_id);
    TypeValuePair tvp{ILLEGAL, {}};

    auto it = table.find(next_char);
    if (it != table.end()) {
      tvp.first = it->second;
      advanceState();
    } else {
      const TokenType& tok_type = table.at('\0');
      if (tok_type == SLASH && isThisLineDocComment()) {
        return parseDocComment(); // ['/', '/', '/'] 0 = current_char, 1 = next_char, 2 = next_next_char
      }

      if (tok_type == ILLEGAL) {
        tvp.second = std::string{this->m_state.current_char};
      } else {
        tvp.first = tok_type;
      }
    }

    return tvp;
  }

  // ['/', '/', '/'] 0 = current_char, 1 = next_char, 2 = next_next_char
  TypeValuePair Lexer::parseDocComment() {
    advanceState(3); // consume '///'
    uint32_t start_pos = this->m_state.pos;
    uint32_t line = this->m_state.line;
    std::string comment_str;

    while (stateIsNotAtEof()) {
      // Stop BEFORE consuming the newline peek at it
      if (this->m_state.current_char == '\n') {
        comment_str += this->m_input.substr(start_pos, this->m_state.pos - start_pos);

        // peek ahead: is the next line also a doc comment?
        // current_char='\n', peekChar()='/', peekChar(1)='/', peekChar(2)='/'
        if (peekChar() == '/' && peekChar(1) == '/' && peekChar(2) == '/') {
          comment_str += '\n';
          advanceState(4); // consume '\n///'
          line = this->m_state.line;
          start_pos = this->m_state.pos;
        } else {
          // Leave current_char on '\n' fillBuffer() will advance past it
          return {DOC_COMMENT, comment_str};
        }
      }

      advanceState();
    }
    return {DOC_COMMENT, comment_str};
  }

  void Lexer::advanceState() {
    State& state = this->m_state;

    if (state.next_pos >= this->m_input.size()) {
      size_t len = (state.current_char == '\n') ? (state.pos - state.bol) : (state.pos - state.bol + 1);
      m_line_cache[state.line] = this->m_input.substr(state.bol, len);

      state.current_char = '\0';
      state.pos = this->m_input.size();
    } else {
      if (state.current_char == '\n') {
        m_line_cache[state.line] = this->m_input.substr(state.bol, state.pos - state.bol);
        ++state.line;
        state.bol = state.next_pos;
      }

      state.current_char = this->m_input[state.next_pos];
      state.pos = state.next_pos;
      ++state.next_pos;
    }
  }

  void Lexer::consumeSpace() {
    const char& cc = this->m_state.current_char;

    const char& next_char = peekChar();
    const char& next_next_char = peekChar(1);

    if (cc == '/' && (next_char == '*' || next_char == '/') && next_next_char != '/') {
      skipComment(cc, next_char);
    }

    while (stateIsNotAtEof() && isSpace(cc)) {
      advanceState();
      const char& next_char = peekChar();
      const char& next_next_char = peekChar(1);

      if (cc == '/' && (next_char == '*' || next_char == '/') && next_next_char != '/') {
        skipComment(cc, next_char);
      }
    }
  }

  void Lexer::skipComment(const char& current_char, const char& next_char) {
    if (next_char == '/') {
      while (stateIsNotAtEof() && peekChar() != '\n') {
        advanceState();
      }
      advanceState();
    } else if (next_char == '*') {
      std::string current_next_pair = {current_char, next_char};
      while (stateIsNotAtEof() && current_next_pair != "*/") {
        advanceState();
        current_next_pair[0] = current_next_pair[1];
        current_next_pair[1] = peekChar();
      }

      // Two more times to get rid of block comment terminator
      // no need to wrap in if block
      // advanceState() prevents going past m_input.size();
      advanceState(2);
    }
  }

  // default offset is 0 and grabs the char after the current one
  // offset of 1 will grab the char after that etc etc
  char Lexer::peekChar(uint32_t offset) const noexcept {
    if ((this->m_state.next_pos + offset) >= this->m_input.length()) {
      return '\0';
    }
    return this->m_input[this->m_state.next_pos + offset];
  }

  TypeValuePair Lexer::parseNum() {
    const size_t start_index = this->m_state.pos;
    TypeValuePair tvp{ILLEGAL, {}};

    bool seen_dot{false};
    char next_char = peekChar();
    while (isNum(next_char) || (next_char == '.' && !seen_dot)) {
      advanceState();
      if (next_char == '.') seen_dot = true;

      next_char = peekChar();
    }

    if (!isValidNumFollower(next_char)) {
      while (!isValidNumFollower(next_char)) {
        advanceState();
        next_char = peekChar();
      }

      size_t end_index = (this->m_state.pos - start_index) + 1;
      tvp.second = this->m_input.substr(start_index, end_index);
    } else {
      tvp = processValidNumLiteral(start_index);
    }

    return tvp;
  }

  TypeValuePair Lexer::processValidNumLiteral(const size_t& index_start) {
    TypeValuePair tvp{ILLEGAL, {}};
    std::string num_str = this->m_input.substr(index_start, (this->m_state.pos - index_start) + 1);

    double double_val = std::stod(num_str);
    bool fits_in_double = double_val >= std::numeric_limits<double>::lowest() && double_val <= std::numeric_limits<double>::max();

    if (fits_in_double) {
      tvp.first = NUM;
      tvp.second = double_val;
    } else {
      tvp.second = num_str;
    }

    return tvp;
  }

  TypeValuePair Lexer::parseIdent() {
    const size_t start_index = this->m_state.pos;
    TypeValuePair tvp{ILLEGAL, "FAILED TO PARSE IDENTIFIER"};

    char next_char = peekChar();
    while (isAlpha(next_char) || isNum(next_char)) {
      advanceState();

      next_char = peekChar();
    }

    if (!isValidWordFollower(next_char)) {
      while (!isValidWordFollower(next_char)) {
        advanceState();

        next_char = peekChar();
      }

      size_t end_index = (this->m_state.pos - start_index) + 1;
      tvp.second = this->m_input.substr(start_index, end_index);

    } else {
      size_t end_index = (this->m_state.pos - start_index) + 1;
      std::string str = this->m_input.substr(start_index, end_index);

      tvp.first = lookupIdentifier(str);

      if (tvp.first == IDENT) {
        tvp.second = str;
      } else {
        tvp.second = std::monostate();
      }
    }

    return tvp;
  }

  TypeValuePair Lexer::parseString() {
    advanceState(); // consume initial quote

    const size_t start_index = this->m_state.pos;
    const char& current_char = this->m_state.current_char;

    TypeValuePair tvp{ILLEGAL, {}};

    while (stateIsNotAtEof() && current_char != '"') {
      if (current_char == '\\' && peekChar() == '"') {
        advanceState();
      }
      advanceState();
    }

    size_t end_index = this->m_state.pos;

    std::string str = this->m_input.substr(start_index, end_index - start_index);
    escapeChars(str);
    if (!stateIsNotAtEof()) {
      tvp.second = "Unterminated string literal";
      return tvp;
    }

    tvp = {STRING, str};
    return tvp;
  }

  void Lexer::escapeChars(std::string& str) {
    size_t write_index = 0;

    for (size_t read_index = 0; read_index < str.size(); ++read_index) {
      if (str[read_index] == '\\' && read_index + 1 < str.size()) {
        ++read_index;
        switch (str[read_index]) {
          case 'n': str[write_index++] = '\n'; break;
          case 'r': str[write_index++] = '\r'; break;
          case 't': str[write_index++] = '\t'; break;
          case 'b': str[write_index++] = '\b'; break;
          case 'f': str[write_index++] = '\f'; break;
          case 'v': str[write_index++] = '\v'; break;
          case 'a': str[write_index++] = '\a'; break;
          case '0': str[write_index++] = '\0'; break; // NOTE: this will likely break when calling std::string::c_str()
          case '\\': str[write_index++] = '\\'; break;
          case '"': str[write_index++] = '"'; break;
          case '\'': str[write_index++] = '\''; break;
          default:
            str[write_index++] = '\\';
            str[write_index++] = str[read_index];
            break;
        }
      } else {
        str[write_index++] = str[read_index];
      }
    }

    str.resize(write_index);
  }

  void Lexer::fillBuffer() {
    consumeSpace();

    size_t start_pos = this->m_state.pos;
    size_t line = this->m_state.line;
    size_t column = this->m_state.pos - this->m_state.bol + 1;

    char& current_char = this->m_state.current_char;
    TypeValuePair tvp{ILLEGAL, std::string{current_char}};

    switch (current_char) {

      // Single char tokens
      case '(': tvp = {L_PAREN, {}}; break;
      case ')': tvp = {R_PAREN, {}}; break;
      case '[': tvp = {L_BRACK, {}}; break;
      case ']': tvp = {R_BRACK, {}}; break;
      case '{': tvp = {L_BRACE, {}}; break;
      case '}': tvp = {R_BRACE, {}}; break;
      case ',': tvp = {COMMA, {}}; break;
      case '.': tvp = {DOT, {}}; break;
      case ';': tvp = {SEMICOLON, {}}; break;
      case '?': tvp = {QUESTION_MARK, {}}; break;
      case ':': tvp = {COLON, {}}; break;

      // 1-2 char long tokens
      case '+':
      case '-':
      case '*':
      case '%':
      case '=':
      case '<':
      case '>':
      case '|':
      case '&':
      case '!':
      case '/': tvp = parsePossiblePair(current_char); break;

      // Special tokens
      case '"': tvp = parseString(); break;
      case 0:
        tvp = {SIS_EOF, {}};
        ;
        break;
      default: {
        if (isNum(current_char)) {
          tvp = parseNum();

        } else if (isAlpha(current_char)) {
          tvp = parseIdent();
        }
      } break;
    }

    size_t length = (this->m_state.pos - start_pos) + 1;
    advanceState();
    // clang-format off
    this->m_buffer.append(Token{
      .source = this->m_source_path,
      .type = tvp.first,
      .value = tvp.second,
      .line = line,
      .column = column,
      .length = length,
    });
    // clang-format on
  }

  Token Lexer::nextToken() {
    Token token = m_buffer.pop();
    fillBuffer();
    return token;
  }

  TokenStream Lexer::tokenize() {
    TokenStream tokens;
    Token tok;
    do {
      tok = nextToken();
      tokens.push_back(tok);
    } while (tok.type != TokenType::SIS_EOF);
    return tokens;
  }
} // namespace lex
