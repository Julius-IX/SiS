#pragma once

#include <Value.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace eval {
  // A scope: maps variable names to values, with an optional parent scope to
  // fall back to when a name isn't found locally. That fallback chain is what
  // gives lexical scoping, a block or function body gets its own
  // Environment whose parent is the scope it was created inside.
  //
  // Held via shared_ptr because closures need to keep their defining
  // environment alive even after that scope has otherwise "ended" (e.g. a
  // function returned from another function still needs its captured
  // variables to exist).
  //
  // `this` binding for methods reuses this exact mechanism: callFunction
  // just defines a variable named "this" in the fresh call scope before
  // running the method body, same as any other parameter. No separate
  // machinery needed, lookup/shadowing/closures all fall out for free.
  class Environment : public std::enable_shared_from_this<Environment> {
    public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr)
      : m_parent(std::move(parent)) {}

    [[nodiscard]] std::unordered_map<std::string, Value> snapshot() const { return m_values; }

    void define(const std::string& name, Value value) { m_values[name] = std::move(value); }

    bool assign(const std::string& name, Value value) {
      if (m_values.contains(name)) {
        m_values[name] = std::move(value);
        return true;
      }
      if (m_parent) {
        return m_parent->assign(name, std::move(value));
      }
      return false;
    }

    // Looks up a variable, searching up the parent chain. Returns nullopt if
    // it's not defined anywhere, callers should treat that as an "undefined
    // variable" error.
    [[nodiscard]] std::optional<Value> get(const std::string& name) const {
      auto it = m_values.find(name);
      if (it != m_values.end()) {
        return it->second;
      }
      if (m_parent) {
        return m_parent->get(name);
      }
      return std::nullopt;
    }

    private:
    std::unordered_map<std::string, Value> m_values;
    std::shared_ptr<Environment> m_parent;
  };
} // namespace eval
