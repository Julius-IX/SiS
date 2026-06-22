#include <Evaluator.h>
#include <Token.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <print>
#include <spdlog/fmt/fmt.h>
#include <stdexcept>

namespace eval {
  static bool isAssignmentOperator(lex::TokenType type) {
    switch (type) {
      case lex::TokenType::ASSIGN:
      case lex::TokenType::PLUS_ASSIGN:
      case lex::TokenType::MINUS_ASSIGN:
      case lex::TokenType::STAR_ASSIGN:
      case lex::TokenType::SLASH_ASSIGN:
      case lex::TokenType::PERCENT_ASSIGN: return true;
      default: return false;
    }
  }

  // Maps a compound assignment operator (PLUS_ASSIGN) to the plain binary
  // operator it implies (PLUS), so "x += 1" can reuse the same arithmetic
  // code path as "x = x + 1" instead of duplicating it.
  static lex::TokenType compoundToBinaryOp(lex::TokenType type) {
    switch (type) {
      case lex::TokenType::PLUS_ASSIGN: return lex::TokenType::PLUS;
      case lex::TokenType::MINUS_ASSIGN: return lex::TokenType::MINUS;
      case lex::TokenType::STAR_ASSIGN: return lex::TokenType::STAR;
      case lex::TokenType::SLASH_ASSIGN: return lex::TokenType::SLASH;
      case lex::TokenType::PERCENT_ASSIGN: return lex::TokenType::PERCENT;
      default: return type; // ASSIGN has no binary equivalent, caller handles it separately
    }
  }

  // Equality between two runtime values. Different variant alternatives are
  // never equal (a number is never == a string, even "0" and 0). Arrays and
  // instances compare by identity (same underlying storage), not by
  // contents, that's a deliberate simplification, swap for a deep compare
  // later if you want value semantics instead.
  static bool valuesEqual(const Value& a, const Value& b) {
    if (a.data.index() != b.data.index()) return false;
    return std::visit(
      [&](const auto& av) -> bool {
        using T = std::decay_t<decltype(av)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return true;
        } else if constexpr (std::is_same_v<T, Array>) {
          return av == std::get<Array>(b.data);
        } else if constexpr (std::is_same_v<T, Function>) {
          return av.declaration == std::get<Function>(b.data).declaration;
        } else if constexpr (std::is_same_v<T, NativeFunction>) {
          return false;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Class>>) {
          return av == std::get<std::shared_ptr<Class>>(b.data);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Instance>>) {
          return av == std::get<std::shared_ptr<Instance>>(b.data);
        } else {
          return av == std::get<T>(b.data);
        }
      },
      a.data);
  }

  // ---------------------------------------------------------------------
  // Built-in functions
  //
  // This is the single place to register a native function: build a
  // NativeFunction and env->define() it into the global scope. Every
  // built-in receives already-evaluated arguments as a vector<Value>& and
  // returns a Value, exactly the same calling convention evalCall uses for
  // user-defined functions, so from the language's point of view there's no
  // visible difference between print(...) and a function someone wrote
  // themselves.
  // ---------------------------------------------------------------------
  void Evaluator::registerBuiltins(const std::shared_ptr<Environment>& env) {
    env->define("print",
                Value(NativeFunction{
                  .name = "print",
                  .fn = [](std::vector<Value>& args) -> Value {
                    for (size_t i = 0; i < args.size(); ++i) {
                      fmt::print("{}", args[i].toString());
                      if (i + 1 < args.size()) fmt::print(" ");
                    }
                    fmt::print("\n");
                    return Value{};
                  },
                }));

    env->define("len",
                Value(NativeFunction{
                  .name = "len",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() != 1) {
                      throw std::runtime_error("len() expects exactly 1 argument, got " + std::to_string(args.size()));
                    }
                    if (const auto* arr = std::get_if<Array>(&args[0].data)) {
                      return Value(static_cast<double>((*arr)->size()));
                    }
                    if (const auto* str = std::get_if<std::string>(&args[0].data)) {
                      return Value(static_cast<double>(str->size()));
                    }
                    throw std::runtime_error("len() expects an array or string, got " + args[0].typeName());
                  },
                }));

    env->define("type",
                Value(NativeFunction{
                  .name = "type",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() != 1) {
                      throw std::runtime_error("type() expects exactly 1 argument, got " + std::to_string(args.size()));
                    }
                    return Value(args[0].typeName());
                  },
                }));

    env->define("str",
                Value(NativeFunction{
                  .name = "str",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() != 1) {
                      throw std::runtime_error("str() expects exactly 1 argument, got " + std::to_string(args.size()));
                    }
                    return Value(args[0].toString());
                  },
                }));

    env->define("num",
                Value(NativeFunction{
                  .name = "num",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() != 1) {
                      throw std::runtime_error("num() expects exactly 1 argument, got " + std::to_string(args.size()));
                    }
                    if (const auto* d = std::get_if<double>(&args[0].data)) {
                      return Value(*d);
                    }
                    if (const auto* s = std::get_if<std::string>(&args[0].data)) {
                      try {
                        return Value(std::stod(*s));
                      } catch (const std::exception&) {
                        throw std::runtime_error("num(): could not convert string '" + *s + "' to a number");
                      }
                    }
                    if (const auto* b = std::get_if<bool>(&args[0].data)) {
                      return Value(*b ? 1.0 : 0.0);
                    }
                    throw std::runtime_error("num() cannot convert a value of type " + args[0].typeName());
                  },
                }));

    env->define("push",
                Value(NativeFunction{
                  .name = "push",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() != 2) {
                      throw std::runtime_error("push() expects exactly 2 arguments (array, value), got " + std::to_string(args.size()));
                    }
                    const auto* arr = std::get_if<Array>(&args[0].data);
                    if (!arr || !*arr) {
                      throw std::runtime_error("push() expects an array as its first argument, got " + args[0].typeName());
                    }
                    (*arr)->push_back(args[1]);
                    return args[0];
                  },
                }));

    env->define("pop",
                Value(NativeFunction{
                  .name = "pop",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() != 1) {
                      throw std::runtime_error("pop() expects exactly 1 argument, got " + std::to_string(args.size()));
                    }
                    const auto* arr = std::get_if<Array>(&args[0].data);
                    if (!arr || !*arr || (*arr)->empty()) {
                      throw std::runtime_error("pop() expects a non-empty array");
                    }
                    Value back = (*arr)->back();
                    (*arr)->pop_back();
                    return back;
                  },
                }));

    env->define("read",
                Value(NativeFunction{
                  .name = "read",
                  .fn = [](std::vector<Value>& args) -> Value {
                    if (args.size() > 1) {
                      throw std::runtime_error("read() expects at most 1 argument, got " + std::to_string(args.size()));
                    }
                    if (args.size() == 1) {
                      fmt::print("{}", args[0].toString());
                    }
                    std::string input;
                    std::getline(std::cin, input);
                    return Value{input};
                  },
                }));
  }

  Evaluator::Evaluator()
    : m_global(std::make_shared<Environment>()) {
    registerBuiltins(m_global);
  }

  Value Evaluator::run(const par::Block& program) { return evalBlock(&program, m_global); }

  // Central dispatch. Every parseX function in the parser has a matching
  // evalX function here, same shape as Parser::printNode, switch on
  // node->type and hand off to the right handler.
  Value Evaluator::evaluate(const par::Node* node, const std::shared_ptr<Environment>& env) {
    if (node == nullptr) {
      return Value{};
    }

    // NOLINTBEGIN (cppcoreguidelines-pro-type-static-cast-downcast)
    switch (node->type) {
      case par::NodeType::LITERAL: return evalLiteral(static_cast<const par::Literal*>(node));
      case par::NodeType::IDENTIFIER: return evalIdentifier(static_cast<const par::Identifier*>(node), env);
      case par::NodeType::UNARY: return evalUnary(static_cast<const par::Unary*>(node), env);
      case par::NodeType::BINARY: return evalBinary(static_cast<const par::Binary*>(node), env);
      case par::NodeType::BLOCK: return evalBlock(static_cast<const par::Block*>(node), env);
      case par::NodeType::IF: return evalIf(static_cast<const par::If*>(node), env);
      case par::NodeType::WHILE: return evalWhile(static_cast<const par::While*>(node), env);
      case par::NodeType::FOR: return evalFor(static_cast<const par::For*>(node), env);
      case par::NodeType::SWITCH: return evalSwitch(static_cast<const par::Switch*>(node), env);
      case par::NodeType::TERNARY: return evalTernary(static_cast<const par::Ternary*>(node), env);
      case par::NodeType::VAR_DECL: return evalVarDecl(static_cast<const par::VarDecl*>(node), env);
      case par::NodeType::EXPR_STMT: return evalExprStmt(static_cast<const par::ExprStmt*>(node), env);
      case par::NodeType::CALL: return evalCall(static_cast<const par::Call*>(node), env);
      case par::NodeType::FN_LITERAL: return evalFnLiteral(static_cast<const par::FnLiteral*>(node), env);
      case par::NodeType::MEMBER_ACCESS: return evalMemberAccess(static_cast<const par::MemberAccess*>(node), env);
      case par::NodeType::ARRAY_LITERAL: return evalArrayLiteral(static_cast<const par::ArrayLiteral*>(node), env);
      case par::NodeType::SUBSCRIPT: return evalSubscript(static_cast<const par::Subscript*>(node), env);
      case par::NodeType::RETURN: return evalReturn(static_cast<const par::Return*>(node), env);
      case par::NodeType::JUMP: return evalJump(static_cast<const par::Jump*>(node), env);
      case par::NodeType::CLASS_DECL: return evalClassDecl(static_cast<const par::ClassDecl*>(node), env);
      case par::NodeType::NEW_EXPR: return evalNewExpr(static_cast<const par::NewExpr*>(node), env);
      case par::NodeType::SELF:
        // NOLINTEND
        // Self never appears as its own statement/expression, the parser
        // only ever produces it as the object child of a MemberAccess
        // (this->field, super->field), handled by evalMemberAccess. Hitting
        // this case means a bare `this`/`super` reached evaluate() directly,
        // which the grammar doesn't allow.
        throw std::runtime_error("Evaluator: 'this'/'super' used outside of a member access");
    }

    throw std::runtime_error("Evaluator: unhandled NodeType");
  }

  // Creates a new child scope, runs each statement in order, returns the
  // value of the LAST statement. This is what gives blocks (and therefore
  // function bodies) implicit return semantics, "fn(x) { x + 1; }" evaluates
  // to x + 1 without needing an explicit return statement. Return/break/
  // continue signals thrown by a statement simply propagate out of this
  // function unmodified, they're caught further up by callFunction/evalWhile.
  Value Evaluator::evalBlock(const par::Block* node, const std::shared_ptr<Environment>& env) {
    auto scope = std::make_shared<Environment>(env);
    Value result;
    for (const auto& stmt : node->statements) {
      result = evaluate(stmt.get(), scope);
    }
    return result;
  }

  Value Evaluator::evalLiteral(const par::Literal* node) {
    return std::visit(
      [](const auto& v) -> Value {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return Value{};
        } else {
          return Value(v);
        }
      },
      node->value);
  }

  Value Evaluator::evalIdentifier(const par::Identifier* node, const std::shared_ptr<Environment>& env) {
    auto value = env->get(node->name);
    if (!value) {
      throw std::runtime_error("Undefined variable: " + node->name);
    }
    return *value;
  }

  Value Evaluator::evalUnary(const par::Unary* node, const std::shared_ptr<Environment>& env) {
    Value operand = evaluate(node->operand.get(), env);

    switch (node->operation) {
      case lex::TokenType::NOT: return Value(!operand.isTruthy());
      case lex::TokenType::MINUS:
        if (const auto* d = std::get_if<double>(&operand.data)) {
          return Value(-(*d));
        }
        throw std::runtime_error("Unary '-' requires a number, got " + operand.typeName());
      default: throw std::runtime_error("Unsupported unary operator");
    }
  }

  Value Evaluator::evalBinary(const par::Binary* node, const std::shared_ptr<Environment>& env) {
    if (isAssignmentOperator(node->operation)) {
      return evalAssignment(node, env);
    }

    // AND/OR short circuit: the right side is only evaluated if needed, same
    // as every C-family language. Has to happen before evaluating left/right
    // unconditionally below.
    if (node->operation == lex::TokenType::AND) {
      Value left = evaluate(node->left.get(), env);
      if (!left.isTruthy()) return left;
      return evaluate(node->right.get(), env);
    }
    if (node->operation == lex::TokenType::OR) {
      Value left = evaluate(node->left.get(), env);
      if (left.isTruthy()) return left;
      return evaluate(node->right.get(), env);
    }

    Value left = evaluate(node->left.get(), env);
    Value right = evaluate(node->right.get(), env);

    if (node->operation == lex::TokenType::EQUALS) return Value(valuesEqual(left, right));
    if (node->operation == lex::TokenType::NOT_EQUALS) return Value(!valuesEqual(left, right));

    // PLUS is overloaded for string concatenation in addition to numeric add.
    if (node->operation == lex::TokenType::PLUS) {
      if (std::holds_alternative<std::string>(left.data) || std::holds_alternative<std::string>(right.data)) {
        return Value(left.toString() + right.toString());
      }
    }

    const auto* l = std::get_if<double>(&left.data);
    const auto* r = std::get_if<double>(&right.data);
    if (!l || !r) {
      throw std::runtime_error("Operator requires two numbers, got " + left.typeName() + " and " + right.typeName());
    }

    switch (node->operation) {
      case lex::TokenType::PLUS: return Value(*l + *r);
      case lex::TokenType::MINUS: return Value(*l - *r);
      case lex::TokenType::STAR: return Value(*l * *r);
      case lex::TokenType::SLASH:
        if (*r == 0.0) throw std::runtime_error("Division by zero");
        return Value(*l / *r);
      case lex::TokenType::PERCENT:
        if (*r == 0.0) throw std::runtime_error("Modulo by zero");
        return Value(std::fmod(*l, *r));
      case lex::TokenType::LESS_THAN: return Value(*l < *r);
      case lex::TokenType::LESS_THAN_EQUALS: return Value(*l <= *r);
      case lex::TokenType::GREATER_THAN: return Value(*l > *r);
      case lex::TokenType::GREATER_THAN_EQUALS: return Value(*l >= *r);
      default: throw std::runtime_error("Unsupported binary operator");
    }
  }

  // Applies a (possibly compound) assignment operator to `current` and
  // `rhs`, both already evaluated. Shared by the plain-identifier path and
  // the MemberAccess/SuperAccess paths in evalAssignment below, so
  // "x += 1", "obj.field += 1", and "this->field += 1" all go through the
  // same arithmetic instead of three copies of the same switch.
  // PLUS is overloaded for string concatenation matching evalBinary's behaviour,
  // so `x += "hello"` works the same as `x = x + "hello"`.
  static Value applyCompoundOp(lex::TokenType op, const Value& current, const Value& rhs) {
    const lex::TokenType binary_op = compoundToBinaryOp(op);

    if (binary_op == lex::TokenType::PLUS) {
      if (std::holds_alternative<std::string>(current.data) || std::holds_alternative<std::string>(rhs.data)) {
        return {current.toString() + rhs.toString()};
      }
    }

    const auto* l = std::get_if<double>(&current.data);
    const auto* r = std::get_if<double>(&rhs.data);
    if ((l == nullptr) || (r == nullptr)) {
      throw std::runtime_error("Compound assignment requires two numbers");
    }
    switch (binary_op) {
      case lex::TokenType::PLUS: return {*l + *r};
      case lex::TokenType::MINUS: return {*l - *r};
      case lex::TokenType::STAR: return {*l * *r};
      case lex::TokenType::SLASH:
        if (*r == 0.0) throw std::runtime_error("Division by zero");
        return {*l / *r};
      case lex::TokenType::PERCENT:
        if (*r == 0.0) throw std::runtime_error("Modulo by zero");
        return {std::fmod(*l, *r)};
      default: throw std::runtime_error("Unsupported compound assignment operator");
    }
  }

  // Handles =, +=, -=, *=, /=, %=. Two kinds of targets:
  //   - Identifier:   plain variable, resolved/updated through `env`.
  //   - MemberAccess: instance.field = ... via '.' syntax, OR this->field =
  //                   ... (super->field = ... is rejected, same as in most
  //                   languages, you can write to your own fields but
  //                   "assigning into the parent" doesn't mean anything
  //                   since fields aren't per-class, they're per-instance,
  //                   this->field and super->field refer to the SAME
  //                   storage slot, only method lookup differs). Both forms
  //                   mutate the instance's own field map directly, they're
  //                   told apart by whether the MemberAccess's object child
  //                   is a Self node.
  Value Evaluator::evalAssignment(const par::Binary* node, const std::shared_ptr<Environment>& env) {
    // Plain identifier target, e.g. x = 5; x += 1;
    if (node->left->type == par::NodeType::IDENTIFIER) {
      const auto* target = static_cast<const par::Identifier*>(node->left.get());
      Value new_value;
      if (node->operation == lex::TokenType::ASSIGN) {
        new_value = evaluate(node->right.get(), env);
      } else {
        auto current = env->get(target->name);
        if (!current) {
          throw std::runtime_error("Undefined variable: " + target->name);
        }
        Value rhs = evaluate(node->right.get(), env);
        new_value = applyCompoundOp(node->operation, *current, rhs);
      }
      if (!env->assign(target->name, new_value)) {
        throw std::runtime_error("Undefined variable: " + target->name);
      }
      return new_value;
    }

    // instance.field = ... / this->field = ... (both MemberAccess targets)
    if (node->left->type == par::NodeType::MEMBER_ACCESS) {
      const auto* member = static_cast<const par::MemberAccess*>(node->left.get());

      Value object;
      if (member->object->type == par::NodeType::SELF) {
        const auto* self_node = static_cast<const par::Self*>(member->object.get());
        if (self_node->is_super) {
          throw std::runtime_error("Cannot assign through 'super->...', fields belong to the instance, assign via 'this->" + member->field + "' instead");
        }
        auto this_val = env->get("this");
        if (!this_val) {
          throw std::runtime_error("'this' is not defined here (assignment outside of a method body)");
        }
        object = *this_val;
      } else {
        object = evaluate(member->object.get(), env);
      }

      const auto* inst_ptr = std::get_if<std::shared_ptr<Instance>>(&object.data);
      if (inst_ptr == nullptr) {
        throw std::runtime_error("Cannot assign to a field on a non-instance value (" + object.typeName() + ")");
      }

      Value new_value;
      if (node->operation == lex::TokenType::ASSIGN) {
        new_value = evaluate(node->right.get(), env);
      } else {
        auto it = inst_ptr->get()->fields->find(member->field);
        if (it == inst_ptr->get()->fields->end()) {
          throw std::runtime_error("Undefined field '" + member->field + "' on instance of " + inst_ptr->get()->klass->name);
        }
        Value rhs = evaluate(node->right.get(), env);
        new_value = applyCompoundOp(node->operation, it->second, rhs);
      }

      (*inst_ptr->get()->fields)[member->field] = new_value;
      return new_value;
    }

    throw std::runtime_error("Invalid assignment target");
  }

  Value Evaluator::evalIf(const par::If* node, const std::shared_ptr<Environment>& env) {
    Value condition = evaluate(node->condition.get(), env);
    if (condition.isTruthy()) {
      return evaluate(node->then_branch.get(), env);
    }
    if (node->else_branch) {
      return evaluate(node->else_branch.get(), env);
    }
    return Value{};
  }

  // BreakSignal/ContinueSignal are caught right here, that's the loop
  // boundary they're scoped to. ReturnSignal is deliberately NOT caught
  // here, it has to keep propagating up past any enclosing loop until it
  // reaches the function call boundary in callFunction, "return" inside a
  // while loop should exit the whole function, not just the loop.
  Value Evaluator::evalWhile(const par::While* node, const std::shared_ptr<Environment>& env) {
    Value result;
    while (evaluate(node->condition.get(), env).isTruthy()) {
      try {
        result = evaluate(node->body.get(), env);
      } catch (const BreakSignal&) {
        break;
      } catch (const ContinueSignal&) {
        continue;
      }
    }
    return result;
  }

  // Own scope so a `pin i = 0;` initializer doesn't leak past the loop, same
  // reasoning evalBlock uses for ordinary blocks. continue still has to run
  // the increment before looping back, that's what the catch does, run
  // increment, then fall through to re-check the condition.
  Value Evaluator::evalFor(const par::For* node, const std::shared_ptr<Environment>& env) {
    auto scope = std::make_shared<Environment>(env);
    if (node->init) {
      evaluate(node->init.get(), scope);
    }

    Value result;
    while (!node->condition || evaluate(node->condition.get(), scope).isTruthy()) {
      try {
        result = evaluate(node->body.get(), scope);
      } catch (const BreakSignal&) {
        break;
      } catch (const ContinueSignal&) {
        // fall through to increment below, same as a normal iteration
      }
      if (node->increment) {
        evaluate(node->increment.get(), scope);
      }
    }
    return result;
  }

  // Fall-through switch: once a matching case is found (or the default case
  // is reached with no earlier match), every statement from there to the end
  // of the switch runs, across case boundaries, until a BreakSignal escapes
  // (an explicit `break;`) or the cases run out. valuesEqual is the same
  // equality used by `==` elsewhere, so case matching has identical semantics
  // to writing `if (subject == caseExpr)` by hand.
  Value Evaluator::evalSwitch(const par::Switch* node, const std::shared_ptr<Environment>& env) {
    Value subject = evaluate(node->subject.get(), env);
    auto scope = std::make_shared<Environment>(env);

    size_t start = node->cases.size(); // sentinel: no match found yet
    size_t default_index = node->cases.size();
    for (size_t i = 0; i < node->cases.size(); ++i) {
      const auto& c = node->cases[i];
      if (!c.value) {
        default_index = i;
        continue;
      }
      Value case_value = evaluate(c.value.get(), scope);
      if (valuesEqual(subject, case_value)) {
        start = i;
        break;
      }
    }
    if (start == node->cases.size()) {
      start = default_index; // no case matched, fall back to default if present
    }

    Value result;
    try {
      for (size_t i = start; i < node->cases.size(); ++i) {
        for (const auto& stmt : node->cases[i].body) {
          result = evaluate(stmt.get(), scope);
        }
      }
    } catch (const BreakSignal&) {
      // explicit break inside a case, stop running further cases
    }
    return result;
  }

  Value Evaluator::evalTernary(const par::Ternary* node, const std::shared_ptr<Environment>& env) {
    Value condition = evaluate(node->condition.get(), env);
    return condition.isTruthy() ? evaluate(node->then_expr.get(), env) : evaluate(node->else_expr.get(), env);
  }

  Value Evaluator::evalVarDecl(const par::VarDecl* node, const std::shared_ptr<Environment>& env) {
    Value value = node->initializer ? evaluate(node->initializer.get(), env) : Value{};
    env->define(node->name, value);
    return value;
  }

  Value Evaluator::evalExprStmt(const par::ExprStmt* node, const std::shared_ptr<Environment>& env) { return evaluate(node->expr.get(), env); }

  // Dispatches a call to whichever kind of callable the callee evaluated to:
  // a user-defined Function (goes through callFunction), a NativeFunction
  // (built-ins like print/len, just invokes the wrapped std::function
  // directly), or a bound method (also a plain Function under the hood by
  // this point, see resolveMember, which is what evaluating a MemberAccess/
  // SuperAccess callee in method-call position actually returns).
  Value Evaluator::evalCall(const par::Call* node, const std::shared_ptr<Environment>& env) {
    Value callee = evaluate(node->callee.get(), env);

    std::vector<Value> args;
    args.reserve(node->args.size());
    for (const auto& arg : node->args) {
      args.push_back(evaluate(arg.get(), env));
    }

    if (const auto* fn = std::get_if<Function>(&callee.data)) {
      return callFunction(*fn, std::move(args), node);
    }
    if (const auto* native = std::get_if<NativeFunction>(&callee.data)) {
      return native->fn(args);
    }

    throw std::runtime_error("Attempted to call a non-function value (" + callee.typeName() + ")");
  }

  Value Evaluator::evalFnLiteral(const par::FnLiteral* node, const std::shared_ptr<Environment>& env) { return Value(Function{.declaration = node, .closure = env}); }

  Value Evaluator::evalArrayLiteral(const par::ArrayLiteral* node, const std::shared_ptr<Environment>& env) {
    auto elements = std::make_shared<std::vector<Value>>();
    elements->reserve(node->elements.size());
    for (const auto& elem : node->elements) {
      elements->push_back(evaluate(elem.get(), env));
    }
    return Value(elements);
  }

  // obj[index]. Arrays index by number (truncated to size_t, bounds
  // checked), strings index by number too and return a one-character
  // string. Nothing else supports subscripting yet.
  Value Evaluator::evalSubscript(const par::Subscript* node, const std::shared_ptr<Environment>& env) {
    Value object = evaluate(node->object.get(), env);
    Value index = evaluate(node->index.get(), env);

    const auto* idx = std::get_if<double>(&index.data);
    if (!idx) {
      throw std::runtime_error("Subscript index must be a number, got " + index.typeName());
    }
    if (*idx < 0) {
      throw std::runtime_error("Subscript index cannot be negative");
    }
    auto i = static_cast<size_t>(*idx);

    if (const auto* arr = std::get_if<Array>(&object.data)) {
      if (!*arr || i >= (*arr)->size()) {
        throw std::runtime_error("Array index " + std::to_string(i) + " out of bounds (size " + std::to_string(*arr ? (*arr)->size() : 0) + ")");
      }
      return (**arr)[i];
    }
    if (const auto* str = std::get_if<std::string>(&object.data)) {
      if (i >= str->size()) {
        throw std::runtime_error("String index " + std::to_string(i) + " out of bounds (length " + std::to_string(str->size()) + ")");
      }
      return Value(std::string(1, (*str)[i]));
    }

    throw std::runtime_error("Subscript is not supported on a value of type " + object.typeName());
  }

  // obj.field, and this->field / super->field once the parser's MemberAccess
  // wrapping is unwound below. A Self object child (par::Self) means this
  // node came from `this->...`/`super->...` syntax rather than plain `.`, so
  // it gets routed to evalSelfMemberAccess instead of being evaluated as an
  // ordinary expression (a bare Self has no Value of its own to evaluate to).
  Value Evaluator::evalMemberAccess(const par::MemberAccess* node, const std::shared_ptr<Environment>& env) {
    if (node->object->type == par::NodeType::SELF) {
      const auto* self_node = static_cast<const par::Self*>(node->object.get());
      return evalSelfMemberAccess(self_node, node->field, node, env);
    }
    Value object = evaluate(node->object.get(), env);
    return resolveMember(object, node->field, node);
  }

  // this->field / super->field. `this` resolves `field` starting at the
  // instance's own runtime class. `super` resolves it starting at the parent
  // of whichever class the CURRENTLY EXECUTING method was defined on (the
  // "__class__" entry callFunction stashes in the call scope), not the
  // parent of the instance's runtime class, that distinction is what makes
  // multi-level inheritance chains resolve super-> to the right generation
  // instead of looping back on themselves.
  Value Evaluator::evalSelfMemberAccess(const par::Self* self_node, const std::string& field, const par::Node* node, const std::shared_ptr<Environment>& env) {
    auto this_val = env->get("this");
    if (!this_val) {
      throw std::runtime_error("'this'/'super' used outside of a method body");
    }

    if (!self_node->is_super) {
      return resolveMember(*this_val, field, node);
    }

    auto defining_class_val = env->get("__class__");
    if (!defining_class_val) {
      throw std::runtime_error("'super' used outside of a method body");
    }
    const auto* defining_class = std::get_if<std::shared_ptr<Class>>(&defining_class_val->data);
    if (!defining_class || !*defining_class || !(*defining_class)->parent) {
      throw std::runtime_error("'super' used in a class with no parent (no 'extends')");
    }

    return resolveMember(*this_val, field, node, (*defining_class)->parent);
  }

  // Shared field/method resolution for '.' access. `search_class` lets
  // callers (super->) look up methods starting somewhere other than the
  // instance's own runtime class.
  //
  // Resolution order: fields first, then methods. A class can't have a
  // field and a method with the same name anyway (nothing stops you from
  // writing one today, but field lookup always wins since it's checked
  // first, consider that a "first one wins" rule rather than an error if
  // you ever add a name clash, no validation exists for it yet).
  Value Evaluator::resolveMember(const Value& object, const std::string& field, const par::Node* node, const std::shared_ptr<Class>& search_class) {
    if (const auto* arr = std::get_if<Array>(&object.data)) {
      if (field == "length") {
        return Value(static_cast<double>((*arr)->size()));
      }
      throw std::runtime_error("Array has no member '" + field + "'");
    }

    if (const auto* str = std::get_if<std::string>(&object.data)) {
      if (field == "length") {
        return Value(static_cast<double>(str->size()));
      }
      throw std::runtime_error("String has no member '" + field + "'");
    }

    if (const auto* instance = std::get_if<std::shared_ptr<Instance>>(&object.data)) {
      // Fields take priority over methods.
      auto field_it = instance->get()->fields->find(field);
      if (field_it != instance->get()->fields->end()) {
        return field_it->second;
      }

      std::shared_ptr<Class> lookup_class = search_class ? search_class : instance->get()->klass;
      std::shared_ptr<Class> owner;
      const Function* method = lookup_class->findMethod(field, &owner);
      if (method == nullptr) {
        throw std::runtime_error("'" + instance->get()->klass->name + "' has no field or method '" + field + "'");
      }

      // Bind `this` (and `__class__`) by creating a fresh closure scope
      // (parented to the method's original closure, NOT the call site, same
      // lexical-scoping rule as ordinary closures) with both predefined.
      // evalCall sees an ordinary Function value and runs it through
      // callFunction exactly like any other call, callFunction doesn't need
      // to know "this" or "__class__" came from a method bind, define()
      // already put them in scope here, that's the whole mechanism: stashing
      // them here (rather than only passing them as callFunction's optional
      // bound_this/defining_class params, which is what evalNewExpr does for
      // the constructor call specifically) means a bound method can ALSO be
      // stored in a variable and called later (`pin m = obj.method; m();`)
      // and still know what `this`/`__class__` are, since by then the Call
      // node has no idea it ever came from a MemberAccess. `__class__` is
      // what makes super-> inside an ordinary method (not just inside a
      // constructor) resolve to the right ancestor, see evalSuperAccess.
      auto bound_scope = std::make_shared<Environment>(method->closure);
      bound_scope->define("this", object);
      if (owner) {
        bound_scope->define("__class__", Value(owner));
      }
      return Value(Function{.declaration = method->declaration, .closure = bound_scope});
    }

    throw std::runtime_error("Member access on '" + field + "' is not supported on a value of type " + object.typeName());
  }

  Value Evaluator::evalReturn(const par::Return* node, const std::shared_ptr<Environment>& env) {
    Value value = node->value ? evaluate(node->value.get(), env) : Value{};
    throw ReturnSignal{std::move(value)};
  }

  // break; / continue; one Jump node, kind tells them apart.
  Value Evaluator::evalJump(const par::Jump* node, const std::shared_ptr<Environment>& /*env*/) {
    switch (node->kind) {
      case par::JumpKind::BREAK: throw BreakSignal{};
      case par::JumpKind::CONTINUE: throw ContinueSignal{};
    }
    throw std::runtime_error("Evaluator: unhandled JumpKind");
  }

  // class Name [extends Parent] { ... }
  //
  // Building the runtime Class is a two-step process:
  //   1. Resolve the parent (if any) by name, must already exist as a Class
  //      Value, error if `extends` names something that isn't a class.
  //   2. Bind every method as a Function whose closure is `env` (the scope
  //      the class itself was declared in, this is what lets methods see
  //      top-level helper functions/classes defined alongside the class,
  //      same as how evalFnLiteral captures its surrounding scope).
  //
  // Field DEFAULTS aren't evaluated here, only their AST stays attached to
  // the ClassDecl node (m_classes stores `declaration`, see evalNewExpr),
  // they're evaluated fresh per-instance at construction time since each
  // instance needs its own independent value (e.g. `pin list = [];` as a
  // field default must not have every instance share the same array).
  Value Evaluator::evalClassDecl(const par::ClassDecl* node, const std::shared_ptr<Environment>& env) {
    auto klass = std::make_shared<Class>();
    klass->name = node->name;
    klass->declaration = node;

    if (!node->parent_name.empty()) {
      auto parent_it = m_classes.find(node->parent_name);
      if (parent_it == m_classes.end()) {
        throw std::runtime_error("Class '" + node->name + "' extends unknown class '" + node->parent_name + "'");
      }
      klass->parent = parent_it->second;
    }

    for (size_t i = 0; i < node->methods.size(); ++i) {
      klass->methods[node->method_names[i]] = Function{.declaration = node->methods[i].get(), .closure = env};
    }

    m_classes[node->name] = klass;
    env->define(node->name, Value(klass));
    return Value(klass);
  }

  // new ClassName(args)
  //
  // Field initialization walks the inheritance chain ROOT-FIRST (grandparent,
  // then parent, then this class), applying each class's own field defaults
  // in declaration order as it goes. Each default is evaluated in a fresh
  // child scope with `this` bound to the partially-constructed instance, so
  // defaults can reference sibling fields initialized earlier in the chain.
  // Child field defaults overwrite any same-named field set by an ancestor.
  //
  // After fields are populated, the constructor is looked up via the normal
  // method-resolution chain (klass->findMethod, walks up through parents
  // same as any other method call) and invoked with `this` bound to the new
  // instance and `defining_class` set to whichever class actually DEFINES
  // the constructor that ran, so a super->constructor() call inside it
  // resolves starting from THAT class's parent. If no constructor exists
  // anywhere in the chain, construction succeeds with only field defaults applied.
  Value Evaluator::evalNewExpr(const par::NewExpr* node, const std::shared_ptr<Environment>& env) {
    auto class_it = m_classes.find(node->class_name);
    if (class_it == m_classes.end()) {
      throw std::runtime_error("Unknown class '" + node->class_name + "'");
    }
    std::shared_ptr<Class> klass = class_it->second;

    auto fields = std::make_shared<std::unordered_map<std::string, Value>>();
    auto instance = std::make_shared<Instance>(Instance{.klass = klass, .fields = fields});

    std::vector<Class*> chain;
    for (Class* c = klass.get(); c != nullptr; c = c->parent.get()) {
      chain.push_back(c);
    }
    std::ranges::reverse(chain);

    Value instance_value(instance);

    for (Class* c : chain) {
      for (const auto& field_decl : c->declaration->fields) {
        auto field_init_env = std::make_shared<Environment>(env);
        field_init_env->define("this", instance_value);
        Value default_value = field_decl->initializer ? evaluate(field_decl->initializer.get(), field_init_env) : Value{};
        (*fields)[field_decl->name] = std::move(default_value);
      }
    }

    std::shared_ptr<Class> owner;
    const Function* ctor = klass->findMethod("constructor", &owner);
    if (ctor != nullptr) {
      std::vector<Value> args;
      args.reserve(node->args.size());
      for (const auto& arg : node->args) {
        args.push_back(evaluate(arg.get(), env));
      }

      callFunction(*ctor, std::move(args), node, instance_value, owner);
    } else if (!node->args.empty()) {
      throw std::runtime_error("Class '" + node->class_name + "' has no constructor but was called with arguments");
    }

    return instance_value;
  }

  // Binds args to params in a fresh scope whose parent is the closure (NOT
  // the call site), then evaluates the body. Catches ReturnSignal so a
  // return statement unwinds cleanly back here, that's the function-call
  // boundary return is scoped to: anything thrown by evalReturn deeper
  // inside (through any number of nested blocks/ifs/whiles) bubbles up
  // through all of them untouched until it lands in this catch.
  //
  // `bound_this`/`defining_class` are set for method calls (see
  // resolveMember and evalNewExpr): "this" and "__class__" get defined in
  // the fresh call scope alongside the regular parameters, using the exact
  // same Environment mechanism, no special-casing needed elsewhere.
  Value Evaluator::callFunction(
    const Function& fn, std::vector<Value> args, const par::Node* /*call_node*/, const std::optional<Value>& bound_this, const std::shared_ptr<Class>& defining_class) {
    if (args.size() != fn.declaration->params.size()) {
      throw std::runtime_error("Expected " + std::to_string(fn.declaration->params.size()) + " arguments but got " + std::to_string(args.size()));
    }

    auto call_env = std::make_shared<Environment>(fn.closure);
    if (bound_this) {
      call_env->define("this", *bound_this);
    }
    if (defining_class) {
      call_env->define("__class__", Value(defining_class));
    }
    for (size_t i = 0; i < args.size(); ++i) {
      call_env->define(fn.declaration->params[i], std::move(args[i]));
    }

    try {
      return evaluate(fn.declaration->body.get(), call_env);
    } catch (const ReturnSignal& ret) {
      return ret.value;
    }
  }
} // namespace eval
