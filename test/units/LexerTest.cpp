#include <Token.h>
#include <gtest/gtest.h>

#include <Lexer.h>
#include <spdlog/fmt/fmt.h>

#include <algorithm>

typedef std::vector<lex::Token> TokenVector;

int32_t tokenToLength(const lex::Token& token) {
  switch (token.type) {
    case lex::ILLEGAL:
    case lex::IDENT: return std::holds_alternative<std::string>(token.value) ? static_cast<int32_t>(std::get<std::string>(token.value).length()) : 0;

    case lex::NUM:
      if (std::holds_alternative<double>(token.value)) {
        std::ostringstream oss;
        oss << std::get<double>(token.value);
        return static_cast<int32_t>(oss.str().length());
      }
      return 0;

    case lex::STRING: return std::holds_alternative<std::string>(token.value) ? static_cast<int32_t>(std::get<std::string>(token.value).size()) + 2 : 0; // +2 for quotes

    case lex::COMMENT: return 0;

    case lex::SIS_EOF:
    case lex::PLUS:
    case lex::MINUS:
    case lex::STAR:
    case lex::SLASH:
    case lex::PERCENT:
    case lex::QUESTION_MARK:
    case lex::ASSIGN:
    case lex::LESS_THAN:
    case lex::GREATER_THAN:
    case lex::NOT:
    case lex::L_PAREN:
    case lex::R_PAREN:
    case lex::L_BRACK:
    case lex::R_BRACK:
    case lex::L_BRACE:
    case lex::R_BRACE:
    case lex::COMMA:
    case lex::DOT:
    case lex::COLON:
    case lex::SEMICOLON: return 1;

    case lex::IF:
    case lex::FN:
    case lex::PLUS_ASSIGN:
    case lex::MINUS_ASSIGN:
    case lex::STAR_ASSIGN:
    case lex::SLASH_ASSIGN:
    case lex::PERCENT_ASSIGN:
    case lex::EQUALS:
    case lex::NOT_EQUALS:
    case lex::LESS_THAN_EQUALS:
    case lex::GREATER_THAN_EQUALS:
    case lex::AND:
    case lex::OR:
    case lex::ARROW: return 2;

    case lex::FOR:
    case lex::PIN:
    case lex::NEW: return 3;

    case lex::TRUE:
    case lex::ELSE:
    case lex::CASE:
    case lex::THIS:
    case lex::SIS_NULL: return 4;

    case lex::BREAK:
    case lex::FALSE:
    case lex::WHILE:
    case lex::CLASS:
    case lex::SUPER: return 5;

    case lex::RETURN:
    case lex::SWITCH: return 6;

    case lex::DEFAULT:
    case lex::EXTENDS:
    case lex::INCLUDE: return 7;

    case lex::CONTINUE: return 8;

    default: {
      fmt::print("Invalid Token Type received: {}\n", lex::literalTokenToString(token.type));
      return -1;
    }
  }
}

void populateTokenLengths(TokenVector& tokens) {
  for (auto& token : tokens) {
    int32_t token_len = tokenToLength(token);

    if (token_len < 0) throw std::runtime_error("Mock lex::Token.length is negative");

    token.length = token_len;
  }
}

static std::string tokenVariantToString(const lex::Token& token) {
  lex::TokenVariant tok_var = token.value;
  if (std::holds_alternative<std::monostate>(tok_var)) {
    return "std::monostate";
  }

  if (std::holds_alternative<double>(tok_var)) {
    return std::to_string(std::get<double>(tok_var));
  }

  if (std::holds_alternative<bool>(tok_var)) {
    return std::to_string(std::get<bool>(tok_var));
  }

  if (std::holds_alternative<std::string>(tok_var)) {
    return std::get<std::string>(tok_var);
  }

  return "ERROR: could not parse TokenVariant value to std::string";
}

std::string formatSingleToken(const lex::Token& token) {
  return fmt::format("Token:\n"
                     "Type   = {}\n"
                     "Value  = {}\n"
                     "Line   = {}\n"
                     "Column = {}\n"
                     "Length = {}\n",
                     lex::literalTokenToString(token.type),
                     tokenVariantToString(token),
                     token.line,
                     token.column,
                     token.length);
}

std::string formatComparedToken(const lex::Token& actual, const lex::Token& expected) {
  return fmt::format("Actual : Expected\nType = {} : {}\nValue = {} : {}\nLine = {} : {}\nColumn = {} : {}\nLength = {} : {}\n",
                     lex::literalTokenToString(actual.type),
                     lex::literalTokenToString(expected.type),
                     tokenVariantToString(actual),
                     tokenVariantToString(expected),
                     actual.line,
                     expected.line,
                     actual.column,
                     expected.column,
                     actual.length,
                     expected.length);
}

void compareTokenStream(lex::Lexer& lexer, const TokenVector& expected_tokens) {
  lex::Token token = lexer.nextToken();
  size_t index{0};

  while (true) {
    if (token != expected_tokens.at(index)) {
      ADD_FAILURE() << formatComparedToken(token, expected_tokens.at(index));
    }

    if (token.type == lex::SIS_EOF) {
      break;
    }

    token = lexer.nextToken();
    ++index;
  }
}

TEST(Lexer, TreatCommentAsSpaces) {
  std::string input = "class DaClass /* Random comment */ {};\n"
                      "pin this_variable = //line comment getting in the way \" value of variable \"";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected {
    {.type = lex::CLASS,     .value = {}, .line = 1, .column = 1},
    {.type = lex::IDENT,     .value = "DaClass", .line = 1, .column = 7},
    {.type = lex::L_BRACE,   .value = {}, .line = 1, .column = 36},
    {.type = lex::R_BRACE,   .value = {}, .line = 1, .column = 37},
    {.type = lex::SEMICOLON, .value = {}, .line = 1, .column = 38},

    {.type = lex::PIN,     .value = {}, .line = 2, .column = 1},
    {.type = lex::IDENT,   .value = "this_variable", .line = 2, .column = 5},
    {.type = lex::ASSIGN,  .value = {}, .line = 2, .column = 19},
    {.type = lex::SIS_EOF, .value = {}, .line = 2, .column = 76},
  };
  // NOLINTEND
  // clang-format on
  populateTokenLengths(expected);

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, CanDiscernValidInvalidNumber) {
  std::string input = "123, 111.018, 1100., 1414..4, 12B34";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected {
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
  populateTokenLengths(expected);

  // NOTE: tokenToLength for NUM uses the double's string form, which drops trailing dots.
  // "1100." → 1100.0 → "1100" → 4, but the source span is 5.
  auto num_it = std::ranges::find_if(expected, [](const lex::Token& t) {
      return t.type == lex::NUM &&
      std::holds_alternative<double>(t.value)
      && std::get<double>(t.value) == 1100.0;
  });

  if (num_it != expected.end()) {
    num_it->length = 5;
  }
  // NOLINTEND
  // clang-format on

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, CanDiscernValidInvalidIdentifier) {
  std::string input = "validIdent valid_ident_\n"
                      "_valid_ valid123\n"
                      "valid{}valid\n"
                      "invalid@\n";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected {
    {.type = lex::IDENT,   .value = "validIdent",   .line = 1, .column = 1  },
    {.type = lex::IDENT,   .value = "valid_ident_", .line = 1, .column = 12 },
    {.type = lex::IDENT,   .value = "_valid_",      .line = 2, .column = 1  },
    {.type = lex::IDENT,   .value = "valid123",     .line = 2, .column = 9  },
    {.type = lex::IDENT,   .value = "valid",        .line = 3, .column = 1  },
    {.type = lex::L_BRACE, .value = {},             .line = 3, .column = 6  },
    {.type = lex::R_BRACE, .value = {},             .line = 3, .column = 7  },
    {.type = lex::IDENT,   .value = "valid",        .line = 3, .column = 8  },
    {.type = lex::ILLEGAL, .value = "invalid@",     .line = 4, .column = 1  },
    {.type = lex::SIS_EOF, .value = {},             .line = 4, .column = 10  },
  };
  // NOLINTEND
  // clang-format on

  populateTokenLengths(expected);
  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, ReturnIllegalOnUnknownChar) {
  std::string input = "@ beans & |\n"
                      "~ ` ^ pin\n"
                      "for $ in\n";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected {
    {.type = lex::ILLEGAL, .value = "@",     .line = 1, .column = 1 },
    {.type = lex::IDENT  , .value = "beans", .line = 1, .column = 3  },
    {.type = lex::ILLEGAL, .value = "&",     .line = 1, .column = 9  },
    {.type = lex::ILLEGAL, .value = "|",     .line = 1, .column = 11 },
    {.type = lex::ILLEGAL, .value = "~",     .line = 2, .column = 1  },
    {.type = lex::ILLEGAL, .value = "`",     .line = 2, .column = 3  },
    {.type = lex::ILLEGAL, .value = "^",     .line = 2, .column = 5  },
    {.type = lex::PIN,     .value = {},      .line = 2, .column = 7  },
    {.type = lex::FOR,     .value = {},      .line = 3, .column = 1  },
    {.type = lex::ILLEGAL, .value = "$",     .line = 3, .column = 5  },
    {.type = lex::IDENT,   .value = "in",    .line = 3, .column = 7  },
    {.type = lex::SIS_EOF, .value = {},      .line = 3, .column = 10 },
  };
  // NOLINTEND
  // clang-format on

  populateTokenLengths(expected);
  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, ReusabilityWorks) {
  std::string first_input = "pin var1 = 20;";
  std::string second_input = "if (true) {/* comment */}";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected_set_one {
    {.type = lex::PIN,       .value = {},     .line = 1, .column = 1  },
    {.type = lex::IDENT,     .value = "var1", .line = 1, .column = 5  },
    {.type = lex::ASSIGN,    .value = {},     .line = 1, .column = 10 },
    {.type = lex::NUM,       .value = 20.,    .line = 1, .column = 12 },
    {.type = lex::SEMICOLON, .value = {},     .line = 1, .column = 14 },
    {.type = lex::SIS_EOF,   .value = {},     .line = 1, .column = 15 },
  };
  TokenVector expected_set_two {
    {.type = lex::IF,      .value = {}, .line = 1, .column = 1  },
    {.type = lex::L_PAREN, .value = {}, .line = 1, .column = 4  },
    {.type = lex::TRUE,    .value = {}, .line = 1, .column = 5  },
    {.type = lex::R_PAREN, .value = {}, .line = 1, .column = 9  },
    {.type = lex::L_BRACE, .value = {}, .line = 1, .column = 11 },
    {.type = lex::R_BRACE, .value = {}, .line = 1, .column = 25 },
    {.type = lex::SIS_EOF, .value = {}, .line = 1, .column = 26 },
  };
  // NOLINTEND
  // clang-format on

  populateTokenLengths(expected_set_one);
  populateTokenLengths(expected_set_two);

  lex::Lexer lexer(first_input);

  compareTokenStream(lexer, expected_set_one);

  lexer.reset();
  lexer.newInput(second_input);

  compareTokenStream(lexer, expected_set_two);
}

TEST(Lexer, SplitsTokensCorretly) {
  const std::string input =
    // clang-format off
    // NOLINTBEGIN
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
    /*30 */  "/* This is a random comment */\n"
             ;

  TokenVector expected {
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

    {.type = lex::ILLEGAL       , .value = {"#"}           , .line = 17 , .column = 1  }, // TODO: add support for later
    {.type = lex::INCLUDE       , .value = {}              , .line = 17 , .column = 2  },
    {.type = lex::STRING        , .value = "this is a string literal", .line = 17 , .column = 10 },

    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 1  },
    {.type = lex::PLUS          , .value = {}              , .line = 18 , .column = 4  }, // TODO: add support for later SHOULD BE '++'
    {.type = lex::PLUS          , .value = {}              , .line = 18 , .column = 5  }, // PART OF '++'
    {.type = lex::PLUS          , .value = {}              , .line = 18 , .column = 7  }, // TODO: add support for later
    {.type = lex::PLUS          , .value = {}              , .line = 18 , .column = 8  }, // PART OF '++'
    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 9  },
    {.type = lex::MINUS         , .value = {}              , .line = 18 , .column = 13 }, // TODO: add support for later SHOULD BE '--'
    {.type = lex::MINUS         , .value = {}              , .line = 18 , .column = 14 }, // PART OF '--'
    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 15 },
    {.type = lex::IDENT         , .value = "num"           , .line = 18 , .column = 19 },
    {.type = lex::MINUS         , .value = {}              , .line = 18 , .column = 22 }, // TODO: add support for later SHOULD BE '--'
    {.type = lex::MINUS         , .value = {}              , .line = 18 , .column = 23 }, // PART OF '--'

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
    {.type = lex::COLON         , .value = {}              , .line = 23 , .column = 7  }, // TODO: add support for later SHOULD BE '::'
    {.type = lex::COLON         , .value = {}              , .line = 23 , .column = 8  }, // PART OF '::'
    {.type = lex::SEMICOLON     , .value = {}              , .line = 23 , .column = 10 },
    {.type = lex::ARROW         , .value = {}              , .line = 23 , .column = 12 },

    {.type = lex::IDENT         , .value = "THIS_IS_A_TEST_IDENTIFIER", .line = 29, .column = 1},
    {.type = lex::SIS_EOF       , .value = {}              ,  .line = 30, .column = 32 }
  };

  populateTokenLengths(expected);

  // NOTE: this is here because escaped characters are not counted as part of the string in the helper function
  auto it = std::ranges::find(expected, lex::TokenType::STRING, &lex::Token::type);
  if (it != expected.end()) { 
    it->length = 17;
  }
  // NOLINTEND
  // clang-format on

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, EmptyInputReturnsEOF) {
  lex::Lexer lexer("");

  const lex::Token& token = lexer.peekToken();
  if (token.type != lex::SIS_EOF) {
    ADD_FAILURE() << formatSingleToken(token);
  } else {
    SUCCEED();
  }
}

TEST(Lexer, StringEscapeSequences) {
  std::string input = R"("hello\n\nworld\t\r\\\"end")";

  // clang-format off
  TokenVector expected {
    {.type = lex::STRING,  .value = "hello\n\nworld\t\r\\\"end", .line = 1, .column = 1},
    {.type = lex::SIS_EOF, .value = {},                        .line = 1, .column = 28},
  };
  // clang-format on

  populateTokenLengths(expected);
  expected[0].length = static_cast<int32_t>(input.size());

  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, UnterminatedStringReturnsIllegal) {
  std::string input = "\"hello world";

  lex::Lexer lexer(input);
  lex::Token tok = lexer.nextToken();

  EXPECT_EQ(tok.type, lex::ILLEGAL);
  ASSERT_TRUE(std::holds_alternative<std::string>(tok.value));
  EXPECT_EQ(std::get<std::string>(tok.value), "Unterminated string literal");
}

TEST(Lexer, MultilineBlockCommentTracksLineCorrectly) {
  std::string input = "pin a = 1;\n"
                      "/* this comment\n"
                      "   spans three\n"
                      "   lines */\n"
                      "pin b = 2;";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected {
    {.type = lex::PIN,       .value = {},    .line = 1, .column = 1},
    {.type = lex::IDENT,     .value = "a",   .line = 1, .column = 5},
    {.type = lex::ASSIGN,    .value = {},    .line = 1, .column = 7},
    {.type = lex::NUM,       .value = 1.0,   .line = 1, .column = 9},
    {.type = lex::SEMICOLON, .value = {},    .line = 1, .column = 10},
    {.type = lex::PIN,       .value = {},    .line = 5, .column = 1},
    {.type = lex::IDENT,     .value = "b",   .line = 5, .column = 5},
    {.type = lex::ASSIGN,    .value = {},    .line = 5, .column = 7},
    {.type = lex::NUM,       .value = 2.0,   .line = 5, .column = 9},
    {.type = lex::SEMICOLON, .value = {},    .line = 5, .column = 10},
    {.type = lex::SIS_EOF,   .value = {},    .line = 5, .column = 11},
  };
  // NOLINTEND
  // clang-format on

  populateTokenLengths(expected);
  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}

TEST(Lexer, PeekTokenDoesNotConsumeTokens) {
  std::string input = "pin x = 5;";
  lex::Lexer lexer(input);

  // peek at first 3 tokens without consuming
  EXPECT_EQ(lexer.peekToken(1).type, lex::PIN);
  EXPECT_EQ(lexer.peekToken(2).type, lex::IDENT);
  EXPECT_EQ(lexer.peekToken(3).type, lex::ASSIGN);

  // now consume and verify order is preserved
  EXPECT_EQ(lexer.nextToken().type, lex::PIN);
  EXPECT_EQ(lexer.nextToken().type, lex::IDENT);
  EXPECT_EQ(lexer.nextToken().type, lex::ASSIGN);
}

TEST(Lexer, ArrowTokenParsedCorrectly) {
  std::string input = "obj->field obj - > other";

  // clang-format off
  // NOLINTBEGIN
  TokenVector expected {
    {.type = lex::IDENT,        .value = "obj",   .line = 1, .column = 1 },
    {.type = lex::ARROW,        .value = {},       .line = 1, .column = 4 },
    {.type = lex::IDENT,        .value = "field",  .line = 1, .column = 6 },
    {.type = lex::IDENT,        .value = "obj",   .line = 1, .column = 12},
    {.type = lex::MINUS,        .value = {},       .line = 1, .column = 16},
    {.type = lex::GREATER_THAN, .value = {},       .line = 1, .column = 18},
    {.type = lex::IDENT,        .value = "other",  .line = 1, .column = 20},
    {.type = lex::SIS_EOF,      .value = {},       .line = 1, .column = 25},
  };
  // NOLINTEND
  // clang-format on

  populateTokenLengths(expected);
  lex::Lexer lexer(input);
  compareTokenStream(lexer, expected);
}
