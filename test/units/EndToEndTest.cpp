#include <Evaluator.h>
#include <Parser.h>
#include <Value.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <io.h> // _open, _dup, _dup2 on Windows
#else
#include <unistd.h>
#endif
#include <sstream>
#include <string>
#include <vector>

// Locates sis_sources/ relative to the test binary via argv[0], so no path
// needs to be hard-coded or passed on the command line.
//
// runFile()   – parse + evaluate a .sis file, capture stdout, return the last
//               Value. Throws std::runtime_error on parse failure so ASSERT_NO_THROW
//               can be used when the test cares about that specifically.
//
// captureRun() – same but only returns the captured stdout lines.

namespace {
  namespace fs = std::filesystem;

  // Derived once at test-binary startup from argv[0]. Everything is relative
  // to this so the suite runs from any working directory.
  fs::path g_sis_sources_dir;

  // Called by the fixture below; separated so it can also be used directly.
  fs::path sourcePath(const std::string& relative) { return g_sis_sources_dir / relative; }

  // Full pipeline: read file → parse (with real include resolution rooted at
  // the file's own directory) → evaluate. Captured stdout is appended to
  // `output_lines`. Returns the last Value produced by the program.
  eval::Value runFile(const fs::path& path, std::vector<std::string>& output_lines) {
    par::Parser parser;
    bool ok = parser.parseRoot(path);
    LOG_DEBUG_FLUSH("parseRoot returned: {}", ok);
    if (!ok) throw std::runtime_error("Parse failed: " + path.string());

    // Redirect stdout to a temp file — avoids pipe buffer deadlock if the
    // evaluator produces more output than the pipe can hold before we drain it.
    fs::path tmp = fs::temp_directory_path() / "sis_test_capture.txt";
    fflush(stdout);
    int saved_fd = dup(STDOUT_FILENO);

#ifdef _WIN32
    int out_fd = open(tmp.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
#else
    int out_fd = open(tmp.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
#endif

    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);

    eval::Evaluator evaluator;
    eval::Value result = evaluator.run(parser);

    fflush(stdout);
    dup2(saved_fd, STDOUT_FILENO);
    close(saved_fd);

    // Read captured output back from the temp file
    std::ifstream captured(tmp);
    std::string line;
    while (std::getline(captured, line))
      output_lines.push_back(line);

    return result;
  }

  // Convenience wrapper when the caller only cares about output lines.
  std::vector<std::string> captureRun(const std::string& relative_path) {
    std::vector<std::string> out;
    runFile(sourcePath(relative_path), out);
    return out;
  }

  // Convenience wrapper when the caller only cares about the final Value.
  eval::Value evalRun(const std::string& relative_path) {
    std::vector<std::string> out;
    return runFile(sourcePath(relative_path), out);
  }

  // Base fixture resolves paths and surfaces runFile/captureRun as members.
  class E2E : public ::testing::Test {
    protected:
    static std::vector<std::string> run(const std::string& relative_path) { return captureRun(relative_path); }
    static eval::Value eval(const std::string& relative_path) { return evalRun(relative_path); }
  };

  // Locates sis_sources/ by walking up from the binary until the directory
  // is found, or falls back to CWD/sis_sources if nothing is found.
  // Registered as a global environment setup so it runs before any test.
  class PathSetup : public ::testing::Environment {
    public:
    void SetUp() override {
      const auto& argv = ::testing::internal::GetArgvs();
      fs::path binary = argv.empty() ? fs::current_path() / "test_binary" : fs::path(argv[0]);
      fs::path dir = fs::weakly_canonical(binary).parent_path();
      while (true) {
        fs::path candidate = dir / "sis_sources" / "tests";
        if (fs::is_directory(candidate)) {
          g_sis_sources_dir = candidate;
          return;
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break; // filesystem root
        dir = parent;
      }
      // Last resort: CWD
      g_sis_sources_dir = fs::current_path() / "sis_sources" / "tests";
    }
  };

  // Registers PathSetup with GTest without touching main().
  const ::testing::Environment* const m_path_env = ::testing::AddGlobalTestEnvironment(new PathSetup);

} // namespace

namespace { // Arithmetic and operators

  TEST_F(E2E, BasicArithmetic) {
    auto out = run("arithmetic/basic.sis");
    EXPECT_EQ(out[0], "5");  // 2 + 3
    EXPECT_EQ(out[1], "5");  // 7 - 2
    EXPECT_EQ(out[2], "12"); // 3 * 4
    EXPECT_EQ(out[3], "4");  // 12 / 3
  }

  TEST_F(E2E, ModuloOperator) {
    auto out = run("arithmetic/modulo.sis");
    EXPECT_EQ(out[0], "1"); // 7 % 3
    EXPECT_EQ(out[1], "0"); // 6 % 2
  }

  TEST_F(E2E, CompoundAssignment) {
    auto out = run("arithmetic/compound_assign.sis");
    EXPECT_EQ(out[0], "15"); // x = 10; x += 5
    EXPECT_EQ(out[1], "10"); // x -= 5
    EXPECT_EQ(out[2], "20"); // x *= 2
    EXPECT_EQ(out[3], "10"); // x /= 2
    EXPECT_EQ(out[4], "1");  // x %= 3
  }

  TEST_F(E2E, ComparisonOperators) {
    auto out = run("arithmetic/comparisons.sis");
    EXPECT_EQ(out[0], "true"); // 5 > 3
    EXPECT_EQ(out[1], "true"); // 3 < 5
    EXPECT_EQ(out[2], "true"); // 5 >= 5
    EXPECT_EQ(out[3], "true"); // 5 <= 5
    EXPECT_EQ(out[4], "true"); // 5 != 4
    EXPECT_EQ(out[5], "true"); // 5 == 5
  }

} // namespace

namespace { // Variables and scoping

  TEST_F(E2E, GlobalVariableDeclarationAndMutation) {
    auto out = run("variables/globals.sis");
    EXPECT_EQ(out[0], "42");
    EXPECT_EQ(out[1], "99");
  }

  TEST_F(E2E, BlockScopeShadowing) {
    // Inner pin x shadows outer x; outer is unchanged afterwards.
    auto out = run("variables/shadowing.sis");
    EXPECT_EQ(out[0], "inner");
    EXPECT_EQ(out[1], "outer");
  }

  TEST_F(E2E, NullIsDefaultUninitialised) {
    auto out = run("variables/null_default.sis");
    EXPECT_EQ(out[0], "null");
  }

} // namespace

namespace { // Control flow

  TEST_F(E2E, IfElseTrueBranch) {
    auto out = run("control_flow/if_else.sis");
    EXPECT_EQ(out[0], "yes");
  }

  TEST_F(E2E, IfElseFalseBranch) {
    auto out = run("control_flow/if_else_false.sis");
    EXPECT_EQ(out[0], "no");
  }

  TEST_F(E2E, ElseIfChain) {
    auto out = run("control_flow/else_if.sis");
    EXPECT_EQ(out[0], "second");
  }

  TEST_F(E2E, TernaryTrueBranch) {
    auto out = run("control_flow/ternary.sis");
    EXPECT_EQ(out[0], "yes");
    EXPECT_EQ(out[1], "no");
  }

  TEST_F(E2E, WhileLoopAccumulates) {
    auto out = run("control_flow/while.sis");
    EXPECT_EQ(out[0], "10"); // 0+1+2+3+4
  }

  TEST_F(E2E, WhileBreakExitsEarly) {
    auto out = run("control_flow/while_break.sis");
    EXPECT_EQ(out[0], "3"); // breaks when count reaches 3
  }

  TEST_F(E2E, WhileContinueSkipsIteration) {
    // Skips index 3; sum = 0+1+2+4 = 7 (or whatever the .sis encodes).
    auto out = run("control_flow/while_continue.sis");
    EXPECT_EQ(out[0], "7");
  }

  TEST_F(E2E, ForLoopBasic) {
    auto out = run("control_flow/for_basic.sis");
    EXPECT_EQ(out[0], "10"); // 0+1+2+3+4
  }

  TEST_F(E2E, ForLoopBreak) {
    auto out = run("control_flow/for_break.sis");
    EXPECT_EQ(out[0], "3");
  }

  TEST_F(E2E, SwitchMatchesCase) {
    auto out = run("control_flow/switch_match.sis");
    EXPECT_EQ(out[0], "two");
  }

  TEST_F(E2E, SwitchFallsToDefault) {
    auto out = run("control_flow/switch_default.sis");
    EXPECT_EQ(out[0], "other");
  }

  TEST_F(E2E, SwitchBreakPreventsBleedthrough) {
    // Only the matched case body runs; subsequent cases must not bleed through.
    auto out = run("control_flow/switch_no_bleedthrough.sis");
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0], "one");
  }

} // namespace

namespace { // Functions

  TEST_F(E2E, SimpleFunctionCall) {
    auto out = run("functions/basic_call.sis");
    EXPECT_EQ(out[0], "15");
  }

  TEST_F(E2E, RecursiveFactorial) {
    auto out = run("functions/factorial.sis");
    EXPECT_EQ(out[0], "120"); // factorial(5)
  }

  TEST_F(E2E, FunctionReturnEarlyExits) {
    // First return encountered must exit; nothing after it should print.
    auto out = run("functions/early_return.sis");
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0], "first");
  }

  TEST_F(E2E, FunctionAsValue) {
    // Functions are first-class: assign to pin, pass as arg, return from fn.
    auto out = run("functions/first_class.sis");
    EXPECT_EQ(out[0], "called");
  }

  TEST_F(E2E, ClosureCapturesEnclosingScope) {
    auto out = run("functions/closure.sis");
    EXPECT_EQ(out[0], "15"); // makeAdder(10)(5)
  }

  TEST_F(E2E, ClosureMutatesCapture) {
    // Counter closure: each call increments the same captured variable.
    auto out = run("functions/closure_mutation.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "2");
    EXPECT_EQ(out[2], "3");
  }

} // namespace

namespace { // Arrays

  TEST_F(E2E, ArrayLiteralAndSubscript) {
    auto out = run("arrays/literal.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "3");
    EXPECT_EQ(out[2], "value");
  }

  TEST_F(E2E, ArrayPushAndPop) {
    auto out = run("arrays/push_pop.sis");
    EXPECT_EQ(out[0], "4"); // len after push
    EXPECT_EQ(out[1], "4"); // popped value
    EXPECT_EQ(out[2], "3"); // len after pop
  }

  TEST_F(E2E, ArrayLength) {
    auto out = run("arrays/length.sis");
    EXPECT_EQ(out[0], "3");
  }

  TEST_F(E2E, ArrayMutationByIndex) {
    auto out = run("arrays/mutation.sis");
    EXPECT_EQ(out[0], "99");
  }

  TEST_F(E2E, ArrayReferenceSemantics) {
    // Two pins pointing at the same array; mutating through one is visible
    // through the other.
    auto out = run("arrays/reference_semantics.sis");
    EXPECT_EQ(out[0], "42");
  }

} // namespace

namespace { // Classes and OOP

  TEST_F(E2E, BasicClassInstantiation) {
    auto out = run("classes/basic.sis");
    EXPECT_EQ(out[0], "hello");
  }

  TEST_F(E2E, FieldDefaultValues) {
    auto out = run("classes/field_defaults.sis");
    EXPECT_EQ(out[0], "0");
    EXPECT_EQ(out[1], "unnamed");
  }

  TEST_F(E2E, MethodCanReadAndWriteField) {
    auto out = run("classes/field_access.sis");
    EXPECT_EQ(out[0], "10");
    EXPECT_EQ(out[1], "20");
  }

  TEST_F(E2E, ConstructorRunsOnNew) {
    auto out = run("classes/constructor.sis");
    EXPECT_EQ(out[0], "constructed");
    EXPECT_EQ(out[1], "42");
  }

  TEST_F(E2E, TwoInstancesAreIndependent) {
    // Mutating a field on one instance must not affect the other.
    auto out = run("classes/instance_independence.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "2");
  }

  TEST_F(E2E, ClassIsFirstClassValue) {
    // A class can be stored in a pin and used to construct an instance.
    auto out = run("classes/first_class_class.sis");
    EXPECT_EQ(out[0], "ok");
  }

} // namespace

namespace { // Inheritance and super

  TEST_F(E2E, ChildInheritsParentMethod) {
    auto out = run("inheritance/inherit_method.sis");
    EXPECT_EQ(out[0], "123");
  }

  TEST_F(E2E, OverriddenMethodDispatchesToChild) {
    auto out = run("inheritance/override.sis");
    EXPECT_EQ(out[0], "dog");
  }

  TEST_F(E2E, SuperCallsParentImplementation) {
    auto out = run("inheritance/super_call.sis");
    EXPECT_EQ(out[0], "parent+child");
  }

  TEST_F(E2E, MultiLevelInheritance) {
    // Grandchild → Child → Parent; method lookup walks the full chain.
    auto out = run("inheritance/multi_level.sis");
    EXPECT_EQ(out[0], "grandchild");
    EXPECT_EQ(out[1], "parent-value");
  }

  TEST_F(E2E, ChildFieldsDoNotClobberParentFields) {
    auto out = run("inheritance/field_layering.sis");
    EXPECT_EQ(out[0], "parent-field");
    EXPECT_EQ(out[1], "child-field");
  }

} // namespace

namespace { // Built-in functions

  TEST_F(E2E, PrintOutputsToStdout) {
    auto out = run("builtins/print.sis");
    EXPECT_EQ(out[0], "hello world");
  }

  TEST_F(E2E, StrConvertsToString) {
    auto out = run("builtins/str.sis");
    EXPECT_EQ(out[0], "123");
    EXPECT_EQ(out[1], "true");
    EXPECT_EQ(out[2], "null");
  }

  TEST_F(E2E, NumConvertsToNumber) {
    auto out = run("builtins/num.sis");
    EXPECT_EQ(out[0], "456");
  }

  TEST_F(E2E, TypeReturnsTypeName) {
    auto out = run("builtins/type.sis");
    EXPECT_EQ(out[0], "num");
    EXPECT_EQ(out[1], "string");
    EXPECT_EQ(out[2], "bool");
    EXPECT_EQ(out[3], "null");
    EXPECT_EQ(out[4], "array");
  }

  TEST_F(E2E, LenOnArrayAndString) {
    auto out = run("builtins/len.sis");
    EXPECT_EQ(out[0], "3"); // array
    EXPECT_EQ(out[1], "5"); // "hello"
  }

} // namespace

namespace { // Include resolution

  TEST_F(E2E, IncludeExposesVariableFromLibrary) {
    auto out = run("includes/basic_include.sis");
    EXPECT_EQ(out[0], "0.1"); // LIB_VERSION from library.sis
  }

  TEST_F(E2E, IncludeExposesFunction) {
    auto out = run("includes/basic_include.sis");
    EXPECT_EQ(out[1], "5"); // libraryAdd(2, 3)
  }

  TEST_F(E2E, IncludeExposesClass) {
    auto out = run("includes/basic_include.sis");
    EXPECT_EQ(out[2], "library"); // new LibraryClass().getName()
  }

  TEST_F(E2E, NestedIncludeResolvesRelativeToIncludedFile) {
    // sub/outer.sis includes ../library.sis — path must be resolved relative
    // to sub/, not relative to the top-level test binary.
    auto out = run("includes/sub/outer.sis");
    EXPECT_EQ(out[0], "0.1");
  }

  TEST_F(E2E, DoubleIncludeThrowsCircularInclude) {
    // include error rather than silently skipping it.
    EXPECT_THROW(run("includes/double_include.sis"), std::runtime_error);
  }

} // namespace

namespace { // Error and edge cases

  TEST_F(E2E, ParseErrorOnMissingSemicolonThrows) { EXPECT_THROW(evalRun("errors/missing_semicolon.sis"), std::runtime_error); }

  TEST_F(E2E, EmptyFileRunsWithoutError) { EXPECT_NO_THROW(run("errors/empty.sis")); }

  TEST_F(E2E, NullIsFalsyInCondition) {
    auto out = run("errors/null_falsy.sis");
    EXPECT_EQ(out[0], "was null");
  }

  TEST_F(E2E, ZeroIsFalsyInCondition) {
    auto out = run("errors/zero_falsy.sis");
    EXPECT_EQ(out[0], "was zero");
  }

  TEST_F(E2E, EmptyStringIsFalsy) {
    auto out = run("errors/empty_string_falsy.sis");
    EXPECT_EQ(out[0], "was empty");
  }

  TEST_F(E2E, CompoundAssingOnMissingKeyThrows) { EXPECT_THROW(run("errors/missing_key_compound_assign.sis"), std::runtime_error); }

} // namespace
