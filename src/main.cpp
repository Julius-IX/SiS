#include <Evaluator.h>
#include <Parser.h>
#include <print>
#include <spdlog/fmt/fmt.h>

int main(const int argc, const char* argv[]) {
  if (argc < 2) {
    fmt::print("usage: {} <path/to/file.sis>\n", argv[0]);
    return 1;
  }

  par::Parser parser;
  if (!parser.parseRoot(argv[1])) return 1;

  eval::Evaluator evaluator;
  evaluator.run(parser.peekRoot());

  return 0;
}
