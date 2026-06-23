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

namespace { // Logical operators

  TEST(Evaluator, AndReturnsFalsyLeft) {
    // false && anything => the falsy left operand
    auto v = runScript("false && true;");
    ASSERT_TRUE(std::holds_alternative<bool>(v.data));
    EXPECT_FALSE(std::get<bool>(v.data));
  }

  TEST(Evaluator, AndReturnsTruthyRight) {
    // 1 && 2 => 2 (both truthy, last one wins)
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("1 && 2;").data), 2.0);
  }

  TEST(Evaluator, OrReturnsTruthyLeft) {
    // 1 || 2 => 1 (left is truthy, right never evaluated)
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("1 || 2;").data), 1.0);
  }

  TEST(Evaluator, OrReturnsFalsyRight) {
    // false || 5 => 5
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("false || 5;").data), 5.0);
  }

  TEST(Evaluator, AndShortCircuits) {
    // right operand is never evaluated when left is falsy
    EXPECT_NO_THROW(runScript("false && undefinedVar;"));
  }

  TEST(Evaluator, OrShortCircuits) {
    EXPECT_NO_THROW(runScript("true || undefinedVar;"));
  }

} // namespace

namespace { // Unary operators

  TEST(Evaluator, UnaryNegation) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("-7;").data), -7.0);
  }

  TEST(Evaluator, UnaryNot) {
    EXPECT_FALSE(std::get<bool>(runScript("!true;").data));
    EXPECT_TRUE(std::get<bool>(runScript("!false;").data));
  }

  TEST(Evaluator, NotUsesToInessRules) {
    // 0 is falsy so !0 is true; any non-zero is truthy
    EXPECT_TRUE(std::get<bool>(runScript("!0;").data));
    EXPECT_FALSE(std::get<bool>(runScript("!1;").data));
  }

} // namespace

namespace { // Variable declaration and assignment

  TEST(Evaluator, VarDeclWithInitializer) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 10; x;").data), 10.0);
  }

  TEST(Evaluator, VarDeclNoInitializerIsNull) {
    EXPECT_TRUE(std::holds_alternative<std::monostate>(runScript("pin x; x;").data));
  }

  TEST(Evaluator, Assignment) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 1; x = 99; x;").data), 99.0);
  }

  TEST(Evaluator, CompoundAssignmentOperators) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 5; x += 3; x;").data), 8.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 10; x -= 4; x;").data), 6.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 3; x *= 4; x;").data), 12.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 9; x /= 3; x;").data), 3.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 10; x %= 3; x;").data), 1.0);
  }

  TEST(Evaluator, StringCompoundAssign) {
    // += on strings follows the same string-coercion rule as +
    EXPECT_EQ(std::get<std::string>(runScript(R"(pin s = "hi"; s += "!"; s;)").data), "hi!");
  }

  TEST(Evaluator, AssignmentIsRightAssociative) {
    // a = b = 5 means both a and b end up as 5
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin a = 0; pin b = 0; a = b = 5; a;").data), 5.0);
  }

} // namespace

namespace { // Scope

  TEST(Evaluator, InnerBlockSeesOuterVariable) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 10; { x; }").data), 10.0);
  }

  TEST(Evaluator, InnerBlockDoesNotLeakToOuter) {
    EXPECT_THROW(runScript("{ pin x = 1; } x;"), std::runtime_error);
  }

  TEST(Evaluator, InnerBlockShadowsOuter) {
    // inner pin x is a separate binding; outer x is unchanged after the block
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 1; { pin x = 2; } x;").data), 1.0);
  }

  TEST(Evaluator, InnerBlockMutationVisibleOutside) {
    // assigning (not declaring) x inside the block changes the outer one
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("pin x = 1; { x = 2; } x;").data), 2.0);
  }

} // namespace

namespace { // If / else

  TEST(Evaluator, IfTrueBranchTaken) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("if (true) { 1; } else { 2; }").data), 1.0);
  }

  TEST(Evaluator, IfFalseBranchTaken) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("if (false) { 1; } else { 2; }").data), 2.0);
  }

  TEST(Evaluator, IfFalseNoElseReturnsNull) {
    EXPECT_TRUE(std::holds_alternative<std::monostate>(runScript("if (false) { 1; }").data));
  }

  TEST(Evaluator, ElseIf) {
    auto v = runScript("pin x = 2;"
                       "if (x == 1) { 10; } else if (x == 2) { 20; } else { 30; }");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 20.0);
  }

} // namespace

namespace { // While / For loop

  TEST(Evaluator, WhileBasicCount) {
    auto v = runScript("pin i = 0; while (i < 5) { i += 1; } i;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 5.0);
  }

  TEST(Evaluator, WhileBodySkippedWhenFalse) {
    auto v = runScript("pin i = 0; while (false) { i += 1; } i;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 0.0);
  }

  TEST(Evaluator, WhileBreak) {
    auto v = runScript("pin i = 0; while (true) { i += 1; if (i == 3) { break; } } i;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 3.0);
  }

  TEST(Evaluator, WhileContinue) {
    // sum only even numbers from 1 to 4 => 2 + 4 = 6
    auto v = runScript("pin sum = 0; pin i = 0;"
                       "while (i < 5) { i += 1; if (i % 2 != 0) { continue; } sum += i; }"
                       "sum;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 6.0);
  }

  TEST(Evaluator, ReturnInsideWhileExitsFunction) {
    // return should propagate past the loop boundary and exit the function
    auto v = runScript("fn f() {"
                       "  pin i = 0;"
                       "  while (true) { i += 1; if (i == 2) { return i; } }"
                       "}"
                       "f();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 2.0);
  }

  TEST(Evaluator, ForBasicSum) {
    // 0 + 1 + 2 + 3 = 6
    auto v = runScript("pin acc = 0; for (pin i = 0; i < 4; i += 1) { acc += i; } acc;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 6.0);
  }

  TEST(Evaluator, ForInitDoesNotLeakScope) {
    EXPECT_THROW(runScript("for (pin i = 0; i < 1; i += 1) {} i;"), std::runtime_error);
  }

  TEST(Evaluator, ForBreak) {
    auto v = runScript("pin acc = 0;"
                       "for (pin i = 0; i < 10; i += 1) { if (i == 3) { break; } acc += 1; }"
                       "acc;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 3.0);
  }

  TEST(Evaluator, ForContinueRunsIncrement) {
    // skip even i; odd values 1+3+5+7+9 = 25
    auto v = runScript("pin sum = 0;"
                       "for (pin i = 0; i < 10; i += 1) { if (i % 2 == 0) { continue; } sum += i; }"
                       "sum;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 25.0);
  }

  TEST(Evaluator, ForInfiniteWithBreak) {
    auto v = runScript("pin x = 0; for (;;) { x += 1; if (x == 5) { break; } } x;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 5.0);
  }

} // namespace

namespace { // Switch

  TEST(Evaluator, SwitchMatchesCase) {
    auto v = runScript("pin x = 2;"
                       "switch (x) { case 1: x = 10; break; case 2: x = 20; break; }"
                       "x;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 20.0);
  }

  TEST(Evaluator, SwitchDefault) {
    auto v = runScript("pin x = 99;"
                       "switch (x) { case 1: x = 1; break; default: x = 0; break; }"
                       "x;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 0.0);
  }

  TEST(Evaluator, SwitchFallThrough) {
    // no break between case 1 and case 2 — both bodies run
    auto v = runScript("pin acc = 0;"
                       "switch (1) { case 1: acc += 10; case 2: acc += 20; break; }"
                       "acc;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 30.0);
  }

  TEST(Evaluator, SwitchNoMatchNoDefaultReturnsNull) {
    EXPECT_TRUE(std::holds_alternative<std::monostate>(runScript("switch (99) { case 1: 1; }").data));
  }

} // namespace

namespace { // Ternary

  TEST(Evaluator, TernaryTrueBranch) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("true ? 1 : 2;").data), 1.0);
  }

  TEST(Evaluator, TernaryFalseBranch) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("false ? 1 : 2;").data), 2.0);
  }

  TEST(Evaluator, TernaryOnlyEvaluatesOneBranch) {
    // else branch has an undefined var; since condition is true it's never reached
    EXPECT_NO_THROW(runScript("true ? 1 : undefinedVar;"));
  }

} // namespace

namespace { // Functions and closures

  TEST(Evaluator, BasicFunctionCall) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("fn double(x) { return x * 2; } double(5);").data), 10.0);
  }

  TEST(Evaluator, ReturnExitsEarly) {
    // the 2 is never reached
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("fn f() { return 1; 2; } f();").data), 1.0);
  }

  TEST(Evaluator, ReturnNoValueIsNull) {
    EXPECT_TRUE(std::holds_alternative<std::monostate>(runScript("fn f() { return; } f();").data));
  }

  TEST(Evaluator, ImplicitReturnLastExpression) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("fn add(a, b) { a + b; } add(3, 4);").data), 7.0);
  }

  TEST(Evaluator, ClosureCapturesEnvironment) {
    // getX closes over the environment; mutation after definition is visible
    auto v = runScript("pin x = 10;"
                       "fn getX() { return x; }"
                       "x = 42;"
                       "getX();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 42.0);
  }

  TEST(Evaluator, ClosurePreservesScope) {
    // makeAdder returns a fn that closes over n; n stays alive after makeAdder returns
    auto v = runScript("fn makeAdder(n) { return fn(x) { return x + n; }; }"
                       "pin add5 = makeAdder(5);"
                       "add5(3);");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 8.0);
  }

  TEST(Evaluator, Recursion) {
    EXPECT_DOUBLE_EQ(std::get<double>(
      runScript("fn fact(n) { if (n <= 1) { return 1; } return n * fact(n - 1); } fact(5);").data), 120.0);
  }

  TEST(Evaluator, FunctionAsFirstClassValue) {
    auto v = runScript("fn apply(f, x) { return f(x); }"
                       "fn double(x) { return x * 2; }"
                       "apply(double, 7);");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 14.0);
  }

  TEST(Evaluator, AnonymousFnLiteral) {
    auto v = runScript("pin add = fn(a, b) { return a + b; };"
                       "add(10, 20);");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 30.0);
  }

} // namespace

namespace { // Arrays

  TEST(Evaluator, EmptyArrayLiteral) {
    auto v = runScript("[];");
    ASSERT_TRUE(std::holds_alternative<eval::Array>(v.data));
    EXPECT_EQ(std::get<eval::Array>(v.data)->size(), 0U);
  }

  TEST(Evaluator, ArrayWithElements) {
    auto v = runScript("[1, 2, 3];");
    ASSERT_TRUE(std::holds_alternative<eval::Array>(v.data));
    EXPECT_EQ(std::get<eval::Array>(v.data)->size(), 3U);
    EXPECT_DOUBLE_EQ(std::get<double>((*std::get<eval::Array>(v.data))[0].data), 1.0);
  }

  TEST(Evaluator, ArraySubscript) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("[10, 20, 30][1];").data), 20.0);
  }

  TEST(Evaluator, ArrayLength) {
    EXPECT_DOUBLE_EQ(std::get<double>(runScript("[1, 2, 3].length;").data), 3.0);
  }

  TEST(Evaluator, PushAndPop) {
    auto v = runScript("pin arr = [];"
                       "push(arr, 10);"
                       "push(arr, 20);"
                       "pop(arr);");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 20.0);
  }

  TEST(Evaluator, ArrayReferenceSemantics) {
    // b aliases a; mutating through b is visible through a
    auto v = runScript("pin a = [1]; pin b = a; push(b, 2); a.length;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 2.0);
  }

} // namespace

namespace { // Classes

  TEST(Evaluator, FieldDefaultsApplied) {
    auto v = runScript("class Point { pin x = 0; pin y = 0; }"
                       "pin p = new Point();"
                       "p.x;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 0.0);
  }

  TEST(Evaluator, ConstructorSetsFields) {
    auto v = runScript("class Point {"
                       "  pin x = 0; pin y = 0;"
                       "  fn constructor(a, b) { this->x = a; this->y = b; }"
                       "}"
                       "pin p = new Point(3, 4);"
                       "p.x;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 3.0);
  }

  TEST(Evaluator, MethodCall) {
    auto v = runScript("class Counter {"
                       "  pin count = 0;"
                       "  fn increment() { this->count += 1; }"
                       "  fn get() { return this->count; }"
                       "}"
                       "pin c = new Counter();"
                       "c.increment(); c.increment();"
                       "c.get();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 2.0);
  }

  TEST(Evaluator, InstancesAreIndependent) {
    // two instances do not share field storage
    auto v = runScript("class Box { pin val = 0; }"
                       "pin a = new Box(); pin b = new Box();"
                       "a.val = 5;"
                       "b.val;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 0.0);
  }

  TEST(Evaluator, InstanceReferenceSemantics) {
    // b aliases a; mutation through b is visible through a
    auto v = runScript("class Box { pin val = 0; }"
                       "pin a = new Box(); pin b = a;"
                       "b.val = 99;"
                       "a.val;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 99.0);
  }

  TEST(Evaluator, FieldCompoundAssignInsideMethod) {
    auto v = runScript("class C { pin x = 10; fn addX(n) { this->x += n; } }"
                       "pin c = new C();"
                       "c.addX(5);"
                       "c.x;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 15.0);
  }

  TEST(Evaluator, BoundMethodStoredInVariable) {
    // binding a method to a variable and calling it later still has the right `this`
    auto v = runScript("class C { pin v = 7; fn getV() { return this->v; } }"
                       "pin c = new C();"
                       "pin m = c.getV;"
                       "m();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 7.0);
  }

} // namespace

namespace { // Inheritance and super

  TEST(Evaluator, InheritedFieldDefaultsApplied) {
    auto v = runScript("class Animal { pin legs = 4; }"
                       "class Dog extends Animal {}"
                       "pin d = new Dog();"
                       "d.legs;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 4.0);
  }

  TEST(Evaluator, ChildFieldOverridesParentDefault) {
    auto v = runScript("class Animal { pin legs = 4; }"
                       "class Snake extends Animal { pin legs = 0; }"
                       "pin s = new Snake();"
                       "s.legs;");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 0.0);
  }

  TEST(Evaluator, InheritedMethodCall) {
    auto v = runScript("class Animal { fn speak() { return 42; } }"
                       "class Dog extends Animal {}"
                       "new Dog().speak();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 42.0);
  }

  TEST(Evaluator, OverriddenMethodCall) {
    auto v = runScript("class Animal { fn speak() { return 1; } }"
                       "class Dog extends Animal { fn speak() { return 2; } }"
                       "new Dog().speak();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 2.0);
  }

  TEST(Evaluator, SuperMethodCall) {
    auto v = runScript("class Animal { fn speak() { return 10; } }"
                       "class Dog extends Animal {"
                       "  fn speak() { return super->speak() + 5; }"
                       "}"
                       "new Dog().speak();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 15.0);
  }

  TEST(Evaluator, SuperConstructorChain) {
    auto v = runScript("class Animal {"
                       "  pin type = \"unknown\";"
                       "  fn constructor(t) { this->type = t; }"
                       "}"
                       "class Dog extends Animal {"
                       "  fn constructor() { super->constructor(\"dog\"); }"
                       "}"
                       "pin d = new Dog();"
                       "d.type;");
    EXPECT_EQ(std::get<std::string>(v.data), "dog");
  }

  TEST(Evaluator, MultiLevelInheritance) {
    // A -> B -> C; C instance can call A's method via inheritance chain
    auto v = runScript("class A { fn val() { return 1; } }"
                       "class B extends A {}"
                       "class C extends B {}"
                       "new C().val();");
    EXPECT_DOUBLE_EQ(std::get<double>(v.data), 1.0);
  }

} // namespace
