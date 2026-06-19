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
    FN_LITERAL,
    MEMBER_ACCESS,
    ARRAY_LITERAL,

    RETURN,
    BREAK,
    CONTINUE,

    CLASS_DECL,
    NEW_EXPR,
    THIS_EXPR,
    SUPER_ACCESS,
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
  // transfer ownership, it's for inline inspection/mutation, not for holding on to.
  //
  // WARN: T must be a concrete leaf type with its own TYPE constant, never call
  // as<Node>(...), that's the base type and has no TYPE of its own to compare against.
  template <typename T>
  T* as(Node* node) {
    if (node == nullptr || node->type != T::TYPE) return nullptr;
    return static_cast<T*>(node);
  }

  // const overload, same rules as above, just for `const Node*`.
  template <typename T>
  const T* as(const Node* node) {
    if (node == nullptr || node->type != T::TYPE) return nullptr;
    return static_cast<const T*>(node);
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

  // pin name [= initializer];  -- also reused for class field declarations,
  // where `initializer` becomes the default value applied before the
  // constructor runs (nullptr means "default to null").
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

  // Represents a function literal, e.g. fn(x, y) { return x + y; }
  // Params are plain names for now, body is always a Block.
  struct FnLiteral final : Node {
    static constexpr NodeType TYPE = NodeType::FN_LITERAL;
    std::vector<std::string> params;
    std::unique_ptr<Node> body; // a Block

    FnLiteral(std::vector<std::string> params, std::unique_ptr<Node> body)
      : Node(TYPE),
        params(std::move(params)),
        body(std::move(body)) {}
  };

  // Represents a single '.' or '->' access, e.g. the "something" part of
  // ident.something or this->something. `via_arrow` records which token was
  // used, the evaluator/parser enforce that '->' is only legal when `object`
  // is a ThisExpr or SuperAccess root (this->x->y is fine because by the
  // time you've built the outer node `object` is a MemberAccess, not a raw
  // SuperAccess, so the rule only applies to the immediate `this`/`super`).
  // Chains like a.b.c are built as nested MemberAccess nodes by the parser loop.
  struct MemberAccess final : Node {
    static constexpr NodeType TYPE = NodeType::MEMBER_ACCESS;
    std::unique_ptr<Node> object;
    std::string field;

    MemberAccess(std::unique_ptr<Node> object, std::string field)
      : Node(TYPE),
        object(std::move(object)),
        field(std::move(field)) {}
  };

  // Represents an array literal, e.g. [1, 2, 3]
  struct ArrayLiteral final : Node {
    static constexpr NodeType TYPE = NodeType::ARRAY_LITERAL;
    std::vector<std::unique_ptr<Node>> elements;

    explicit ArrayLiteral(std::vector<std::unique_ptr<Node>> elements)
      : Node(TYPE),
        elements(std::move(elements)) {}
  };

  // return [expr];
  struct Return final : Node {
    static constexpr NodeType TYPE = NodeType::RETURN;
    std::unique_ptr<Node> value; // nullptr means "return null"

    explicit Return(std::unique_ptr<Node> value = nullptr)
      : Node(TYPE),
        value(std::move(value)) {}
  };

  // break;
  struct Break final : Node {
    static constexpr NodeType TYPE = NodeType::BREAK;
    Break()
      : Node(TYPE) {}
  };

  // continue;
  struct Continue final : Node {
    static constexpr NodeType TYPE = NodeType::CONTINUE;
    Continue()
      : Node(TYPE) {}
  };

  // this
  struct ThisExpr final : Node {
    static constexpr NodeType TYPE = NodeType::THIS_EXPR;
    ThisExpr()
      : Node(TYPE) {}
  };

  // this->field or super->field (and this->method()/super->method() once
  // wrapped in a Call, same as MemberAccess + Call). `is_super` distinguishes
  // "look this up starting from my own class" (this) from "look this up
  // starting from my parent class, but keep `this` bound to me" (super).
  struct SuperAccess final : Node {
    static constexpr NodeType TYPE = NodeType::SUPER_ACCESS;
    bool is_super;
    std::string field;

    SuperAccess(bool is_super, std::string field)
      : Node(TYPE),
        is_super(is_super),
        field(std::move(field)) {}
  };

  // new ClassName(args)
  struct NewExpr final : Node {
    static constexpr NodeType TYPE = NodeType::NEW_EXPR;
    std::string class_name;
    std::vector<std::unique_ptr<Node>> args;

    NewExpr(std::string class_name, std::vector<std::unique_ptr<Node>> args)
      : Node(TYPE),
        class_name(std::move(class_name)),
        args(std::move(args)) {}
  };

  // class Name [extends Parent] { pin field [= init]; ... fn method(...) {...} ... }
  // Fields and methods are split into separate vectors rather than kept as one
  // mixed statement list because the evaluator needs to process all field
  // defaults before any method becomes callable (methods may reference
  // sibling fields, but field initializers run in constructor order, not
  // method-definition order).
  struct ClassDecl final : Node {
    static constexpr NodeType TYPE = NodeType::CLASS_DECL;
    std::string name;
    std::string parent_name; // empty if no `extends`
    std::vector<std::unique_ptr<VarDecl>> fields;
    std::vector<std::unique_ptr<FnLiteral>> methods;
    std::vector<std::string> method_names; // parallel to `methods`

    ClassDecl(std::string name, std::string parent_name, std::vector<std::unique_ptr<VarDecl>> fields, std::vector<std::unique_ptr<FnLiteral>> methods,
        std::vector<std::string> method_names)
      : Node(TYPE),
        name(std::move(name)),
        parent_name(std::move(parent_name)),
        fields(std::move(fields)),
        methods(std::move(methods)),
        method_names(std::move(method_names)) {}
  };
} // namespace par
