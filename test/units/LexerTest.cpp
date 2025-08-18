#include <gtest/gtest.h>

#include "Lexer.h"
#include "Token.h"

static std::string tokenVariantToString(const lex::Token token) {
    lex::TokenVariant tok_var = token.value;
    if (std::holds_alternative<std::monostate>(tok_var)) {
        return "std::monostate";

    } else if (std::holds_alternative<int>(tok_var)) {
        return std::to_string(std::get<int>(tok_var));

    } else if (std::holds_alternative<double>(tok_var)) {
        return std::to_string(std::get<double>(tok_var));

    } else if (std::holds_alternative<bool>(tok_var)) {
        return std::to_string(std::get<bool>(tok_var));
        
    } else if (std::holds_alternative<std::string>(tok_var)) {
        return std::get<std::string>(tok_var);
    }

    return "ERROR: could not parse TokenVariant value to std::string";
} 

/* For a string apparently the first column is the first character after a new line
 * So in this string "this is a string \nStart of new line"
 * the 't' from "this" is column 1 and then after the \n the
 * 'S' from "Start" is again the column 1
 */
TEST(Lexer, CorrectTokenSplits) {
    const std::string input = R"(
    pin int_literal = 5;
    pin double_literal = 5.5;
    pin str_literal = "false true \n\r"
    {true, false, null}
    else for while switch case

    if (true) {
        funcCall();
        wait();
    } else {
        otherFuncCall();
    }

    return break continue
    fn name(par1, par2) {};
    class ClassName {};
    #include "this is a string literal"
    num++ ++num --num num--
    * / %
    = += -= *= /= %=
    == != < <= > >= && || !
    () {} [] <>
    , . : :: ; ->
    // comment line
    /* comment block
     * comment block
     */
    )";

    static const std::vector<lex::Token> expected_tokens = {
        {.type = lex::PIN           , .value = {}              , .line = 2  , .column = 1  },
        {.type = lex::IDENT         , .value = "int_literal"   , .line = 2  , .column = 5  },
        {.type = lex::ASSIGN        , .value = {}              , .line = 2  , .column = 17 },
        {.type = lex::NUM           , .value = 5               , .line = 2  , .column = 19 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 2  , .column = 20 },

        {.type = lex::PIN           , .value = {}              , .line = 3  , .column = 1  },
        {.type = lex::IDENT         , .value = "double_literal", .line = 3  , .column = 5  },
        {.type = lex::ASSIGN        , .value = {}              , .line = 3  , .column = 20 },
        {.type = lex::NUM           , .value = 5.5             , .line = 3  , .column = 22 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 3  , .column = 25 },

        {.type = lex::PIN           , .value = {}              , .line = 4  , .column = 1  },
        {.type = lex::IDENT         , .value = "str_literal"   , .line = 4  , .column = 5  },
        {.type = lex::ASSIGN        , .value = {}              , .line = 4  , .column = 17 },
        {.type = lex::STRING        , .value = "\"false true \n\r\"", .line = 4  , .column = 19 },

        {.type = lex::L_BRACE       , .value = {}              , .line = 5  , .column = 1  },
        {.type = lex::TRUE          , .value = {}              , .line = 5  , .column = 2  },
        {.type = lex::COMMA         , .value = {}              , .line = 5  , .column = 6  },
        {.type = lex::FALSE         , .value = {}              , .line = 5  , .column = 8  },
        {.type = lex::COMMA         , .value = {}              , .line = 5  , .column = 13 },
        {.type = lex::SIS_NULL      , .value = {}              , .line = 5  , .column = 15 },
        {.type = lex::R_BRACE       , .value = {}              , .line = 5  , .column = 19 },

        {.type = lex::ELSE          , .value = {}              , .line = 6  , .column = 1  },
        {.type = lex::FOR           , .value = {}              , .line = 6  , .column = 6  },
        {.type = lex::WHILE         , .value = {}              , .line = 6  , .column = 10 },
        {.type = lex::SWITCH        , .value = {}              , .line = 6  , .column = 16 },
        {.type = lex::CASE          , .value = {}              , .line = 6  , .column = 23 },

        {.type = lex::IF            , .value = {}              , .line = 8  , .column = 1  },
        {.type = lex::L_PAREN       , .value = {}              , .line = 8  , .column = 4  },
        {.type = lex::TRUE          , .value = {}              , .line = 8  , .column = 5  },
        {.type = lex::R_PAREN       , .value = {}              , .line = 8  , .column = 9  },
        {.type = lex::L_BRACE       , .value = {}              , .line = 8  , .column = 11 },
        
        {.type = lex::IDENT         , .value = "funcCall"      , .line = 9  , .column = 5  },
        {.type = lex::L_PAREN       , .value = {}              , .line = 9  , .column = 13 },
        {.type = lex::R_PAREN       , .value = {}              , .line = 9  , .column = 14 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 9  , .column = 15 },

        {.type = lex::IDENT         , .value = "wait"          , .line = 10 , .column = 5  },
        {.type = lex::L_PAREN       , .value = {}              , .line = 10 , .column = 9  },
        {.type = lex::R_PAREN       , .value = {}              , .line = 10 , .column = 10 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 10 , .column = 11 },

        {.type = lex::R_BRACE       , .value = {}              , .line = 11 , .column = 1  },
        {.type = lex::ELSE          , .value = {}              , .line = 11 , .column = 3  },
        {.type = lex::L_BRACE       , .value = {}              , .line = 11 , .column = 8  },

        {.type = lex::IDENT         , .value = "otherFuncCall" , .line = 12 , .column = 5  },
        {.type = lex::L_PAREN       , .value = {}              , .line = 12 , .column = 18 },
        {.type = lex::R_PAREN       , .value = {}              , .line = 12 , .column = 19 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 12 , .column = 20 },

        {.type = lex::R_BRACE       , .value = {}              , .line = 13 , .column = 1  },

        {.type = lex::RETURN        , .value = {}              , .line = 15 , .column = 1  },
        {.type = lex::BREAK         , .value = {}              , .line = 15 , .column = 8  },
        {.type = lex::CONTINUE      , .value = {}              , .line = 15 , .column = 14 },

        {.type = lex::FN            , .value = {}              , .line = 16 , .column = 1  },
        {.type = lex::IDENT         , .value = "name"          , .line = 16 , .column = 4  },
        {.type = lex::L_PAREN       , .value = {}              , .line = 16 , .column = 8  },
        {.type = lex::IDENT         , .value = "par1"          , .line = 16 , .column = 9  },
        {.type = lex::COMMA         , .value = {}              , .line = 16 , .column = 10 },
        {.type = lex::IDENT         , .value = "par2"          , .line = 16 , .column = 11 },
        {.type = lex::R_PAREN       , .value = {}              , .line = 16 , .column = 19 },
        {.type = lex::L_BRACE       , .value = {}              , .line = 16 , .column = 21 },
        {.type = lex::R_BRACE       , .value = {}              , .line = 16 , .column = 22 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 16 , .column = 23 },

        {.type = lex::CLASS         , .value = {}              , .line = 17 , .column = 1  },
        {.type = lex::IDENT         , .value = "ClassName"     , .line = 17 , .column = 7  },
        {.type = lex::L_BRACE       , .value = {}              , .line = 17 , .column = 17 },
        {.type = lex::R_BRACE       , .value = {}              , .line = 17 , .column = 18 },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 17 , .column = 19 },

        {.type = lex::HASH          , .value = {}              , .line = 18 , .column = 1  },
        {.type = lex::INCLUDE       , .value = {}              , .line = 18 , .column = 2  },
        {.type = lex::STRING        , .value = "this is a string literal", .line = 18 , .column = 10 },

        {.type = lex::IDENT         , .value = "num"           , .line = 19 , .column = 1  },
        {.type = lex::PLUS_PLUS     , .value = {}              , .line = 19 , .column = 4  },
        {.type = lex::PLUS_PLUS     , .value = {}              , .line = 19 , .column = 7  },
        {.type = lex::IDENT         , .value = "num"           , .line = 19 , .column = 9  },
        {.type = lex::MINUS_MINUS   , .value = {}              , .line = 19 , .column = 13 },
        {.type = lex::IDENT         , .value = "num"           , .line = 19 , .column = 15 },
        {.type = lex::IDENT         , .value = "num"           , .line = 19 , .column = 19 },
        {.type = lex::MINUS_MINUS   , .value = {}              , .line = 19 , .column = 22 },

        {.type = lex::STAR          , .value = {}              , .line = 20 , .column = 1  },
        {.type = lex::SLASH         , .value = {}              , .line = 20 , .column = 3  },
        {.type = lex::PERCENT       , .value = {}              , .line = 20 , .column = 4  },

        {.type = lex::ASSIGN        , .value = {}              , .line = 21 , .column = 1  },
        {.type = lex::PLUS_ASSIGN   , .value = {}              , .line = 21 , .column = 3  },
        {.type = lex::MINUS_ASSIGN  , .value = {}              , .line = 21 , .column = 6  },
        {.type = lex::STAR_ASSIGN   , .value = {}              , .line = 21 , .column = 9  },
        {.type = lex::SLASH_ASSIGN  , .value = {}              , .line = 21 , .column = 12 },
        {.type = lex::PERCENT_ASSIGN, .value = {}              , .line = 21 , .column = 15 }, 

        {.type = lex::EQUALS        , .value = {}              , .line = 22 , .column = 1  },
        {.type = lex::NOT_EQUALS    , .value = {}              , .line = 22 , .column = 4  },
        {.type = lex::LESS_THAN     , .value = {}              , .line = 22 , .column = 7  },
        {.type = lex::LESS_THAN_EQUALS , .value = {}           , .line = 22 , .column = 9  },
        {.type = lex::GREATER_THAN  , .value = {}              , .line = 22 , .column = 12 },
        {.type = lex::GREATER_THAN_EQUALS, .value = {}         , .line = 22 , .column = 14 },
        {.type = lex::AND           , .value = {}              , .line = 22 , .column = 17 },
        {.type = lex::OR            , .value = {}              , .line = 22 , .column = 20 },
        {.type = lex::NOT           , .value = {}              , .line = 22 , .column = 23 },
        
        {.type = lex::L_PAREN       , .value = {}              , .line = 23 , .column = 1  },
        {.type = lex::R_PAREN       , .value = {}              , .line = 23 , .column = 2  },
        {.type = lex::L_BRACE       , .value = {}              , .line = 23 , .column = 4  },
        {.type = lex::R_BRACE       , .value = {}              , .line = 23 , .column = 5  },
        {.type = lex::L_BRACK       , .value = {}              , .line = 23 , .column = 7  },
        {.type = lex::R_BRACK       , .value = {}              , .line = 23 , .column = 8  },
        {.type = lex::LESS_THAN     , .value = {}              , .line = 23 , .column = 10 },
        {.type = lex::GREATER_THAN  , .value = {}              , .line = 23 , .column = 11 },

        {.type = lex::COMMA         , .value = {}              , .line = 24 , .column = 1  },
        {.type = lex::DOT           , .value = {}              , .line = 24 , .column = 3  },
        {.type = lex::COLON         , .value = {}              , .line = 24 , .column = 5  },
        {.type = lex::SCOPE_RES     , .value = {}              , .line = 24 , .column = 7  },
        {.type = lex::SEMICOLON     , .value = {}              , .line = 24 , .column = 10 },
        {.type = lex::ARROW         , .value = {}              , .line = 25 , .column = 12 },

        {.type = lex::COMMENT_LINE  , .value = "// comment line", .line = 26 , .column = 1 },

        {.type = lex::COMMENT_BLOCK , .value = "/* comment block\n* comment block\n*/", .line = 27, .column = 1 },
        {.type = lex::SIS_EOF       , .value = {}              ,  .line = 28, .column = 1 } 
    };

    lex::Lexer* lexer = lex::newLexer(input);
    int index{0};
    while (true) {
        const lex::Token actual = lex::nextToken(*lexer);
        const lex::Token& expected = expected_tokens.at(index);

        if (actual.type == lex::SIS_EOF) break;
 
        std::string actual_val_str = tokenVariantToString(actual);
        std::string expected_val_str = tokenVariantToString(expected);

        ASSERT_EQ(actual.type, expected.type) << "Expected token type: " << lex::literalTokenToString(expected) << "\nGot: " << lex::literalTokenToString(actual) << '\n';
        ASSERT_EQ(actual_val_str, expected_val_str) << "Expected token value: " << expected_val_str << "\nGot: " << actual_val_str << '\n'; 
        ASSERT_EQ(actual.line, expected.line) << "Expected token line: " << expected.line << "\nGot: " << actual.line << '\n';
        ++index;
    }
}
