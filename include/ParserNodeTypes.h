#pragma once

#include <Token.h>
#include <cstdint>
#include <memory>
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

  // Downcasts Node* to a concrete derived type (e.g. as<Binary>(node)) so you can
  // read/modify its typed fields in place. Returns nullptr if node is null or its
  // ->type doesn't match T::TYPE, so you can use the result directly in an `if`.
  // Relies on every derived struct exposing `static constexpr NodeType TYPE`.
  //
  // Non-owning: the returned pointer is only valid as long as the Node it points
  // to is alive. Don't store it beyond that node's lifetime, and don't use it to
  // transfer ownership — it's for inline inspection/mutation, not for holding on to.
  //
  // WARN: T must be a concrete leaf type with its own TYPE constant never call
  // as<Node>(...), that's the base type and has no TYPE of its own to compare against.
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
    std::unique_ptr<Node> operand;

    explicit Unary(lex::TokenType operation, Node* operand)
      : Node(TYPE),
        operation(operation),
        operand(operand) {}
  };

  struct Binary final : Node {
    static constexpr NodeType TYPE = NodeType::BINARY;
    lex::TokenType operation;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    // clang-format off
    explicit Binary(
      lex::TokenType operation,
      std::unique_ptr<Node> left,
      std::unique_ptr<Node> right
    )
      : Node(TYPE),
        operation(operation),
        left(std::move(left)),
        right(std::move(right)) {}
    // clang-format on
  };

  struct Block final : Node {
    static constexpr NodeType TYPE = NodeType::BLOCK;
    std::vector<std::unique_ptr<Node>> statements;

    explicit Block(std::vector<std::unique_ptr<Node>> statements)
      : Node(TYPE),
        statements(std::move(statements)) {}
  };

  struct If final : Node {
    static constexpr NodeType TYPE = NodeType::IF;
    std::unique_ptr<Node> condition;
    std::unique_ptr<Node> then_branch;
    std::unique_ptr<Node> else_branch; // nullptr if there's no else

    // clang-format off
    explicit If(
      std::unique_ptr<Node> condition,
      std::unique_ptr<Node> then_branch,
      std::unique_ptr<Node> else_branch = nullptr
    )
      : Node(TYPE),
        condition(std::move(condition)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}
    // clang-format on
  };

  struct While final : Node {
    static constexpr NodeType TYPE = NodeType::WHILE;
    std::unique_ptr<Node> condition;
    std::unique_ptr<Node> body;

    explicit While(std::unique_ptr<Node> condition, std::unique_ptr<Node> body)
      : Node(TYPE),
        condition(std::move(condition)),
        body(std::move(body)) {}
  };

  struct VarDecl final : Node {
    static constexpr NodeType TYPE = NodeType::VAR_DECL;
    std::string name;
    std::unique_ptr<Node> initializer; // nullptr if declared without a value

    explicit VarDecl(std::string name, std::unique_ptr<Node> initializer = nullptr)
      : Node(TYPE),
        name(std::move(name)),
        initializer(std::move(initializer)) {}
  };

  struct ExprStmt final : Node {
    static constexpr NodeType TYPE = NodeType::EXPR_STMT;
    std::unique_ptr<Node> expr;

    explicit ExprStmt(std::unique_ptr<Node> expr)
      : Node(TYPE),
        expr(std::move(expr)) {}
  };

  struct Call final : Node {
    static constexpr NodeType TYPE = NodeType::CALL;
    std::unique_ptr<Node> callee;
    std::vector<std::unique_ptr<Node>> args;

    Call(std::unique_ptr<Node> callee, std::vector<std::unique_ptr<Node>> args)
      : Node(TYPE),
        callee(std::move(callee)),
        args(std::move(args)) {}
  };
} // namespace par
