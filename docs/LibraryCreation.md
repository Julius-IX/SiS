### Library Creation

This document covers API details for creating dynamic libraries for SiS in C++.
It does not explain how to make native SiS language libraries, as those are directly used as normal source code when placed in a dedicated path.
SiS language libraries are placed in `$SIS_PATH/lib/managed/` and dynamic libraries under `$SIS_PATH/lib/dynamic/`.
For SiS language based libraries refer to [LangSyntax.md](LangSyntax.md) to learn basic syntax.

---

### Starting Out

The first step is to decide if you want to use the existing set up provided in the [/stdlib/](../stdlib/) directory, or create your own.
For details on how to build either one refer to [Building.md](Building.md).

A minimal example project can be found in [/stdlib/demo/stdlib.cpp](../stdlib/demo/stdlib.cpp) or [/stdlib/demo/stdlib_macrofied.cpp](../stdlib/demo/stdlib_macrofied.cpp) (for a simplified version using the helper macros).
For a deeper dive into the workings keep reading. For clarity no convenience macros will be used from, [SisDynamicLibMacros.h](../include/SisDynamicLibMacros.h), in the examples.

##### Entry Point
First the library needs to make itself known to SiS by providing an entry point:

```c
extern "C" void sis_module_init(eval::SisRegistry* reg) {
  // This will contain all the links to the functions, classes and variables.
}
```
The function **must** match this exact signature as this is the symbol that SiS will look for. The `sis_module_init` function gives access to the `eval::SisRegistry` variable.
This is the main handle for registering all exposed functions, classes and variables.

`eval::SisRegistry` has three functions:

```c
void defineVariable(const std::string& name, eval::Value value);
void defineFn(const char* name, std::function<Value(std::vector<Value>&)> fn, std::string docs_string = "");
NativeClassBuilder defineClass(const char* name, std::string docs_string = "");
```
All three functions take as their first argument the name of the exposed value. The main communication type between C++ and SiS is `eval::Value`. This is the main runtime type used to represent all values in SiS.
`eval::Value` holds a variant of the following types: `std::variant<std::monostate, double, bool, std::string, Array, Function, NativeFunction, std::shared_ptr<Class>, std::shared_ptr<Instance>>`.
It is suggested to rely on the implicit conversions for basic return types. Please refer to [/include/Value.h](../include/Value.h) for more detailed definitions of complex types.

--- 

#### Exposing Values

`defineVariable` is mainly used to expose global variables to SiS, but keep in mind that SiS does not enforce immutability on them.
Example of variable registration:

```c
extern "C" void sis_module_init(eval::SisRegistry* reg) {
  reg->defineVariable("PI", M_PI);
}
```
This now gives access to the variable `PI` from SiS:

```C
include "my_lib" as ml;

print(ml.PI);
```

Native free functions require an exact signature: `static eval::Value name(std::vector<eval::Value>& args)`.
Arguments passed from SiS are provided in a single `std::vector` which are unpack and validate manually.

Example of a simple native function:

```c
static eval::Value add(std::vector<eval::Value>& args) {
  if (args.size() != 2) throw std::runtime_error("add() expects 2 arguments");

  const auto* a = std::get_if<double>(&args[0].data);
  if (a == nullptr) throw std::runtime_error("add(): first argument is not a number");

  const auto* b = std::get_if<double>(&args[1].data);
  if (b == nullptr) throw std::runtime_error("add(): second argument is not a number");

  return {*a + *b};
}

extern "C" void sis_module_init(eval::SisRegistry* reg) {
  reg->defineFn("add", add, "Optional documentation string");
}
```
Usage from SiS:
```C
include "my_lib" as ml;

print(ml.add(1, 2));
```
To avoid the repeated manual variant access and error checking, `SisDynamicLibMacros.h` provides helper functions with the general signature `inline type requireType(const eval::Value& val, const char* ctx)`. See the [Type Helpers](#type-helpers) section below.

#### Defining a Native Class

Native classes let you back a SiS class with a real C++ object. The general pattern is:

1. Write a plain C++ class with the logic you want to expose.
2. Register it via `defineClass`, which returns a `NativeClassBuilder`.
3. Chain `.constructor()` and `.method()` calls on the builder to wire up the SiS-facing API.

`defineClass` returns a `NativeClassBuilder` with the following API, all methods return `*this` to allow chaining:

```c
NativeClassBuilder& docs(std::string text);
NativeClassBuilder& constructor(std::function<void(std::shared_ptr<Instance>, std::vector<Value>&)> ctor);
NativeClassBuilder& method(const char* name, std::function<Value(std::shared_ptr<Instance>, std::vector<Value>&)> fn);
NativeClassBuilder& field(const char* name, Value default_value);
```

##### Keeping the C++ Object Alive

The core challenge with native classes is lifetime: the C++ object backing a SiS instance needs to stay alive for as long as that instance exists. Since `eval::Value` has no generic pointer slot, SiS uses a specific convention to solve this.

The approach is to store a `shared_ptr` to the C++ object inside a special `__native` field on the instance. That field holds an `eval::Array` containing two elements:
- `[0]` the raw address of a heap-allocated `shared_ptr<CppType>`, stored as a `double` via `reinterpret_cast`.
- `[1]` a `NativeFunction` whose closure captures the `shared_ptr`, keeping the ref-count alive. When the instance is garbage collected and the array is dropped, this closure is destroyed, the ref-count drops, and the C++ object is freed.

In full, without any convenience macros:

```c
.constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
  double initial = args.empty() ? 0.0 : /* extract from args */;

  auto native = std::make_shared<NativeCounter>(initial);

  // Box the shared_ptr: heap-allocate a copy so we can store its address as a double.
  auto* box = new std::shared_ptr<NativeCounter>(native);
  auto handle = std::make_shared<eval::InternalArray>();

  // Element 0: raw address of the box, stored as a double.
  handle->emplaceBack(static_cast<double>(reinterpret_cast<uintptr_t>(box)));

  // Element 1: keepalive closure. Captures the shared_ptr to hold the ref-count,
  // and deletes the box when the instance is eventually collected.
  handle->emplaceBack(eval::NativeFunction{
    .name = "__counter_keepalive",
    .fn   = [native, box](std::vector<eval::Value>&) -> eval::Value {
      (void)native;
      delete box;
      return eval::Value{};
    }
  });

  (*inst->fields)["__native"] = eval::Value(handle);
})
```

Recovering the pointer in a method works by reversing the process:

```c
static std::shared_ptr<NativeCounter> getCounter(const std::shared_ptr<eval::Instance>& inst) {
  auto it = inst->fields->find("__native");
  if (it == inst->fields->end())
    throw std::runtime_error("Counter: missing __native field");

  const auto* arr = std::get_if<eval::Array>(&it->second.data);
  if (!arr || !*arr || (*arr)->elements.empty())
    throw std::runtime_error("Counter: __native field is not a valid handle");

  const auto* addr = std::get_if<double>(&(**arr).elements[0].second.data);
  if (!addr)
    throw std::runtime_error("Counter: __native handle is corrupt");

  return *reinterpret_cast<std::shared_ptr<NativeCounter>*>(static_cast<uintptr_t>(*addr));
}
```

This boilerplate is exactly what `SIS_NATIVE_CTOR` and `SIS_GET_NATIVE` from [SisDynamicLibMacros.h](../include/SisDynamicMacros.h) encapsulate. It is recommended to use those macros in real libraries.

##### Full Example

```c
class NativeCounter {
  public:
  explicit NativeCounter(double initial) : m_value(initial) {}
  void increment() { m_value += 1.0; }
  void add(double n) { m_value += n; }
  [[nodiscard]] double value() const { return m_value; }
  double reset() { double old = m_value; m_value = 0.0; return old; }

  private:
  double m_value;
};

static std::shared_ptr<NativeCounter> getCounter(const std::shared_ptr<eval::Instance>& inst) {
  // ... as shown above
}

extern "C" void sis_module_init(eval::SisRegistry* reg) {
  reg->defineClass("Counter", "A simple counter class.")
    .field("__native", eval::Value{})
    .constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
      double initial = args.empty() ? 0.0 : /* extract double from args[0] */;
      // ... keepalive boilerplate shown above ...
    })
    .method("increment", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
      getCounter(inst)->increment();
      return eval::Value{};
    })
    .method("add", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) -> eval::Value {
      if (args.size() != 1) throw std::runtime_error("Counter.add() expects 1 argument");
      getCounter(inst)->add(/* extract double from args[0] */);
      return eval::Value{};
    })
    .method("value", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
      return {getCounter(inst)->value()};
    })
    .method("reset", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
      return {getCounter(inst)->reset()};
    });
}
```

This can then be used in SiS as:

```C
include "my_counter" as mc;

pin c = new mc.Counter(5);
c.increment();
c.add(10);
print(c.value()); // 16
print(c.reset()); // 16
print(c.value()); // 0
```

##### Fields

Default field values can be registered with `.field()`. These are set on every new instance before the constructor runs, the same as AST-declared fields. The `__native` field in the example above is registered this way as a `null` placeholder that the constructor then overwrites.

```c
.field("label", eval::Value{std::string("default")})
```

Fields are readable and writable from SiS like any other instance field.

#### Documentation

Documentation strings are optional throughout. `defineFn` and `defineClass` accept one as their last argument. For methods and constructors on a `NativeClassBuilder`, a `.docs()` call placed **before** the item it describes buffers the string and stamps it onto the next `.constructor()` or `.method()` call.

```c
reg->defineFn("add", fnAdd,
    "@brief Adds two numbers.\n"
    "@param a The first number.\n"
    "@param b The second number.\n"
    "@return The sum of a and b."
);

reg->defineClass("Counter", "@brief A simple counter class.")
  .field("__native", eval::Value{})
  .docs("@brief Creates a new Counter.\n"
        "@param initial Optional starting value. Defaults to 0.")
  .constructor(...)
  .docs("@brief Increments the counter by 1.")
  .method("increment", ...)
  .method("value", ...)  // no .docs() call before this, docs will be empty
```

`.docs()` before `.field()` is silently discarded since fields do not carry documentation. All doc calls are optional and can be omitted entirely.

Documentation strings are stored at runtime and accessible from SiS via the `__docs__` field on any function, method, or class value:

```c
print(Counter.__docs__);
print(Counter.increment.__docs__);
```

It is recommended to use [Doxygen-style tags](https://www.doxygen.nl/manual/commands.html) (`@brief`, `@param`, `@return`, `@throws`, `@note`) for consistency with the standard library.
But more importantly because it looks nice.

#### Type Helpers

[SisDynamicLibMacros.h](../include/SisDynamicMacros.h) generates a `require` helper for each value type. These extract the underlying C++ value from an `eval::Value` or throw a descriptive `std::runtime_error` if the type does not match. `ctx` is included in the error message to identify the call site.

| Function | Extracted type |
|-------------------------------|-----------------------------------|
| `requireNull(val, ctx)`       | `std::monostate`                  |
| `requireNum(val, ctx)`        | `double`                          |
| `requireBool(val, ctx)`       | `bool`                            |
| `requireStr(val, ctx)`        | `std::string`                     |
| `requireArr(val, ctx)`        | `eval::Array`                     |
| `requireFunc(val, ctx)`       | `eval::Function`                  |
| `requireNativeFunc(val, ctx)` | `eval::NativeFunction`            |
| `requireClass(val, ctx)`      | `std::shared_ptr<eval::Class>`    |
| `requireInstance(val, ctx)`   | `std::shared_ptr<eval::Instance>` |

```c
double x = requireNum(args[0], "myFn");
std::string s = requireStr(args[1], "myFn");
```

#### Building

See [Building.md](Building.md) for full build instructions. The short version for a single-file library:

```bash
g++ -std=c++23 -shared -fPIC \
    -I/path/to/sis/include \
    mylibrary.cpp \
    -o $SIS_PATH/dynamic/linux/mylibrary.so
```

The output filename determines the name used in `include` statements from SiS. A file named `mylibrary.so` is loaded with `include "mylibrary";`.
