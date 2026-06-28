// demo_stdlib.cpp sample SiS native stdlib module
//
// Demonstrates three things:
//   1. Free top-level functions exposed directly into scope
//   2. A SiS-instantiable class (Counter) backed by a C++ class
//   3. Using that same C++ class inside a free function
//
// Build:
//   g++ -std=c++23 -shared -fPIC \
//       -I/path/to/sis/include \
//       demo_stdlib.cpp \
//       -o $SIS_PATH/dynamic/linux/demo.so
//
// Usage in SiS:
//   include "demo";
//
//   print(add(10, 20));           // free function -> 30
//   print(clamp(15, 0, 10));      // free function -> 10
//   print(counter_sum([1,2,3]));  // free function using C++ Counter internally -> 6
//
//   pin c = new Counter(0);
//   c.increment();
//   c.increment();
//   c.add(10);
//   print(c.value());             // -> 12
//   print(c.reset());             // -> 12
//   print(c.value());             // -> 0

#include <SisRegistry.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// Internal C++ class not exposed directly to SiS, just used as the actual
// implementation behind the SiS-facing Counter class and the counter_sum fn.
// Think of it exactly like how print() calls fmt::print internally the C++
// machinery is invisible to the user, they just see a SiS function.
class NativeCounter {
  public:
  explicit NativeCounter(double initial)
    : m_value(initial) {}

  void increment() { m_value += 1.0; }
  void add(double n) { m_value += n; }
  [[nodiscard]] double value() const { return m_value; }
  double reset() {
    double old = m_value;
    m_value = 0.0;
    return old;
  }

  private:
  double m_value;
};

// -------------------------------------------------------------------------
// Helper: pull a double out of a Value or throw with a useful message.
// -------------------------------------------------------------------------
static double requireNum(const eval::Value& val, const char* ctx) {
  const auto* d = std::get_if<double>(&val.data);
  if (d == nullptr) throw std::runtime_error(std::string(ctx) + ": expected a number, got " + val.typeName());
  return *d;
}

// Helper: get the NativeCounter stored in an instance's "__native" field.
// The Counter SiS class stores a shared_ptr<NativeCounter> wrapped in an
// Array (since Value has no generic pointer slot Array is just a
// shared_ptr<vector<Value>> and we use it as an opaque handle here).
//
// The pattern: wrap a C++ object in a shared_ptr, box it inside a
// single-element Array, store that Array as a field. Then unwrap on each
// method call. It's the equivalent of storing a pointer in a void* field.
static std::shared_ptr<NativeCounter> getCounter(const std::shared_ptr<eval::Instance>& inst) {
  auto it = inst->fields->find("__native");
  if (it == inst->fields->end()) {
    throw std::runtime_error("Counter: missing __native field (corrupt instance)");
  }

  const auto* arr = std::get_if<eval::Array>(&it->second.data);
  if ((arr == nullptr) || !*arr || (*arr)->elements.empty()) {
    throw std::runtime_error("Counter: __native field is not a valid handle");
  }

  // The NativeCounter* was stashed as a double (its raw address).
  // See the constructor below for where this is written.
  const auto* addr = std::get_if<double>(&(**arr).elements[0].second.data);
  if (addr == nullptr) throw std::runtime_error("Counter: __native handle is corrupt");

  // Reinterpret the stored address back to a shared_ptr via a raw pointer.
  // We keep the shared_ptr alive by also storing a second element in the
  // array that holds it see constructor.
  return *reinterpret_cast<std::shared_ptr<NativeCounter>*>(static_cast<uintptr_t>(*addr));
}

// 1. Free top-level functions

// Simple arithmetic utility same shape as len() or num() in the interpreter
static eval::Value fnAdd(std::vector<eval::Value>& args) {
  if (args.size() != 2) throw std::runtime_error("add() expects 2 arguments");
  return {requireNum(args[0], "add") + requireNum(args[1], "add")};
}

static eval::Value fnClamp(std::vector<eval::Value>& args) {
  if (args.size() != 3) throw std::runtime_error("clamp() expects 3 arguments (value, min, max)");
  double v = requireNum(args[0], "clamp");
  double lo = requireNum(args[1], "clamp");
  double hi = requireNum(args[2], "clamp");
  if (lo > hi) throw std::runtime_error("clamp(): min > max");
  return {v < lo ? lo : (v > hi ? hi : v)};
}

// Free function that uses NativeCounter internally exactly like print()
// uses fmt:: internally. The user just calls counter_sum([1, 2, 3]).
static eval::Value fnCounterSum(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("counter_sum() expects 1 argument (array)");
  const auto* arr = std::get_if<eval::Array>(&args[0].data);
  if (!arr || !*arr) throw std::runtime_error("counter_sum() expects an array");

  NativeCounter acc(0.0);
  for (auto& [key, elem] : (**arr).elements) {
    acc.add(requireNum(elem, "counter_sum"));
  }
  return {acc.value()};
}

// 2. Counter class exposed to SiS

// We need the NativeCounter to live as long as the SiS
// instance. We do this by storing a shared_ptr<NativeCounter> inside the
// instance's fields. Since Value can't hold an arbitrary pointer, we use a
// trick: store the shared_ptr itself in a heap-allocated box, then store
// the box's address as a double in a 1-element Array field ("__native").
// A second element in that array holds a NativeFunction that acts as the
// deleter when the Array is destroyed the shared_ptr goes with it.
//
// Cleaner long-term: add a std::any or void* slot to Value. For now this
// works and is self-contained.

static void registerCounterClass(eval::SisRegistry* reg) {
  reg->defineClass("Counter", "A class that counts things")
    .field("__native", eval::Value{}) // placeholder; constructor fills this in
    .docs("The constructor takes an optional number, defaults to 0.")
    .constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
      double initial = args.empty() ? 0.0 : requireNum(args[0], "Counter()");

      // Allocate the C++ object on the heap via shared_ptr so lifetime is
      // tied to the SiS instance's field map.
      auto native = std::make_shared<NativeCounter>(initial);

      // Box the shared_ptr: heap-allocate a copy and store its address as
      // a double. Then put both the address and a "keep-alive" NativeFunction
      // (which holds a copy of the shared_ptr) into the array.
      auto* box = new std::shared_ptr<NativeCounter>(native);
      auto handle = std::make_shared<eval::InternalArray>();

      // Element 0: the raw address as a double (used to get the pointer back)
      handle->emplaceBack(static_cast<double>(reinterpret_cast<uintptr_t>(box)));

      // Element 1: a NativeFunction whose closure captures native this keeps
      // the shared_ptr alive as long as the array lives, and when the array
      // is destroyed (instance GC'd) the shared_ptr ref-count drops.
      handle->emplaceBack(eval::NativeFunction{.name = "__counter_keepalive", .fn = [native, box](std::vector<eval::Value>&) -> eval::Value {
                                                  // Also clean up the box when this lambda is finally destroyed.
                                                  // The lambda itself keeps `native` alive via capture, and `box`
                                                  // is deleted here on first (and only) call, or on destruction.
                                                  (void)native;
                                                  delete box;
                                                  return eval::Value{};
                                                }});

      (*inst->fields)["__native"] = eval::Value(handle);
    })
    .docs("Increment the counter by 1.")
    .method("increment",
            [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
              getCounter(inst)->increment();
              return eval::Value{};
            })
    .docs("Add a number to the counter.")
    .method("add",
            [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) -> eval::Value {
              if (args.size() != 1) throw std::runtime_error("Counter.add() expects 1 argument");
              getCounter(inst)->add(requireNum(args[0], "Counter.add"));
              return eval::Value{};
            })
    .docs("Get the current value of the counter.")
    .method("value", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value { return {getCounter(inst)->value()}; })
    .docs("Reset the counter to 0.")
    .method("reset", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value { return {getCounter(inst)->reset()}; });
}

// Entry point called by the evaluator when the .so is loaded.
extern "C" void sis_module_init(eval::SisRegistry* reg) {
  // Free functions land directly in scope, callable without any prefix.
  reg->defineFn("add", fnAdd, "add(a, b) -> a + b");
  reg->defineFn("clamp", fnClamp, "clamp(v, lo, hi) -> min(max(v, lo), hi)");
  reg->defineFn("counter_sum", fnCounterSum, "counter_sum(arr) -> sum(arr)");

  // Class registration after this, `new Counter(n)` works in SiS.
  registerCounterClass(reg);
}
