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

namespace { // Arithmetic

  TEST(Evaluator, Addition) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("1 + 2;").data), 3.0);
  }

  TEST(Evaluator, Subtraction) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("10 - 3;").data), 7.0);
  }

  TEST(Evaluator, Multiplication) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("4 * 5;").data), 20.0);
  }

  TEST(Evaluator, Division) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("10 / 4;").data), 2.5);
  }

  TEST(Evaluator, Modulo) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("10 % 3;").data), 1.0);
  }

  TEST(Evaluator, OperatorPrecedence) {
    // 2 + 3 * 4 = 14, not 20
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("2 + 3 * 4;").data), 14.0);
  }

  TEST(Evaluator, GroupedExpressionOverridesPrecedence) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("(2 + 3) * 4;").data), 20.0);
  }

} // namespace

namespace { // String operations

  TEST(Evaluator, StringConcatenation) {
    auto v = runScript(R"("hello" + " " + "world";)");
    EXPECT_EQ(std::get<std::string>(v.data), "hello world");
  }

  TEST(Evaluator, StringNumberCoercion) {
    // either operand being a string coerces the other via toString()
    auto v = runScript(R"("x=" + 42;)");
    EXPECT_EQ(std::get<std::string>(v.data), "x=42");
  }

  TEST(Evaluator, StringLength) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript(R"(pin s = "hello"; s.length;)").data), 5.0);
  }

  TEST(Evaluator, StringSubscript) {
    EXPECT_EQ(std::get<std::string>(runScript(R"("abc"[1];)").data), "b");
  }

} // namespace

namespace { // Comparison and equality

  TEST(Evaluator, LessThan) {
    EXPECT_TRUE(std::get<bool>(runScript("1 < 2;").data));
    EXPECT_FALSE(std::get<bool>(runScript("2 < 1;").data));
  }

  TEST(Evaluator, LessThanOrEqual) {
    EXPECT_TRUE(std::get<bool>(runScript("2 <= 2;").data));
    EXPECT_FALSE(std::get<bool>(runScript("3 <= 2;").data));
  }

  TEST(Evaluator, GreaterThan) {
    EXPECT_TRUE(std::get<bool>(runScript("3 > 2;").data));
    EXPECT_FALSE(std::get<bool>(runScript("1 > 2;").data));
  }

  TEST(Evaluator, GreaterThanOrEqual) {
    EXPECT_TRUE(std::get<bool>(runScript("2 >= 2;").data));
  }

  TEST(Evaluator, Equality) {
    EXPECT_TRUE(std::get<bool>(runScript("1 == 1;").data));
    EXPECT_FALSE(std::get<bool>(runScript("1 == 2;").data));
  }

  TEST(Evaluator, Inequality) {
    EXPECT_TRUE(std::get<bool>(runScript("1 != 2;").data));
    EXPECT_FALSE(std::get<bool>(runScript("1 != 1;").data));
  }

  TEST(Evaluator, DifferentTypesNeverEqual) {
    // number 0 and bool false have the same logical falsiness but are distinct types
    EXPECT_FALSE(std::get<bool>(runScript("0 == false;").data));
  }

} // namespace
