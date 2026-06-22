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

namespace { // Identifiers
  TEST(Parser, BareIdentifier) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("myVar;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Identifier);
    EXPECT_EQ(as_Identifier->name, "myVar");
  }
} // namespace

namespace { // Variable Declarations

  TEST(Parser, VarDeclNoInitializer) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("pin x;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    EXPECT_EQ(as_VarDecl->name, "x");
    EXPECT_EQ(as_VarDecl->initializer, nullptr);
  }

  TEST(Parser, VarDeclWithInitializer) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("pin answer = 42;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    EXPECT_EQ(as_VarDecl->name, "answer");
    ASSERT_NE(as_VarDecl->initializer, nullptr);
    ASSERT_EQ(as_VarDecl->initializer->type, par::NodeType::LITERAL);
    auto* lit = static_cast<par::Literal*>(as_VarDecl->initializer.get());
    EXPECT_DOUBLE_EQ(std::get<double>(lit->value), 42.0);
  }

  TEST(Parser, VarDeclWithStringInitializer) {
    TestParser p;
    ASSERT_TRUE(p.parseSource(R"(pin msg = "hi";)"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    EXPECT_EQ(as_VarDecl->name, "msg");
    ASSERT_NE(as_VarDecl->initializer, nullptr);
    auto* lit = static_cast<par::Literal*>(as_VarDecl->initializer.get());
    EXPECT_EQ(std::get<std::string>(lit->value), "hi");
  }
} // namespace

namespace { // Expressions binary, unary, precedence
  TEST(Parser, BinaryAddition) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("1 + 2;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Binary);
    EXPECT_EQ(as_Binary->operation, lex::TokenType::PLUS);
    ASSERT_EQ(as_Binary->left->type, par::NodeType::LITERAL);
    ASSERT_EQ(as_Binary->right->type, par::NodeType::LITERAL);
  }

  TEST(Parser, BinaryPrecedenceMulBeforeAdd) {
    // 1 + 2 * 3 should parse as 1 + (2 * 3)
    TestParser p;
    ASSERT_TRUE(p.parseSource("1 + 2 * 3;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Binary);
    EXPECT_EQ(as_Binary->operation, lex::TokenType::PLUS);

    // right child must be the multiplication
    ASSERT_EQ(as_Binary->right->type, par::NodeType::BINARY);
    auto* mul = static_cast<par::Binary*>(as_Binary->right.get());
    EXPECT_EQ(mul->operation, lex::TokenType::STAR);
  }

  TEST(Parser, UnaryNegation) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("-5;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Unary);
    EXPECT_EQ(as_Unary->operation, lex::TokenType::MINUS);
    ASSERT_EQ(as_Unary->operand->type, par::NodeType::LITERAL);
  }

  TEST(Parser, UnaryLogicalNot) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("!true;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Unary);
    EXPECT_EQ(as_Unary->operation, lex::TokenType::NOT);
  }

  TEST(Parser, AssignmentIsRightAssociative) {
    // a = b = 1 should parse as a = (b = 1)
    TestParser p;
    ASSERT_TRUE(p.parseSource("a = b = 1;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Binary);
    EXPECT_EQ(as_Binary->operation, lex::TokenType::ASSIGN);

    // right child must also be an assignment
    ASSERT_EQ(as_Binary->right->type, par::NodeType::BINARY);
    auto* inner = static_cast<par::Binary*>(as_Binary->right.get());
    EXPECT_EQ(inner->operation, lex::TokenType::ASSIGN);
  }

  TEST(Parser, CompoundAssignmentOperators) {
    const std::vector<std::pair<std::string, lex::TokenType>> cases = {
      {"x += 1;", lex::TokenType::PLUS_ASSIGN},
      {"x -= 1;", lex::TokenType::MINUS_ASSIGN},
      {"x *= 2;", lex::TokenType::STAR_ASSIGN},
      {"x /= 2;", lex::TokenType::SLASH_ASSIGN},
      {"x %= 3;", lex::TokenType::PERCENT_ASSIGN},
    };

    for (const auto& [src, expected_op] : cases) {
      TestParser p;
      ASSERT_TRUE(p.parseSource(src)) << "Failed to parse: " << src;
      const par::Block& root = p.peekRoot();
      GET_STMT(root, 0, ExprStmt);
      ASSERT_NODE(as_ExprStmt->expr.get(), Binary);
      EXPECT_EQ(as_Binary->operation, expected_op) << "Wrong op for: " << src;
    }
  }

  TEST(Parser, GroupedExpressionOverridesPrecedence) {
    // (1 + 2) * 3 — the addition must be inside the mul
    TestParser p;
    ASSERT_TRUE(p.parseSource("(1 + 2) * 3;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Binary);
    EXPECT_EQ(as_Binary->operation, lex::TokenType::STAR);
    ASSERT_EQ(as_Binary->left->type, par::NodeType::BINARY);
    auto* add = static_cast<par::Binary*>(as_Binary->left.get());
    EXPECT_EQ(add->operation, lex::TokenType::PLUS);
  }
} // namespace

namespace { // Ternary
  TEST(Parser, TernaryExpression) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("a ? 1 : 2;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Ternary);
    ASSERT_EQ(as_Ternary->condition->type, par::NodeType::IDENTIFIER);
    ASSERT_EQ(as_Ternary->then_expr->type, par::NodeType::LITERAL);
    ASSERT_EQ(as_Ternary->else_expr->type, par::NodeType::LITERAL);
  }
} // namespace

namespace { // Member access & subscript

  TEST(Parser, MemberAccessDot) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("obj.field;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), MemberAccess);
    EXPECT_EQ(as_MemberAccess->field, "field");
    ASSERT_EQ(as_MemberAccess->object->type, par::NodeType::IDENTIFIER);
  }

  TEST(Parser, ChainedMemberAccess) {
    // a.b.c  =>  MemberAccess(MemberAccess(a, b), c)
    TestParser p;
    ASSERT_TRUE(p.parseSource("a.b.c;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), MemberAccess);
    EXPECT_EQ(as_MemberAccess->field, "c");
    ASSERT_EQ(as_MemberAccess->object->type, par::NodeType::MEMBER_ACCESS);
    auto* inner = static_cast<par::MemberAccess*>(as_MemberAccess->object.get());
    EXPECT_EQ(inner->field, "b");
  }

  TEST(Parser, Subscript) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("arr[0];"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Subscript);
    ASSERT_EQ(as_Subscript->object->type, par::NodeType::IDENTIFIER);
    ASSERT_EQ(as_Subscript->index->type, par::NodeType::LITERAL);
  }
} // namespace
