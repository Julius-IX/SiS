#include <Evaluator.h>
#include <Parser.h>
#include <Value.h>
#include <gtest/gtest.h>

#include <spdlog/fmt/fmt.h>

class TestEval : public par::Parser {
  public:
  bool parseSource(const std::string& source) {
    m_hooks.read_file    = [](const Path&) -> std::optional<std::string> { return std::nullopt; };
    m_hooks.format_error = [](par::State*, const lex::Token& token, std::string_view msg) -> std::string { return fmt::format("{}:{}: {}", token.line, token.column, msg); };
    m_hooks.resolve_file = [](const Path&, const Path&) -> std::optional<Path> { return std::nullopt; };

    par::State state{
      .lexer      = std::make_unique<lex::Lexer>(source),
      .last_token = {},
    };
    return parse(&state);
  }

  eval::Value run() {
    eval::Evaluator ev;
    return ev.run(peekRoot());
  }
};

// Parse and evaluate a script, returning the value of the last statement.
// Throws std::runtime_error if parsing fails so test output is clear.
static eval::Value runScript(const std::string& source) {
  TestEval t;
  if (!t.parseSource(source)) {
    throw std::runtime_error("Parse failed: " + source);
  }
  return t.run();
}

namespace { // Literals

  TEST(Evaluator, NumberLiteral) {
    auto v = runScript("42;");
    ASSERT_TRUE(std::holds_alternative<double>(v.data));
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 42.0);
  }

  TEST(Evaluator, StringLiteral) {
    auto v = runScript(R"("hello";)");
    ASSERT_TRUE(std::holds_alternative<std::string>(v.data));
    EXPECT_EQ(std::get<std::string>(v.data), "hello");
  }

  TEST(Evaluator, BoolLiterals) {
    EXPECT_TRUE(std::get<bool>(runScript("true;").data));
    EXPECT_FALSE(std::get<bool>(runScript("false;").data));
  }

  TEST(Evaluator, NullLiteral) {
    EXPECT_TRUE(std::holds_alternative<std::monostate>(runScript("null;").data));
  }

} // namespace
