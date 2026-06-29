#pragma once

#include <Evaluator.h>
#include <Lexer.h>
#include <Parser.h>

#include <string_parser.h>

#include <string>

#include <linenoise.h>
#include <fmt/core.h>

class Repl {
  public:
  Repl() = default;

  void run() { loop(m_eval); }

  void runWithPrelude(const par::Program& program) {
    m_eval.run(program);
    loop(m_eval);
  }

  private:
  eval::Evaluator m_eval;

  static bool isComplete(const std::string& input) {
    lex::Lexer lexer(input, std::nullopt);
    lex::TokenStream tokens = lexer.tokenize();
    int depth = 0;
    for (const lex::Token& tok : tokens) {
      switch (tok.type) {
        case lex::TokenType::L_BRACE:
        case lex::TokenType::L_BRACK:
        case lex::TokenType::L_PAREN: ++depth; break;
        case lex::TokenType::R_BRACE:
        case lex::TokenType::R_BRACK:
        case lex::TokenType::R_PAREN: --depth; break;
        default: break;
      }
    }
    return depth <= 0;
  }

  static void loop(eval::Evaluator& eval) {
    std::string input;
    while (true) {
      const char* prompt = input.empty() ? ">> " : ".. ";
      char* raw = linenoise(prompt);
      if (!raw) break; // Ctrl+D || Ctlr+C
      std::string line(raw);
      linenoiseFree(raw);
      if (!line.empty() && input.empty()) linenoiseHistoryAdd(line.c_str());
      if (!input.empty()) input += '\n';
      input += line;
      if (!isComplete(input)) continue;
      StringParser parser;
      try {
        auto program = parser.parseEvalString(input);
        if (program) {
          eval::Value result = eval.run(*program);
          if (!std::holds_alternative<std::monostate>(result.data)) {
            fmt::print("{}\n", result.toString());
          }
        }
      } catch (const std::runtime_error& e) {
        fmt::print("Error: {}\n", e.what());
      }
      input.clear();
    }
    fmt::print("\n");
  }
};
