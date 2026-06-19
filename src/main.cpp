#include <Evaluator.h>
#include <Lexer.h>
#include <Parser.h>
#include <Token.h>
#include <print>

int main(const int argc, const char* argv[]) {
  if (argc < 2) {
    std::print("usage: {} <path/to/file.sis>\n", argv[0]);
    return 1;
  }

  par::Parser parser;
  if (!parser.parseFile(argv[1])) return 1;

  eval::Evaluator evaluator;
  evaluator.run(*parser.getRoot());

  return 0;
}
