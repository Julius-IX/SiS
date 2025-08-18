#include "Lexer.h"
#include "Token.h"

#include <unordered_set>

namespace lex {
    namespace {
        void readChar(Lexer& lexer) {
            if (lexer.next_pos >= lexer.input.length()) {
                lexer.current_char = {};
                lexer.eof_reached = true;
            } else {
                lexer.current_char = lexer.input[lexer.next_pos]; 
            }
            lexer.cursor_pos = lexer.next_pos;
            ++lexer.next_pos;
        }

        Token newToken(TokenType token_type, TokenVariant value, size_t line, size_t column) {
            return Token{.type = token_type, .value = value, .line = line, .column = column};
        }

        std::string peekChar(Lexer& lexer) {
            std::string nex_char{""};

            if (lexer.next_pos >= lexer.input.length());
            else nex_char = lexer.input[lexer.next_pos];

            return nex_char;
        }

        void consumeSpace(Lexer& lexer) { 
            std::string cc;
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
                return newToken(it->second, tok_val, line, column); 
            }

            return newToken(default_tok, tok_val, line, column);
        }

        template <typename... Args>
        std::string readTill(Lexer& lexer, std::string terminator, Args... extra_terminators) {
            std::string cc = lexer.current_char;
            std::string final_string = cc;
            std::unordered_set<std::string> terminators = {terminator, extra_terminators...};

            while (!terminators.contains(cc) && !lexer.eof_reached) {
                readChar(lexer);
                final_string += cc = lexer.current_char;
            }
            return final_string;
        }

        Token parseSlashToken(Lexer& lexer) {
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            std::string peeked_char = peekChar(lexer);

            if (peeked_char.empty()) return newToken(SLASH, {}, line, column);

            if (peeked_char == "=") {
                readChar(lexer);
                return newToken(SLASH_ASSIGN, {}, line, column);

            } else if (peeked_char == "/") {
                readChar(lexer);
                std::string line_comment = readTill(lexer, "\n");
                return newToken(COMMENT_LINE, line_comment, line, column);

            } else if (peeked_char == "*") {
                readChar(lexer);
                std::string block_comment = readTill(lexer, "*/");
                return newToken(COMMENT_BLOCK, block_comment, line, column);
            }

            return newToken(SLASH, {}, line, column);
        }
        
        
        bool isNum(const std::string& current_char) {
            return !current_char.empty() && (current_char[0] >= 48 && current_char[0] <= 57);
        }

        bool isAlpha(const std::string& str) {
            return !str.empty() && ((str[0] >= 65 && str[0] <= 90) || (str[0] >= 97 && str[0] <= 122));
        }

        Token parseNum(Lexer& lexer) {
            std::string cc = lexer.current_char;
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;
            bool encountered_dot{false};
            std::string str_number = cc;

            while (isNum(cc) || (cc == "." && !encountered_dot)) {
                str_number += cc;
                readChar(lexer);
                cc = lexer.current_char;
            }
            if (isAlpha(cc)) {
                while (isAlpha(cc)) {
                    readChar(lexer);
                    str_number += lexer.current_char;
                }
                return newToken(ILLEGAL, str_number, line, column);
            }

            if (encountered_dot) return newToken(NUM, std::stod(str_number), line, column);
            else return newToken(NUM, std::stoi(str_number), line, column);
        }

        Token parseWord(Lexer& lexer) {
            std::string tok_val = lexer.current_char;
            while (isAlpha(lexer.current_char) || isNum(lexer.current_char)) {
                readChar(lexer);
                tok_val += lexer.current_char;
            }

            TokenType tok_type = lookupIdentifier(tok_val);
            if (tok_type == IDENT) return newToken(IDENT, tok_val, lexer.line, lexer.cursor_pos - lexer.begin_of_line);
            else return newToken(tok_type, {}, lexer.line, lexer.cursor_pos - lexer.begin_of_line);
        }

    } // namespace ''

    /* I wish c++ switch statements accepted more complex types ;-;
     * performance difference is negligible but its too damn ugly 
     * (╯°□°）╯︵ ┻━┻ 
     */
    Token nextToken(Lexer& lexer) {
        const std::string& cc = lexer.current_char;
        size_t line = lexer.line;
        size_t col = lexer.cursor_pos - lexer.begin_of_line;

        if (lexer.eof_reached) return newToken(SIS_EOF, {}, line, col);
        consumeSpace(lexer);

        // Single char tokens
        if      (cc == "(") return newToken(L_PAREN  , {}, line, col);
        else if (cc == ")") return newToken(R_PAREN  , {}, line, col);
        else if (cc == "[") return newToken(R_BRACK  , {}, line, col);
        else if (cc == "]") return newToken(L_BRACK  , {}, line, col);
        else if (cc == "{") return newToken(L_BRACE  , {}, line, col);
        else if (cc == "}") return newToken(R_BRACE  , {}, line, col);
        else if (cc == ",") return newToken(COMMA    , {}, line, col);
        else if (cc == ".") return newToken(DOT      , {}, line, col);
        else if (cc == ";") return newToken(SEMICOLON, {}, line, col);
        
        // Possible double char tokens
        else if (cc == "+") return parseDoubleCharToken(lexer, PLUS, {{"+", PLUS_PLUS} , {"=", PLUS_ASSIGN}});
        else if (cc == "-") return parseDoubleCharToken(lexer, MINUS, {{"-", MINUS_MINUS}, {"=", MINUS_ASSIGN}});
        else if (cc == "*") return parseDoubleCharToken(lexer, STAR, {{"=", STAR_ASSIGN}});
        else if (cc == "%") return parseDoubleCharToken(lexer, PERCENT, {{"=", PERCENT_ASSIGN}});
        else if (cc == "=") return parseDoubleCharToken(lexer, ASSIGN, {{"=", EQUALS}});
        else if (cc == "<") return parseDoubleCharToken(lexer, LESS_THAN, {{"=", LESS_THAN_EQUALS}});
        else if (cc == ">") return parseDoubleCharToken(lexer, GREATER_THAN, {{"=", GREATER_THAN_EQUALS}});
        else if (cc == "&") return parseDoubleCharToken(lexer, ILLEGAL, {{"&", AND}});
        else if (cc == "!") return parseDoubleCharToken(lexer, NOT, {{"=", NOT_EQUALS}});
        else if (cc == ":") return parseDoubleCharToken(lexer, COLON, {{":", SCOPE_RES}});
        else if (cc == "-") return parseDoubleCharToken(lexer, ILLEGAL, {{">", ARROW}});
        else if (cc == "/") return parseSlashToken(lexer);

        // Multi char tokens
        else {
            if (isNum(cc)) return parseNum(lexer);
            else if (isAlpha(cc)) return parseWord(lexer);
        }
        return Token{ILLEGAL, cc, line, col};
    }

} // namespace lex

/* TODO: account for the these token types
 * SIS_EOF,
 * ILLEGAL,
 * IDENT,
 *
 * NUM,
 * STRING,
 * TRUE,
 * FALSE,
 * SIS_NULL,
 *
 * IF, ELSE,
 * FOR, WHILE,
 * SWITCH, CASE, RETURN, BREAK, CONTINUE,
 *
 * FN,
 * PIN,
 * CLASS,
 * INCLUDE,
 *
 * SLASH,
 * SLASH_ASSIGN,
 *
 * COMMENT_LINE, COMMENT_BLOCK, 
 */
