#pragma once

#include <MXLex/token.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mx {

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

    class ASTVisitor {
      public:
        virtual ~ASTVisitor() = default;
        virtual void visit(NumberNode &node) = 0;
        virtual void visit(BinaryNode &node) = 0;
        virtual void visit(UnaryNode &node) = 0;
        virtual void visit(AssignmentNode &node) = 0;
        virtual void visit(VariableNode &node) = 0;
        virtual void visit(LiteralNode &node);
        virtual void visit(ConditionalNode &node);
        virtual void visit(CallNode &node);
        virtual void visit(SubscriptNode &node);
        virtual void visit(MemberNode &node);
        virtual void visit(CastNode &node);
        virtual void visit(SizeofNode &node);
    };

    class ASTNode {
      public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor &visitor) = 0;
        virtual std::string to_string() const = 0;
    };

    class ExprNode : public ASTNode {
      public:
    };

    class NumberNode : public ExprNode {
      public:
        NumberNode(const Token &value) : value(value) {}
        Token value;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class UnaryNode : public ExprNode {
      public:
        UnaryNode(const Token &op, std::unique_ptr<ExprNode> value, bool postfix = false) : op(op), value(std::move(value)), postfix(postfix) {}
        Token op;
        std::unique_ptr<ExprNode> value;
        bool postfix = false;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class BinaryNode : public ExprNode {
      public:
        BinaryNode(std::unique_ptr<ExprNode> left, const Token &op, std::unique_ptr<ExprNode> right) : left(std::move(left)), op(op), right(std::move(right)) {}
        std::unique_ptr<ExprNode> left;
        Token op;
        std::unique_ptr<ExprNode> right;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class VariableNode : public ExprNode {
      public:
        VariableNode(const Token &type) : var(type) {}
        Token var;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return var.get_token(); }
    };

    class AssignmentNode : public ASTNode {
      public:
        AssignmentNode(const Token &var, std::unique_ptr<ExprNode> expression) : var(var), expression(std::move(expression)) {}
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return var.get_token() + "=" + expression->to_string(); }
        Token var;
        std::unique_ptr<ExprNode> expression;
    };

    class LiteralNode : public ExprNode {
      public:
        LiteralNode(const Token &value) : value(value) {}
        Token value;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override { return value.get_token(); }
    };

    class ConditionalNode : public ExprNode {
      public:
        ConditionalNode(std::unique_ptr<ExprNode> condition, std::unique_ptr<ExprNode> then_expr, std::unique_ptr<ExprNode> else_expr) : condition(std::move(condition)), then_expr(std::move(then_expr)), else_expr(std::move(else_expr)) {}
        std::unique_ptr<ExprNode> condition, then_expr, else_expr;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class CallNode : public ExprNode {
      public:
        CallNode(std::unique_ptr<ExprNode> callee, std::vector<std::unique_ptr<ExprNode>> arguments) : callee(std::move(callee)), arguments(std::move(arguments)) {}
        std::unique_ptr<ExprNode> callee;
        std::vector<std::unique_ptr<ExprNode>> arguments;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class SubscriptNode : public ExprNode {
      public:
        SubscriptNode(std::unique_ptr<ExprNode> base, std::unique_ptr<ExprNode> subscript) : base(std::move(base)), subscript(std::move(subscript)) {}
        std::unique_ptr<ExprNode> base, subscript;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class MemberNode : public ExprNode {
      public:
        MemberNode(std::unique_ptr<ExprNode> base, const Token &op, const Token &member) : base(std::move(base)), op(op), member(member) {}
        std::unique_ptr<ExprNode> base;
        Token op, member;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class CastNode : public ExprNode {
      public:
        CastNode(std::string type_name, std::unique_ptr<ExprNode> value) : type_name(std::move(type_name)), value(std::move(value)) {}
        std::string type_name;
        std::unique_ptr<ExprNode> value;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class SizeofNode : public ExprNode {
      public:
        SizeofNode(std::unique_ptr<ExprNode> value) : value(std::move(value)) {}
        SizeofNode(std::string type_name) : type_name(std::move(type_name)) {}
        std::unique_ptr<ExprNode> value;
        std::string type_name;
        void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
        std::string to_string() const override;
    };

    class Evaluate : public ASTVisitor {
      public:
        void visit(NumberNode &number) override;
        void visit(ConditionalNode &node) override;
        void visit(BinaryNode &bin_node) override;
        void visit(UnaryNode &unode) override;
        void visit(AssignmentNode &anode) override;
        void visit(VariableNode &vnode) override;
        std::int64_t result() const;
        void clear();

      private:
        std::vector<std::int64_t> values;
        std::unordered_map<std::string, std::int64_t> vars;
    };

    class ICode : public ASTVisitor {
      public:
        void visit(NumberNode &number) override;
        void visit(ConditionalNode &node) override;
        void visit(BinaryNode &bin_node) override;
        void visit(UnaryNode &unode) override;
        void visit(AssignmentNode &anode) override;
        void visit(VariableNode &var) override;
        std::string code();
        void write_to_stream(std::ostream &out);
        std::string get_temp();
        void clear();

      private:
        std::ostringstream stream;
        std::vector<std::string> values;
        std::int64_t temp = 0;
    };

    class HTMLCode : public ASTVisitor {
      public:
        void visit(NumberNode &number) override;
        void visit(ConditionalNode &node) override;
        void visit(BinaryNode &bin_node) override;
        void visit(UnaryNode &unode) override;
        void visit(AssignmentNode &anode) override;
        void visit(VariableNode &var) override;
        std::string code();
        void write_to_stream(std::ostream &out);
        std::string get_temp();
        void clear();

      private:
        std::ostringstream stream;
        std::vector<std::string> values;
        std::int64_t temp = 0;
    };

} // namespace mx
