#include <Evaluator.h>
#include <Parser.h>

#include <repl.h>
#include <string_parser.h>

#include <string>

#include <spdlog/fmt/fmt.h>

constexpr std::string VERSION = "SiS 1.0.0";

struct CliArgs {
  enum class Mode : uint8_t { SCRIPT, EVAL_STRING, REPL };
  Mode mode = Mode::REPL;
  std::string source;
  bool interactive_after = false;
  bool parallel = false;
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
                 "  --version / -v      show version\n"
                 "  --parallel / -p     use parallel lexing\n",
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

    if (a == "--parallel" || a == "-p") {
      args.parallel = true;
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
        parser.setParallel(args.parallel);

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
