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
        if (postfix)
            stream << "(" << value->to_string() << ")" << op.get_token();
        else
            stream << op.get_token() << "(" << value->to_string() << ")";
        return stream.str();
    }

    void ASTVisitor::visit(LiteralNode &) { throw std::runtime_error("Visitor does not support C literals"); }
    void ASTVisitor::visit(ConditionalNode &) { throw std::runtime_error("Visitor does not support conditional expressions"); }
    void ASTVisitor::visit(CallNode &) { throw std::runtime_error("Visitor does not support function calls"); }
    void ASTVisitor::visit(SubscriptNode &) { throw std::runtime_error("Visitor does not support subscripts"); }
    void ASTVisitor::visit(MemberNode &) { throw std::runtime_error("Visitor does not support member access"); }
    void ASTVisitor::visit(CastNode &) { throw std::runtime_error("Visitor does not support casts"); }
    void ASTVisitor::visit(SizeofNode &) { throw std::runtime_error("Visitor does not support sizeof"); }

    std::string ConditionalNode::to_string() const { return "(" + condition->to_string() + "?" + then_expr->to_string() + ":" + else_expr->to_string() + ")"; }
    std::string CallNode::to_string() const {
        std::string result = "(" + callee->to_string() + ")(";
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i)
                result += ",";
            result += arguments[i]->to_string();
        }
        return result + ")";
    }
    std::string SubscriptNode::to_string() const { return "(" + base->to_string() + ")[" + subscript->to_string() + "]"; }
    std::string MemberNode::to_string() const { return "(" + base->to_string() + ")" + op.get_token() + member.get_token(); }
    std::string CastNode::to_string() const { return "(" + type_name + ")(" + value->to_string() + ")"; }
    std::string SizeofNode::to_string() const { return "sizeof(" + (value ? value->to_string() : type_name) + ")"; }

    void Evaluate::visit(NumberNode &number) {
        const auto &text = number.value.get_token();
        if (text.starts_with("0b") || text.starts_with("0B"))
            values.push_back(std::stoll(text.substr(2), nullptr, 2));
        else
            values.push_back(std::stoll(text, nullptr, 0));
    }

    namespace {
        std::int64_t binary_value(const std::string &op, std::int64_t left, std::int64_t right) {
            if (op == "+")
                return left + right;
            if (op == "-")
                return left - right;
            if (op == "*")
                return left * right;
            if (op == "/" || op == "%") {
                if (right == 0)
                    throw std::runtime_error("Error divide by zero");
                if (left == INT64_MIN && right == -1)
                    throw std::runtime_error("Integer division overflow");
                return op == "/" ? left / right : left % right;
            }
            if (op == "<<" || op == ">>") {
                if (right < 0 || right >= 64)
                    throw std::runtime_error("Invalid shift count");
                return op == "<<" ? left << right : left >> right;
            }
            if (op == "<")
                return left < right;
            if (op == "<=")
                return left <= right;
            if (op == ">")
                return left > right;
            if (op == ">=")
                return left >= right;
            if (op == "==")
                return left == right;
            if (op == "!=")
                return left != right;
            if (op == "&")
                return left & right;
            if (op == "^")
                return left ^ right;
            if (op == "|")
                return left | right;
            if (op == "&&")
                return left && right;
            if (op == "||")
                return left || right;
            throw std::runtime_error("Unsupported binary operator: " + op);
        }

        bool is_assignment(const std::string &op) { return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" || op == "|="; }

        std::string instruction(const std::string &op) {
            static const std::unordered_map<std::string, std::string> names = {{"+", "ADD"}, {"-", "SUB"}, {"*", "MUL"}, {"/", "DIV"}, {"%", "MOD"}, {"<<", "SHL"}, {">>", "SHR"}, {"<", "LT"}, {"<=", "LE"}, {">", "GT"}, {">=", "GE"}, {"==", "EQ"}, {"!=", "NE"}, {"&", "AND"}, {"^", "XOR"}, {"|", "OR"}};
            auto found = names.find(op);
            if (found == names.end())
                throw std::runtime_error("Code generation does not support operator: " + op);
            return found->second;
        }
    } // namespace

    void Evaluate::visit(BinaryNode &node) {
        const auto &op = node.op.get_token();
        if (is_assignment(op)) {
            auto *variable = dynamic_cast<VariableNode *>(node.left.get());
            if (!variable)
                throw std::runtime_error("Evaluator requires a variable assignment target");
            std::int64_t left = 0;
            if (op != "=") {
                variable->accept(*this);
                left = values.back();
                values.pop_back();
            }
            node.right->accept(*this);
            auto value = values.back();
            if (op != "=")
                value = binary_value(op.substr(0, op.size() - 1), left, value);
            vars[variable->var.get_token()] = value;
            values.back() = value;
            return;
        }
        node.left->accept(*this);
        auto left = values.back();
        values.pop_back();
        if ((op == "&&" && !left) || (op == "||" && left)) {
            values.push_back(op == "||");
            return;
        }
        node.right->accept(*this);
        if (op != ",")
            values.back() = binary_value(op, left, values.back());
    }

    void Evaluate::visit(UnaryNode &node) {
        const auto &op = node.op.get_token();
        if (op == "++" || op == "--") {
            auto *variable = dynamic_cast<VariableNode *>(node.value.get());
            if (!variable)
                throw std::runtime_error("Evaluator requires a variable increment target");
            variable->accept(*this);
            auto old_value = values.back();
            auto value = old_value + (op == "++" ? 1 : -1);
            vars[variable->var.get_token()] = value;
            values.back() = node.postfix ? old_value : value;
            return;
        }
        node.value->accept(*this);
        if (op == "-")
            values.back() = -values.back();
        else if (op == "!")
            values.back() = !values.back();
        else if (op == "~")
            values.back() = ~values.back();
        else if (op != "+")
            throw std::runtime_error("Evaluator does not support unary operator: " + op);
    }

    void Evaluate::visit(ConditionalNode &node) {
        node.condition->accept(*this);
        auto condition = values.back();
        values.pop_back();
        (condition ? node.then_expr : node.else_expr)->accept(*this);
    }

    void Evaluate::visit(AssignmentNode &anode) {
        anode.expression->accept(*this);
        if (!values.empty()) {
            auto val = values.back();
            vars[anode.var.get_token()] = val;
            values.pop_back();
            values.push_back(val);
        }
    }

    void Evaluate::visit(VariableNode &vnode) {
        auto value_found = vars.find(vnode.var.get_token());
        if (value_found == vars.end()) {
            throw std::runtime_error("Variable " + vnode.var.get_token() + " not found!");
        } else {
            values.push_back(vars[vnode.var.get_token()]);
        }
    }

    std::int64_t Evaluate::result() const {
        if (values.empty()) {
            throw std::runtime_error("No values.\n");
        }
        return values.back();
    }

    void Evaluate::clear() { values.clear(); }

    void ICode::clear() {
        values.clear();
        stream.str("");
        stream.clear();
    }

    void ICode::visit(VariableNode &vnode) { values.push_back(vnode.var.get_token()); }

    void ICode::visit(AssignmentNode &anode) {
        anode.expression->accept(*this);
        if (!values.empty()) {
            stream << "SET " << anode.var.get_token() << ", " << values.back() << "\n";
            values.pop_back();
        }
    }
    void ICode::visit(NumberNode &number) { values.push_back(number.value.get_token()); }
    void ICode::visit(BinaryNode &node) {
        const auto &op = node.op.get_token();
        if (is_assignment(op)) {
            auto *variable = dynamic_cast<VariableNode *>(node.left.get());
            if (!variable)
                throw std::runtime_error("Code generation requires a variable assignment target");
            node.right->accept(*this);
            std::string value = values.back();
            if (op != "=") {
                auto temp = get_temp();
                stream << instruction(op.substr(0, op.size() - 1)) << " " << temp << ", " << variable->var.get_token() << ", " << value << "\n";
                value = temp;
            }
            stream << "SET " << variable->var.get_token() << ", " << value << "\n";
            values.back() = value;
            return;
        }
        node.left->accept(*this);
        std::string left = values.back();
        values.pop_back();
        if (op == "&&" || op == "||") {
            auto result = get_temp();
            auto done = get_temp();
            stream << "CONST " << result << ", " << (op == "||" ? "1" : "0") << "\n";
            stream << (op == "||" ? "JNZ" : "JZ") << " " << left << ", " << done << "\n";
            node.right->accept(*this);
            std::string right = values.back();
            values.pop_back();
            stream << "BOOL " << result << ", " << right << "\n";
            stream << "LABEL " << done << "\n";
            values.push_back(result);
            return;
        }
        node.right->accept(*this);
        if (op == ",")
            return;
        std::string right = values.back();
        values.pop_back();
        auto result = get_temp();
        stream << instruction(op) << " " << result << ", " << left << ", " << right << "\n";
        values.push_back(result);
    }

    void ICode::visit(ConditionalNode &node) {
        node.condition->accept(*this);
        auto condition = values.back();
        values.pop_back();
        auto result = get_temp(), otherwise = get_temp(), done = get_temp();
        stream << "JZ " << condition << ", " << otherwise << "\n";
        node.then_expr->accept(*this);
        stream << "SET " << result << ", " << values.back() << "\n";
        values.pop_back();
        stream << "JMP " << done << "\n";
        stream << "LABEL " << otherwise << "\n";
        node.else_expr->accept(*this);
        stream << "SET " << result << ", " << values.back() << "\n";
        values.pop_back();
        stream << "LABEL " << done << "\n";
        values.push_back(result);
    }

    void ICode::visit(UnaryNode &node) {
        const auto &op = node.op.get_token();
        if (op == "++" || op == "--") {
            auto *variable = dynamic_cast<VariableNode *>(node.value.get());
            if (!variable)
                throw std::runtime_error("Code generation requires a variable increment target");
            auto name = variable->var.get_token();
            auto before = get_temp(), after = get_temp();
            stream << "SET " << before << ", " << name << "\n";
            stream << (op == "++" ? "ADD" : "SUB") << " " << after << ", " << before << ", 1" << "\n";
            stream << "SET " << name << ", " << after << "\n";
            values.push_back(node.postfix ? before : after);
            return;
        }
        node.value->accept(*this);
        if (op == "+")
            return;
        auto value = values.back();
        values.pop_back();
        auto result = get_temp();
        std::string name;
        if (op == "-")
            name = "NEG";
        else if (op == "!")
            name = "NOT";
        else if (op == "~")
            name = "BNOT";
        else
            throw std::runtime_error("Code generation does not support unary operator: " + op);
        stream << name << " " << result << ", " << value << "\n";
        values.push_back(result);
    }

    std::string ICode::get_temp() { return "temp_" + std::to_string(temp++); }

    std::string ICode::code() {
        if (values.size() == 1 && stream.str().empty()) {
            std::string temp = get_temp();
            stream << "CONST " << temp << ", " << values.back() << "\n";
            values.back() = temp;
            return stream.str();
        }
        return stream.str();
    }

    // HTMLCode

    void HTMLCode::clear() {
        values.clear();
        stream.str("");
        stream.clear();
    }

    void HTMLCode::visit(VariableNode &vnode) { values.push_back(vnode.var.get_token()); }

    void HTMLCode::visit(AssignmentNode &anode) {
        anode.expression->accept(*this);
        if (!values.empty()) {
            stream << "<b>SET</b> " << anode.var.get_token() << ", " << values.back() << "<br>\n";
            values.pop_back();
        }
    }
    void HTMLCode::visit(NumberNode &number) { values.push_back(number.value.get_token()); }
    void HTMLCode::visit(BinaryNode &node) {
        const auto &op = node.op.get_token();
        if (is_assignment(op)) {
            auto *variable = dynamic_cast<VariableNode *>(node.left.get());
            if (!variable)
                throw std::runtime_error("Code generation requires a variable assignment target");
            node.right->accept(*this);
            std::string value = values.back();
            if (op != "=") {
                auto temp = get_temp();
                stream << "<b>" << instruction(op.substr(0, op.size() - 1)) << "</b> " << temp << ", " << variable->var.get_token() << ", " << value << "<br>\n";
                value = temp;
            }
            stream << "<b>SET</b> " << variable->var.get_token() << ", " << value << "<br>\n";
            values.back() = value;
            return;
        }
        node.left->accept(*this);
        std::string left = values.back();
        values.pop_back();
        if (op == "&&" || op == "||") {
            auto result = get_temp();
            auto done = get_temp();
            stream << "<b>CONST</b> " << result << ", " << (op == "||" ? "1" : "0") << "<br>\n";
            stream << "<b>" << (op == "||" ? "JNZ" : "JZ") << "</b> " << left << ", " << done << "<br>\n";
            node.right->accept(*this);
            std::string right = values.back();
            values.pop_back();
            stream << "<b>BOOL</b> " << result << ", " << right << "<br>\n";
            stream << "<b>LABEL</b> " << done << "<br>\n";
            values.push_back(result);
            return;
        }
        node.right->accept(*this);
        if (op == ",")
            return;
        std::string right = values.back();
        values.pop_back();
        auto result = get_temp();
        stream << "<b>" << instruction(op) << "</b> " << result << ", " << left << ", " << right << "<br>\n";
        values.push_back(result);
    }

    void HTMLCode::visit(ConditionalNode &node) {
        node.condition->accept(*this);
        auto condition = values.back();
        values.pop_back();
        auto result = get_temp(), otherwise = get_temp(), done = get_temp();
        stream << "<b>JZ</b> " << condition << ", " << otherwise << "<br>\n";
        node.then_expr->accept(*this);
        stream << "<b>SET</b> " << result << ", " << values.back() << "<br>\n";
        values.pop_back();
        stream << "<b>JMP</b> " << done << "<br>\n";
        stream << "<b>LABEL</b> " << otherwise << "<br>\n";
        node.else_expr->accept(*this);
        stream << "<b>SET</b> " << result << ", " << values.back() << "<br>\n";
        values.pop_back();
        stream << "<b>LABEL</b> " << done << "<br>\n";
        values.push_back(result);
    }

    void HTMLCode::visit(UnaryNode &node) {
        const auto &op = node.op.get_token();
        if (op == "++" || op == "--") {
            auto *variable = dynamic_cast<VariableNode *>(node.value.get());
            if (!variable)
                throw std::runtime_error("Code generation requires a variable increment target");
            auto name = variable->var.get_token();
            auto before = get_temp(), after = get_temp();
            stream << "<b>SET</b> " << before << ", " << name << "<br>\n";
            stream << "<b>" << (op == "++" ? "ADD" : "SUB") << "</b> " << after << ", " << before << ", 1" << "<br>\n";
            stream << "<b>SET</b> " << name << ", " << after << "<br>\n";
            values.push_back(node.postfix ? before : after);
            return;
        }
        node.value->accept(*this);
        if (op == "+")
            return;
        auto value = values.back();
        values.pop_back();
        auto result = get_temp();
        std::string name;
        if (op == "-")
            name = "NEG";
        else if (op == "!")
            name = "NOT";
        else if (op == "~")
            name = "BNOT";
        else
            throw std::runtime_error("Code generation does not support unary operator: " + op);
        stream << "<b>" << name << "</b> " << result << ", " << value << "<br>\n";
        values.push_back(result);
    }

    std::string HTMLCode::get_temp() { return "temp_" + std::to_string(temp++); }

    std::string HTMLCode::code() {
        if (values.size() == 1 && stream.str().empty()) {
            std::string temp = get_temp();
            stream << "<b>CONST</b> " << temp << ", " << values.back() << "<br>\n";
            values.back() = temp;
            return stream.str();
        }
        return stream.str();
    }

} // namespace mx
