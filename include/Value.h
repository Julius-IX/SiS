#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace par {
  struct FnLiteral;
  struct ClassDecl;
} // namespace par

namespace eval {
  class Environment;
  struct Value;

  // A function value at runtime: the AST node describing params/body, plus the
  // environment that was active when the function was created. That pairing
  // (code + captured environment) is what makes this a closure, the function
  // can still see variables from its defining scope even when called later
  // from somewhere else.
  struct Function {
    const par::FnLiteral* declaration; // non-owning, the AST outlives evaluation
    std::shared_ptr<Environment> closure;
  };

  // A native (C++-implemented) function exposed to user code, e.g. print(),
  // len(). Takes already-evaluated arguments and returns a Value, same
  // calling convention as a user-defined Function as far as evalCall cares,
  // they're distinguished only by which variant alternative they live in.
  struct NativeFunction {
    std::string name; // for error messages ("X expects 1 argument", etc)
    std::function<Value(std::vector<Value>&)> fn;
  };

  using Array = std::shared_ptr<std::vector<Value>>;

  // A class at runtime: its declared fields (name -> default initializer
  // AST, evaluated fresh per instance), its methods (name -> Function,
  // closure is the environment the class was declared in, NOT a particular
  // instance, `this` gets bound separately per call), and an optional
  // pointer to the parent class for single inheritance / `super`.
  //
  // Held by shared_ptr because every Instance needs to keep its class (and
  // transitively its parent chain) alive, and the class itself is a
  // first-class Value that can be passed around independent of any instance.
  struct Class : std::enable_shared_from_this<Class> {
    std::string name;
    std::shared_ptr<Class> parent;
    const par::ClassDecl* declaration; // nullptr for native classes
    std::unordered_map<std::string, Function> methods;
    std::unordered_map<std::string, NativeFunction> native_methods; // methods implemented in C++
    std::unordered_map<std::string, Value> default_fields;          // field defaults for native classes
                                                                    // (AST classes use declaration->fields)
    // Walks up the parent chain looking for a method named `name`, declared
    // directly on this class or inherited. Returns nullptr if nobody in the
    // chain defines it. If `owner_out` is non-null, it's set to the class
    // that actually DECLARES the matched method (which may be an ancestor,
    // not `this`), callers need that to correctly bind "__class__" so a
    // super-> call inside the method resolves starting from the right
    // generation rather than looping back on itself.
    [[nodiscard]] const Function* findMethod(const std::string& method_name, std::shared_ptr<Class>* owner_out = nullptr) {
      auto it = methods.find(method_name);
      if (it != methods.end()) {
        if (owner_out != nullptr) *owner_out = shared_from_this();
        return &it->second;
      }
      if (parent) return parent->findMethod(method_name, owner_out);
      return nullptr;
    }

    // Same walk as findMethod but for native methods. Used by resolveMember
    // and evalNewExpr to dispatch to C++-implemented methods/constructors.
    [[nodiscard]] const NativeFunction* findNativeMethod(const std::string& method_name, std::shared_ptr<Class>* owner_out = nullptr) {
      auto it = native_methods.find(method_name);
      if (it != native_methods.end()) {
        if (owner_out) *owner_out = shared_from_this();
        return &it->second;
      }
      if (parent) return parent->findNativeMethod(method_name, owner_out);
      return nullptr;
    }
  };

  // An instance of a Class: its own field storage (every instance gets its
  // own map, that's what makes two instances independent of each other) plus
  // a pointer back to the Class that describes its shape and methods.
  //
  // Held by shared_ptr (same as Array) so instances have reference semantics:
  // `pin b = a;` aliases the same object, mutating through `b` is visible
  // through `a`. This also avoids copying the struct on every evaluate()
  // return — only the shared_ptr is copied, not the fields map or klass ptr.
  struct Instance {
    std::shared_ptr<Class> klass;
    std::shared_ptr<std::unordered_map<std::string, Value>> fields;
  };

  // Runtime value type. Distinct from par::LiteralType (which is parse-time
  // only) because the evaluator also needs to represent things the parser
  // never produces directly, like functions, arrays, classes and instances.
  struct Value {
    std::variant<std::monostate, double, bool, std::string, Array, Function, NativeFunction, std::shared_ptr<Class>, std::shared_ptr<Instance>> data;

    Value()
      : data(std::monostate{}) {}
    Value(double d) // NOLINT
      : data(d) {}
    Value(bool b) // NOLINT
      : data(b) {}
    Value(std::string s) // NOLINT
      : data(std::move(s)) {}
    Value(Array a) // NOLINT
      : data(std::move(a)) {}
    Value(Function f) // NOLINT
      : data(std::move(f)) {}
    Value(NativeFunction f) // NOLINT
      : data(std::move(f)) {}
    Value(std::shared_ptr<Class> c) // NOLINT
      : data(std::move(c)) {}
    Value(std::shared_ptr<Instance> i) // NOLINT
      : data(std::move(i)) {}

    // Truthiness rules: null is false, bool is itself, numbers are false only
    // at exactly 0, strings/arrays are false only when empty, functions,
    // classes and instances are always true. Adjust this if your language
    // wants different semantics.
    [[nodiscard]] bool isTruthy() const;

    // Human readable form, used for debugging and string concatenation.
    [[nodiscard]] std::string toString() const;

    [[nodiscard]] std::string typeName() const {
      return std::visit(
        [](const auto& v) -> std::string {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, std::monostate>)
            return "null";
          else if constexpr (std::is_same_v<T, bool>)
            return "bool";
          else if constexpr (std::is_same_v<T, double>)
            return "num";
          else if constexpr (std::is_same_v<T, std::string>)
            return "string";
          else if constexpr (std::is_same_v<T, Array>)
            return "array";
          else if constexpr (std::is_same_v<T, Function>)
            return "function";
          else if constexpr (std::is_same_v<T, NativeFunction>)
            return "function";
          else if constexpr (std::is_same_v<T, std::shared_ptr<Class>>)
            return "class";
          else
            return "instance"; // shared_ptr<Instance>
        },
        data);
    }
  };
} // namespace eval
