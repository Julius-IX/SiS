#pragma once

#include <ParserNodeTypes.h>

namespace par {
  void printNode(const Node* node, const std::string& prefix, bool is_last, std::string_view label);
  void printTree(const Node* root_node);
} // namespace par
