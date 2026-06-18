#pragma once
#include <Token.h>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace par {
  enum class NodeType : uint8_t {
    LITERAL,
    IDENTIFIER,
    UNARY,
    BINARY,
    BLOCK,
    IF,
    WHILE,
    VAR_DECL,
    EXPR_STMT,
    CALL,
  };

  struct Node {
    const NodeType type; // NOLINT

    Node(const Node&) = default;
    Node(Node&&) = delete;
    Node& operator=(const Node&) = delete;
    Node& operator=(Node&&) = delete;

    explicit Node(NodeType type)
      : type(type) {}
    virtual ~Node() = default;
  };

  // generic downcast helper relies on every derived struct exposing static constexpr TYPE
  template <typename T>
  T* as(Node* node) {
    if (node == nullptr || node->type != T::TYPE) return nullptr;
    return static_cast<T*>(node);
  }
  typedef std::variant<std::monostate, double, bool, std::string> LiteralType;

  struct Literal final : Node {
    static constexpr NodeType TYPE = NodeType::LITERAL;
    LiteralType value;

    explicit Literal(LiteralType value)
      : Node(TYPE),
        value(std::move(value)) {}
  };

  struct Identifier final : Node {
    static constexpr NodeType TYPE = NodeType::IDENTIFIER;
    std::string name;

    explicit Identifier(std::string name)
      : Node(TYPE),
        name(std::move(name)) {}
  };

  struct Unary final : Node {
    static constexpr NodeType TYPE = NodeType::UNARY;
    lex::TokenType operation;
    Node* operand;

    Unary(lex::TokenType operation, Node* operand)
      : Node(TYPE),
        operation(operation),
        operand(operand) {}
  };

  struct Binary final : Node {
    static constexpr NodeType TYPE = NodeType::BINARY;
    lex::TokenType operation;
    Node* left;
    Node* right;

    Binary(lex::TokenType operation, Node* left, Node* right)
      : Node(TYPE),
        operation(operation),
        left(left),
        right(right) {}
  };

  struct Block final : Node {
    static constexpr NodeType TYPE = NodeType::BLOCK;
    std::vector<Node*> statements;

    explicit Block(std::vector<Node*> statements)
      : Node(TYPE),
        statements(std::move(statements)) {}
  };

  struct If final : Node {
    static constexpr NodeType TYPE = NodeType::IF;
    Node* condition;
    Node* then_branch;
    Node* else_branch; // nullptr if there's no else

    If(Node* condition, Node* then_branch, Node* else_branch = nullptr)
      : Node(TYPE),
        condition(condition),
        then_branch(then_branch),
        else_branch(else_branch) {}
  };

  struct While final : Node {
    static constexpr NodeType TYPE = NodeType::WHILE;
    Node* condition;
    Node* body;

    While(Node* condition, Node* body)
      : Node(TYPE),
        condition(condition),
        body(body) {}
  };

  struct VarDecl final : Node {
    static constexpr NodeType TYPE = NodeType::VAR_DECL;
    std::string name;
    Node* initializer; // nullptr if declared without a value

    explicit VarDecl(std::string name, Node* initializer = nullptr)
      : Node(TYPE),
        name(std::move(name)),
        initializer(initializer) {}
  };

  struct ExprStmt final : Node {
    static constexpr NodeType TYPE = NodeType::EXPR_STMT;
    Node* expr;

    explicit ExprStmt(Node* expr)
      : Node(TYPE),
        expr(expr) {}
  };

  struct Call final : Node {
    static constexpr NodeType TYPE = NodeType::CALL;
    Node* callee;
    std::vector<Node*> args;

    Call(Node* callee, std::vector<Node*> args)
      : Node(TYPE),
        callee(callee),
        args(std::move(args)) {}
  };
} // namespace par
