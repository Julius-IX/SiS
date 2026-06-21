#include <Parser.h>
#include <print>
#include <AstPrinter.h>

int main(const int argc, const char* argv[]) {
  if (argc < 2) {
    std::print("Usage: {} <file>\n", argv[0]);
    return 1;
  }

  par::Parser parser;
  parser.parseRoot(argv[1]);
  if (argc > 2) printTree(&parser.peekRoot());
}
