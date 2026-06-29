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
  reg->defineFn("abs", fnAbs,
      "@brief Returns the absolute value of a number.\n"
      "@param n The number.\n"
      "@return The absolute value of n."
  );
  reg->defineFn("floor", fnFloor,
      "@brief Rounds a number down to the nearest integer.\n"
      "@param n The number.\n"
      "@return The largest integer less than or equal to n."
  );
  reg->defineFn("ceil", fnCeil,
      "@brief Rounds a number up to the nearest integer.\n"
      "@param n The number.\n"
      "@return The smallest integer greater than or equal to n."
  );
  reg->defineFn("round", fnRound,
      "@brief Rounds a number to the nearest integer.\n"
      "@param n The number.\n"
      "@return n rounded to the nearest integer, with ties rounding away from zero."
  );

  reg->defineFn("sqrt", fnSqrt,
      "@brief Returns the square root of a number.\n"
      "@param n The number. Must be non-negative.\n"
      "@return The square root of n."
  );
  reg->defineFn("cbrt", fnCbrt,
      "@brief Returns the cube root of a number.\n"
      "@param n The number.\n"
      "@return The cube root of n."
  );

  reg->defineFn("pow", fnPow,
      "@brief Raises a base to the power of an exponent.\n"
      "@param base The base number.\n"
      "@param exp The exponent.\n"
      "@return base raised to the power of exp."
  );
  reg->defineFn("log", fnLog,
      "@brief Returns the natural logarithm (base e) of a number.\n"
      "@param n The number. Must be positive.\n"
      "@return The natural logarithm of n."
  );
  reg->defineFn("log2", fnLog2,
      "@brief Returns the base-2 logarithm of a number.\n"
      "@param n The number. Must be positive.\n"
      "@return The base-2 logarithm of n."
  );
  reg->defineFn("log10", fnLog10,
      "@brief Returns the base-10 logarithm of a number.\n"
      "@param n The number. Must be positive.\n"
      "@return The base-10 logarithm of n."
  );

  reg->defineFn("sin", fnSin,
      "@brief Returns the sine of an angle in radians.\n"
      "@param radians The angle in radians.\n"
      "@return The sine of the angle."
  );
  reg->defineFn("cos", fnCos,
      "@brief Returns the cosine of an angle in radians.\n"
      "@param radians The angle in radians.\n"
      "@return The cosine of the angle."
  );
  reg->defineFn("tan", fnTan,
      "@brief Returns the tangent of an angle in radians.\n"
      "@param radians The angle in radians.\n"
      "@return The tangent of the angle."
  );

  reg->defineFn("asin", fnAsin,
      "@brief Returns the arc sine of a value, in radians.\n"
      "@param n A value in the range [-1, 1].\n"
      "@return The arc sine in radians, in the range [-PI/2, PI/2]."
  );
  reg->defineFn("acos", fnAcos,
      "@brief Returns the arc cosine of a value, in radians.\n"
      "@param n A value in the range [-1, 1].\n"
      "@return The arc cosine in radians, in the range [0, PI]."
  );
  reg->defineFn("atan", fnAtan,
      "@brief Returns the arc tangent of a value, in radians.\n"
      "@param n The value.\n"
      "@return The arc tangent in radians, in the range [-PI/2, PI/2]."
  );
  reg->defineFn("atan2", fnAtan2,
      "@brief Returns the angle in radians between the positive x-axis and the point (x, y).\n"
      "@param y The y coordinate.\n"
      "@param x The x coordinate.\n"
      "@return The angle in radians, in the range [-PI, PI]."
  );

  reg->defineFn("min", fnMin,
      "@brief Returns the smaller of two numbers.\n"
      "@param a The first number.\n"
      "@param b The second number.\n"
      "@return The smaller of a and b."
  );
  reg->defineFn("max", fnMax,
      "@brief Returns the larger of two numbers.\n"
      "@param a The first number.\n"
      "@param b The second number.\n"
      "@return The larger of a and b."
  );
  reg->defineFn("clamp", fnClamp,
      "@brief Clamps a value to the range [min, max].\n"
      "@param value The value to clamp.\n"
      "@param min The lower bound.\n"
      "@param max The upper bound.\n"
      "@return value if within range, min if below, max if above."
  );

  reg->defineVariable("PI", M_PI);
  reg->defineVariable("E", M_E);
  reg->defineVariable("INF", INFINITY);
  reg->defineVariable("NAN", NAN);
}
