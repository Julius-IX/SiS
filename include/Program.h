#pragma once

#include <ParserNodeTypes.h>
#include <Token.h>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace par {
  struct ParsedFile {
    lex::TokenStream tokens;
    std::unique_ptr<Block> ast;
    std::vector<Path> includes;
    bool is_dynamic = false;
  };

  struct Program {
    std::vector<Path> load_order;
    std::unordered_map<Path, ParsedFile> files;

    [[nodiscard]] const Block& root() const { return *files.at(load_order.front()).ast; }
  };
} // namespace par
