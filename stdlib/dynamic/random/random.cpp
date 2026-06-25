#include <SisDynamicLibMacros.h>

#include <algorithm>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static std::mt19937 s_engine{std::random_device{}()};

FN_SIGNATURE(fnSeed, args) {
  if (args.size() != 1) throw std::runtime_error("seed(): expected 1 argument [n], got " + std::to_string(args.size()));
  s_engine.seed(static_cast<uint32_t>(requireNum(args[0], "seed")));
  return {};
}

FN_SIGNATURE(fnRand, args) {
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  return {dist(s_engine)};
}

FN_SIGNATURE(fnRandInt, args) {
  if (args.size() != 2) throw std::runtime_error("randint(): expected 2 arguments [lo, hi], got " + std::to_string(args.size()));
  auto lo = static_cast<long long>(requireNum(args[0], "randint"));
  auto hi = static_cast<long long>(requireNum(args[1], "randint"));
  if (lo > hi) throw std::runtime_error("randint(): lo > hi");
  std::uniform_int_distribution<long long> dist(lo, hi);
  return {static_cast<double>(dist(s_engine))};
}

FN_SIGNATURE(fnChoice, args) {
  if (args.size() != 1) throw std::runtime_error("choice(): expected 1 argument [list], got " + std::to_string(args.size()));
  const auto* arr = std::get_if<eval::Array>(&args[0].data);
  if (arr == nullptr || !*arr || (*arr)->size() == 0)
    throw std::runtime_error("choice(): expected a non-empty array");
  std::uniform_int_distribution<size_t> dist(0, (*arr)->size() - 1);
  return (*arr)->elements[dist(s_engine)].second;
}

FN_SIGNATURE(fnShuffle, args) {
  if (args.size() != 1) throw std::runtime_error("shuffle(): expected 1 argument [list], got " + std::to_string(args.size()));
  const auto* arr = std::get_if<eval::Array>(&args[0].data);
  if (arr == nullptr || !*arr) throw std::runtime_error("shuffle(): expected an array");
  std::shuffle((*arr)->elements.begin(), (*arr)->elements.end(), s_engine);
  return args[0]; // mutated in place, return same array
}

FN_SIGNATURE(fnSample, args) {
  if (args.size() != 2) throw std::runtime_error("sample(): expected 2 arguments [list, n], got " + std::to_string(args.size()));
  const auto* arr = std::get_if<eval::Array>(&args[0].data);
  if (arr == nullptr || !*arr) throw std::runtime_error("sample(): expected an array");
  auto n = static_cast<size_t>(requireNum(args[1], "sample"));
  if (n > (*arr)->size()) throw std::runtime_error("sample(): n exceeds array size");

  // copy elements, partial shuffle, take first n
  auto elems = (*arr)->elements;
  for (size_t i = 0; i < n; ++i) {
    std::uniform_int_distribution<size_t> dist(i, elems.size() - 1);
    std::swap(elems[i], elems[dist(s_engine)]);
  }

  std::vector<eval::Value> out;
  out.reserve(n);
  for (size_t i = 0; i < n; ++i)
    out.emplace_back(std::move(elems[i].second));

  return {std::make_shared<eval::InternalArray>(std::move(out))};
}

SIS_MODULE_INIT(reg) {
  reg->defineFn("seed",    fnSeed);
  reg->defineFn("rand",    fnRand);
  reg->defineFn("randint", fnRandInt);
  reg->defineFn("choice",  fnChoice);
  reg->defineFn("shuffle", fnShuffle);
  reg->defineFn("sample",  fnSample);
}
