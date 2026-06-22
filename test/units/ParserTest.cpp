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

namespace { // Array literals
  TEST(Parser, EmptyArrayLiteral) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("[];"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), ArrayLiteral);
    EXPECT_TRUE(as_ArrayLiteral->elements.empty());
  }

  TEST(Parser, ArrayLiteralWithElements) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("[1, 2, 3];"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), ArrayLiteral);
    EXPECT_EQ(as_ArrayLiteral->elements.size(), 3U);
    for (auto& elem : as_ArrayLiteral->elements) {
      EXPECT_EQ(elem->type, par::NodeType::LITERAL);
    }
  }
} // namespace

namespace { // Function calls
  TEST(Parser, CallNoArgs) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("foo();"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Call);
    ASSERT_EQ(as_Call->callee->type, par::NodeType::IDENTIFIER);
    EXPECT_TRUE(as_Call->args.empty());
  }

  TEST(Parser, CallWithArgs) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("add(1, 2, 3);"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Call);
    EXPECT_EQ(as_Call->args.size(), 3U);
  }

  TEST(Parser, MethodCallChain) {
    // obj.method(1) — Call whose callee is a MemberAccess
    TestParser p;
    ASSERT_TRUE(p.parseSource("obj.method(1);"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), Call);
    ASSERT_EQ(as_Call->callee->type, par::NodeType::MEMBER_ACCESS);
    EXPECT_EQ(as_Call->args.size(), 1U);
  }
} // namespace

namespace { // Function literals & top-level fn declarations
  TEST(Parser, AnonymousFnLiteral) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("pin f = fn(x, y) {};"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    EXPECT_EQ(as_VarDecl->name, "f");
    ASSERT_NE(as_VarDecl->initializer, nullptr);
    ASSERT_EQ(as_VarDecl->initializer->type, par::NodeType::FN_LITERAL);
    auto* fn = static_cast<par::FnLiteral*>(as_VarDecl->initializer.get());
    EXPECT_EQ(fn->params.size(), 2U);
    EXPECT_EQ(fn->params[0], "x");
    EXPECT_EQ(fn->params[1], "y");
    ASSERT_EQ(fn->body->type, par::NodeType::BLOCK);
  }

  TEST(Parser, TopLevelFnDeclarationDesugarsToVarDecl) {
    // fn add(a, b) { return a + b; }
    // should produce: VarDecl("add", FnLiteral(["a","b"], Block(...)))
    TestParser p;
    ASSERT_TRUE(p.parseSource("fn add(a, b) { return a + b; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    EXPECT_EQ(as_VarDecl->name, "add");
    ASSERT_NE(as_VarDecl->initializer, nullptr);
    ASSERT_EQ(as_VarDecl->initializer->type, par::NodeType::FN_LITERAL);
    auto* fn = static_cast<par::FnLiteral*>(as_VarDecl->initializer.get());
    ASSERT_EQ(fn->params.size(), 2U);
    EXPECT_EQ(fn->params[0], "a");
    EXPECT_EQ(fn->params[1], "b");
  }

  TEST(Parser, FnNoParams) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("fn greet() {}"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    auto* fn = static_cast<par::FnLiteral*>(as_VarDecl->initializer.get());
    EXPECT_TRUE(fn->params.empty());
  }
} // namespace

namespace { // Return
  TEST(Parser, ReturnWithValue) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("fn f() { return 1; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    auto* fn = static_cast<par::FnLiteral*>(as_VarDecl->initializer.get());
    auto* body = static_cast<par::Block*>(fn->body.get());
    ASSERT_EQ(body->statements.size(), 1U);
    ASSERT_EQ(body->statements[0]->type, par::NodeType::RETURN);
    auto* ret = static_cast<par::Return*>(body->statements[0].get());
    ASSERT_NE(ret->value, nullptr);
    EXPECT_EQ(ret->value->type, par::NodeType::LITERAL);
  }

  TEST(Parser, ReturnNoValue) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("fn f() { return; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, VarDecl);
    auto* fn = static_cast<par::FnLiteral*>(as_VarDecl->initializer.get());
    auto* body = static_cast<par::Block*>(fn->body.get());
    ASSERT_EQ(body->statements.size(), 1U);
    auto* ret = static_cast<par::Return*>(body->statements[0].get());
    EXPECT_EQ(ret->value, nullptr);
  }
} // namespace

namespace { // If / else
  TEST(Parser, IfNoElse) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("if (x) { }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, If);
    ASSERT_NE(as_If->condition, nullptr);
    ASSERT_NE(as_If->then_branch, nullptr);
    EXPECT_EQ(as_If->else_branch, nullptr);
  }

  TEST(Parser, IfElse) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("if (x) { } else { }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, If);
    ASSERT_NE(as_If->else_branch, nullptr);
    EXPECT_EQ(as_If->else_branch->type, par::NodeType::BLOCK);
  }

  TEST(Parser, IfElseIf) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("if (a) { } else if (b) { } else { }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, If);
    ASSERT_NE(as_If->else_branch, nullptr);
    // else-branch is another If node (not a Block)
    EXPECT_EQ(as_If->else_branch->type, par::NodeType::IF);
  }
} // namespace

namespace { // While / For loop
  TEST(Parser, WhileLoop) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("while (x > 0) { x = x - 1; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, While);
    ASSERT_NE(as_While->condition, nullptr);
    ASSERT_NE(as_While->body, nullptr);
    EXPECT_EQ(as_While->body->type, par::NodeType::BLOCK);
  }

  TEST(Parser, ForLoopFullClauses) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("for (pin i = 0; i < 10; i = i + 1) { }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, For);
    ASSERT_NE(as_For->init, nullptr);
    ASSERT_NE(as_For->condition, nullptr);
    ASSERT_NE(as_For->increment, nullptr);
    ASSERT_NE(as_For->body, nullptr);
    EXPECT_EQ(as_For->init->type, par::NodeType::VAR_DECL);
  }

  TEST(Parser, ForLoopEmptyClauses) {
    // for(;;) is valid — all three clauses are null
    TestParser p;
    ASSERT_TRUE(p.parseSource("for (;;) { break; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, For);
    EXPECT_EQ(as_For->init, nullptr);
    EXPECT_EQ(as_For->condition, nullptr);
    EXPECT_EQ(as_For->increment, nullptr);
  }
} // namespace

namespace { // Jump statements (break / continue)
  TEST(Parser, BreakStatement) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("while (true) { break; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, While);
    auto* body = static_cast<par::Block*>(as_While->body.get());
    ASSERT_EQ(body->statements.size(), 1U);
    ASSERT_EQ(body->statements[0]->type, par::NodeType::JUMP);
    auto* jmp = static_cast<par::Jump*>(body->statements[0].get());
    EXPECT_EQ(jmp->kind, par::JumpKind::BREAK);
  }

  TEST(Parser, ContinueStatement) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("while (true) { continue; }"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, While);
    auto* body = static_cast<par::Block*>(as_While->body.get());
    ASSERT_EQ(body->statements.size(), 1U);
    auto* jmp = static_cast<par::Jump*>(body->statements[0].get());
    EXPECT_EQ(jmp->kind, par::JumpKind::CONTINUE);
  }

} // namespace

namespace { // Switch
  TEST(Parser, SwitchWithCasesAndDefault) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("switch (x) {"
                              "  case 1: break;"
                              "  case 2: break;"
                              "  default: break;"
                              "}"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, Switch);
    ASSERT_NE(as_Switch->subject, nullptr);
    EXPECT_EQ(as_Switch->cases.size(), 3U);

    // First two cases have a value; last one is default (value == nullptr).
    ASSERT_NE(as_Switch->cases[0].value, nullptr);
    ASSERT_NE(as_Switch->cases[1].value, nullptr);
    EXPECT_EQ(as_Switch->cases[2].value, nullptr); // default
  }

  TEST(Parser, SwitchCaseBodiesAreCollected) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("switch (v) {"
                              "  case 0: pin a = 1; pin b = 2;"
                              "}"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, Switch);
    ASSERT_EQ(as_Switch->cases.size(), 1U);
    EXPECT_EQ(as_Switch->cases[0].body.size(), 2U);
  }
} // namespace

namespace { // new expression
  TEST(Parser, NewExprNoArgs) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("new Foo();"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), NewExpr);
    EXPECT_EQ(as_NewExpr->class_name, "Foo");
    EXPECT_TRUE(as_NewExpr->args.empty());
  }

  TEST(Parser, NewExprWithArgs) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("new Point(1, 2);"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), NewExpr);
    EXPECT_EQ(as_NewExpr->class_name, "Point");
    EXPECT_EQ(as_NewExpr->args.size(), 2U);
  }
} // namespace

namespace { // this / super member access
  TEST(Parser, ThisArrowMember) {
    // this->field must produce MemberAccess(Self(false), "field")
    TestParser p;
    ASSERT_TRUE(p.parseSource("this->field;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), MemberAccess);
    EXPECT_EQ(as_MemberAccess->field, "field");
    ASSERT_EQ(as_MemberAccess->object->type, par::NodeType::SELF);
    auto* self = static_cast<par::Self*>(as_MemberAccess->object.get());
    EXPECT_FALSE(self->is_super);
  }

  TEST(Parser, SuperArrowMember) {
    TestParser p;
    ASSERT_TRUE(p.parseSource("super->init;"));

    const par::Block& root = p.peekRoot();
    GET_STMT(root, 0, ExprStmt);
    ASSERT_NODE(as_ExprStmt->expr.get(), MemberAccess);
    ASSERT_EQ(as_MemberAccess->object->type, par::NodeType::SELF);
    auto* self = static_cast<par::Self*>(as_MemberAccess->object.get());
    EXPECT_TRUE(self->is_super);
  }
} // namespace

