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

    std::string UnaryNode::to_string() const {
        std::ostringstream stream;
        stream << op.get_token() << value->to_string();
        return stream.str();
    }


    void Evaluate::visit(NumberNode &number) {
        values.push_back(std::stoi(number.value.get_token()));
    }

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

    void Evaluate::visit(UnaryNode &unode) {
        unode.value->accept(*this);
        int num_value = values.back();
        values.pop_back();
        if(unode.op.get_token() == "-")
            values.push_back(-num_value);
        else if(unode.op.get_token() == "+")
            values.push_back(num_value);
        else
            throw std::runtime_error("Unexpected value as unary operator");
    }

    void Evaluate::visit(AssignmentNode &anode) {
        anode.expression->accept(*this);
        if(!values.empty()) {
            int val = values.back();
            vars[anode.var.get_token()] = val;
            values.pop_back();
            values.push_back(val);
        }
    }

    void Evaluate::visit(VariableNode &vnode) {
        auto value_found = vars.find(vnode.var.get_token());
        if(value_found == vars.end()) {
            throw std::runtime_error("Variable " + vnode.var.get_token() + " not found!");
        } else {
            values.push_back(vars[vnode.var.get_token()]);
        }
    }

    int Evaluate::result() const {
        if(values.empty()) {
            throw std::runtime_error("No values.\n");
        }
        return values.back();

    }

    void ICode::visit(VariableNode &vnode) {
        values.push_back(vnode.var.get_token());
    }

    void ICode::visit(AssignmentNode &anode) {
        anode.expression->accept(*this);
        if(!values.empty()) {
            stream << "SET " << anode.var.get_token() << ", " << values.back() << "\n";
            values.pop_back();
        }
    }
    void ICode::visit(NumberNode &number) {
        values.push_back(number.value.get_token());
    }
    void ICode::visit(BinaryNode &bin_node) {
        bin_node.left->accept(*this);
        bin_node.right->accept(*this);
        std::string right  = values.back();
        values.pop_back();
        std::string  left = values.back();
        values.pop_back();
        std::string result = get_temp();
        if (bin_node.op.get_token() == "+")
            stream << "ADD " << result << ", " <<  left << ", " << right  << "\n";
        else if (bin_node.op.get_token() == "-")
            stream << "SUB " << result << ", " << left << ", " << right << "\n";
        else if (bin_node.op.get_token() == "*")
            stream << "MUL " << result << ", " << left << ", " << right << "\n";
        else if (bin_node.op.get_token() == "/") {
            if (right == "0")
                throw std::runtime_error("Error divide by zero");
            stream << "DIV " << result << ", " << left << ", " <<  right << "\n";
        } else {
            throw std::runtime_error("Unknown binary operator. ");
        }
        values.push_back(result);
    }

    void ICode::visit(UnaryNode &unode) {
        unode.value->accept(*this);
        std::string value = values.back();
        values.pop_back();
        if(unode.op.get_token() == "-") {
            std::string temp_value = get_temp();
            stream << "NEG " << temp_value <<", " << value << "\n";
            values.push_back(temp_value);
        } else if(unode.op.get_token() == "+") {
            values.push_back(value);
        } else {
            throw std::runtime_error ("Unexpected unary opreator ");
        }
    }

    std::string ICode::get_temp() {
        return "temp_" +  std::to_string(temp++);
    }

    std::string ICode::code() {
        if(values.size() == 1 && stream.str().empty()) {
            std::string temp = get_temp();
            stream << "CONST " << temp << ", " << values.back() << "\n";
            values.back() = temp;
            return stream.str();
        }
        return stream.str();
    }

    // HTMLCode

    void HTMLCode::visit(VariableNode &vnode) {
        values.push_back(vnode.var.get_token());
    }

    void HTMLCode::visit(AssignmentNode &anode) {
        anode.expression->accept(*this);
        if(!values.empty()) {
            stream << "<b>SET</b> " << anode.var.get_token() << ", " << values.back() << "<br>\n";
            values.pop_back();
        }
    }
    void HTMLCode::visit(NumberNode &number) {
        values.push_back(number.value.get_token());
    }
    void HTMLCode::visit(BinaryNode &bin_node) {
        bin_node.left->accept(*this);
        bin_node.right->accept(*this);
        std::string right  = values.back();
        values.pop_back();
        std::string  left = values.back();
        values.pop_back();
        std::string result = get_temp();
        if (bin_node.op.get_token() == "+")
            stream << "<b>ADD</b> " << result << ", " <<  left << ", " << right  << "<br>\n";
        else if (bin_node.op.get_token() == "-")
            stream << "<b>SUB</b> " << result << ", " << left << ", " << right << "<br>\n";
        else if (bin_node.op.get_token() == "*")
            stream << "<b>MUL</b> " << result << ", " << left << ", " << right << "<br>\n";
        else if (bin_node.op.get_token() == "/") {
            if (right == "0")
                throw std::runtime_error("Error divide by zero");
            stream << "<b>DIV</b> " << result << ", " << left << ", " <<  right << "<br>\n";
        } else {
            throw std::runtime_error("Unknown binary operator. ");
        }
        values.push_back(result);
    }

    void HTMLCode::visit(UnaryNode &unode) {
        unode.value->accept(*this);
        std::string value = values.back();
        values.pop_back();
        if(unode.op.get_token() == "-") {
            std::string temp_value = get_temp();
            stream << "<b>NEG</b> " << temp_value <<", " << value << "<br>\n";
            values.push_back(temp_value);
        } else if(unode.op.get_token() == "+") {
            values.push_back(value);
        } else {
            throw std::runtime_error ("Unexpected unary opreator ");
        }
    }

    std::string HTMLCode::get_temp() {
        return "temp_" +  std::to_string(temp++);
    }

    std::string HTMLCode::code() {
        if(values.size() == 1 && stream.str().empty()) {
            std::string temp = get_temp();
            stream << "<b>CONST</b> " << temp << ", " << values.back() << "<br>\n";
            values.back() = temp;
            return stream.str();
        }
        return stream.str();
    }

} // namespace mx
