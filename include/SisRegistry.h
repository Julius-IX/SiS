#pragma once

#include <Environment.h>
#include <Value.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eval {

  // Fluent builder returned by SisRegistry::defineClass. Chain calls to
  // register the constructor and methods before the class is used.
  //
  // Convention for native method lambdas:
  //   args[0] is always the Instance (i.e. `this`)
  //   args[1..n] are the user-supplied call arguments
  //
  // The binding of `this` into args[0] is done automatically by
  // resolveMember when it wraps a native method for dispatch, so the
  // lambda never has to think about it just grab self from args[0]
  // and your actual args from args[1] onward.
  struct NativeClassBuilder {
    std::shared_ptr<Class> klass;
    std::string m_pending_docs;
  
    // Buffer a doc string to be consumed by the next constructor() or method() call.
    NativeClassBuilder& docs(std::string text) {
      m_pending_docs = std::move(text);
      return *this;
    }
  
    // Register the constructor. `ctor` receives the partially-constructed
    // instance (fields already set to defaults by evalNewExpr) and the
    // user-supplied constructor arguments. Mutate inst->fields directly
    // to initialize the instance.
    NativeClassBuilder& constructor(std::function<void(std::shared_ptr<Instance>, std::vector<Value>&)> ctor) {
      klass->native_methods["constructor"] = NativeFunction{
        .name = "constructor",
        .docs = std::move(m_pending_docs),
        .fn   = [ctor](std::vector<Value>& args) -> Value {
          auto inst = std::get<std::shared_ptr<Instance>>(args[0].data);
          std::vector<Value> ctor_args(args.begin() + 1, args.end());
          ctor(inst, ctor_args);
          return Value{};
        }
      };
      return *this;
    }
  
    NativeClassBuilder& method(const char* name, std::function<Value(std::shared_ptr<Instance>, std::vector<Value>&)> fn) {
      klass->native_methods[name] = NativeFunction{
        .name = name,
        .docs = std::move(m_pending_docs),
        .fn   = [fn](std::vector<Value>& args) -> Value {
          auto inst = std::get<std::shared_ptr<Instance>>(args[0].data);
          std::vector<Value> method_args(args.begin() + 1, args.end());
          return fn(inst, method_args);
        }
      };
      return *this;
    }
  
    // Register a default field value. evalNewExpr will populate this on
    // every new instance before the constructor runs, just like AST-declared
    // fields. Use Value{} (null) if there is no meaningful default.
    // NOTE: clears pending docs since fields don't carry them, preventing
    // a stray .docs() from bleeding into the next method.
    NativeClassBuilder& field(const char* name, Value default_value) {
      m_pending_docs = {};
      klass->default_fields[name] = std::move(default_value);
      return *this;
    }
  };

  // Thin handle given to native (.so/.dll) stdlib modules.
  // Lets them register free functions and classes without touching
  // Evaluator internals directly.
  struct SisRegistry {
    std::shared_ptr<Environment> env;
    std::unordered_map<std::string, std::shared_ptr<Class>>& classes;

    void defineVariable(const std::string& name, Value value) {
      env->define(name, std::move(value));
    }

    void defineFn(const char* name, std::function<Value(std::vector<Value>&)> fn, std::string docs_string = "") {
      env->define(name, Value(NativeFunction{
        .name = std::string(name),
        .docs = std::move(docs_string),
        .fn   = std::move(fn)
      }));
    }

    // Register a class. Returns a builder so you can chain
    // .constructor(...).method(...).field(...) calls.
    // The class is inserted into both the env (so `ClassName` resolves
    // as a Value) and the evaluator's m_classes map (so `new ClassName()`
    // resolves correctly).
    NativeClassBuilder defineClass(const char* name, std::string docs_string = "") {
      auto klass = std::make_shared<Class>();
      klass->name = name;
      klass->docs = std::move(docs_string);
      klass->declaration = nullptr; // native class, no AST node
      classes[name] = klass;
      env->define(name, Value(klass));
      return NativeClassBuilder{.klass = klass};
    }
  };

} // namespace eval
