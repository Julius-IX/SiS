#include <SisDynamicLibMacros.h>

#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

static std::regex makeRegex(const std::string& pattern, const std::string& flags = "") {
  std::regex_constants::syntax_option_type opts = std::regex_constants::ECMAScript;
  for (char f : flags) {
    switch (f) {
      case 'i': opts |= std::regex_constants::icase;    break;
      case 'm': opts |= std::regex_constants::multiline; break;
      case 's': opts |= std::regex_constants::ECMAScript; break; // dotall not in std, ignore gracefully
      default: throw std::runtime_error("re: unknown flag '" + std::string(1, f) + "'");
    }
  }
  try {
    return std::regex(pattern, opts);
  } catch (const std::regex_error& e) {
    throw std::runtime_error("re: invalid pattern '" + pattern + "': " + e.what());
  }
}

// A match result is returned as an array of strings:
//   [0] = full match, [1..n] = capture groups
// Returns null Value if no match.
static eval::Value matchToValue(const std::smatch& m) {
  std::vector<eval::Value> out;
  out.reserve(m.size());
  for (const auto& sub : m)
    out.emplace_back(sub.str());
  return {std::make_shared<eval::InternalArray>(std::move(out))};
}

FN_SIGNATURE(fnMatch, args) {
  if (args.size() != 2) throw std::runtime_error("match(): expected 2 arguments [pattern, s], got " + std::to_string(args.size()));
  std::string pattern = requireStr(args[0], "match");
  std::string s       = requireStr(args[1], "match");
  std::regex  re      = makeRegex(pattern);
  std::smatch m;
  if (!std::regex_match(s, m, re)) return {};
  return matchToValue(m);
}

FN_SIGNATURE(fnSearch, args) {
  if (args.size() != 2) throw std::runtime_error("search(): expected 2 arguments [pattern, s], got " + std::to_string(args.size()));
  std::string pattern = requireStr(args[0], "search");
  std::string s       = requireStr(args[1], "search");
  std::regex  re      = makeRegex(pattern);
  std::smatch m;
  if (!std::regex_search(s, m, re)) return {};
  return matchToValue(m);
}

FN_SIGNATURE(fnFindAll, args) {
  if (args.size() != 2) throw std::runtime_error("find_all(): expected 2 arguments [pattern, s], got " + std::to_string(args.size()));
  std::string pattern = requireStr(args[0], "find_all");
  std::string s       = requireStr(args[1], "find_all");
  std::regex  re      = makeRegex(pattern);

  std::vector<eval::Value> matches;
  auto it  = std::sregex_iterator(s.begin(), s.end(), re);
  auto end = std::sregex_iterator{};
  for (; it != end; ++it)
    matches.emplace_back(matchToValue(*it));

  return {std::make_shared<eval::InternalArray>(std::move(matches))};
}

FN_SIGNATURE(fnReplace, args) {
  if (args.size() != 3) throw std::runtime_error("replace(): expected 3 arguments [pattern, s, replacement], got " + std::to_string(args.size()));
  std::string pattern     = requireStr(args[0], "replace");
  std::string s           = requireStr(args[1], "replace");
  std::string replacement = requireStr(args[2], "replace");
  std::regex  re          = makeRegex(pattern);
  return {std::regex_replace(s, re, replacement)};
}

FN_SIGNATURE(fnSplit, args) {
  if (args.size() != 2) throw std::runtime_error("split(): expected 2 arguments [pattern, s], got " + std::to_string(args.size()));
  std::string pattern = requireStr(args[0], "split");
  std::string s       = requireStr(args[1], "split");
  std::regex  re      = makeRegex(pattern);

  std::vector<eval::Value> parts;
  std::sregex_token_iterator it(s.begin(), s.end(), re, -1);
  std::sregex_token_iterator end{};
  for (; it != end; ++it)
    parts.emplace_back(it->str());

  return {std::make_shared<eval::InternalArray>(std::move(parts))};
}

class NativeRegex {
  public:
  explicit NativeRegex(const std::string& pattern, const std::string& flags = "")
    : m_pattern(pattern),
      m_re(makeRegex(pattern, flags)) {}

  eval::Value match(const std::string& s) {
    std::smatch m;
    if (!std::regex_match(s, m, m_re)) return {};
    return matchToValue(m);
  }

  eval::Value search(const std::string& s) {
    std::smatch m;
    if (!std::regex_search(s, m, m_re)) return {};
    return matchToValue(m);
  }

  eval::Value findAll(const std::string& s) {
    std::vector<eval::Value> matches;
    auto it  = std::sregex_iterator(s.begin(), s.end(), m_re);
    auto end = std::sregex_iterator{};
    for (; it != end; ++it)
      matches.emplace_back(matchToValue(*it));
    return {std::make_shared<eval::InternalArray>(std::move(matches))};
  }

  eval::Value replace(const std::string& s, const std::string& repl) {
    return {std::regex_replace(s, m_re, repl)};
  }

  eval::Value split(const std::string& s) {
    std::vector<eval::Value> parts;
    std::sregex_token_iterator it(s.begin(), s.end(), m_re, -1);
    std::sregex_token_iterator end{};
    for (; it != end; ++it)
      parts.emplace_back(it->str());
    return {std::make_shared<eval::InternalArray>(std::move(parts))};
  }

  private:
  std::string m_pattern;
  std::regex  m_re;
};

SIS_MODULE_INIT(reg) {
  reg->defineFn("match", fnMatch,
      "@brief Tests whether a string matches a regex pattern in full.\n"
      "@param pattern The ECMAScript regex pattern.\n"
      "@param s The string to test.\n"
      "@return An array where [0] is the full match and [1..n] are capture groups, or null if no match."
  );
  reg->defineFn("search", fnSearch,
      "@brief Searches for the first occurrence of a pattern anywhere in a string.\n"
      "@param pattern The ECMAScript regex pattern.\n"
      "@param s The string to search.\n"
      "@return An array where [0] is the full match and [1..n] are capture groups, or null if no match."
  );
  reg->defineFn("find_all", fnFindAll,
      "@brief Finds all non-overlapping matches of a pattern in a string.\n"
      "@param pattern The ECMAScript regex pattern.\n"
      "@param s The string to search.\n"
      "@return An array of match arrays, each in the same format as search()."
  );
  reg->defineFn("replace", fnReplace,
      "@brief Replaces all matches of a pattern in a string with a replacement.\n"
      "@param pattern The ECMAScript regex pattern.\n"
      "@param s The string to modify.\n"
      "@param replacement The replacement string. Supports back-references (e.g. $1).\n"
      "@return The resulting string with all matches replaced."
  );
  reg->defineFn("split", fnSplit,
      "@brief Splits a string by a regex pattern delimiter.\n"
      "@param pattern The ECMAScript regex pattern to split on.\n"
      "@param s The string to split.\n"
      "@return An array of substrings between matches of the pattern."
  );

  // clang-format off
  SIS_NATIVE_CLASS_BEGIN(reg, "Regex", NativeRegex, "@brief A compiled regular expression object for repeated matching operations.")
    .docs("@brief Compiles a regex pattern with optional flags.\n"
          "@param pattern The ECMAScript regex pattern to compile.\n"
          "@param flags Optional flags string. Supported: 'i' (case-insensitive), 'm' (multiline).\n"
          "@throws If the pattern is invalid.")
    .constructor([](std::shared_ptr<eval::Instance> inst, std::vector<eval::Value>& args) {
      if (args.empty()) throw std::runtime_error("Regex(): expected at least 1 argument [pattern, flags?]");
      std::string pattern = requireStr(args[0], "Regex()");
      std::string flags   = args.size() >= 2 ? requireStr(args[1], "Regex()") : "";
      SIS_NATIVE_CTOR(NativeRegex, inst, native_var, pattern, flags);
    })
    .docs("@brief Tests whether the entire string matches this pattern.\n"
          "@param s The string to test.\n"
          "@return An array where [0] is the full match and [1..n] are capture groups, or null if no match.")
    .NATIVE_METHOD("match", inst, args, {
      if (args.size() != 1) throw std::runtime_error("Regex.match(): expected 1 argument [s]");
      return SIS_GET_NATIVE(NativeRegex, inst)->match(requireStr(args[0], "Regex.match"));
    })
    .docs("@brief Searches for the first occurrence of this pattern anywhere in a string.\n"
          "@param s The string to search.\n"
          "@return An array where [0] is the full match and [1..n] are capture groups, or null if no match.")
    .NATIVE_METHOD("search", inst, args, {
      if (args.size() != 1) throw std::runtime_error("Regex.search(): expected 1 argument [s]");
      return SIS_GET_NATIVE(NativeRegex, inst)->search(requireStr(args[0], "Regex.search"));
    })
    .docs("@brief Finds all non-overlapping matches of this pattern in a string.\n"
          "@param s The string to search.\n"
          "@return An array of match arrays, each in the same format as search().")
    .NATIVE_METHOD("find_all", inst, args, {
      if (args.size() != 1) throw std::runtime_error("Regex.find_all(): expected 1 argument [s]");
      return SIS_GET_NATIVE(NativeRegex, inst)->findAll(requireStr(args[0], "Regex.find_all"));
    })
    .docs("@brief Replaces all matches of this pattern in a string with a replacement.\n"
          "@param s The string to modify.\n"
          "@param replacement The replacement string. Supports back-references (e.g. $1).\n"
          "@return The resulting string with all matches replaced.")
    .NATIVE_METHOD("replace", inst, args, {
      if (args.size() != 2) throw std::runtime_error("Regex.replace(): expected 2 arguments [s, replacement]");
      return SIS_GET_NATIVE(NativeRegex, inst)->replace(requireStr(args[0], "Regex.replace"), requireStr(args[1], "Regex.replace"));
    })
    .docs("@brief Splits a string by this pattern.\n"
          "@param s The string to split.\n"
          "@return An array of substrings between matches of the pattern.")
    .NATIVE_METHOD("split", inst, args, {
      if (args.size() != 1) throw std::runtime_error("Regex.split(): expected 1 argument [s]");
      return SIS_GET_NATIVE(NativeRegex, inst)->split(requireStr(args[0], "Regex.split"));
    })
  SIS_NATIVE_CLASS_END();
  // clang-format on
}
