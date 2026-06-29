#pragma once

#include <cstdint>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using Path = std::filesystem::path;

namespace lex {
  // clang-format off
  typedef enum TokenType: std::uint8_t {
    SIS_EOF = 0,
    ILLEGAL,
    IDENT,

    NUM,
    STRING,
    TRUE,
    FALSE,
    SIS_NULL,

    IF, ELSE,
    FOR, WHILE,
    SWITCH, CASE, RETURN, BREAK, CONTINUE, DEFAULT,

    FN,
    PIN,
    CLASS, EXTENDS, NEW, THIS, SUPER,
    INCLUDE,

    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    QUESTION_MARK,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, PERCENT_ASSIGN,

    EQUALS, NOT_EQUALS,
    LESS_THAN, LESS_THAN_EQUALS,
    GREATER_THAN, GREATER_THAN_EQUALS,
    AND, OR, NOT, // symbols only no natural language equivalents

    L_PAREN, R_PAREN,
    L_BRACK, R_BRACK,
    L_BRACE, R_BRACE,

    COMMA,
    DOT,
    COLON,
    SEMICOLON,
    ARROW,

    AS,
    DOC_COMMENT,
    COMMENT,
  } TokenType;
  // clang-format on

  // Against accidental mangling of variants types order
  typedef std::variant<std::monostate, double, bool, std::string> TokenVariant;

  typedef struct Token {
    std::optional<Path> source; // TODO: deduplicate via string_view or some other bs
    TokenType type;
    TokenVariant value;
    size_t line;
    size_t column;
    size_t length;

    [[nodiscard]] bool equivalent(const Token& other) const { return this->type == other.type && this->value == other.value; }

    auto operator<=>(const Token&) const = delete;
    bool operator==(const Token& other) const {
      // clang-format off
      return 
        this->type == other.type &&
        this->value == other.value &&
        this->source == other.source &&
        this->line == other.line &&
        this->column == other.column &&
        this->length == other.length;
      // clang-format on
    }
  } Token;

  // A fully materialized token sequence produced by Lexer::tokenize().
  // Owning, contiguous, and EOF-terminated (last element always has type SIS_EOF).
  // This is the handoff type between the lexing and parsing stages.
  using TokenStream = std::vector<Token>;

  inline TokenType lookupIdentifier(const std::string& identifier) {
    // clang-format off
    static std::unordered_map<std::string, TokenType> keywords = {
      {"true", TRUE}, {"false", FALSE},   {"null", SIS_NULL},

      {"if", IF},     {"else", ELSE},     {"for", FOR},         {"while", WHILE},       {"switch", SWITCH},
      {"case", CASE}, {"return", RETURN}, {"break", BREAK},     {"continue", CONTINUE}, {"default", DEFAULT},

      {"fn", FN},     {"pin", PIN},       {"class", CLASS},     {"extends", EXTENDS},   {"new", NEW},
      {"this", THIS}, {"super", SUPER},   {"include", INCLUDE}, {"as", AS}
    };
    // clang-format on

    auto keyword_it = keywords.find(identifier);
    if (keyword_it != keywords.end()) {
      return keyword_it->second;
    }
    return IDENT;
  }

  inline std::string literalTokenToString(const Token& token) {
    switch (token.type) {
      case SIS_EOF: return "SIS_EOF";
      case ILLEGAL: return "ILLEGAL";

      case IDENT: return "IDENT";
      case NUM: return "NUM";
      case STRING: return "STRING";

      case TRUE: return "TRUE";
      case FALSE: return "FALSE";
      case SIS_NULL: return "SIS_NULL";

      case IF: return "IF";
      case ELSE: return "ELSE";
      case FOR: return "FOR";
      case WHILE: return "WHILE";
      case SWITCH: return "SWITCH";
      case CASE: return "CASE";
      case RETURN: return "RETURN";
      case BREAK: return "BREAK";
      case CONTINUE: return "CONTINUE";
      case DEFAULT: return "DEFAULT";

      case FN: return "FN";
      case PIN: return "PIN";
      case CLASS: return "CLASS";
      case EXTENDS: return "EXTENDS";
      case NEW: return "NEW";
      case THIS: return "THIS";
      case SUPER: return "SUPER";
      case INCLUDE: return "INCLUDE";

      case PLUS: return "PLUS";
      case MINUS: return "MINUS";
      case STAR: return "STAR";
      case SLASH: return "SLASH";
      case PERCENT: return "PERCENT";
      case QUESTION_MARK: return "QUESTION_MARK";
      case ASSIGN: return "ASSIGN";
      case PLUS_ASSIGN: return "PLUS_ASSIGN";
      case MINUS_ASSIGN: return "MINUS_ASSIGN";
      case STAR_ASSIGN: return "STAR_ASSIGN";
      case SLASH_ASSIGN: return "SLASH_ASSIGN";
      case PERCENT_ASSIGN: return "PERCENT_ASSIGN";

      case EQUALS: return "EQUALS";
      case NOT_EQUALS: return "NOT_EQUALS";
      case LESS_THAN: return "LESS_THAN";
      case LESS_THAN_EQUALS: return "LESS_THAN_EQUALS";
      case GREATER_THAN: return "GREATER_THAN";
      case GREATER_THAN_EQUALS: return "GREATER_THAN_EQUALS";
      case AND: return "AND";
      case OR: return "OR";
      case NOT: return "NOT";

      case L_PAREN: return "L_PAREN";
      case R_PAREN: return "R_PAREN";
      case L_BRACK: return "L_BRACK";
      case R_BRACK: return "R_BRACK";
      case L_BRACE: return "L_BRACE";
      case R_BRACE: return "R_BRACE";

      case COMMA: return "COMMA";
      case DOT: return "DOT";
      case COLON: return "COLON";
      case SEMICOLON: return "SEMICOLON";
      case ARROW: return "ARROW";

      case AS: return "AS";
      case DOC_COMMENT: return "DOC_COMMENT";
      case COMMENT: return "COMMENT";

      default: return fmt::format("Invalid Token Type received: {}", static_cast<int>(token.type));
    }
  }

  inline std::string literalTokenToString(const TokenType& token_type) { return literalTokenToString(Token{.type = token_type, .value = "", .line = 0, .column = 0}); }
} // namespace lex
