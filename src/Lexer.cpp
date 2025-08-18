#include "Lexer.h"
#include "Token.h"

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

        std::string peekChar(Lexer& lexer) {
            std::string nex_char{""};

            if (lexer.next_pos >= lexer.input.length());
            else nex_char = lexer.input[lexer.next_pos];

            return nex_char;
        }

        void consumeSpace(Lexer& lexer) { 
            std::string cc;
            while ((cc = lexer.current_char) == " " || cc ==  "\t" || cc == "\r" || cc == "\n") {
                if (cc == "\n") ++lexer.line;
                readChar(lexer);
            }
        }

        Token newToken(TokenType token_type, TokenVariant value, size_t line, size_t column) {
            return Token{.type = token_type, .value = value, .line = line, .column = column};
        }

        Token parseDoubleCharToken(Lexer& lexer, TokenType default_tok, std::unordered_map<std::string, TokenType> possible_pairs) {
            std::string next_char = peekChar(lexer);
            size_t line = lexer.line;
            size_t column = lexer.cursor_pos - lexer.begin_of_line;

            if (next_char.empty()) return newToken(default_tok, {}, line, column);

            auto it = possible_pairs.find(next_char);
            if (it != possible_pairs.end()) {
                readChar(lexer);
                return newToken(it->second, {}, line, column); 
            }
            return newToken(default_tok, {}, line, column);
        }

        // TODO: implement
        Token parseSlashToken(Lexer& lexer);
        bool isNum(const std::string& current_char);
        bool isAlpha(const std::string& current_char);
        Token parseNum(Lexer& lexer);
        Token parseWord(Lexer& lexer);


    } // namespace ''

    /* I wish c++ switch statements accepted more complex types ;-;
     * performance difference is negligible but its too damn ugly 
     *(╯°□°）╯︵ ┻━┻ 
     */
    Token nextToken(Lexer& lexer) {
        const std::string& cc = lexer.current_char;
        size_t line = lexer.line;
        size_t col = lexer.cursor_pos - lexer.begin_of_line;

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
        return Token{ILLEGAL, "", line, col};
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
