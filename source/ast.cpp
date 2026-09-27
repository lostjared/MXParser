#include "MXParser/ast.hpp"
#include <sstream>

namespace mx {

    std::string NumberNode::to_string() const {
        std::ostringstream stream;
        stream << value.get_token();
        return stream.str();
    }

    std::string BinaryNode::to_string() const {
        std::ostringstream stream;
        stream << "(";
        stream << left->to_string();
        stream << op.get_token();
        stream << right->to_string();
        stream << ")";
        return stream.str();
    }

    void Evaluate::visit(NumberNode &number) { values.push_back(std::stoi(number.value.get_token())); }

    void Evaluate::visit(BinaryNode &bin_node) {
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

        int Evaluate::result() const { return values.back(); }


} // namespace mx
