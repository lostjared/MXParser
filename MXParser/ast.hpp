#pragma once

#include "MXParser/source.hpp"
#include "MXParser/type.hpp"
#include <MXLex/token.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace mx {

    class InitializerListNode;
    class NumberNode;
    class BinaryNode;
    class UnaryNode;
    class AssignmentNode;
    class VariableNode;

    class LiteralNode;
    class ConditionalNode;
    class CallNode;
    class SubscriptNode;
    class MemberNode;
    class CastNode;
    class SizeofNode;

    class ProgramNode;
    class FunctionNode;
    class BlockNode;
    class ReturnNode;
    class DeclarationNode;
    class IfNode;
    class WhileNode;
    class DoWhileNode;
    class SwitchNode;
    class CaseNode;
    class DefaultNode;
    class ForNode;
    class JumpNode;

    class ASTVisitor {
      public:
        virtual ~ASTVisitor() = default;
        virtual void visit(NumberNode &node) = 0;
        virtual void visit(BinaryNode &node) = 0;
        virtual void visit(UnaryNode &node) = 0;
        virtual void visit(AssignmentNode &node) = 0;
        virtual void visit(VariableNode &node) = 0;
        virtual void visit(InitializerListNode &node);
        virtual void visit(LiteralNode &node);
        virtual void visit(ConditionalNode &node);
        virtual void visit(CallNode &node);
        virtual void visit(SubscriptNode &node);
        virtual void visit(MemberNode &node);
        virtual void visit(CastNode &node);
        virtual void visit(SizeofNode &node);
        virtual void visit(ProgramNode &node);
        virtual void visit(FunctionNode &node);
        virtual void visit(BlockNode &node);
        virtual void visit(ReturnNode &node);
        virtual void visit(DeclarationNode &node);
        virtual void visit(IfNode &node);
        virtual void visit(WhileNode &node);
        virtual void visit(DoWhileNode &node);
        virtual void visit(SwitchNode &node);
        virtual void visit(CaseNode &node);
        virtual void visit(DefaultNode &node);
        virtual void visit(ForNode &node);
        virtual void visit(JumpNode &node);
    };

    class ASTNode {
      public:
        ASTNode(SourceLocation location = {}) : location(std::move(location)) {}
        virtual ~ASTNode() = default;
        SourceLocation location;
        virtual void accept(ASTVisitor &visitor) = 0;
        virtual std::string to_string() const = 0;
    };

    class ExprNode : public ASTNode {
      public:
        using ASTNode::ASTNode;
    };

    class InitializerListNode : public ExprNode {
      public:
        std::vector<std::unique_ptr<ExprNode>> elements;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class NumberNode : public ExprNode {
      public:
        NumberNode(const Token &value) : ExprNode(SourceLocation::from_token(value)), value(value) {}
        Token value;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class UnaryNode : public ExprNode {
      public:
        UnaryNode(const Token &op, std::unique_ptr<ExprNode> value, bool postfix = false) : ExprNode(postfix ? value->location : SourceLocation::from_token(op)), op(op), value(std::move(value)), postfix(postfix) {}
        Token op;
        std::unique_ptr<ExprNode> value;
        bool postfix = false;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class BinaryNode : public ExprNode {
      public:
        BinaryNode(std::unique_ptr<ExprNode> left, const Token &op, std::unique_ptr<ExprNode> right) : ExprNode(left->location), left(std::move(left)), op(op), right(std::move(right)) {}
        std::unique_ptr<ExprNode> left;
        Token op;
        std::unique_ptr<ExprNode> right;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class VariableNode : public ExprNode {
      public:
        VariableNode(const Token &type) : ExprNode(SourceLocation::from_token(type)), var(type) {}
        Token var;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return var.get_token(); }
    };

    class AssignmentNode : public ASTNode {
      public:
        AssignmentNode(const Token &var, std::unique_ptr<ExprNode> expression) : ASTNode(SourceLocation::from_token(var)), var(var), expression(std::move(expression)) {}
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return var.get_token() + "=" + expression->to_string(); }
        Token var;
        std::unique_ptr<ExprNode> expression;
    };

    class LiteralNode : public ExprNode {
      public:
        LiteralNode(const Token &value) : ExprNode(SourceLocation::from_token(value)), value(value) {}
        Token value;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return value.get_token(); }
    };

    class ConditionalNode : public ExprNode {
      public:
        ConditionalNode(std::unique_ptr<ExprNode> condition, std::unique_ptr<ExprNode> then_expr, std::unique_ptr<ExprNode> else_expr) : ExprNode(condition->location), condition(std::move(condition)), then_expr(std::move(then_expr)), else_expr(std::move(else_expr)) {}
        std::unique_ptr<ExprNode> condition, then_expr, else_expr;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class CallNode : public ExprNode {
      public:
        CallNode(std::unique_ptr<ExprNode> callee, std::vector<std::unique_ptr<ExprNode>> arguments) : ExprNode(callee->location), callee(std::move(callee)), arguments(std::move(arguments)) {}
        std::unique_ptr<ExprNode> callee;
        std::vector<std::unique_ptr<ExprNode>> arguments;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class SubscriptNode : public ExprNode {
      public:
        SubscriptNode(std::unique_ptr<ExprNode> base, std::unique_ptr<ExprNode> subscript) : ExprNode(base->location), base(std::move(base)), subscript(std::move(subscript)) {}
        std::unique_ptr<ExprNode> base, subscript;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class MemberNode : public ExprNode {
      public:
        MemberNode(std::unique_ptr<ExprNode> base, const Token &op, const Token &member) : ExprNode(base->location), base(std::move(base)), op(op), member(member) {}
        std::unique_ptr<ExprNode> base;
        Token op, member;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class CastNode : public ExprNode {
      public:
        CastNode(std::string type_name, std::unique_ptr<ExprNode> value) : CastNode(Type::from_name(type_name), std::move(value)) {}
        CastNode(TypePtr type, std::unique_ptr<ExprNode> value) : ExprNode(value->location), type_name(type->to_string()), type(std::move(type)), value(std::move(value)) {}
        TypePtr declared_type() const { return type ? type : Type::from_name(type_name); }
        std::string type_name;
        TypePtr type;
        std::unique_ptr<ExprNode> value;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class SizeofNode : public ExprNode {
      public:
        SizeofNode(std::unique_ptr<ExprNode> value) : ExprNode(value->location), value(std::move(value)) {}
        SizeofNode(std::string type_name) : SizeofNode(Type::from_name(type_name)) {}
        SizeofNode(TypePtr type) : type_name(type->to_string()), type(std::move(type)) {}
        TypePtr declared_type() const { return type ? type : Type::from_name(type_name); }
        std::unique_ptr<ExprNode> value;
        std::string type_name;
        TypePtr type;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    struct Parameter {
        TypePtr declared_type() const { return type ? type : Type::from_name(type_name); }
        std::string type_name;
        Token name;
        TypePtr type;
        SourceLocation location;
        TypePtr declaration_type;
    };

    class BlockNode : public ASTNode {
      public:
        std::vector<std::unique_ptr<ASTNode>> statements;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class FunctionNode : public ASTNode {
      public:
        std::string return_type;
        std::vector<std::string> storage;
        TypePtr type;
        TypePtr result_type() const { return type ? type->element : Type::from_name(return_type); }
        TypePtr signature_type() const {
            if (type)
                return type;
            std::vector<TypePtr> types;
            for (const auto &parameter : parameters)
                types.push_back(parameter.declared_type());
            return Type::function(result_type(), std::move(types));
        }
        Token name;
        std::vector<Parameter> parameters;
        std::unique_ptr<BlockNode> body;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class ProgramNode : public ASTNode {
      public:
        std::vector<std::unique_ptr<ASTNode>> declarations;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class ReturnNode : public ASTNode {
      public:
        std::unique_ptr<ExprNode> expression;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    struct Declarator {
        Token name;
        std::unique_ptr<ExprNode> initializer;
        TypePtr type;
        SourceLocation location;
        std::vector<Parameter> parameters;
    };

    class DeclarationNode : public ASTNode {
      public:
        TypePtr declared_type() const { return type ? type : Type::from_name(type_name); }
        std::string type_name;
        TypePtr type;
        bool is_typedef = false;
        std::vector<std::string> storage;
        std::vector<Declarator> declarators;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    struct Enumerator {
        Token name;
        std::unique_ptr<ExprNode> value;
    };

    struct AggregateDefinition {
        SourceLocation location;
        std::vector<std::unique_ptr<DeclarationNode>> members;
        std::vector<Enumerator> enumerators;
        std::string to_string(TagKind kind) const;
    };

    class IfNode : public ASTNode {
      public:
        std::unique_ptr<ExprNode> condition;
        std::unique_ptr<ASTNode> then_statement, else_statement;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class WhileNode : public ASTNode {
      public:
        std::unique_ptr<ExprNode> condition;
        std::unique_ptr<ASTNode> body;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class ForNode : public ASTNode {
      public:
        std::unique_ptr<ASTNode> initializer, body;
        std::unique_ptr<ExprNode> condition, update;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class DoWhileNode : public ASTNode {
      public:
        std::unique_ptr<ASTNode> body;
        std::unique_ptr<ExprNode> condition;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class SwitchNode : public ASTNode {
      public:
        std::unique_ptr<ExprNode> condition;
        std::unique_ptr<ASTNode> body;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class CaseNode : public ASTNode {
      public:
        std::unique_ptr<ExprNode> value;
        std::unique_ptr<ASTNode> statement;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class DefaultNode : public ASTNode {
      public:
        std::unique_ptr<ASTNode> statement;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class JumpNode : public ASTNode {
      public:
        bool is_break = true;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return is_break ? "break;" : "continue;"; }
    };

    /** Evaluated scalar: void, signed integer, or floating point. */
    using NumericValue = std::variant<std::monostate, std::int64_t, double>;

    class Evaluate : public ASTVisitor {
      public:
        void visit(NumberNode &node) override;
        void visit(LiteralNode &node) override;
        void visit(ConditionalNode &node) override;
        void visit(BinaryNode &node) override;
        void visit(UnaryNode &node) override;
        void visit(AssignmentNode &node) override;
        void visit(VariableNode &node) override;
        void visit(CastNode &node) override;
        void visit(CallNode &node) override;
        void visit(ProgramNode &node) override;
        void visit(FunctionNode &node) override;
        void visit(BlockNode &node) override;
        void visit(ReturnNode &node) override;
        void visit(DeclarationNode &node) override;
        void visit(IfNode &node) override;
        void visit(WhileNode &node) override;
        void visit(ForNode &node) override;
        void visit(JumpNode &node) override;
        long double result() const;
        NumericValue result_value() const;
        /** Call a function registered by visiting a ProgramNode; keep that AST alive. */
        void call(const std::string &name, const std::vector<NumericValue> &arguments = {});
        bool has_function(const std::string &name) const;
        /** Clear expression results while preserving variables and functions. */
        void clear();

      private:
        struct Binding {
            NumericValue value;
            TypePtr type;
        };
        using Scope = std::unordered_map<std::string, Binding>;
        struct Frame {
            std::vector<Scope> scopes;
        };
        std::vector<NumericValue> values;
        Scope globals;
        std::vector<Frame> frames;
        std::unordered_map<std::string, FunctionNode *> functions;
        Binding *lookup(const std::string &name);
        NumericValue evaluate(ExprNode &expression);
        void execute(ASTNode &statement);
        NumericValue invoke(const std::string &name, const std::vector<NumericValue> &arguments);
        void store(const std::string &name, NumericValue value);
        bool floating_expression(ExprNode &expression);
    };

    class ICode : public ASTVisitor {
      public:
        void visit(NumberNode &number) override;
        void visit(ConditionalNode &node) override;
        void visit(BinaryNode &bin_node) override;
        void visit(UnaryNode &unode) override;
        void visit(AssignmentNode &anode) override;
        void visit(VariableNode &var) override;
        void visit(LiteralNode &node) override;
        void visit(CastNode &node) override;
        void visit(CallNode &node) override;
        void visit(ProgramNode &node) override;
        void visit(FunctionNode &node) override;
        void visit(BlockNode &node) override;
        void visit(ReturnNode &node) override;
        void visit(DeclarationNode &node) override;
        void visit(IfNode &node) override;
        void visit(WhileNode &node) override;
        void visit(ForNode &node) override;
        void visit(JumpNode &node) override;
        std::string code();
        void write_to_stream(std::ostream &out);
        std::string get_temp();
        void clear();

      private:
        std::ostringstream stream;
        std::vector<std::string> values;
        std::int64_t temp = 0;
        std::vector<std::pair<std::string, std::string>> loop_labels;
        void emit_statement(ASTNode &node);
    };

    class HTMLCode : public ASTVisitor {
      public:
        void visit(NumberNode &node) override { generator.visit(node); }
        void visit(LiteralNode &node) override { generator.visit(node); }
        void visit(ConditionalNode &node) override { generator.visit(node); }
        void visit(BinaryNode &node) override { generator.visit(node); }
        void visit(UnaryNode &node) override { generator.visit(node); }
        void visit(AssignmentNode &node) override { generator.visit(node); }
        void visit(VariableNode &node) override { generator.visit(node); }
        void visit(CastNode &node) override { generator.visit(node); }
        void visit(CallNode &node) override { generator.visit(node); }
        void visit(ProgramNode &node) override { generator.visit(node); }
        void visit(FunctionNode &node) override { generator.visit(node); }
        void visit(BlockNode &node) override { generator.visit(node); }
        void visit(ReturnNode &node) override { generator.visit(node); }
        void visit(DeclarationNode &node) override { generator.visit(node); }
        void visit(IfNode &node) override { generator.visit(node); }
        void visit(WhileNode &node) override { generator.visit(node); }
        void visit(ForNode &node) override { generator.visit(node); }
        void visit(JumpNode &node) override { generator.visit(node); }
        std::string code();
        void write_to_stream(std::ostream &out);
        std::string get_temp() { return generator.get_temp(); }
        void clear() { generator.clear(); }

      private:
        ICode generator;
    };

} // namespace mx
