#include <Evaluator.h>
#include <Parser.h>
#include <Program.h>
#include <Value.h>

#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

namespace {
  namespace fs = std::filesystem;

  fs::path g_sis_sources_dir;

  fs::path sourcePath(const std::string& relative) { return g_sis_sources_dir / relative; }

  eval::Value runFile(const fs::path& path, std::vector<std::string>& output_lines, bool parallel) {
    par::Parser parser;
    parser.setParallel(parallel);
    auto program = parser.parseRoot(path);
    if (!program) throw std::runtime_error("Parse failed: " + path.string());

    fs::path tmp = fs::temp_directory_path() / ("sis_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
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
    eval::Value result = evaluator.run(*program);

    fflush(stdout);
    dup2(saved_fd, STDOUT_FILENO);
    close(saved_fd);

    std::ifstream captured(tmp);
    std::string line;
    while (std::getline(captured, line))
      output_lines.push_back(line);
    captured.close();
    fs::remove(tmp);

    return result;
  }

  // E2E is parameterized on bool (false = serial, true = parallel).
  // Every TEST_P runs twice automatically -- once per mode.
  class E2E : public ::testing::TestWithParam<bool> {
    protected:
    std::vector<std::string> run(const std::string& relative_path) const {
      std::vector<std::string> out;
      runFile(sourcePath(relative_path), out, GetParam());
      return out;
    }

    eval::Value eval(const std::string& relative_path) const {
      std::vector<std::string> out;
      return runFile(sourcePath(relative_path), out, GetParam());
    }
  };

  INSTANTIATE_TEST_SUITE_P(SerialAndParallel, E2E, ::testing::Values(false, true), [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "Parallel" : "Serial"; });

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
        if (parent == dir) break;
        dir = parent;
      }
      g_sis_sources_dir = fs::current_path() / "sis_sources" / "tests";
    }
  };

  const ::testing::Environment* const m_path_env = ::testing::AddGlobalTestEnvironment(new PathSetup);

} // namespace

namespace { // Documentation
  TEST_P(E2E, BasicDocumentationWorks) {
    auto out = run("documentation/docs_works.sis");
    EXPECT_EQ(out[0], " This Class does this");
    EXPECT_EQ(out[1], " This method says hello");
    EXPECT_EQ(out[2], " This free function does nothing"); 
  }
}

namespace { // Arithmetic and operators

  TEST_P(E2E, BasicArithmetic) {
    auto out = run("arithmetic/basic.sis");
    EXPECT_EQ(out[0], "5");
    EXPECT_EQ(out[1], "5");
    EXPECT_EQ(out[2], "12");
    EXPECT_EQ(out[3], "4");
  }

  TEST_P(E2E, ModuloOperator) {
    auto out = run("arithmetic/modulo.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "0");
  }

  TEST_P(E2E, CompoundAssignment) {
    auto out = run("arithmetic/compound_assign.sis");
    EXPECT_EQ(out[0], "15");
    EXPECT_EQ(out[1], "10");
    EXPECT_EQ(out[2], "20");
    EXPECT_EQ(out[3], "10");
    EXPECT_EQ(out[4], "1");
  }

  TEST_P(E2E, ComparisonOperators) {
    auto out = run("arithmetic/comparisons.sis");
    EXPECT_EQ(out[0], "true");
    EXPECT_EQ(out[1], "true");
    EXPECT_EQ(out[2], "true");
    EXPECT_EQ(out[3], "true");
    EXPECT_EQ(out[4], "true");
    EXPECT_EQ(out[5], "true");
  }

} // namespace

namespace { // Variables and scoping

  TEST_P(E2E, GlobalVariableDeclarationAndMutation) {
    auto out = run("variables/globals.sis");
    EXPECT_EQ(out[0], "42");
    EXPECT_EQ(out[1], "99");
  }

  TEST_P(E2E, BlockScopeShadowing) {
    auto out = run("variables/shadowing.sis");
    EXPECT_EQ(out[0], "inner");
    EXPECT_EQ(out[1], "outer");
  }

  TEST_P(E2E, NullIsDefaultUninitialised) {
    auto out = run("variables/null_default.sis");
    EXPECT_EQ(out[0], "null");
  }

} // namespace

namespace { // Control flow

  TEST_P(E2E, IfElseTrueBranch) {
    auto out = run("control_flow/if_else.sis");
    EXPECT_EQ(out[0], "yes");
  }

  TEST_P(E2E, IfElseFalseBranch) {
    auto out = run("control_flow/if_else_false.sis");
    EXPECT_EQ(out[0], "no");
  }

  TEST_P(E2E, ElseIfChain) {
    auto out = run("control_flow/else_if.sis");
    EXPECT_EQ(out[0], "second");
  }

  TEST_P(E2E, TernaryTrueBranch) {
    auto out = run("control_flow/ternary.sis");
    EXPECT_EQ(out[0], "yes");
    EXPECT_EQ(out[1], "no");
  }

  TEST_P(E2E, WhileLoopAccumulates) {
    auto out = run("control_flow/while.sis");
    EXPECT_EQ(out[0], "10");
  }

  TEST_P(E2E, WhileBreakExitsEarly) {
    auto out = run("control_flow/while_break.sis");
    EXPECT_EQ(out[0], "3");
  }

  TEST_P(E2E, WhileContinueSkipsIteration) {
    auto out = run("control_flow/while_continue.sis");
    EXPECT_EQ(out[0], "7");
  }

  TEST_P(E2E, ForLoopBasic) {
    auto out = run("control_flow/for_basic.sis");
    EXPECT_EQ(out[0], "10");
  }

  TEST_P(E2E, ForLoopBreak) {
    auto out = run("control_flow/for_break.sis");
    EXPECT_EQ(out[0], "3");
  }

  TEST_P(E2E, SwitchMatchesCase) {
    auto out = run("control_flow/switch_match.sis");
    EXPECT_EQ(out[0], "two");
  }

  TEST_P(E2E, SwitchFallsToDefault) {
    auto out = run("control_flow/switch_default.sis");
    EXPECT_EQ(out[0], "other");
  }

  TEST_P(E2E, SwitchBreakPreventsBleedthrough) {
    auto out = run("control_flow/switch_no_bleedthrough.sis");
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0], "one");
  }

} // namespace

namespace { // Functions

  TEST_P(E2E, SimpleFunctionCall) {
    auto out = run("functions/basic_call.sis");
    EXPECT_EQ(out[0], "15");
  }

  TEST_P(E2E, RecursiveFactorial) {
    auto out = run("functions/factorial.sis");
    EXPECT_EQ(out[0], "120");
  }

  TEST_P(E2E, FunctionReturnEarlyExits) {
    auto out = run("functions/early_return.sis");
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0], "first");
  }

  TEST_P(E2E, FunctionAsValue) {
    auto out = run("functions/first_class.sis");
    EXPECT_EQ(out[0], "called");
  }

  TEST_P(E2E, ClosureCapturesEnclosingScope) {
    auto out = run("functions/closure.sis");
    EXPECT_EQ(out[0], "15");
  }

  TEST_P(E2E, ClosureMutatesCapture) {
    auto out = run("functions/closure_mutation.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "2");
    EXPECT_EQ(out[2], "3");
  }

} // namespace

namespace { // Arrays

  TEST_P(E2E, ArrayLiteralAndSubscript) {
    auto out = run("arrays/literal.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "3");
    EXPECT_EQ(out[2], "value");
  }

  TEST_P(E2E, ArrayPushAndPop) {
    auto out = run("arrays/push_pop.sis");
    EXPECT_EQ(out[0], "4");
    EXPECT_EQ(out[1], "4");
    EXPECT_EQ(out[2], "3");
  }

  TEST_P(E2E, ArrayLength) {
    auto out = run("arrays/length.sis");
    EXPECT_EQ(out[0], "3");
  }

  TEST_P(E2E, ArrayMutationByIndex) {
    auto out = run("arrays/mutation.sis");
    EXPECT_EQ(out[0], "99");
  }

  TEST_P(E2E, ArrayReferenceSemantics) {
    auto out = run("arrays/reference_semantics.sis");
    EXPECT_EQ(out[0], "42");
  }

} // namespace

namespace { // Classes and OOP

  TEST_P(E2E, BasicClassInstantiation) {
    auto out = run("classes/basic.sis");
    EXPECT_EQ(out[0], "hello");
  }

  TEST_P(E2E, FieldDefaultValues) {
    auto out = run("classes/field_defaults.sis");
    EXPECT_EQ(out[0], "0");
    EXPECT_EQ(out[1], "unnamed");
  }

  TEST_P(E2E, MethodCanReadAndWriteField) {
    auto out = run("classes/field_access.sis");
    EXPECT_EQ(out[0], "10");
    EXPECT_EQ(out[1], "20");
  }

  TEST_P(E2E, ConstructorRunsOnNew) {
    auto out = run("classes/constructor.sis");
    EXPECT_EQ(out[0], "constructed");
    EXPECT_EQ(out[1], "42");
  }

  TEST_P(E2E, TwoInstancesAreIndependent) {
    auto out = run("classes/instance_independence.sis");
    EXPECT_EQ(out[0], "1");
    EXPECT_EQ(out[1], "2");
  }

  TEST_P(E2E, ClassIsFirstClassValue) {
    auto out = run("classes/first_class_class.sis");
    EXPECT_EQ(out[0], "ok");
  }

} // namespace

namespace { // Inheritance and super

  TEST_P(E2E, ChildInheritsParentMethod) {
    auto out = run("inheritance/inherit_method.sis");
    EXPECT_EQ(out[0], "123");
  }

  TEST_P(E2E, OverriddenMethodDispatchesToChild) {
    auto out = run("inheritance/override.sis");
    EXPECT_EQ(out[0], "dog");
  }

  TEST_P(E2E, SuperCallsParentImplementation) {
    auto out = run("inheritance/super_call.sis");
    EXPECT_EQ(out[0], "parent+child");
  }

  TEST_P(E2E, MultiLevelInheritance) {
    auto out = run("inheritance/multi_level.sis");
    EXPECT_EQ(out[0], "grandchild");
    EXPECT_EQ(out[1], "parent-value");
  }

  TEST_P(E2E, ChildFieldsDoNotClobberParentFields) {
    auto out = run("inheritance/field_layering.sis");
    EXPECT_EQ(out[0], "parent-field");
    EXPECT_EQ(out[1], "child-field");
  }

} // namespace

namespace { // Built-in functions

  TEST_P(E2E, PrintOutputsToStdout) {
    auto out = run("builtins/print.sis");
    EXPECT_EQ(out[0], "hello world");
  }

  TEST_P(E2E, StrConvertsToString) {
    auto out = run("builtins/str.sis");
    EXPECT_EQ(out[0], "123");
    EXPECT_EQ(out[1], "true");
    EXPECT_EQ(out[2], "null");
  }

  TEST_P(E2E, NumConvertsToNumber) {
    auto out = run("builtins/num.sis");
    EXPECT_EQ(out[0], "456");
  }

  TEST_P(E2E, TypeReturnsTypeName) {
    auto out = run("builtins/type.sis");
    EXPECT_EQ(out[0], "num");
    EXPECT_EQ(out[1], "string");
    EXPECT_EQ(out[2], "bool");
    EXPECT_EQ(out[3], "null");
    EXPECT_EQ(out[4], "array");
  }

  TEST_P(E2E, LenOnArrayAndString) {
    auto out = run("builtins/len.sis");
    EXPECT_EQ(out[0], "3");
    EXPECT_EQ(out[1], "5");
  }

} // namespace

namespace { // Include resolution

  TEST_P(E2E, IncludeExposesVariableFromLibrary) {
    auto out = run("includes/basic_include.sis");
    EXPECT_EQ(out[0], "0.1");
  }

  TEST_P(E2E, IncludeExposesFunction) {
    auto out = run("includes/basic_include.sis");
    EXPECT_EQ(out[1], "5");
  }

  TEST_P(E2E, IncludeExposesClass) {
    auto out = run("includes/basic_include.sis");
    EXPECT_EQ(out[2], "library");
  }

  TEST_P(E2E, NestedIncludeResolvesRelativeToIncludedFile) {
    auto out = run("includes/sub/outer.sis");
    EXPECT_EQ(out[0], "0.1");
  }

  TEST_P(E2E, DoubleIncludeThrowsCircularInclude) { EXPECT_THROW(run("includes/double_include.sis"), std::runtime_error); }

} // namespace

namespace { // Namespacing

  TEST_P(E2E, NamespacedVariableAccess) {
    auto out = run("includes/namespace_access.sis");
    EXPECT_EQ(out[0], "0.1");
    EXPECT_EQ(out[1], "15");
  }

  TEST_P(E2E, BareVariableFromIncludeIsUndefined) {
    EXPECT_THROW(run("includes/bare_name_fails.sis"), std::runtime_error);
  }

  TEST_P(E2E, BareFunctionFromIncludeIsUndefined) {
    EXPECT_THROW(run("includes/bare_fn_fails.sis"), std::runtime_error);
  }

  TEST_P(E2E, BareClassFromIncludeIsUndefined) {
    EXPECT_THROW(run("includes/bare_new_fails.sis"), std::runtime_error);
  }

  TEST_P(E2E, AliasWorks) {
    auto out = run("includes/alias_works.sis");
    EXPECT_EQ(out[0], "0.1");
    EXPECT_EQ(out[1], "3");
    EXPECT_EQ(out[2], "library");
  }

  TEST_P(E2E, AliasThrowsOnDefaultNameUsage) {
    EXPECT_THROW(run("includes/alias_prevent_default_name.sis"), std::runtime_error);
  }

  TEST_P(E2E, AliasNestingThrows) {
    EXPECT_THROW(run("includes/nested_namespacing.sis"), std::runtime_error);
  }

} // namespace

namespace { // Error and edge cases

  TEST_P(E2E, ParseErrorOnMissingSemicolonThrows) { EXPECT_THROW(eval("errors/missing_semicolon.sis"), std::runtime_error); }

  TEST_P(E2E, EmptyFileRunsWithoutError) { EXPECT_NO_THROW(run("errors/empty.sis")); }

  TEST_P(E2E, NullIsFalsyInCondition) {
    auto out = run("errors/null_falsy.sis");
    EXPECT_EQ(out[0], "was null");
  }

  TEST_P(E2E, ZeroIsFalsyInCondition) {
    auto out = run("errors/zero_falsy.sis");
    EXPECT_EQ(out[0], "was zero");
  }

  TEST_P(E2E, EmptyStringIsFalsy) {
    auto out = run("errors/empty_string_falsy.sis");
    EXPECT_EQ(out[0], "was empty");
  }

  TEST_P(E2E, CompoundAssingOnMissingKeyThrows) { EXPECT_THROW(run("errors/missing_key_compound_assign.sis"), std::runtime_error); }

} // namespace
