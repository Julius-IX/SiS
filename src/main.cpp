#include <Evaluator.h>
#include <Lexer.h>
#include <Parser.h>
#include <Token.h>
#include <print>

int main(const int argc, const char* argv[]) {
  if (argc < 1) {
    std::print("usage: SiS <path/to/file.sis>");
  }

  par::Parser parser;
  if (!parser.parseFile(argv[1])) return 1;

  eval::Evaluator evaluator;
  evaluator.run(*parser.getRoot());

  return 0;
}
