#include <SisDynamicLibMacros.h>

#include <algorithm>
#include <print>

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
  if (args.size() < 2) throw std::runtime_error("split() expected 2 arguments [string, delimiter], got " + std::to_string(args.size()));

  std::string str = requireStr(args[0], "split");
  std::string delim = requireStr(args[1], "split");

  if (delim.empty()) return str;

  size_t start = 0;
  size_t pos = 0;

  auto out = std::make_shared<eval::InternalArray>();
  while ((pos = str.find(delim, start)) != std::string::npos) {
    out->emplaceBack(str.substr(start, pos - start));
    start = pos + delim.size();
  }

  out->emplaceBack(str.substr(start));
  return out;
}

FN_SIGNATURE(join, args) {
  eval::Array arr = requireArr(args[0], "join");
  std::string delim = requireStr(args[1], "join");

  std::string out;
  bool first = true;

  for (auto& [k, v] : arr->elements) {
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
  reg->defineFn("upper", upper,
      "@brief Converts a string to uppercase.\n"
      "@param s The input string.\n"
      "@return A new string with all characters converted to uppercase."
  );
  reg->defineFn("lower", lower,
      "@brief Converts a string to lowercase.\n"
      "@param s The input string.\n"
      "@return A new string with all characters converted to lowercase."
  );
  reg->defineFn("trim", trim,
      "@brief Removes leading and trailing whitespace from a string.\n"
      "@param s The input string.\n"
      "@return A new string with whitespace stripped from both ends."
  );
  reg->defineFn("ltrim", ltrim,
      "@brief Removes leading whitespace from a string.\n"
      "@param s The input string.\n"
      "@return A new string with whitespace stripped from the left side."
  );
  reg->defineFn("rtrim", rtrim,
      "@brief Removes trailing whitespace from a string.\n"
      "@param s The input string.\n"
      "@return A new string with whitespace stripped from the right side."
  );

  reg->defineFn("contains", contains,
      "@brief Checks whether a string contains a given substring.\n"
      "@param s The string to search in.\n"
      "@param sub The substring to search for.\n"
      "@return true if sub is found in s, false otherwise."
  );
  reg->defineFn("starts_with", startsWith,
      "@brief Checks whether a string begins with a given prefix.\n"
      "@param s The string to check.\n"
      "@param prefix The prefix to look for.\n"
      "@return true if s starts with prefix, false otherwise."
  );
  reg->defineFn("ends_with", endsWith,
      "@brief Checks whether a string ends with a given suffix.\n"
      "@param s The string to check.\n"
      "@param suffix The suffix to look for.\n"
      "@return true if s ends with suffix, false otherwise."
  );
  reg->defineFn("find", find,
      "@brief Returns the index of the first occurrence of a substring.\n"
      "@param s The string to search in.\n"
      "@param sub The substring to find.\n"
      "@return The zero-based index of the first match, or -1 if not found."
  );
  reg->defineFn("substr", substr,
      "@brief Extracts a substring starting at a given index.\n"
      "@param s The source string.\n"
      "@param start The zero-based start index.\n"
      "@param len Optional maximum number of characters to extract.\n"
      "@return The extracted substring, or an empty string if start is out of range."
  );
  reg->defineFn("replace", replace,
      "@brief Replaces all occurrences of a substring with another string.\n"
      "@param s The source string.\n"
      "@param from The substring to find.\n"
      "@param to The replacement string.\n"
      "@return A new string with all occurrences of from replaced by to."
  );

  reg->defineFn("split", split,
      "@brief Splits a string into an array of substrings by a delimiter.\n"
      "@param s The string to split.\n"
      "@param delimiter The delimiter string to split on.\n"
      "@return An array of substrings. Returns the original string if delimiter is empty."
  );
  reg->defineFn("join", join,
      "@brief Joins an array of strings into a single string with a delimiter between each element.\n"
      "@param list An array of strings.\n"
      "@param delimiter The string to insert between each element.\n"
      "@return The concatenated result."
  );

  reg->defineFn("format", format,
      "@brief Formats a string by replacing {} placeholders with provided arguments.\n"
      "@param template The format string. Use {} for positional substitution, {{ and }} for literal braces.\n"
      "@param ... Values to substitute into the placeholders, in order.\n"
      "@return The formatted string.\n"
      "@throws If the number of placeholders and arguments do not match."
  );
}
