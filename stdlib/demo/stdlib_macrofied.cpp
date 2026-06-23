// demo_stdlib_macros.cpp  —  same as demo_stdlib.cpp, rewritten with SisMacros.h
//
// Build:
//   g++ -std=c++23 -shared -fPIC \
//       -I/path/to/sis/include \
//       demo_stdlib_macros.cpp \
//       -o $SIS_PATH/dynamic/linux/demo.so

#include <SisDynamicLibMacros.h>
#include <stdexcept>
#include <string>
#include <vector>

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

static double requireNum(const eval::Value& val, const char* ctx) {
  const auto* d = std::get_if<double>(&val.data);
  if (d == nullptr) throw std::runtime_error(std::string(ctx) + ": expected a number, got " + val.typeName());
  return *d;
}

// free functions
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

static eval::Value fnCounterSum(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("counter_sum() expects 1 argument (array)");
  const auto* arr = std::get_if<eval::Array>(&args[0].data);
  if ((arr == nullptr) || !*arr) throw std::runtime_error("counter_sum() expects an array");
  NativeCounter acc(0.0);
  for (const eval::Value& elem : **arr) {
    acc.add(requireNum(elem, "counter_sum"));
  }
  return {acc.value()};
}

SIS_MODULE_INIT(reg) {
  reg->defineFn("add", fnAdd);
  reg->defineFn("clamp", fnClamp);
  reg->defineFn("counter_sum", fnCounterSum);

  // clang-format off
  SIS_NATIVE_CLASS_BEGIN(reg, "Counter", NativeCounter)
    .constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
      double initial = args.empty() ? 0.0 : requireNum(args[0], "Counter()");
      SIS_NATIVE_CTOR(NativeCounter, inst, ctr, initial);
    })
    .method("increment", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
      SIS_GET_NATIVE(NativeCounter, inst)->increment();
      return eval::Value{};
    })
    .method("add", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) -> eval::Value {
      if (args.size() != 1) throw std::runtime_error("Counter.add() expects 1 argument");
      SIS_GET_NATIVE(NativeCounter, inst)->add(requireNum(args[0], "Counter.add"));
      return eval::Value{};
    })
    .method("value", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
      return {SIS_GET_NATIVE(NativeCounter, inst)->value()};
    })
    .method("reset", [](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>&) -> eval::Value {
      return {SIS_GET_NATIVE(NativeCounter, inst)->reset()};
    })
  SIS_NATIVE_CLASS_END();
  // clang-format on
}
