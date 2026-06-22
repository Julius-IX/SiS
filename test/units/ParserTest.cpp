#include <Parser.h>
#include <ParserNodeTypes.h>
#include <gtest/gtest.h>

#include <spdlog/fmt/fmt.h>

class TestParser : public par::Parser {
  public:
  // Parse a raw source string and return whether it succeeded.
  bool parseSource(const std::string& source) {
    // Wire hooks: no file I/O, dummy error formatter, no include resolution.
    m_hooks.read_file = [](const Path&) -> std::optional<std::string> { return std::nullopt; };
    m_hooks.format_error = [](par::State*, const lex::Token& t, std::string_view msg) -> std::string { return fmt::format("{}:{}: {}", t.line, t.column, msg); };
    m_hooks.resolve_file = [](const Path&, const Path&) -> std::optional<Path> { return std::nullopt; };

    par::State state{
      .lexer = std::make_unique<lex::Lexer>(source),
      .last_token = {},
    };
    return parse(&state);
  }
};

// Cast a Node* to a derived type and ASSERT it is not null.
// Using a macro so gtest reports the right line number.
#define ASSERT_NODE(ptr, Type)                                                                                                                                                     \
  ASSERT_NE((ptr), nullptr) << "Node pointer is null";                                                                                                                             \
  ASSERT_EQ((ptr)->type, par::Type::TYPE) << "Wrong NodeType";                                                                                                                     \
  auto* as_##Type = static_cast<par::Type*>((ptr))

// Convenience: get the Nth top-level statement and cast it.
#define GET_STMT(root, n, Type)                                                                                                                                                    \
  ASSERT_GT((root).statements.size(), static_cast<size_t>(n));                                                                                                                     \
  ASSERT_NODE((root).statements[(n)].get(), Type)

namespace { // Literals

  TEST(Parser, NumericLiteral) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("42;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Literal);
    ASSERT_TRUE(std::holds_alternative<double>(as_Literal->value));
    EXPECT_DOUBLE_EQ(std::get<double>(as_Literal->value), 42.0);
  }

  TEST(Parser, StringLiteral) {
    TestParser p;
    ASSERT_TRUE(p.parseSource(R"("hello";)"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Literal);
    ASSERT_TRUE(std::holds_alternative<std::string>(as_Literal->value));
    EXPECT_EQ(std::get<std::string>(as_Literal->value), "hello");
  }

  TEST(Parser, BoolLiterals) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("true; false;"));

    const par::Block& root = p.peekRoot();
    ASSERT_EQ(root.statements.size(), 2U);

    // true
    {
      GET_STMT(root, 0, ExprStmt);
      ASSERT_NODE(as_ExprStmt->expr.get(), Literal);
      ASSERT_TRUE(std::holds_alternative<bool>(as_Literal->value));
      EXPECT_TRUE(std::get<bool>(as_Literal->value));
    }

    // false
    {
      GET_STMT(root, 1, ExprStmt);
      ASSERT_NODE(as_ExprStmt->expr.get(), Literal);
      ASSERT_TRUE(std::holds_alternative<bool>(as_Literal->value));
      EXPECT_FALSE(std::get<bool>(as_Literal->value));
    }
  }

  TEST(Parser, NullLiteral) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("null;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Literal);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(as_Literal->value));
  }
} // namespace
