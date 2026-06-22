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
        } else { // Function, NativeFunction, shared_ptr<Class>, shared_ptr<Instance>
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
          // Use the shortest round-trip representation, then strip trailing
          // zeros after the decimal point. If nothing remains after the dot,
          // strip that too — so 123.0 becomes "123", 1.50 becomes "1.5".
          std::string s = fmt::format("{}", v);
          if (s.find('.') != std::string::npos) {
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.') s.pop_back();
          }
          return s;
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
        } else { // shared_ptr<Instance>
          return v ? ("<" + (v->klass ? v->klass->name : std::string("instance")) + " instance>") : "<instance>";
        }
      },
      data);
  }

  std::string Value::typeName() const {
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
} // namespace eval
