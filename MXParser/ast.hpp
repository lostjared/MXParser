#pragma once

#include <MXLex/token.hpp>
#include <iostream>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

namespace mx {

    class NumberNode;
    class BinaryNode;

    class ASTVisitor {
      public:
        virtual ~ASTVisitor() = default;
        virtual void visit(NumberNode &node) = 0;
        virtual void visit(BinaryNode &node) = 0;
    };

    class ASTNode {
      public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor &visitor) = 0;
        virtual std::string to_string() const = 0;
    };

    class ExprNode : public ASTNode {
      public:
        void accept(ASTVisitor &visitor) override {}
        std::string to_string() const override { return ""; }
    };

    class NumberNode : public ExprNode {
      public:
        NumberNode(const Token &value) : value(value) {}
        Token value;
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

    class Evaluate : public ASTVisitor {
      public:
        void visit(NumberNode &number) override { values.push_back(std::stoi(number.value.get_token())); }
        void visit(BinaryNode &bin_node) override {
            bin_node.left->accept(*this);
            bin_node.right->accept(*this);
            int right = values.back();

            values.pop_back();
            int left = values.back();
            values.pop_back();
            int result = 0;

            if (bin_node.op.get_token() == "+")
                result = left + right;
            else if (bin_node.op.get_token() == "-")
                result = left - right;
            else if (bin_node.op.get_token() == "*")
                result = left * right;
            else if (bin_node.op.get_token() == "/") {
                if (right == 0)
                    throw std::runtime_error("Error divide by zero");
                result = left / right;
            } else {
                throw std::runtime_error("Unknown binary operator. ");
            }
            values.push_back(result);
        }

        int result() const { return values.back(); }

      private:
        std::vector<int> values;
    };
} // namespace mx
