#pragma once

#include <MXLex/token.hpp>
#include <iostream>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>
#include<sstream>

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
        void accept([[maybe_unused]] ASTVisitor &visitor) override {}
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
        void visit(NumberNode &number) override;
        void visit(BinaryNode &bin_node) override;
        int result() const;
      private:
        std::vector<int> values;
    };

    class ICode : public ASTVisitor {
    public:
      void visit(NumberNode &number) override;
      void visit(BinaryNode &bin_node) override;
      std::string code();
      std::string get_temp();
    private:
      std::ostringstream stream;
      std::vector<std::string> values;
      int temp = 0;
    };

} // namespace mx
