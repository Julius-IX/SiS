#include <Evaluator.h>
#include <Parser.h>
#include <print>
#include <spdlog/fmt/fmt.h>
#include <string>

constexpr std::string version = "SiS 0.2.1";
class StringParser : public par::Parser {
  public:
  std::optional<par::Program> parseEvalString(const std::string& source) {
    const Path synthetic{"<eval>"};
    par::State state;
    state.lexer = std::make_unique<lex::Lexer>(source, std::nullopt);
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
      fmt::print("{}\n", version);
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
          fmt::print("error: REPL not yet implemented\n");
          return 1;
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
      case CliArgs::Mode::REPL: fmt::print("error: REPL not yet implemented\n"); return 1;
    }
  } catch (const std::runtime_error& e) {
    fmt::print("Error: {}\n", e.what());
    return 1;
  }

  return 0;
}
