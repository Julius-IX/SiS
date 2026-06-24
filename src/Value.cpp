#include <Value.h>
#include <algorithm>
#include <spdlog/fmt/fmt.h>

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
          return v && !v->elements.empty();
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
            bool no_table_entries = std::all_of(v->elements.begin(), v->elements.end(), [](const auto& p) {
              return std::holds_alternative<double>(p.first.data);
            });
            if (no_table_entries) {
              bool first = true;
              for (const auto& [key, value] : v->elements) {
                if (!first) out += ", ";
                first = false;
                out += value.toString();
              }
            } else {
              bool first = true;
              for (const auto& [key, value] : v->elements) {
                if (!first) out += ", ";
                first = false;
                out += key.toString();
                out += ": ";
                out += value.toString();
              }
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

} // namespace eval
