#include <Evaluator.h>
#include <Lexer.h>
#include <Parser.h>

#include <print>
#include <string>

#include <linenoise.h>
#include <spdlog/fmt/fmt.h>

constexpr std::string VERSION = "SiS 0.2.1";

class StringParser : public par::Parser {
  public:
  std::optional<par::Program> parseEvalString(const std::string& source) {
    static int counter = 0;
    const Path synthetic{fmt::format("<eval:{}>", counter++)};
    lex::Lexer lexer(source, std::nullopt);
    lex::TokenStream tokens = lexer.tokenize();
    auto line_cache = lexer.takeLineCache();
    par::State state;
    state.tokens = std::move(tokens);
    state.line_cache = std::move(line_cache);
    registerTestState(synthetic, std::move(state));
    if (!parse(&getStateMut(synthetic))) return std::nullopt;
    par::State& s = getStateMut(synthetic);
    if (!s.block) return std::nullopt;
    par::Program program;
    program.files[synthetic] = par::ParsedFile{
      .tokens = {},
      .ast = std::move(s.block),
      .includes = {},
      .is_dynamic = false,
    };
    program.load_order.push_back(synthetic);
    return program;
  }
};

class Repl {
  public:
  Repl() = default;

  void run() { loop(m_eval); }

  void runWithPrelude(const par::Program& program) {
    m_eval.run(program);
    loop(m_eval);
  }

  private:
  eval::Evaluator m_eval;

  static bool isComplete(const std::string& input) {
    lex::Lexer lexer(input, std::nullopt);
    lex::TokenStream tokens = lexer.tokenize();
    int depth = 0;
    for (const lex::Token& tok : tokens) {
      switch (tok.type) {
        case lex::TokenType::L_BRACE:
        case lex::TokenType::L_BRACK:
        case lex::TokenType::L_PAREN: ++depth; break;
        case lex::TokenType::R_BRACE:
        case lex::TokenType::R_BRACK:
        case lex::TokenType::R_PAREN: --depth; break;
        default: break;
      }
    }
    return depth <= 0;
  }

  static void loop(eval::Evaluator& eval) {
    std::string input;
    while (true) {
      const char* prompt = input.empty() ? ">> " : ".. ";
      char* raw = linenoise(prompt);
      if (!raw) break; // Ctrl+D
      std::string line(raw);
      linenoiseFree(raw);
      if (!line.empty() && input.empty()) linenoiseHistoryAdd(line.c_str());
      if (!input.empty()) input += '\n';
      input += line;
      if (!isComplete(input)) continue;
      StringParser parser;
      try {
        auto program = parser.parseEvalString(input);
        if (program) {
          eval::Value result = eval.run(*program);
          if (!std::holds_alternative<std::monostate>(result.data)) {
            fmt::print("{}\n", result.toString());
          }
        }
      } catch (const std::runtime_error& e) {
        fmt::print("Error: {}\n", e.what());
      }
      input.clear();
    }
    fmt::print("\n");
  }
};

struct CliArgs {
  enum class Mode { SCRIPT, EVAL_STRING, REPL };
  Mode mode = Mode::REPL;
  std::string source;
  bool interactive_after = false;
};

static CliArgs parseArgs(int argc, const char* argv[]) {
  CliArgs args;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];

    if (a == "--help" || a == "-h") {
      fmt::print("usage: {} [options] [file]\n"
                 "  <file>              run script\n"
                 "  -e <code>           evaluate string\n"
                 "  --interactive / -i  enter REPL after script (or alone)\n"
                 "  --help    / -h      show this message\n"
                 "  --version / -v      show version\n",
                 argv[0]);
      std::exit(0);
    }

    if (a == "--version" || a == "-v") {
      fmt::print("{}\n", VERSION);
      std::exit(0);
    }

    if (a == "--interactive" || a == "-i") {
      args.interactive_after = true;
      continue;
    }

    if (a == "-e" || a == "--eval") {
      if (++i >= argc) {
        fmt::print("error: -e requires an argument\n");
        std::exit(1);
      }
      args.mode = CliArgs::Mode::EVAL_STRING;
      args.source = argv[i];
      continue;
    }

    if (args.mode == CliArgs::Mode::REPL && a[0] != '-') {
      args.mode = CliArgs::Mode::SCRIPT;
      args.source = a;
      continue;
    }

    fmt::print("error: unknown argument '{}'\n", a);
    std::exit(1);
  }

  return args;
}

int main(const int argc, const char* argv[]) {
  CliArgs args = parseArgs(argc, argv);

  try {
    switch (args.mode) {
      case CliArgs::Mode::SCRIPT: {
        par::Parser parser;
        auto program = parser.parseRoot(args.source);
        if (!program) return 1;
        eval::Evaluator evaluator(argc, argv);
        evaluator.run(*program);
        if (args.interactive_after) {
          Repl repl;
          repl.runWithPrelude(*program);
        }
        break;
      }
      case CliArgs::Mode::EVAL_STRING: {
        StringParser parser;
        auto program = parser.parseEvalString(args.source);
        if (!program) return 1;
        eval::Evaluator evaluator(argc, argv);
        evaluator.run(*program);
        break;
      }
      case CliArgs::Mode::REPL: {
        Repl repl;
        repl.run();
        break;
      }
    }
  } catch (const std::runtime_error& e) {
    fmt::print("Error: {}\n", e.what());
    return 1;
  }

  return 0;
}
