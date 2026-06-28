#pragma once

#include <Environment.h>
#include <Program.h>
#include <ParserNodeTypes.h>
#include <SisRegistry.h>
#include <Value.h>

#include <filesystem>
#include <memory>
#include <unordered_map>
#ifdef __unix__
#include <dlfcn.h>
#endif

namespace eval {
  // Thrown to unwind the C++ call stack back up to the nearest enclosing
  // function call when a return statement runs. evalReturn throws this,
  // callFunction is what catches it (see Evaluator.cpp), any block/if/while
  // frames in between just let it pass through unmodified, which is exactly
  // the "unwind until you hit a function boundary" semantics return needs.
  struct ReturnSignal {
    Value value;
  };

  // Same idea for loop control: evalJump throws these (one per JumpKind),
  // evalWhile is what catches them. Anything nested inside the loop body
  // (if-statements, nested blocks) just lets the exception fly through.
  struct BreakSignal {};
  struct ContinueSignal {};

  class Evaluator {
    public:
    Evaluator();
    Evaluator(const int argc, const char* argv[])
      : Evaluator() {
      m_argc = argc;
      m_argv = argv;
    }

    // Runs an entire parsed program (the top level Block from Parser::parse)
    // in the global environment. Returns the value of the last statement,
    // mostly useful for REPL style usage, the return value doesn't matter for
    // running a script for its side effects.
    Value run(const par::Program& program);

    std::shared_ptr<Environment> globalEnv() const { return m_global; }

    private:
    const Path* m_current_eval_file;
    std::shared_ptr<Environment> m_global;
    std::unordered_map<Path, std::shared_ptr<Environment>> m_file_cache;
    int m_argc = 0;
    const char** m_argv = nullptr;

    // Maps each FnLiteral AST node to the source file it was declared in.
    // Populated in evalFnLiteral; read in callFunction to switch
    // m_current_eval_file to the callee's file before running its body.
    std::unordered_map<const par::FnLiteral*, const Path*> m_fn_source_file;

    // One frame per active callFunction invocation. Records the CALL SITE
    // (the caller's file and the Call/NewExpr node), pushed before
    // m_current_eval_file is updated so it reflects where the call was made,
    // not where the callee lives. Used by throwKnownScopeErr to print a
    // "called from ..." stack trace alongside the error location.
    struct CallFrame {
      const Path* file;      // source file at the call site
      const par::Node* node; // Call/NewExpr node — gives line/col of the call
    };
    std::vector<CallFrame> m_call_stack;

    [[noreturn]] void throwKnownScopeErr(const par::Node* node, std::string msg);

    // Name -> runtime Class, populated as ClassDecl statements are
    // evaluated. Looked up by name from evalNewExpr and from MemberAccess
    // method-call resolution. Keyed separately from m_global (rather than
    // just stuffing the Class Value into the environment under its name,
    // which ALSO happens, see evalClassDecl) so `new Foo(...)` can resolve
    // the class even if `Foo` the variable got shadowed or reassigned
    // somewhere, classes are looked up by their declared identity, not by
    // whatever a variable currently holds.
    std::unordered_map<std::string, std::shared_ptr<Class>> m_classes;

    // Registers every native (built-in) function into `env`, e.g. print(),
    // len(). Called once from the constructor against m_global. See
    // registerBuiltins() in Evaluator.cpp for the actual list, that's the
    // single place to add a new built-in: define a NativeFunction and
    // env->define() it.
    static void registerBuiltins(const std::shared_ptr<Environment>& env);

    Value cmpDouble(const par::Binary* node, const double* l, const double* r);
    Value applyCompoundOp(const par::Node* node, lex::TokenType op, const Value& current, const Value& rhs);

    std::shared_ptr<Environment> loadFile(const Path& path, const par::Block& block, Value* out_last);
    std::shared_ptr<Environment> loadDynamicLib(const Path& path);
    static void mergeIntoEnv(const std::shared_ptr<Environment>& src, const std::shared_ptr<Environment>& dst);

    Value evaluate(const par::Node* node, const std::shared_ptr<Environment>& env);

    Value evalBlock(const par::Block* node, const std::shared_ptr<Environment>& env);
    Value evalLiteral(const par::Literal* node);
    Value evalIdentifier(const par::Identifier* node, const std::shared_ptr<Environment>& env);
    Value evalUnary(const par::Unary* node, const std::shared_ptr<Environment>& env);
    Value evalBinary(const par::Binary* node, const std::shared_ptr<Environment>& env);
    Value evalIf(const par::If* node, const std::shared_ptr<Environment>& env);
    Value evalWhile(const par::While* node, const std::shared_ptr<Environment>& env);
    Value evalFor(const par::For* node, const std::shared_ptr<Environment>& env);
    Value evalSwitch(const par::Switch* node, const std::shared_ptr<Environment>& env);
    Value evalTernary(const par::Ternary* node, const std::shared_ptr<Environment>& env);
    Value evalVarDecl(const par::VarDecl* node, const std::shared_ptr<Environment>& env);
    Value evalExprStmt(const par::ExprStmt* node, const std::shared_ptr<Environment>& env);
    Value evalCall(const par::Call* node, const std::shared_ptr<Environment>& env);
    Value evalFnLiteral(const par::FnLiteral* node, const std::shared_ptr<Environment>& env);
    Value evalArrayLiteral(const par::ArrayLiteral* node, const std::shared_ptr<Environment>& env);
    Value evalMemberAccess(const par::MemberAccess* node, const std::shared_ptr<Environment>& env);
    Value evalSubscript(const par::Subscript* node, const std::shared_ptr<Environment>& env);

    // return [expr]; throws ReturnSignal{evaluated value}, caught in
    // callFunction. Never returns normally, but has a Value return type to
    // match every other evalX so evaluate()'s switch stays uniform.
    Value evalReturn(const par::Return* node, const std::shared_ptr<Environment>& env);

    // break; / continue; one Jump node, kind tells them apart. Throws
    // BreakSignal{} / ContinueSignal{}, caught in evalWhile. Same "never
    // actually returns" story as evalReturn.
    Value evalJump(const par::Jump* node, const std::shared_ptr<Environment>& env);

    // class Name [extends Parent] { ... }. Builds the runtime Class
    // (resolving the parent class by name if `extends` was used, error if
    // it's not found or isn't actually a class), registers it under m_classes
    // AND defines a variable of the same name in `env` holding the Class as
    // a first-class Value (so e.g. `pin C = SomeClass;` and passing classes
    // around works the same way functions do).
    Value evalClassDecl(const par::ClassDecl* node, const std::shared_ptr<Environment>& env);

    // new ClassName(args). Looks up the class by name, allocates a fresh
    // field map, walks the inheritance chain applying every field default
    // (parent's fields first, so a subclass can re-declare/override a
    // field name and have its own default win), then calls `constructor`
    // if the class (or an ancestor) defines one.
    Value evalNewExpr(const par::NewExpr* node, const std::shared_ptr<Environment>& env);

    // this->field / super->field, and (when wrapped in a Call by evalCall)
    // this->method(...) / super->method(...). The parser always wraps a bare
    // `this`/`super` in a MemberAccess (see par::Self), so this is reached
    // from evalMemberAccess whenever its object child is a Self node rather
    // than getting its own NodeType dispatch case. For `this`, looks up
    // `field` starting at the instance's own class. For `super`, looks up
    // `field` starting at the PARENT of the class the currently-executing
    // method was defined on (not the parent of the instance's runtime class,
    // that distinction matters for multi-level inheritance: a grandchild
    // calling an overridden method that itself calls super-> should resolve
    // to the grandparent, not loop back to itself). That "which class is the
    // current method defined on" context is threaded through via a
    // "__class__" entry placed in the call environment by callFunction.
    Value evalSelfMemberAccess(const par::Self* self_node, const std::string& field, const par::Node* node, const std::shared_ptr<Environment>& env);

    // Handles the case where node->operation is an assignment operator
    // (=, +=, -=, etc). Called from evalBinary, which checks this first
    // before falling into normal arithmetic/comparison handling. Targets can
    // be a plain Identifier, a MemberAccess (instance.field = ...), or a
    // MemberAccess whose object is Self (this->field = ...).
    Value evalAssignment(const par::Binary* node, const std::shared_ptr<Environment>& env);

    // Invokes a Function value with already evaluated arguments. Builds a
    // fresh Environment whose parent is the closure (not the call site),
    // that's what makes scoping lexical instead of dynamic. If `bound_this`
    // is set, it's defined as "this" in that fresh scope (method call), and
    // `defining_class` (when set) is stashed as "__class__" so
    // evalSelfMemberAccess knows which class's parent to start searching from.
    Value callFunction(const Function& fn,
                       std::vector<Value> args,
                       const par::Node* call_node,
                       const std::optional<Value>& bound_this = std::nullopt,
                       const std::shared_ptr<Class>& defining_class = nullptr);

    // Shared implementation behind evalMemberAccess and evalSelfMemberAccess:
    // given an already-evaluated `object` Value and a field name, returns the
    // field's value, a bound method (a Function whose closure has "this"
    // pre-defined), or array/string .length. `search_class` overrides which
    // class's method table to search instead of the instance's own runtime
    // class, used by super-> to skip past the current class straight to its
    // parent.
    Value resolveMember(const Value& object, const std::string& field, const par::Node* node, const std::shared_ptr<Class>& search_class = nullptr);
  };
} // namespace eval
