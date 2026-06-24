#include <SisDynamicLibMacros.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

static eval::Value fnAbs(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("abs() expects 1 argument (number)");
  return std::abs(requireNum(args[0], "abs"));
}

static eval::Value fnFloor(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("floor() expects 1 argument (number)");
  return std::floor(requireNum(args[0], "floor"));
}

static eval::Value fnCeil(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("ceil() expects 1 argument (number)");
  return std::ceil(requireNum(args[0], "ceil"));
}

static eval::Value fnRound(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("round() expects 1 argument (number)");
  return std::round(requireNum(args[0], "round"));
}

static eval::Value fnSqrt(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("sqrt() expects 1 argument (number)");
  return std::sqrt(requireNum(args[0], "sqrt"));
}

static eval::Value fnCbrt(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("cbrt() expects 1 argument (number)");
  return std::cbrt(requireNum(args[0], "cbrt"));
}

static eval::Value fnPow(std::vector<eval::Value>& args) {
  if (args.size() != 2) throw std::runtime_error("pow() expects 2 arguments (number, number)");
  return std::pow(requireNum(args[0], "pow"), requireNum(args[1], "pow"));
}

static eval::Value fnLog(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("log() expects 1 argument (number)");
  return std::log(requireNum(args[0], "log"));
}

static eval::Value fnLog2(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("log2() expects 1 argument (number)");
  return std::log2(requireNum(args[0], "log2"));
}

static eval::Value fnLog10(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("log10() expects 1 argument (number)");
  return std::log10(requireNum(args[0], "log10"));
}

static eval::Value fnSin(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("sin() expects 1 argument (number)");
  return std::sin(requireNum(args[0], "sin"));
}

static eval::Value fnCos(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("cos() expects 1 argument (number)");
  return std::cos(requireNum(args[0], "cos"));
}

static eval::Value fnTan(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("tan() expects 1 argument (number)");
  return std::tan(requireNum(args[0], "tan"));
}

static eval::Value fnAsin(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("asin() expects 1 argument (number)");
  return std::asin(requireNum(args[0], "asin"));
}

static eval::Value fnAcos(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("acos() expects 1 argument (number)");
  return std::acos(requireNum(args[0], "acos"));
}

static eval::Value fnAtan(std::vector<eval::Value>& args) {
  if (args.size() != 1) throw std::runtime_error("atan() expects 1 argument (number)");
  return std::atan(requireNum(args[0], "atan"));
}

static eval::Value fnAtan2(std::vector<eval::Value>& args) {
  if (args.size() != 2) throw std::runtime_error("atan2() expects 2 arguments (number, number)");
  return std::atan2(requireNum(args[0], "atan2"), requireNum(args[1], "atan2"));
}

static eval::Value fnMin(std::vector<eval::Value>& args) {
  if (args.size() != 2) throw std::runtime_error("min() expects 2 arguments (number, number)");
  return std::min(requireNum(args[0], "min"), requireNum(args[1], "min"));
}

static eval::Value fnMax(std::vector<eval::Value>& args) {
  if (args.size() != 2) throw std::runtime_error("max() expects 2 arguments (number, number)");
  return std::max(requireNum(args[0], "max"), requireNum(args[1], "max"));
}

static eval::Value fnClamp(std::vector<eval::Value>& args) {
  if (args.size() != 3) throw std::runtime_error("clamp() expects 3 arguments (value, min, max)");
  return std::clamp(
      requireNum(args[0], "clamp"),
      requireNum(args[1], "clamp"),
      requireNum(args[2], "clamp"));
}

SIS_MODULE_INIT(reg) {
  reg->defineFn("abs", fnAbs);
  reg->defineFn("floor", fnFloor);
  reg->defineFn("ceil", fnCeil);
  reg->defineFn("round", fnRound);

  reg->defineFn("sqrt", fnSqrt);
  reg->defineFn("cbrt", fnCbrt);

  reg->defineFn("pow", fnPow);
  reg->defineFn("log", fnLog);
  reg->defineFn("log2", fnLog2);
  reg->defineFn("log10", fnLog10);

  reg->defineFn("sin", fnSin);
  reg->defineFn("cos", fnCos);
  reg->defineFn("tan", fnTan);

  reg->defineFn("asin", fnAsin);
  reg->defineFn("acos", fnAcos);
  reg->defineFn("atan", fnAtan);
  reg->defineFn("atan2", fnAtan2);

  reg->defineFn("min", fnMin);
  reg->defineFn("max", fnMax);
  reg->defineFn("clamp", fnClamp);

  reg->defineVariable("PI", M_PI);
  reg->defineVariable("E", M_E);
  reg->defineVariable("INF", INFINITY);
  reg->defineVariable("NAN", NAN);
}
