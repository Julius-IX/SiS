#include "Lexer.h"
#include "Token.h"

#include <unordered_set>

namespace lex {
    namespace {
        void readChar(Lexer& lexer) {
            if (lexer.next_pos >= lexer.input.length()) {
                lexer.current_char = {};
            } else {
                lexer.current_char = lexer.input[lexer.next_pos]; 
            }
            lexer.cursor_pos = lexer.next_pos;
            ++lexer.next_pos;
        }


        Token newToken(TokenType token_type, TokenVariant value, size_t line, size_t column) {
            return Token{.type = token_type, .value = value, .line = line, .column = column};
        }
        Token readNewToken(Lexer& lexer, TokenType token_type, TokenVariant value, size_t line, size_t column) {
            readChar(lexer);
            return newToken(token_type, value, line, column);
        }

        std::string peekChar(Lexer& lexer) {
            std::string nex_char{""};

            if (lexer.next_pos >= lexer.input.length()) {}
            else nex_char = lexer.input[lexer.next_pos];

            return nex_char;
        }

        void consumeSpace(Lexer& lexer) { 
            std::string& cc = lexer.current_char;
            while ((cc = lexer.current_char) == " " || cc ==  "\t" || cc == "\r" || cc == "\n") {
                if (cc == "\n") {
                    ++lexer.line;
                    lexer.begin_of_line = lexer.cursor_pos;
                }
                readChar(lexer);
            }
        }

        Token parseDoubleCharToken(Lexer& lexer, TokenType default_tok, std::unordered_map<std::string, TokenType> possible_pairs) {
            std::string next_char = peekChar(lexer);
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            TokenVariant tok_val{};

            if (default_tok == ILLEGAL) tok_val = lexer.current_char;
            if (next_char.empty()) return newToken(default_tok, tok_val, line, column);

            auto it = possible_pairs.find(next_char);
            if (it != possible_pairs.end()) {
                readChar(lexer);
                readChar(lexer);
                return newToken(it->second, {}, line, column); 
            }

            readChar(lexer);
            return newToken(default_tok, tok_val, line, column);
        }

        std::string readTill(Lexer& lexer,  const std::string& terminator) {
            std::string buffer;

            while (!lexer.current_char.empty()) {
                buffer += lexer.current_char;
                if (lexer.current_char == "\n") {
                    ++lexer.line;
                    lexer.begin_of_line = lexer.cursor_pos;
                }
                if (buffer.size() >= terminator.size() &&
                        buffer.compare(buffer.size() - terminator.size(), terminator.size(), terminator) == 0) {
                    break;
                }
                readChar(lexer);
            }

            readChar(lexer);
            return buffer;
        }

        Token parseSlashToken(Lexer& lexer) {
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            std::string peeked_char = peekChar(lexer);

            Token token = newToken(SLASH, {}, line, column);
            std::string buffer;
            
            if (peeked_char == "=") {
                readChar(lexer);
                token = newToken(SLASH_ASSIGN, {}, line, column);
            } else if (peeked_char == "/") {
                buffer = readTill(lexer, "\n");
                return newToken(COMMENT_LINE, buffer, line, column);
            } else if (peeked_char == "*") {
                buffer = readTill(lexer, "*/");
                size_t buf_size = buffer.size();
                if (buffer[buf_size-1] == '/' && buffer[buf_size-2] == '*') {
                    return newToken(COMMENT_BLOCK, buffer, line, column);
                } else token = newToken(ILLEGAL, buffer, line, column);
            }

            readChar(lexer);
            return token;
        }
        
        bool isNum(const std::string& str) {
            return !str.empty() && (str[0] >= '0' && str[0] <= '9');
        }

        /* Could have been on line line but this is not fun to read:
         * return !str.empty() && (((str[0] >= 'A' && str[0] <= 'Z') || (str[0] >= 'a' && str[0] <= 'z')) || str[0] == '_'); 
         */
        bool isAlpha(const std::string& str) {
            if (str.empty()) return false;

            char c = str[0];
            bool isLetter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            bool isUnderscore = (c == '_');

            return isLetter || isUnderscore;
        }

        Token parseNum(Lexer& lexer) {
            size_t index_start = lexer.cursor_pos;
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            std::string tok_val;

            std::string cc = lexer.current_char;
            bool seen_dot{false};
            while (isNum(cc) || (cc == "." && !seen_dot)) {
                readChar(lexer);
                cc = lexer.current_char;
            }
            if (isAlpha(peekChar(lexer))) {
                readChar(lexer);
                cc = lexer.current_char;
                while (isAlpha(cc)) {
                    readChar(lexer);
                    cc = lexer.current_char;
                }
                tok_val = lexer.input.substr(index_start, lexer.cursor_pos - index_start); 
                return newToken(ILLEGAL, tok_val, line, column);
            }

            tok_val = lexer.input.substr(index_start, lexer.cursor_pos - index_start);
            return newToken(NUM, std::stod(tok_val), line, column);
        }

        Token parseWord(Lexer& lexer) {
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            size_t index_start = lexer.cursor_pos;
            Token token{ILLEGAL, lexer.current_char, lexer.line, column};

            while (isAlpha(lexer.current_char) || isNum(lexer.current_char)) {
                readChar(lexer);
            }

            std::string tok_val = lexer.input.substr(index_start, lexer.cursor_pos - index_start);

            TokenType tok_type = lookupIdentifier(tok_val);
            if (tok_type == IDENT) token =  newToken(IDENT, tok_val, lexer.line, column);
            else token = newToken(tok_type, {}, lexer.line, column);

            return token;
        }

        // \b \t \n \r \" \' \? \\ \0
        Token parseStringLiteral(Lexer& lexer) {
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            readChar(lexer);
            size_t start_index = lexer.cursor_pos;

            static const std::unordered_set<char> escaped_chars {
                'b', 't', 'n', 'r', '"', '\'', '?', '\\', '0'
            };

            std::string cc = lexer.current_char;
            while (!cc.empty() && cc != "\"") {
                if (cc == "\\") {
                    char next = peekChar(lexer)[0];
                    if (!escaped_chars.contains(next)) {
                        return newToken(
                                ILLEGAL,
                                lexer.input.substr(start_index, lexer.cursor_pos - start_index),
                                line,
                                column
                                );
                    }
                    readChar(lexer);
                }
                readChar(lexer);
                cc = lexer.current_char;
            }

            std::string final_string = lexer.input.substr(start_index, lexer.cursor_pos - start_index);
            if (cc.empty()) return newToken(ILLEGAL, final_string, line, column);

            readChar(lexer);
            return newToken(STRING, final_string, line, column);
        }
    } // namespace ''

    /* I wish c++ switch statements accepted more complex types ;-;
     * performance difference is negligible but its too damn ugly 
     * (╯°□°）╯︵ ┻━┻ 
     */
    Token nextToken(Lexer& lexer) {
        consumeSpace(lexer);

        const std::string& cc = lexer.current_char;
        size_t line = lexer.line;
        size_t col = lexer.cursor_pos - lexer.begin_of_line;

        Token token = {ILLEGAL, cc, line, col};

        // Single char tokens
        if      (cc == "" ) token = readNewToken(lexer, SIS_EOF  , {}, line, col);
        else if (cc == "(") token = readNewToken(lexer, L_PAREN  , {}, line, col);
        else if (cc == ")") token = readNewToken(lexer, R_PAREN  , {}, line, col);
        else if (cc == "[") token = readNewToken(lexer, L_BRACK  , {}, line, col);
        else if (cc == "]") token = readNewToken(lexer, R_BRACK  , {}, line, col);
        else if (cc == "{") token = readNewToken(lexer, L_BRACE  , {}, line, col);
        else if (cc == "}") token = readNewToken(lexer, R_BRACE  , {}, line, col);
        else if (cc == ",") token = readNewToken(lexer, COMMA    , {}, line, col);
        else if (cc == ".") token = readNewToken(lexer, DOT      , {}, line, col);
        else if (cc == ";") token = readNewToken(lexer, SEMICOLON, {}, line, col);
        else if (cc == "#") token = readNewToken(lexer, HASH     , {}, line, col);
        
        // Possible double char tokens
        else if (cc == "+") token = parseDoubleCharToken(lexer, PLUS, {{"+", PLUS_PLUS} , {"=", PLUS_ASSIGN}});
        else if (cc == "-") token = parseDoubleCharToken(lexer, MINUS, {{"-", MINUS_MINUS}, {"=", MINUS_ASSIGN}, {">", ARROW}});
        else if (cc == "*") token = parseDoubleCharToken(lexer, STAR, {{"=", STAR_ASSIGN}});
        else if (cc == "%") token = parseDoubleCharToken(lexer, PERCENT, {{"=", PERCENT_ASSIGN}});
        else if (cc == "=") token = parseDoubleCharToken(lexer, ASSIGN, {{"=", EQUALS}});
        else if (cc == "<") token = parseDoubleCharToken(lexer, LESS_THAN, {{"=", LESS_THAN_EQUALS}});
        else if (cc == ">") token = parseDoubleCharToken(lexer, GREATER_THAN, {{"=", GREATER_THAN_EQUALS}});
        else if (cc == "|") token = parseDoubleCharToken(lexer, ILLEGAL, {{"|", OR}});
        else if (cc == "&") token = parseDoubleCharToken(lexer, ILLEGAL, {{"&", AND}});
        else if (cc == "!") token = parseDoubleCharToken(lexer, NOT, {{"=", NOT_EQUALS}});
        else if (cc == ":") token = parseDoubleCharToken(lexer, COLON, {{":", SCOPE_RES}});
        else if (cc == "/") token = parseSlashToken(lexer);
        else if (cc == "\"") token = parseStringLiteral(lexer);

        // Multi char tokens
        else {
            if (isNum(cc))   token =  parseNum(lexer);
            if (isAlpha(cc)) token = parseWord(lexer);
        }
        return token;
    }

    Lexer* newLexer(std::string input) {
        Lexer* lexer = new Lexer{std::move(input)};
        readChar(*lexer);
        return lexer;
    }

} // namespace lex

/* TODO: account for the these token types
 * STRING,
 */
