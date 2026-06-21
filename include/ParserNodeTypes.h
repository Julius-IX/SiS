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
    SUBSCRIPT,
    RETURN,
    JUMP, // break / continue same zero-data shape, kind tells them apart
    FOR,
    SELF, // this / super both are self-references, is_super tells them apart
    NEW_EXPR,
    CLASS_DECL,
  };

  // break vs continue
  enum class JumpKind : uint8_t { BREAK, CONTINUE };

  struct Node {
    const NodeType type; // NOLINT
    size_t line = 0;
    size_t column = 0;

    Node(const Node&) = default;
    Node(Node&&) = delete;
    Node&& operator=(const Node&) = delete;
    Node&& operator=(Node&&) = delete;

    explicit Node(NodeType type)
      : type(type) {}
    virtual ~Node() = default;
  };

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
    Unary(lex::TokenType operation, Node* operand)
      : Node(TYPE),
        operation(operation),
        operand(operand) {}
  };

  struct Binary final : Node {
    static constexpr NodeType TYPE = NodeType::BINARY;
    lex::TokenType operation;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    Binary(lex::TokenType operation, std::unique_ptr<Node> left, std::unique_ptr<Node> right)
      : Node(TYPE),
        operation(operation),
        left(std::move(left)),
        right(std::move(right)) {}
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
    std::unique_ptr<Node> else_branch; // nullptr if no else
    If(std::unique_ptr<Node> condition, std::unique_ptr<Node> then_branch, std::unique_ptr<Node> else_branch = nullptr)
      : Node(TYPE),
        condition(std::move(condition)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}
  };

  struct While final : Node {
    static constexpr NodeType TYPE = NodeType::WHILE;
    std::unique_ptr<Node> condition;
    std::unique_ptr<Node> body;
    While(std::unique_ptr<Node> condition, std::unique_ptr<Node> body)
      : Node(TYPE),
        condition(std::move(condition)),
        body(std::move(body)) {}
  };

  // pin name [= initializer]; also used for class field declarations
  struct VarDecl final : Node {
    static constexpr NodeType TYPE = NodeType::VAR_DECL;
    std::string name;
    std::unique_ptr<Node> initializer; // nullptr if no initializer
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

  // fn(x, y) { ... } body is always a Block
  struct FnLiteral final : Node {
    static constexpr NodeType TYPE = NodeType::FN_LITERAL;
    std::vector<std::string> params;
    std::unique_ptr<Node> body;
    FnLiteral(std::vector<std::string> params, std::unique_ptr<Node> body)
      : Node(TYPE),
        params(std::move(params)),
        body(std::move(body)) {}
  };

  // obj.field chains like a.b.c are nested MemberAccess nodes
  struct MemberAccess final : Node {
    static constexpr NodeType TYPE = NodeType::MEMBER_ACCESS;
    std::unique_ptr<Node> object;
    std::string field;
    MemberAccess(std::unique_ptr<Node> object, std::string field)
      : Node(TYPE),
        object(std::move(object)),
        field(std::move(field)) {}
  };

  // [1, 2, 3]
  struct ArrayLiteral final : Node {
    static constexpr NodeType TYPE = NodeType::ARRAY_LITERAL;
    std::vector<std::unique_ptr<Node>> elements;
    explicit ArrayLiteral(std::vector<std::unique_ptr<Node>> elements)
      : Node(TYPE),
        elements(std::move(elements)) {}
  };

  // obj[index]
  struct Subscript final : Node {
    static constexpr NodeType TYPE = NodeType::SUBSCRIPT;
    std::unique_ptr<Node> object;
    std::unique_ptr<Node> index;
    Subscript(std::unique_ptr<Node> object, std::unique_ptr<Node> index)
      : Node(TYPE),
        object(std::move(object)),
        index(std::move(index)) {}
  };

  // return [expr];
  struct Return final : Node {
    static constexpr NodeType TYPE = NodeType::RETURN;
    std::unique_ptr<Node> value; // nullptr means return null
    explicit Return(std::unique_ptr<Node> value = nullptr)
      : Node(TYPE),
        value(std::move(value)) {}
  };

  // break; / continue; same zero-data node, kind distinguishes them
  struct Jump final : Node {
    static constexpr NodeType TYPE = NodeType::JUMP;
    JumpKind kind;
    explicit Jump(JumpKind kind)
      : Node(TYPE),
        kind(kind) {}
  };

  // this / super bare self-reference, field access wraps this in MemberAccess
  struct Self final : Node {
    static constexpr NodeType TYPE = NodeType::SELF;
    bool is_super;
    explicit Self(bool is_super)
      : Node(TYPE),
        is_super(is_super) {}
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

  // class Name [extends Parent] { fields... methods... }
  // Fields and methods are split so the evaluator can run all field defaults
  // before any method is called (methods may reference sibling fields).
  struct ClassDecl final : Node {
    static constexpr NodeType TYPE = NodeType::CLASS_DECL;
    std::string name;
    std::string parent_name; // empty if no extends
    std::vector<std::unique_ptr<VarDecl>> fields;
    std::vector<std::unique_ptr<FnLiteral>> methods;
    std::vector<std::string> method_names; // parallel to methods
    ClassDecl(std::string name,
              std::string parent_name,
              std::vector<std::unique_ptr<VarDecl>> fields,
              std::vector<std::unique_ptr<FnLiteral>> methods,
              std::vector<std::string> method_names)
      : Node(TYPE),
        name(std::move(name)),
        parent_name(std::move(parent_name)),
        fields(std::move(fields)),
        methods(std::move(methods)),
        method_names(std::move(method_names)) {}
  };

  // for (init; condition; increment) body
  // Any of init/condition/increment may be null (for(;;) is valid).
  struct For final : Node {
    static constexpr NodeType TYPE = NodeType::FOR;
    std::unique_ptr<Node> init;      // VarDecl or ExprStmt, nullptr if omitted
    std::unique_ptr<Node> condition; // nullptr means "always true"
    std::unique_ptr<Node> increment; // nullptr if omitted
    std::unique_ptr<Node> body;
    For(std::unique_ptr<Node> init, std::unique_ptr<Node> condition, std::unique_ptr<Node> increment, std::unique_ptr<Node> body)
      : Node(TYPE),
        init(std::move(init)),
        condition(std::move(condition)),
        increment(std::move(increment)),
        body(std::move(body)) {}
  };
} // namespace par
