#include <AstPrinter.h>

#include <print>
#include <string>

namespace par { // Tree-drawing (debugging only don't expect direct access to this)
  namespace {
    constexpr std::string_view BRANCH = "├── ";
    constexpr std::string_view LAST_BRANCH = "└── ";
    constexpr std::string_view PIPE_CONT = "│   ";
    constexpr std::string_view SPACE_CONT = "    ";

    // One (label, node) pair to print as a child. label may be empty.
    struct Child {
      std::string_view label;
      const Node* node;
    };

    std::string literalToString(const LiteralType& value) {
      return std::visit(
        [](auto&& val) -> std::string {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            return "null";
          } else if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
          } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(val);
          } else { // std::string
            return "\"" + val + "\"";
          }
        },
        value);
    }

    // Short, single-line summary printed next to the node's type name,
    // e.g. "IDENTIFIER (x)" or "BINARY (+)".
    std::string nodeSummary(const Node* node) {
      switch (node->type) {
        case NodeType::LITERAL: return literalToString(static_cast<const Literal*>(node)->value);
        case NodeType::IDENTIFIER: return static_cast<const Identifier*>(node)->name;
        case NodeType::UNARY: return lex::literalTokenToString(static_cast<const Unary*>(node)->operation);
        case NodeType::BINARY: return lex::literalTokenToString(static_cast<const Binary*>(node)->operation);
        case NodeType::VAR_DECL: return static_cast<const VarDecl*>(node)->name;
        case NodeType::MEMBER_ACCESS: return "." + static_cast<const MemberAccess*>(node)->field;
        case NodeType::FN_LITERAL: {
          const auto* fn = static_cast<const FnLiteral*>(node);
          std::string params;
          for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i) params += ", ";
            params += fn->params[i];
          }
          return "(" + params + ")";
        }
        case NodeType::NEW_EXPR: return static_cast<const NewExpr*>(node)->class_name;
        case NodeType::CLASS_DECL: {
          const auto* cls = static_cast<const ClassDecl*>(node);
          return cls->parent_name.empty() ? cls->name : cls->name + " extends " + cls->parent_name;
        }
        case NodeType::JUMP: return static_cast<const Jump*>(node)->kind == JumpKind::BREAK ? "break" : "continue";
        case NodeType::SELF: return static_cast<const Self*>(node)->is_super ? "super" : "this";
        default: return "";
      }
    }

    // Children of a node, each with a descriptive label. Built fresh per
    // call since most nodes own their children via unique_ptr.
    std::vector<Child> nodeChildren(const Node* node) {
      std::vector<Child> out;

      switch (node->type) {
        case NodeType::UNARY: {
          const auto* n = static_cast<const Unary*>(node);
          out.push_back({.label = "operand", .node = n->operand.get()});
          break;
        }
        case NodeType::BINARY: {
          const auto* n = static_cast<const Binary*>(node);
          out.push_back({.label = "left", .node = n->left.get()});
          out.push_back({.label = "right", .node = n->right.get()});
          break;
        }
        case NodeType::BLOCK: {
          const auto* n = static_cast<const Block*>(node);
          for (const auto& stmt : n->statements) {
            out.push_back({.label = "", .node = stmt.get()});
          }
          break;
        }
        case NodeType::IF: {
          const auto* n = static_cast<const If*>(node);
          out.push_back({.label = "condition", .node = n->condition.get()});
          out.push_back({.label = "then", .node = n->then_branch.get()});
          if (n->else_branch) out.push_back({.label = "else", .node = n->else_branch.get()});
          break;
        }
        case NodeType::WHILE: {
          const auto* n = static_cast<const While*>(node);
          out.push_back({.label = "condition", .node = n->condition.get()});
          out.push_back({.label = "body", .node = n->body.get()});
          break;
        }
        case NodeType::VAR_DECL: {
          const auto* n = static_cast<const VarDecl*>(node);
          if (n->initializer) out.push_back({.label = "init", .node = n->initializer.get()});
          break;
        }
        case NodeType::EXPR_STMT: {
          const auto* n = static_cast<const ExprStmt*>(node);
          out.push_back({.label = "", .node = n->expr.get()});
          break;
        }
        case NodeType::CALL: {
          const auto* n = static_cast<const Call*>(node);
          out.push_back({.label = "callee", .node = n->callee.get()});
          for (const auto& arg : n->args) {
            out.push_back({.label = "arg", .node = arg.get()});
          }
          break;
        }
        case NodeType::FN_LITERAL: {
          const auto* n = static_cast<const FnLiteral*>(node);
          out.push_back({.label = "body", .node = n->body.get()});
          break;
        }
        case NodeType::MEMBER_ACCESS: {
          const auto* n = static_cast<const MemberAccess*>(node);
          out.push_back({.label = "object", .node = n->object.get()});
          break;
        }
        case NodeType::ARRAY_LITERAL: {
          const auto* n = static_cast<const ArrayLiteral*>(node);
          for (const auto& el : n->elements) {
            out.push_back({.label = "", .node = el.get()});
          }
          break;
        }
        case NodeType::SUBSCRIPT: {
          const auto* n = static_cast<const Subscript*>(node);
          out.push_back({.label = "object", .node = n->object.get()});
          out.push_back({.label = "index", .node = n->index.get()});
          break;
        }
        case NodeType::RETURN: {
          const auto* n = static_cast<const Return*>(node);
          if (n->value) out.push_back({.label = "value", .node = n->value.get()});
          break;
        }
        case NodeType::NEW_EXPR: {
          const auto* n = static_cast<const NewExpr*>(node);
          for (const auto& arg : n->args) {
            out.push_back({.label = "arg", .node = arg.get()});
          }
          break;
        }
        case NodeType::CLASS_DECL: {
          const auto* n = static_cast<const ClassDecl*>(node);
          for (const auto& field : n->fields) {
            out.push_back({.label = "field", .node = field.get()});
          }
          for (size_t i = 0; i < n->methods.size(); ++i) {
            out.push_back({.label = n->method_names[i], .node = n->methods[i].get()});
          }
          break;
        }
        // LITERAL, IDENTIFIER, JUMP, SELF have no children.
        default: break;
      }

      return out;
    }

    std::string_view nodeTypeName(NodeType type) {
      switch (type) {
        case NodeType::LITERAL: return "LITERAL";
        case NodeType::IDENTIFIER: return "IDENTIFIER";
        case NodeType::UNARY: return "UNARY";
        case NodeType::BINARY: return "BINARY";
        case NodeType::BLOCK: return "BLOCK";
        case NodeType::IF: return "IF";
        case NodeType::WHILE: return "WHILE";
        case NodeType::VAR_DECL: return "VAR_DECL";
        case NodeType::EXPR_STMT: return "EXPR_STMT";
        case NodeType::CALL: return "CALL";
        case NodeType::FN_LITERAL: return "FN_LITERAL";
        case NodeType::MEMBER_ACCESS: return "MEMBER_ACCESS";
        case NodeType::ARRAY_LITERAL: return "ARRAY_LITERAL";
        case NodeType::SUBSCRIPT: return "SUBSCRIPT";
        case NodeType::RETURN: return "RETURN";
        case NodeType::JUMP: return "JUMP";
        case NodeType::SELF: return "SELF";
        case NodeType::NEW_EXPR: return "NEW_EXPR";
        case NodeType::CLASS_DECL: return "CLASS_DECL";
      }
      return "UNKNOWN";
    }
  } // namespace

  void printNode(const Node* node, const std::string& prefix, bool is_last, std::string_view label) {
    if (node == nullptr) {
      std::print("{}{}{}null\n", prefix, is_last ? LAST_BRANCH : BRANCH, label.empty() ? "" : std::string(label) + ": ");
      return;
    }

    const std::string_view branch = is_last ? LAST_BRANCH : BRANCH;
    const std::string summary = nodeSummary(node);

    std::print("{}{}{}{}{}{}{} ({}:{})\n",
               prefix,
               branch,
               label.empty() ? "" : std::string(label) + ": ",
               nodeTypeName(node->type),
               summary.empty() ? "" : " ",
               summary.empty() ? "" : "[" + summary + "]",
               "",
               node->line,
               node->column);

    const std::vector<Child> children = nodeChildren(node);
    const std::string child_prefix = prefix + std::string(is_last ? SPACE_CONT : PIPE_CONT);

    for (size_t i = 0; i < children.size(); ++i) {
      const bool child_is_last = (i + 1 == children.size());
      printNode(children[i].node, child_prefix, child_is_last, children[i].label);
    }
  }

  void printTree(const Node* root_node) {
    if (root_node == nullptr) {
      std::print("(empty tree)\n");
      return;
    }

    printNode(root_node, "", true, "root");
  }

} // namespace par
