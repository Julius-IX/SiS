#pragma once

#include <Lexer.h>
#include <Parser.h>

#include <optional>
#include <string>

#include <spdlog/fmt/fmt.h>

class StringParser : public par::Parser {
  public:
  std::optional<par::Program> parseEvalString(const std::string& source) {
    static int counter = 0;
    const Path synthetic{fmt::format("<eval:{}>", counter++)};

    lex::Lexer lexer(source, std::nullopt);
    lex::TokenStream tokens = lexer.tokenize();
    auto line_cache = lexer.takeLineCache();

    par::State state;
    state.tokens = std::move(tokens);
    state.line_cache = std::move(line_cache);
    registerTestState(synthetic, std::move(state));

    if (!parse(&getStateMut(synthetic))) return std::nullopt;

    par::State& s = getStateMut(synthetic);

    if (!s.block) return std::nullopt;

    par::Program program;

    program.files[synthetic] = par::ParsedFile{
      .tokens = {},
      .ast = std::move(s.block),
      .includes = {},
      .is_dynamic = false,
    };
    program.load_order.push_back(synthetic);

    return program;
  }
};
