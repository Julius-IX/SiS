#include <gtest/gtest.h>

#include "Lexer.h"
#include "Token.h"

#include <format>

static std::string tokenVariantToString(const lex::Token& token) {
  lex::TokenVariant tok_var = token.value;
  if (std::holds_alternative<std::monostate>(tok_var)) {
    return "std::monostate";

  } else if (std::holds_alternative<double>(tok_var)) {
    return std::to_string(std::get<double>(tok_var));

  } else if (std::holds_alternative<bool>(tok_var)) {
    return std::to_string(std::get<bool>(tok_var));

  } else if (std::holds_alternative<std::string>(tok_var)) {
    return std::get<std::string>(tok_var);
  }

  return "ERROR: could not parse TokenVariant value to std::string";
} 

std::string getFullTokenStat(const lex::Token token) {
  return std::format("Type: {}\nValue: {}\nLine: {}\nColumn: {}\n",
      lex::literalTokenToString(token.type), tokenVariantToString(token), token.line, token.column);
}

/* For a string apparently the first column is the first character after a new line
 * So in this string "this is a string \nStart of new line"
 * the 't' from "this" is column 1 and then after the \n the
 * 'S' from "Start" is again the column 1
 * TODO: replace with this format
 * "string one\n"
 * "string literal two\n"
 * bc it looks better
 */
TEST(Lexer, CorrectTokenSplits) {
  const std::string input =
       /* 12345678901234567890123456789012345678901234567890*/
/* 1 */  "pin int_literal = 5;\n"
/* 2 */  "pin double_literal = 5.5;\n"
/* 3 */  "pin str_literal = \"false true \\n\\r\"\n"
/* 4 */  "{true, false, null}\n"
/* 5 */  "else for while switch case\n"
/* 6 */  "\n"
/* 7 */  "if (true) {\n"
/* 8 */  "    funcCall();\n"
/* 9 */  "    wait();\n"
/*10 */  "} else {\n"
/*11 */  "    otherFuncCall();\n"
/*12 */  "}\n"
/*13 */  "\n"
/*14 */  "return break continue\n"
/*15 */  "fn name(par1, par2) {};\n"
/*16 */  "class ClassName {};\n"
/*17 */  "#include \"this is a string literal\"\n"
/*18 */  "num++ ++num --num num--\n"
/*19 */  "* / %\n"
/*20 */  "= += -= *= /= %=\n"
/*21 */  "== != < <= > >= && || !\n"
/*22 */  "() {} [] <>\n"
/*23 */  ", . : :: ; ->\n"
/*24 */  "// comment line\n"
/*25 */  "/* comment block\n"
/*26 */  " * comment block\n"
/*27 */  " */\n"
/*28 */  "// TODO: suffer\n"
/*29 */  "THIS_IS_A_TEST_IDENTIFIER\n"
/*30 */  "/* This is a random comment */";

  static const std::vector<lex::Token> expected_tokens = {
    {.type = lex::PIN           , .value = {}              , .line = 1  , .column = 1  },
    {.type = lex::IDENT         , .value = "int_literal"   , .line = 1  , .column = 5  },
    {.type = lex::ASSIGN        , .value = {}              , .line = 1  , .column = 17 },
    {.type = lex::NUM           , .value = 5.0             , .line = 1  , .column = 19 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 1  , .column = 20 },

    {.type = lex::PIN           , .value = {}              , .line = 2  , .column = 1  },
    {.type = lex::IDENT         , .value = "double_literal", .line = 2  , .column = 5  },
    {.type = lex::ASSIGN        , .value = {}              , .line = 2  , .column = 20 },
    {.type = lex::NUM           , .value = 5.5             , .line = 2  , .column = 22 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 2  , .column = 25 },

    {.type = lex::PIN           , .value = {}              , .line = 3  , .column = 1  },
    {.type = lex::IDENT         , .value = "str_literal"   , .line = 3  , .column = 5  },
    {.type = lex::ASSIGN        , .value = {}              , .line = 3  , .column = 17 },
    {.type = lex::STRING        , .value = "false true \n\r",.line = 3  , .column = 19 },

    {.type = lex::L_BRACE       , .value = {}              , .line = 4  , .column = 1  },
    {.type = lex::TRUE          , .value = {}              , .line = 4  , .column = 2  },
    {.type = lex::COMMA         , .value = {}              , .line = 4  , .column = 6  },
    {.type = lex::FALSE         , .value = {}              , .line = 4  , .column = 8  },
    {.type = lex::COMMA         , .value = {}              , .line = 4  , .column = 13 },
    {.type = lex::SIS_NULL      , .value = {}              , .line = 4  , .column = 15 },
    {.type = lex::R_BRACE       , .value = {}              , .line = 4  , .column = 19 },

    {.type = lex::ELSE          , .value = {}              , .line = 5  , .column = 1  },
    {.type = lex::FOR           , .value = {}              , .line = 5  , .column = 6  },
    {.type = lex::WHILE         , .value = {}              , .line = 5  , .column = 10 },
    {.type = lex::SWITCH        , .value = {}              , .line = 5  , .column = 16 },
    {.type = lex::CASE          , .value = {}              , .line = 5  , .column = 23 },

    {.type = lex::IF            , .value = {}              , .line = 7  , .column = 1  },
    {.type = lex::L_PAREN       , .value = {}              , .line = 7  , .column = 4  },
    {.type = lex::TRUE          , .value = {}              , .line = 7  , .column = 5  },
    {.type = lex::R_PAREN       , .value = {}              , .line = 7  , .column = 9  },
    {.type = lex::L_BRACE       , .value = {}              , .line = 7  , .column = 11 },

    {.type = lex::IDENT         , .value = "funcCall"      , .line = 8  , .column = 5  },
    {.type = lex::L_PAREN       , .value = {}              , .line = 8  , .column = 13 },
    {.type = lex::R_PAREN       , .value = {}              , .line = 8  , .column = 14 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 8  , .column = 15 },

    {.type = lex::IDENT         , .value = "wait"          , .line = 9 , .column = 5  },
    {.type = lex::L_PAREN       , .value = {}              , .line = 9 , .column = 9  },
    {.type = lex::R_PAREN       , .value = {}              , .line = 9 , .column = 10 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 9 , .column = 11 },

    {.type = lex::R_BRACE       , .value = {}              , .line = 10 , .column = 1  },
    {.type = lex::ELSE          , .value = {}              , .line = 10 , .column = 3  },
    {.type = lex::L_BRACE       , .value = {}              , .line = 10 , .column = 8  },

    {.type = lex::IDENT         , .value = "otherFuncCall" , .line = 11 , .column = 5  },
    {.type = lex::L_PAREN       , .value = {}              , .line = 11 , .column = 18 },
    {.type = lex::R_PAREN       , .value = {}              , .line = 11 , .column = 19 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 11 , .column = 20 },

    {.type = lex::R_BRACE       , .value = {}              , .line = 12 , .column = 1  },

    {.type = lex::RETURN        , .value = {}              , .line = 14 , .column = 1  },
    {.type = lex::BREAK         , .value = {}              , .line = 14 , .column = 8  },
    {.type = lex::CONTINUE      , .value = {}              , .line = 14 , .column = 14 },

    {.type = lex::FN            , .value = {}              , .line = 15 , .column = 1  },
    {.type = lex::IDENT         , .value = "name"          , .line = 15 , .column = 4  },
    {.type = lex::L_PAREN       , .value = {}              , .line = 15 , .column = 8  },
    {.type = lex::IDENT         , .value = "par1"          , .line = 15 , .column = 9  },
    {.type = lex::COMMA         , .value = {}              , .line = 15 , .column = 13 },
    {.type = lex::IDENT         , .value = "par2"          , .line = 15 , .column = 15 },
    {.type = lex::R_PAREN       , .value = {}              , .line = 15 , .column = 19 },
    {.type = lex::L_BRACE       , .value = {}              , .line = 15 , .column = 21 },
    {.type = lex::R_BRACE       , .value = {}              , .line = 15 , .column = 22 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 15 , .column = 23 },

    {.type = lex::CLASS         , .value = {}              , .line = 16 , .column = 1  },
    {.type = lex::IDENT         , .value = "ClassName"     , .line = 16 , .column = 7  },
    {.type = lex::L_BRACE       , .value = {}              , .line = 16 , .column = 17 },
    {.type = lex::R_BRACE       , .value = {}              , .line = 16 , .column = 18 },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 16 , .column = 19 },

    {.type = lex::HASH          , .value = {}              , .line = 17 , .column = 1  },
    {.type = lex::INCLUDE       , .value = {}              , .line = 17 , .column = 2  },
    {.type = lex::STRING        , .value = "this is a string literal", .line = 17 , .column = 10 },

    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 1  },
    {.type = lex::PLUS_PLUS     , .value = {}              , .line = 18 , .column = 4  },
    {.type = lex::PLUS_PLUS     , .value = {}              , .line = 18 , .column = 7  },
    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 9  },
    {.type = lex::MINUS_MINUS   , .value = {}              , .line = 18 , .column = 13 },
    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 15 },
    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 19 },
    {.type = lex::MINUS_MINUS   , .value = {}              , .line = 18 , .column = 22 },

    {.type = lex::STAR          , .value = {}              , .line = 19 , .column = 1  },
    {.type = lex::SLASH         , .value = {}              , .line = 19 , .column = 3  },
    {.type = lex::PERCENT       , .value = {}              , .line = 19 , .column = 5  },

    {.type = lex::ASSIGN        , .value = {}              , .line = 20 , .column = 1  },
    {.type = lex::PLUS_ASSIGN   , .value = {}              , .line = 20 , .column = 3  },
    {.type = lex::MINUS_ASSIGN  , .value = {}              , .line = 20 , .column = 6  },
    {.type = lex::STAR_ASSIGN   , .value = {}              , .line = 20 , .column = 9  },
    {.type = lex::SLASH_ASSIGN  , .value = {}              , .line = 20 , .column = 12 },
    {.type = lex::PERCENT_ASSIGN, .value = {}              , .line = 20 , .column = 15 },

    {.type = lex::EQUALS        , .value = {}              , .line = 21 , .column = 1  },
    {.type = lex::NOT_EQUALS    , .value = {}              , .line = 21 , .column = 4  },
    {.type = lex::LESS_THAN     , .value = {}              , .line = 21 , .column = 7  },
    {.type = lex::LESS_THAN_EQUALS , .value = {}           , .line = 21 , .column = 9  },
    {.type = lex::GREATER_THAN  , .value = {}              , .line = 21 , .column = 12 },
    {.type = lex::GREATER_THAN_EQUALS, .value = {}         , .line = 21 , .column = 14 },
    {.type = lex::AND           , .value = {}              , .line = 21 , .column = 17 },
    {.type = lex::OR            , .value = {}              , .line = 21 , .column = 20 },
    {.type = lex::NOT           , .value = {}              , .line = 21 , .column = 23 },

    {.type = lex::L_PAREN       , .value = {}              , .line = 22 , .column = 1  },
    {.type = lex::R_PAREN       , .value = {}              , .line = 22 , .column = 2  },
    {.type = lex::L_BRACE       , .value = {}              , .line = 22 , .column = 4  },
    {.type = lex::R_BRACE       , .value = {}              , .line = 22 , .column = 5  },
    {.type = lex::L_BRACK       , .value = {}              , .line = 22 , .column = 7  },
    {.type = lex::R_BRACK       , .value = {}              , .line = 22 , .column = 8  },
    {.type = lex::LESS_THAN     , .value = {}              , .line = 22 , .column = 10 },
    {.type = lex::GREATER_THAN  , .value = {}              , .line = 22 , .column = 11 },

    {.type = lex::COMMA         , .value = {}              , .line = 23 , .column = 1  },
    {.type = lex::DOT           , .value = {}              , .line = 23 , .column = 3  },
    {.type = lex::COLON         , .value = {}              , .line = 23 , .column = 5  },
    {.type = lex::SCOPE_RES     , .value = {}              , .line = 23 , .column = 7  },
    {.type = lex::SEMICOLON     , .value = {}              , .line = 23 , .column = 10 },
    {.type = lex::ARROW         , .value = {}              , .line = 23 , .column = 12 },

    {.type = lex::IDENT         , .value = "THIS_IS_A_TEST_IDENTIFIER", .line = 29, .column = 1},
    {.type = lex::SIS_EOF       , .value = {}              ,  .line = 30, .column = 31 }
  };

lex::Lexer* lexer = new lex::Lexer(input);
int index{0};
while (true) {
  const lex::Token actual = lexer->nextToken();
  const lex::Token& expected = expected_tokens.at(index);

  std::string actual_val_str = tokenVariantToString(actual);
  std::string expected_val_str = tokenVariantToString(expected);

  ASSERT_EQ(lex::literalTokenToString(actual.type), lex::literalTokenToString(expected.type)) << "\nExpected:\n" << getFullTokenStat(expected) << "\nGot:\n" << getFullTokenStat(actual);
  ASSERT_EQ(actual_val_str, expected_val_str) << "\nExpected:\n" << getFullTokenStat(expected) << "\nGot:\n" << getFullTokenStat(actual);
  ASSERT_EQ(actual.line, expected.line) << "\nExpected:\n" << getFullTokenStat(expected) << "\nGot:\n" << getFullTokenStat(actual);
  ASSERT_EQ(actual.column, expected.column) << "\nExpected:\n" << getFullTokenStat(expected) << "\nGot:\n" << getFullTokenStat(actual);

  if (actual.type == lex::SIS_EOF) break;

  index++;
}
}
