#include <Parser.h>
#include <ParserNodeTypes.h>
#include <gtest/gtest.h>

#include <spdlog/fmt/fmt.h>

class TestParser : public par::Parser {
  public:
  // Parse a raw source string and return whether it succeeded.
  bool parseSource(const std::string& source) {
    // Wire hooks: no file I/O, dummy error formatter, no include resolution.
    m_hooks.read_file = [](const Path&) -> std::optional<std::string> { return std::nullopt; };
    m_hooks.format_error = [](par::State*, const lex::Token& t, std::string_view msg) -> std::string { return fmt::format("{}:{}: {}", t.line, t.column, msg); };
    m_hooks.resolve_file = [](const Path&, const Path&) -> std::optional<Path> { return std::nullopt; };

    par::State state{
      .lexer = std::make_unique<lex::Lexer>(source),
      .last_token = {},
    };
    return parse(&state);
  }
};

// Cast a Node* to a derived type and ASSERT it is not null.
// Using a macro so gtest reports the right line number.
#define ASSERT_NODE(ptr, Type)                                                                                                                                                     \
  ASSERT_NE((ptr), nullptr) << "Node pointer is null";                                                                                                                             \
  ASSERT_EQ((ptr)->type, par::Type::TYPE) << "Wrong NodeType";                                                                                                                     \
  auto* as_##Type = static_cast<par::Type*>((ptr))

// Convenience: get the Nth top-level statement and cast it.
#define GET_STMT(root, n, Type)                                                                                                                                                    \
  ASSERT_GT((root).statements.size(), static_cast<size_t>(n));                                                                                                                     \
  ASSERT_NODE((root).statements[(n)].get(), Type)
