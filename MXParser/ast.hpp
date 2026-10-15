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

    class ASTVisitor {
      public:
        virtual ~ASTVisitor() = default;
        virtual void visit(NumberNode &node) = 0;
        virtual void visit(BinaryNode &node) = 0;
        virtual void visit(UnaryNode &node) = 0;
        virtual void visit(AssignmentNode &node) = 0;
        virtual void visit(VariableNode &node) = 0;
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
        UnaryNode(const Token &op, std::unique_ptr<ExprNode> value) : op(op), value(std::move(value)) {}
        Token op;
        std::unique_ptr<ExprNode> value;
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

    class Evaluate : public ASTVisitor {
      public:
        void visit(NumberNode &number) override;
        void visit(BinaryNode &bin_node) override;
        void visit(UnaryNode &unode) override;
        void visit(AssignmentNode &anode) override;
        void visit(VariableNode &vnode) override;
        int result() const;
        void clear();

      private:
        std::vector<std::int64_t> values;
        std::unordered_map<std::string, std::int64_t> vars;
    };

    class ICode : public ASTVisitor {
      public:
        void visit(NumberNode &number) override;
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
