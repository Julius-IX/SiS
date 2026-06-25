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

// free functions
FN_SIGNATURE(fnAdd, args) {
  if (args.size() != 2) throw std::runtime_error("add() expects 2 arguments");
  return {requireNum(args[0], "add") + requireNum(args[1], "add")};
}

FN_SIGNATURE(fnClamp, args) {
  if (args.size() != 3) throw std::runtime_error("clamp() expects 3 arguments (value, min, max)");
  double v = requireNum(args[0], "clamp");
  double lo = requireNum(args[1], "clamp");
  double hi = requireNum(args[2], "clamp");
  if (lo > hi) throw std::runtime_error("clamp(): min > max");
  return {v < lo ? lo : (v > hi ? hi : v)};
}

FN_SIGNATURE(fnCounterSum, args) {
  if (args.size() != 1) throw std::runtime_error("counter_sum() expects 1 argument (array)");
  const auto* arr = std::get_if<eval::Array>(&args[0].data);
  if ((arr == nullptr) || !*arr) throw std::runtime_error("counter_sum() expects an array");
  NativeCounter acc(0.0);
  for (const auto& [key, elem] : arr->get()->elements) {
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
      SIS_NATIVE_CTOR(NativeCounter, inst, native_var, initial);
    })
    .NATIVE_METHOD("increment", inst, args, {
      SIS_GET_NATIVE(NativeCounter, inst)->increment();
      return eval::Value{};
    })
    .NATIVE_METHOD("add", inst, args, {
      if (args.size() != 1) throw std::runtime_error("Counter.add() expects 1 argument");
      SIS_GET_NATIVE(NativeCounter, inst)->add(requireNum(args[0], "Counter.add"));
      return eval::Value{};
    })
    .NATIVE_METHOD("value", inst, args, {
      return {SIS_GET_NATIVE(NativeCounter, inst)->value()};
    })
    .NATIVE_METHOD("reset", inst, args, {
      return {SIS_GET_NATIVE(NativeCounter, inst)->reset()};
    })
  SIS_NATIVE_CLASS_END();
  // clang-format on
}
