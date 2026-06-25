#pragma once

#include <SisRegistry.h>
#include <cstdint>
#include <stdexcept>
#include <string>

// std::variant<std::monostate, double, bool, std::string, Array, Function, NativeFunction, std::shared_ptr<Class>, std::shared_ptr<Instance>> data;

#define SIS_VALUE_TYPES(X)                                                                                                                                                         \
  X(Null, std::monostate, "null")                                                                                                                                                  \
  X(Num, double, "number")                                                                                                                                                         \
  X(Bool, bool, "bool")                                                                                                                                                            \
  X(Str, std::string, "string")                                                                                                                                                    \
  X(Arr, eval::Array, "array")                                                                                                                                                     \
  X(Func, eval::Function, "function")                                                                                                                                              \
  X(NativeFunc, eval::NativeFunction, "native function")                                                                                                                           \
  X(Class, std::shared_ptr<eval::Class>, "class")                                                                                                                                  \
  X(Instance, std::shared_ptr<eval::Instance>, "instance")

#define DEFINE_REQUIRE(name, type, prettyName)                                                                                                                                     \
  inline type require##name(const eval::Value& val, const char* ctx) {                                                                                                             \
    const auto* p = std::get_if<type>(&val.data);                                                                                                                                  \
    if (p == nullptr) {                                                                                                                                                            \
      throw std::runtime_error(std::string(ctx) + ": expected a " prettyName ", got " + val.typeName());                                                                           \
    }                                                                                                                                                                              \
    return *p;                                                                                                                                                                     \
  }

SIS_VALUE_TYPES(DEFINE_REQUIRE)

#define FN_SIGNATURE(name, arguments) static eval::Value name(std::vector<eval::Value>& args)
#define NATIVE_METHOD(name, inst, args, content) method(name, [](std::shared_ptr<eval::Instance> (inst), std::vector<eval::Value>& (args)) -> eval::Value content)

// Usage:
//   SIS_MODULE_INIT(reg) {
//     reg->defineFn("add", fnAdd);
//     SIS_NATIVE_CLASS_BEGIN(reg, "Counter", NativeCounter)
//       ...
//     SIS_NATIVE_CLASS_END();
//   }
//
// Expands to:
//   extern "C" void sis_module_init(eval::SisRegistry* reg) { ... }

// clang-format off
#define SIS_MODULE_INIT(reg_param) \
  extern "C" void sis_module_init(eval::SisRegistry* reg_param)
// clang-format on

// SIS_NATIVE_CLASS_BEGIN(reg, "ClassName", CppType)
//   Calls reg->defineClass("ClassName")
//   Pre-declares the __native field (so you can never forget it).
//   Opens a fluent chain append .constructor(...).method(...) calls
//   directly after the macro.
//
// SIS_NATIVE_CLASS_END()
//   Closes the statement (semicolon).
//
// SIS_NATIVE_CTOR(CppType, ctor_args_vec, native_ptr_name)
//   Use inside a .constructor() lambda body.
//   Allocates a shared_ptr<CppType> named `native_ptr_name`, boxes it into
//   the __native field (address + keepalive), and stores it on `inst`.
//   After this macro, use `native_ptr_name` like a normal shared_ptr<CppType>
//   for any extra field initialisation you need.
//
// Example (rewriting demo_stdlib.cpp's Counter):
//
//   SIS_NATIVE_CLASS_BEGIN(reg, "Counter", NativeCounter)
//     .constructor([](std::shared_ptr<eval::Instance> inst,
//                     std::vector<eval::Value>& args) {
//       double initial = args.empty() ? 0.0 : requireNum(args[0], "Counter()");
//       SIS_NATIVE_CTOR(NativeCounter, args, counter, initial);
//       (void)counter; // nothing else to initialise
//     })
//     .method("increment",
//             [](std::shared_ptr<eval::Instance> inst,
//                std::vector<eval::Value>&) -> eval::Value {
//       SIS_GET_NATIVE(NativeCounter, inst)->increment();
//       return eval::Value{};
//     })
//     ...
//   SIS_NATIVE_CLASS_END();

// clang-format off
#define SIS_NATIVE_CLASS_BEGIN(reg, class_name, CppType)                               \
  (reg)->defineClass(class_name)                                                       \
    .field("__native", eval::Value{})   /* placeholder; SIS_NATIVE_CTOR fills this */  \

#define SIS_NATIVE_CLASS_END() /* just a semicolon guard */ ;
// clang-format on

// SIS_NATIVE_CTOR(CppType, inst, native_var, ...)
//
//   CppType    - the C++ class backing this SiS class
//   inst       - the shared_ptr<eval::Instance> from the constructor lambda
//   native_var - name to give the local shared_ptr<CppType> (usable after
//                the macro for extra init work)
//   ...        - forwarded verbatim to make_shared<CppType>(...) as
//                constructor arguments, so it reads naturally:
//                SIS_NATIVE_CTOR(NativeCounter, inst, ctr, initial);
//
// Internals (same trick as /stdlib/demo/demo_stdlib.cpp, just hidden):
//   - Heap-allocates shared_ptr<CppType> via a "box" so we can recover it
//     from a raw address stored as a double.
//   - Puts two elements in the __native Array:
//       [0] double  - reinterpret_cast address of the box
//       [1] NativeFunction - closure that keeps `native` alive and deletes
//           the box on destruction (the GC drops the Array → lambda fires).
//

// clang-format off
#define SIS_NATIVE_CTOR(CppType, inst, native_var, ...)                             \
  auto native_var = std::make_shared<CppType>(__VA_ARGS__);                         \
  {                                                                                 \
    auto* _sis_box = new std::shared_ptr<CppType>(native_var);                      \
    auto  _sis_handle = std::make_shared<eval::InternalArray>();                    \
    /* Element 0: raw address of the box as a double */                             \
    _sis_handle->emplaceBack(                                                       \
      static_cast<double>(reinterpret_cast<uintptr_t>(_sis_box)));                  \
    /* Element 1: keepalive captures the shared_ptr so the C++ object               \
       stays alive exactly as long as this SiS Instance does.                       \
       Also responsible for delete-ing the box. */                                  \
    _sis_handle->emplaceBack(eval::NativeFunction{                                  \
      .name = "__sis_keepalive_" #CppType,                                          \
      .fn   = [native_var, _sis_box](std::vector<eval::Value>&) -> eval::Value {    \
        (void)native_var; /* keep ref-count alive */                                \
        delete _sis_box;                                                            \
        return eval::Value{};                                                       \
      }                                                                             \
    });                                                                             \
    (*((inst)->fields))["__native"] = eval::Value(_sis_handle);                     \
  }
// clang-format on

// SIS_GET_NATIVE(CppType, inst)
//   Returns a shared_ptr<CppType> recovered from inst's __native field.
//   Throws std::runtime_error with the type name if anything looks wrong.
//
// Usage inside a .method() lambda:
//   SIS_GET_NATIVE(NativeCounter, inst)->increment();
//   double v = SIS_GET_NATIVE(NativeCounter, inst)->value();

// clang-format off
#define SIS_GET_NATIVE(CppType, inst)                                               \
  ([]( const std::shared_ptr<eval::Instance>& _inst) -> std::shared_ptr<CppType> {  \
    auto _it = _inst->fields->find("__native");                                     \
    if (_it == _inst->fields->end()) {                                              \
      throw std::runtime_error(#CppType ": missing __native field");                \
    }                                                                               \
    const auto* _arr = std::get_if<eval::Array>(&_it->second.data);                 \
    if (!_arr || !*_arr || (*_arr)->size() < 2) {                                   \
      throw std::runtime_error(#CppType ": __native field is not a valid handle");  \
    }                                                                               \
    const auto* _addr = std::get_if<double>(&(**_arr).elements[0].second.data);     \
    if (!_addr) {                                                                   \
      throw std::runtime_error(#CppType ": __native handle is corrupt");            \
    }                                                                               \
    return *reinterpret_cast<std::shared_ptr<CppType>*>(                            \
      static_cast<uintptr_t>(*_addr));                                              \
  }(inst))
// clang-format on
