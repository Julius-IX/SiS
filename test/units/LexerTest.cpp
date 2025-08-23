#include <gtest/gtest.h>

#include "Lexer.h"

/* I'm using weird helper functions bc I barely know how Google Test works.
 * Many of the MATCHERS.* automatically print values passed to them upon failure.
 * This is very annoying, especially when it tries to print types such as structs, resulting in binary gibberish.
 * I couldn't find a way to get past that. Maybe with a TEST FIXTURE, but I kinda find those annoying.
 * These helper functions do the job well enough. This isn't gonna be a real product anyway.
 */

typedef std::vector<lex::Token> TokenVector;

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

void areTokensEqual(const lex::Token& actual, const lex::Token& expected) {
    bool same_type = (actual.type == expected.type);
    bool same_line = (actual.line == expected.line);
    bool same_column = (actual.column == expected.column);

    bool same_value = false;

    std::visit([&same_value](auto&& arg1, auto&& arg2) {
        using T1 = std::decay_t<decltype(arg1)>;
        using T2 = std::decay_t<decltype(arg2)>;

        if constexpr (std::is_same_v<T1, T2>) {
            same_value = (arg1 == arg2);
        } else {
            same_value = false;
        }
    }, actual.value, expected.value);

    bool result = same_type && same_value && same_line && same_column;

    if (!result) {
        std::string message = "Token mismatch: ";
        
        if (!same_type) {
            message += "\n- Type mismatch: "
            "\n  actual: " + lex::literalTokenToString(actual.type) +
            "\n  expected: " + lex::literalTokenToString(expected.type) + '\n';
        }
        if (!same_value) {
            message += "\n- Value mismatch: "
            "\n  actual: " + tokenVariantToString(actual) +
            "\n  expected: " + tokenVariantToString(expected) + '\n';
        }
        if (!same_line) {
            message +=
                "\n- Line mismatch: "
                "\n actual: " + std::to_string(actual.line) +
                "\n expected: " + std::to_string(expected.line) + '\n';
        }
        if (!same_column) {
            message +=
              "\n- Column mismatch: "
              "\n actual: " + std::to_string(actual.column) +
              "\n expected: " + std::to_string(expected.column) + '\n';
        }
        ADD_FAILURE();
        std::cout << message << "\n\n";
    }
}

void compareTokenStream(lex::Lexer& lexer, const TokenVector& expected_tokens) {
    lex::Token token = lexer.nextToken();
    size_t index{0};

    while (true) {
      areTokensEqual(token, expected_tokens.at(index));
      token = lexer.nextToken();

      if (token.type == lex::SIS_EOF) break;
      ++index;
    }
}

TEST(Lexer, SkipComments) {
  std::string input =
    "class DaClass /* Random comment */ {};\n"
    "pin this_variable = //line comment getting in the way \" value of variable \""
    ;

  const TokenVector expected {
    {.type = lex::CLASS,     .value = {}, .line = 1, .column = 1},
    {.type = lex::IDENT,     .value = "DaClass", .line = 1, .column = 7},
    {.type = lex::L_BRACE,   .value = {}, .line = 1, .column = 36},
    {.type = lex::R_BRACE,   .value = {}, .line = 1, .column = 37},
    {.type = lex::SEMICOLON, .value = {}, .line = 1, .column = 38},

    {.type = lex::PIN,     .value = {}, .line = 2, .column = 1},
    {.type = lex::IDENT,   .value = "this_variable", .line = 2, .column = 5},
    {.type = lex::ASSIGN,  .value = {}, .line = 2, .column = 19},
    {.type = lex::SIS_EOF, .value = {}, .line = 2, .column = 87},
  };

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, CanDiscernValidInvalidNumber) {
  std::string input =
    "123, 111.018, 1100., 1414..4, 12B34";

  const TokenVector expected {
    {.type = lex::NUM,     .value = 123.,      .line = 1, .column = 1  },
    {.type = lex::COMMA,   .value = {},        .line = 1, .column = 4  },
    {.type = lex::NUM,     .value = 111.018,   .line = 1, .column = 6  },
    {.type = lex::COMMA,   .value = {},        .line = 1, .column = 13 },
    {.type = lex::NUM,     .value = 1100.,     .line = 1, .column = 15 },
    {.type = lex::COMMA,   .value = {},        .line = 1, .column = 20 },
    {.type = lex::ILLEGAL, .value = "1414..4", .line = 1, .column = 22 },
    {.type = lex::COMMA,   .value = {},        .line = 1, .column = 29 },
    {.type = lex::ILLEGAL, .value = "12B34",   .line = 1, .column = 31 },
    {.type = lex::SIS_EOF, .value = {},        .line = 1, .column = 36 },
  };

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, CanDiscernValidInvalidIdentifier) {
  std::string input =
    "validIdent valid_ident_\n"
    "_valid_ valid123\n"
    "valid{}valid\n"
    "invalid@\n";

  const TokenVector expected {
    {.type = lex::IDENT,   .value = "validIdent",   .line = 1, .column = 1  },
    {.type = lex::IDENT,   .value = "valid_ident_", .line = 1, .column = 12 },
    {.type = lex::IDENT,   .value = "_valid_",      .line = 2, .column = 1  },
    {.type = lex::IDENT,   .value = "valid123",     .line = 2, .column = 9  },
    {.type = lex::IDENT,   .value = "valid",        .line = 3, .column = 1  },
    {.type = lex::L_BRACE, .value = {},             .line = 3, .column = 6  },
    {.type = lex::R_BRACE, .value = {},             .line = 3, .column = 7  },
    {.type = lex::IDENT,   .value = "valid",        .line = 3, .column = 8  },
    {.type = lex::ILLEGAL, .value = "invalid@",     .line = 4, .column = 1  },
    {.type = lex::SIS_EOF, .value = {},             .line = 5, .column = 1  },
  };

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, ReturnIllegalOnUnknownChar) {
  std::string input =
    "@ beans & |\n"
    "~ ` ? pin\n"
    "for $ in\n";

  const TokenVector expected {
    {.type = lex::ILLEGAL, .value = "@",     .line = 1, .column = 1 },
    {.type = lex::IDENT  , .value = "beans", .line = 1, .column = 3  },
    {.type = lex::ILLEGAL, .value = "&",     .line = 1, .column = 9  },
    {.type = lex::ILLEGAL, .value = "|",     .line = 1, .column = 11 },
    {.type = lex::ILLEGAL, .value = "~",     .line = 2, .column = 1  },
    {.type = lex::ILLEGAL, .value = "`",     .line = 2, .column = 3  },
    {.type = lex::ILLEGAL, .value = "?",     .line = 2, .column = 5  },
    {.type = lex::PIN,     .value = {},      .line = 2, .column = 7  },
    {.type = lex::FOR,     .value = {},      .line = 3, .column = 1  },
    {.type = lex::ILLEGAL, .value = "$",     .line = 3, .column = 5  },
    {.type = lex::IDENT,   .value = "in",    .line = 3, .column = 7  },
    {.type = lex::SIS_EOF, .value = {},      .line = 4, .column = 10 },
  };

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}


TEST(Lexer, LexerReusabilityWorks) {
  std::string first_input = "pin var1 = 20;";
  std::string second_input = "if (true) {/* comment */}";

  const TokenVector set_one_expected_tokens {
    {.type = lex::PIN,       .value = {},     .line = 1, .column = 1  },
    {.type = lex::IDENT,     .value = "var1", .line = 1, .column = 5  },
    {.type = lex::ASSIGN,    .value = {},     .line = 1, .column = 10 },
    {.type = lex::NUM,       .value = 20.,    .line = 1, .column = 12 },
    {.type = lex::SEMICOLON, .value = {},     .line = 1, .column = 14 },
    {.type = lex::SIS_EOF,   .value = {},     .line = 1, .column = 15 },
  };
  const TokenVector set_two_expected_tokens {
    {.type = lex::IF,      .value = {}, .line = 1, .column = 1  },
    {.type = lex::L_PAREN, .value = {}, .line = 1, .column = 4  },
    {.type = lex::TRUE,    .value = {}, .line = 1, .column = 5  },
    {.type = lex::R_PAREN, .value = {}, .line = 1, .column = 9  },
    {.type = lex::L_BRACE, .value = {}, .line = 1, .column = 11 },
    {.type = lex::R_BRACE, .value = {}, .line = 1, .column = 25 },
    {.type = lex::SIS_EOF, .value = {}, .line = 1, .column = 26 },
  };

  lex::Lexer lexer(first_input);

  compareTokenStream(lexer, set_one_expected_tokens);

  lexer.reset();
  lexer.newInput(second_input);

  compareTokenStream(lexer, set_two_expected_tokens);
}

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
    /*30 */  "/* This is a random comment */"
             ;

  static const TokenVector expected_tokens = {
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

    areTokensEqual(actual, expected);

    if (actual.type == lex::SIS_EOF) break;
    index++;
  }
}
