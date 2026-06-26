#include <Evaluator.h>
#include <Parser.h>
#include <print>
#include <spdlog/fmt/fmt.h>

int main(const int argc, const char* argv[]) {
  if (argc < 2) {
    fmt::print("usage: {} <path/to/file.sis>\n", argv[0]);
    return 1;
  }

  try {
    par::Parser parser;
    auto program = parser.parseRoot(argv[1]);
    if (!program) return 1;
    eval::Evaluator evaluator(argc, argv);
    evaluator.run(*program);
  } catch (const std::runtime_error& e) {
    fmt::print("Error: {}\n", e.what());
    return 1;
  }

  return 0;
}
