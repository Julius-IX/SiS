#include <SisDynamicLibMacros.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

FN_SIGNATURE(fnAbs, args) {
  if (args.size() != 1) throw std::runtime_error("abs() expects 1 argument (number)");
  return std::abs(requireNum(args[0], "abs"));
}

FN_SIGNATURE(fnFloor, args) {
  if (args.size() != 1) throw std::runtime_error("floor() expects 1 argument (number)");
  return std::floor(requireNum(args[0], "floor"));
}

FN_SIGNATURE(fnCeil, args) {
  if (args.size() != 1) throw std::runtime_error("ceil() expects 1 argument (number)");
  return std::ceil(requireNum(args[0], "ceil"));
}

FN_SIGNATURE(fnRound, args) {
  if (args.size() != 1) throw std::runtime_error("round() expects 1 argument (number)");
  return std::round(requireNum(args[0], "round"));
}

FN_SIGNATURE(fnSqrt, args) {
  if (args.size() != 1) throw std::runtime_error("sqrt() expects 1 argument (number)");
  return std::sqrt(requireNum(args[0], "sqrt"));
}

FN_SIGNATURE(fnCbrt, args) {
  if (args.size() != 1) throw std::runtime_error("cbrt() expects 1 argument (number)");
  return std::cbrt(requireNum(args[0], "cbrt"));
}

FN_SIGNATURE(fnPow, args) {
  if (args.size() != 2) throw std::runtime_error("pow() expects 2 arguments (number, number)");
  return std::pow(requireNum(args[0], "pow"), requireNum(args[1], "pow"));
}

FN_SIGNATURE(fnLog, args) {
  if (args.size() != 1) throw std::runtime_error("log() expects 1 argument (number)");
  return std::log(requireNum(args[0], "log"));
}

FN_SIGNATURE(fnLog2, args) {
  if (args.size() != 1) throw std::runtime_error("log2() expects 1 argument (number)");
  return std::log2(requireNum(args[0], "log2"));
}

FN_SIGNATURE(fnLog10, args) {
  if (args.size() != 1) throw std::runtime_error("log10() expects 1 argument (number)");
  return std::log10(requireNum(args[0], "log10"));
}

FN_SIGNATURE(fnSin, args) {
  if (args.size() != 1) throw std::runtime_error("sin() expects 1 argument (number)");
  return std::sin(requireNum(args[0], "sin"));
}

FN_SIGNATURE(fnCos, args) {
  if (args.size() != 1) throw std::runtime_error("cos() expects 1 argument (number)");
  return std::cos(requireNum(args[0], "cos"));
}

FN_SIGNATURE(fnTan, args) {
  if (args.size() != 1) throw std::runtime_error("tan() expects 1 argument (number)");
  return std::tan(requireNum(args[0], "tan"));
}

FN_SIGNATURE(fnAsin, args) {
  if (args.size() != 1) throw std::runtime_error("asin() expects 1 argument (number)");
  return std::asin(requireNum(args[0], "asin"));
}

FN_SIGNATURE(fnAcos, args) {
  if (args.size() != 1) throw std::runtime_error("acos() expects 1 argument (number)");
  return std::acos(requireNum(args[0], "acos"));
}

FN_SIGNATURE(fnAtan, args) {
  if (args.size() != 1) throw std::runtime_error("atan() expects 1 argument (number)");
  return std::atan(requireNum(args[0], "atan"));
}

FN_SIGNATURE(fnAtan2, args) {
  if (args.size() != 2) throw std::runtime_error("atan2() expects 2 arguments (number, number)");
  return std::atan2(requireNum(args[0], "atan2"), requireNum(args[1], "atan2"));
}

FN_SIGNATURE(fnMin, args) {
  if (args.size() != 2) throw std::runtime_error("min() expects 2 arguments (number, number)");
  return std::min(requireNum(args[0], "min"), requireNum(args[1], "min"));
}

FN_SIGNATURE(fnMax, args) {
  if (args.size() != 2) throw std::runtime_error("max() expects 2 arguments (number, number)");
  return std::max(requireNum(args[0], "max"), requireNum(args[1], "max"));
}

FN_SIGNATURE(fnClamp, args) {
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
