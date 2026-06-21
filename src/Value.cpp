#include <Value.h>

namespace eval {
  bool Value::isTruthy() const {
    return std::visit(
      [](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return false;
        } else if constexpr (std::is_same_v<T, bool>) {
          return v;
        } else if constexpr (std::is_same_v<T, double>) {
          return v != 0.0;
        } else if constexpr (std::is_same_v<T, std::string>) {
          return !v.empty();
        } else if constexpr (std::is_same_v<T, Array>) {
          return v && !v->empty();
        } else { // Function, NativeFunction, shared_ptr<Class>, Instance
          return true;
        }
      },
      data);
  }

  std::string Value::toString() const {
    return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return v;
        } else if constexpr (std::is_same_v<T, Array>) {
          std::string out = "[";
          if (v) {
            for (size_t i = 0; i < v->size(); ++i) {
              out += (*v)[i].toString();
              if (i + 1 < v->size()) out += ", ";
            }
          }
          out += "]";
          return out;
        } else if constexpr (std::is_same_v<T, Function>) {
          return "<function>";
        } else if constexpr (std::is_same_v<T, NativeFunction>) {
          return "<native function " + v.name + ">";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Class>>) {
          return v ? ("<class " + v->name + ">") : "<class>";
        } else { // Instance
          return "<" + (v.klass ? v.klass->name : std::string("instance")) + " instance>";
        }
      },
      data);
  }

  std::string Value::typeName() const {
    return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "null";
        else if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, double>) return "num";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, Array>) return "array";
        else if constexpr (std::is_same_v<T, Function>) return "function";
        else if constexpr (std::is_same_v<T, NativeFunction>) return "function";
        else if constexpr (std::is_same_v<T, std::shared_ptr<Class>>) return "class";
        else return "instance"; // Instance
      },
      data);
  }
} // namespace eval
