#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
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

  struct InternalArray;
  using Array = std::shared_ptr<InternalArray>;

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

  inline bool operator==(const Value& lhs, const Value& rhs) {
    if (lhs.data.index() != rhs.data.index()) return false;

    return std::visit(
      [](const auto& a, const auto& b) -> bool {
        using A = std::decay_t<decltype(a)>;
        using B = std::decay_t<decltype(b)>;

        if constexpr (!std::is_same_v<A, B>) {
          return false;
        } else if constexpr (std::is_same_v<A, std::monostate>) {
          return true;
        } else if constexpr (std::is_same_v<A, double> || std::is_same_v<A, bool> || std::is_same_v<A, std::string>) {
          return a == b;
        } else if constexpr (std::is_same_v<A, Array> || std::is_same_v<A, std::shared_ptr<Class>> || std::is_same_v<A, std::shared_ptr<Instance>>) {
          return a.get() == b.get();
        } else if constexpr (std::is_same_v<A, Function>) {
          return a.declaration == b.declaration && a.closure.get() == b.closure.get();
        } else if constexpr (std::is_same_v<A, NativeFunction>) {
          return a.name == b.name;
        }
      },
      lhs.data,
      rhs.data);
  }

  struct ValueHash {
    std::size_t operator()(const Value& value) const {
      return std::visit(
        [](const auto& v) -> std::size_t {
          using T = std::decay_t<decltype(v)>;

          if constexpr (std::is_same_v<T, std::monostate>) {
            return 0;
          } else if constexpr (std::is_same_v<T, double>) {
            return std::hash<double>{}(v);
          } else if constexpr (std::is_same_v<T, bool>) {
            return std::hash<bool>{}(v);
          } else if constexpr (std::is_same_v<T, std::string>) {
            return std::hash<std::string>{}(v);
          } else if constexpr (std::is_same_v<T, Array> || std::is_same_v<T, std::shared_ptr<Class>> || std::is_same_v<T, std::shared_ptr<Instance>>) {
            return std::hash<const void*>{}(v.get());
          } else if constexpr (std::is_same_v<T, Function>) {
            std::size_t h1 = std::hash<const void*>{}(v.declaration);
            std::size_t h2 = std::hash<const void*>{}(v.closure.get());

            return h1 ^ (h2 << 1);
          } else if constexpr (std::is_same_v<T, NativeFunction>) {
            return std::hash<std::string>{}(v.name);
          }
        },
        value.data);
    }
  };

  // Ordered key-value store backing the Array type. Using a vector of pairs
  // rather than a hash map so insertion order is preserved — arr[0], arr[1]
  // etc. iterate in the order they were inserted, which is what users expect.
  // Lookup is O(n) linear scan, which is fine for typical scripting array
  // sizes. Keys can be any Value (number, string, bool, etc), so [1, 2, 3]
  // auto-assigns keys 0.0, 1.0, 2.0 while ["a": 1, "b": 2] uses string keys.
  struct InternalArray {
    std::vector<std::pair<Value, Value>> elements;

    InternalArray() = default;

    // Construct from a flat list of values, auto-assigning sequential numeric
    // keys starting at 0. This is what evalArrayLiteral uses for positional
    // elements (the ones without an explicit key).
    explicit InternalArray(std::vector<Value> values) {
      elements.reserve(values.size());
      for (size_t i = 0; i < values.size(); ++i) {
        elements.emplace_back(Value(static_cast<double>(i)), std::move(values[i]));
      }
    }

    [[nodiscard]] size_t size() const { return elements.size(); }

    // Linear scan for a key. Returns a pointer into the pair so the caller
    // can read or mutate the value in place. Returns nullptr if not found.
    [[nodiscard]] Value* get(const Value& key) {
      for (auto& [k, v] : elements) {
        if (k == key) return &v;
      }
      return nullptr;
    }

    [[nodiscard]] const Value* get(const Value& key) const {
      for (const auto& [k, v] : elements) {
        if (k == key) return &v;
      }
      return nullptr;
    }

    // Insert or overwrite. If the key already exists, updates the value in
    // place (preserving its position). If not, appends a new pair.
    void set(const Value& key, Value value) {
      for (auto& [k, v] : elements) {
        if (k == key) {
          v = std::move(value);
          return;
        }
      }
      elements.emplace_back(key, std::move(value));
    }

    void emplaceBack(Value value) {
      // Find the smallest non-negative integer key not already present.
      // e.g. if keys {0, 2} exist, next auto key is 1; if {0,1,2} exist, it's 3.
      double next = 0.0;
      bool found = true;
      while (found) {
        found = false;
        for (const auto& [k, v] : elements) {
          if (const auto* d = std::get_if<double>(&k.data); (d != nullptr) && *d == next) {
            found = true;
            next += 1.0;
            break;
          }
        }
      }
      elements.emplace_back(Value(next), std::move(value));
    }
  };
} // namespace eval
