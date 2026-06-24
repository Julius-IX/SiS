#include <SisDynamicLibMacros.h>

#include <algorithm>

FN_SIGNATURE(upper, args) {
  std::string str = requireStr(args[0], "upper");
  std::ranges::transform(str, str.begin(), ::toupper);
  return str;
}

FN_SIGNATURE(lower, args) {
  std::string str = requireStr(args[0], "lower");
  std::ranges::transform(str, str.begin(), ::tolower);
  return str;
}

static void localLtrim(std::string& str) {
  auto pos = str.find_first_not_of(" \t\n\r\f\v");
  if (pos == std::string::npos) {
    str.clear();
  } else {
    str.erase(0, pos);
  }
}

static void localRtrim(std::string& str) {
  auto pos = str.find_last_not_of(" \t\n\r\f\v");
  if (pos == std::string::npos) {
    str.clear();
  } else {
    str.erase(pos + 1);
  }
}

FN_SIGNATURE(trim, args) {
  std::string str = requireStr(args[0], "trim");
  localLtrim(str);
  localRtrim(str);
  return str;
}

FN_SIGNATURE(ltrim, args) {
  std::string str = requireStr(args[0], "ltrim");
  localLtrim(str);
  return str;
}

FN_SIGNATURE(rtrim, args) {
  std::string str = requireStr(args[0], "rtrim");
  localRtrim(str);
  return str;
}

FN_SIGNATURE(contains, args) {
  std::string str = requireStr(args[0], "contains");
  std::string sub = requireStr(args[1], "contains");
  return str.find(sub) != std::string::npos;
}

FN_SIGNATURE(startsWith, args) {
  std::string str = requireStr(args[0], "starts_with");
  std::string pre = requireStr(args[1], "starts_with");
  return str.starts_with(pre);
}

FN_SIGNATURE(endsWith, args) {
  std::string str = requireStr(args[0], "ends_with");
  std::string suf = requireStr(args[1], "ends_with");
  if (suf.size() > str.size()) return false;
  return str.ends_with(suf);
}

FN_SIGNATURE(find, args) {
  std::string str = requireStr(args[0], "find");
  std::string sub = requireStr(args[1], "find");
  size_t pos = str.find(sub);
  if (pos == std::string::npos) return static_cast<double>(-1);
  return static_cast<double>(pos);
}

FN_SIGNATURE(substr, args) {
  std::string str = requireStr(args[0], "substr");
  double start = requireNum(args[1], "substr");

  if (start < 0 || (size_t)start >= str.size()) return std::string("");

  if (args.size() >= 3) {
    double len = requireNum(args[2], "substr");
    return str.substr(start, len);
  }

  return str.substr(start);
}

FN_SIGNATURE(replace, args) {
  std::string str = requireStr(args[0], "replace");
  std::string from = requireStr(args[1], "replace");
  std::string to = requireStr(args[2], "replace");

  if (from.empty()) return str;

  size_t pos = 0;
  while ((pos = str.find(from, pos)) != std::string::npos) {
    str.replace(pos, from.size(), to);
    pos += to.size();
  }

  return str;
}

FN_SIGNATURE(split, args) {
  std::string str = requireStr(args[0], "split");
  std::string delim = requireStr(args[1], "split");

  if (delim.empty()) return str;

  size_t start = 0;
  size_t pos = 0;

  eval::Array out = std::make_shared<std::vector<eval::Value>>();
  while ((pos = str.find(delim, start)) != std::string::npos) {
    out->emplace_back(str.substr(start, pos - start));
    start = pos + delim.size();
  }

  out->emplace_back(str.substr(start));
  return out;
}

FN_SIGNATURE(join, args) {
  eval::Array arr = requireArr(args[0], "join");
  std::string delim = requireStr(args[1], "join");

  std::string out;
  bool first = true;

  for (auto& v : *arr) {
    if (!first) out += delim;
    first = false;
    out += requireStr(v, "join");
  }

  return out;
}

FN_SIGNATURE(format, args) {
  if (args.empty()) {
    throw std::runtime_error("format: missing format string");
  }

  std::string str = requireStr(args[0], "format");
  std::string out;
  out.reserve(str.size());

  size_t arg_index = 1;

  for (size_t i = 0; i < str.size(); ++i) {
    char c = str[i];

    if (c == '{') {
      if (i + 1 >= str.size()) {
        throw std::runtime_error("format: unmatched '{' at end of string");
      }

      if (str[i + 1] == '{') {
        // escaped {{
        out += '{';
        ++i;
        continue;
      }

      if (str[i + 1] == '}') {
        // placeholder {}
        if (arg_index >= args.size()) {
          throw std::runtime_error("format: not enough arguments for placeholders");
        }

        out += requireStr(args[arg_index++], "format");
        ++i;
        continue;
      }

      throw std::runtime_error("format: invalid '{' usage (expected '{}' or '{{')");
    }

    if (c == '}') {
      if (i + 1 < str.size() && str[i + 1] == '}') {
        // escaped }}
        out += '}';
        ++i;
        continue;
      }

      throw std::runtime_error("format: unmatched '}'");
    }

    out += c;
  }

  if (arg_index < args.size()) {
    throw std::runtime_error("format: too many arguments provided");
  }

  return out;
}

SIS_MODULE_INIT(reg) {
  reg->defineFn("upper", upper);
  reg->defineFn("lower", lower);
  reg->defineFn("trim", trim);
  reg->defineFn("ltrim", ltrim);
  reg->defineFn("rtrim", rtrim);

  reg->defineFn("contains", contains);
  reg->defineFn("starts_with", startsWith);
  reg->defineFn("ends_with", endsWith);
  reg->defineFn("find", find);
  reg->defineFn("substr", substr);
  reg->defineFn("replace", replace);

  reg->defineFn("split", split);
  reg->defineFn("join", join);

  reg->defineFn("format", format);
}
