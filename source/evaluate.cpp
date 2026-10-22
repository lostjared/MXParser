#include "MXParser/ast.hpp"
#include <cmath>
#include <limits>
#include <sstream>

namespace mx {
    namespace {
        struct Returned {
            NumericValue value;
        };
        struct Jumped {
            bool is_break;
        };

        bool integral(const NumericValue &value) { return std::holds_alternative<std::int64_t>(value); }
        std::int64_t integer(const NumericValue &value) {
            if (!integral(value))
                throw std::runtime_error("Operator requires integer operands");
            return std::get<std::int64_t>(value);
        }
        long double number(const NumericValue &value) {
            if (integral(value))
                return std::get<std::int64_t>(value);
            if (auto *floating = std::get_if<double>(&value))
                return *floating;
            throw std::runtime_error("A void value cannot be used as a number");
        }
        bool truth(const NumericValue &value) { return number(value) != 0; }
        NumericValue boolean(bool value) { return std::int64_t(value); }

        enum class Scalar { Void, Integer, Float, Double };
        Scalar scalar_type(const TypePtr &type) {
            if (!type)
                throw std::runtime_error("Missing evaluator type");
            const auto &base = type->unqualified();
            if (base.kind != TypeKind::Scalar || type->qualifiers.is_atomic)
                throw std::runtime_error("Evaluator does not support type: " + type->to_string());
            switch (base.scalar) {
            case ScalarKind::Void:
                return Scalar::Void;
            case ScalarKind::Float:
                return Scalar::Float;
            case ScalarKind::Double:
            case ScalarKind::LongDouble:
                return Scalar::Double;
            default:
                return Scalar::Integer;
            }
        }
        NumericValue convert(const NumericValue &value, const TypePtr &type) {
            auto kind = scalar_type(type);
            if (kind == Scalar::Void)
                return std::monostate{};
            auto numeric = number(value);
            if (kind == Scalar::Float)
                return static_cast<double>(static_cast<float>(numeric));
            if (kind == Scalar::Double)
                return static_cast<double>(numeric);
            if (type->unqualified().scalar == ScalarKind::Bool)
                return boolean(truth(value));
            if (integral(value))
                return value;
            if (!std::isfinite(numeric) || numeric < -9223372036854775808.0L || numeric >= 9223372036854775808.0L)
                throw std::runtime_error("Floating-to-integer conversion out of range");
            return static_cast<std::int64_t>(numeric);
        }
        NumericValue binary_value(const std::string &op, const NumericValue &left, const NumericValue &right) {
            if (op == "&&")
                return boolean(truth(left) && truth(right));
            if (op == "||")
                return boolean(truth(left) || truth(right));
            if (op == "%" || op == "<<" || op == ">>" || op == "&" || op == "^" || op == "|") {
                auto lhs = integer(left), rhs = integer(right);
                if (op == "%") {
                    if (!rhs)
                        throw std::runtime_error("Error divide by zero");
                    if (lhs == INT64_MIN && rhs == -1)
                        throw std::runtime_error("Integer remainder overflow");
                    return lhs % rhs;
                }
                if (op == "&")
                    return lhs & rhs;
                if (op == "^")
                    return lhs ^ rhs;
                if (op == "|")
                    return lhs | rhs;
                if (rhs < 0 || rhs >= 64)
                    throw std::runtime_error("Invalid shift count");
                return op == "<<" ? lhs << rhs : lhs >> rhs;
            }
            // Integer comparisons retain precision; mixed comparisons use double promotion.
            auto compare = [&](auto lhs, auto rhs) -> NumericValue {
                if (op == "<")
                    return boolean(lhs < rhs);
                if (op == "<=")
                    return boolean(lhs <= rhs);
                if (op == ">")
                    return boolean(lhs > rhs);
                if (op == ">=")
                    return boolean(lhs >= rhs);
                if (op == "==")
                    return boolean(lhs == rhs);
                return boolean(lhs != rhs);
            };
            if (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=") {
                if (integral(left) && integral(right))
                    return compare(integer(left), integer(right));
                return compare(static_cast<double>(number(left)), static_cast<double>(number(right)));
            }
            if (integral(left) && integral(right)) {
                auto lhs = integer(left), rhs = integer(right);
                std::int64_t result = 0;
                bool overflow = false;
                if (op == "+")
                    overflow = __builtin_add_overflow(lhs, rhs, &result);
                else if (op == "-")
                    overflow = __builtin_sub_overflow(lhs, rhs, &result);
                else if (op == "*")
                    overflow = __builtin_mul_overflow(lhs, rhs, &result);
                else if (op == "/") {
                    if (!rhs)
                        throw std::runtime_error("Error divide by zero");
                    if (lhs == INT64_MIN && rhs == -1)
                        throw std::runtime_error("Integer division overflow");
                    result = lhs / rhs;
                } else
                    throw std::runtime_error("Unsupported binary operator: " + op);
                if (overflow)
                    throw std::runtime_error("Integer arithmetic overflow");
                return result;
            }
            double lhs = static_cast<double>(number(left)), rhs = static_cast<double>(number(right));
            if (op == "+")
                return lhs + rhs;
            if (op == "-")
                return lhs - rhs;
            if (op == "*")
                return lhs * rhs;
            if (op == "/") {
                if (rhs == 0)
                    throw std::runtime_error("Error divide by zero");
                return lhs / rhs;
            }
            throw std::runtime_error("Unsupported binary operator: " + op);
        }
        bool assignment_operator(const std::string &op) { return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" || op == "|="; }
    } // namespace

    Evaluate::Binding *Evaluate::lookup(const std::string &name) {
        if (!frames.empty()) {
            for (auto scope = frames.back().scopes.rbegin(); scope != frames.back().scopes.rend(); ++scope) {
                auto found = scope->find(name);
                if (found != scope->end())
                    return &found->second;
            }
        }
        auto found = globals.find(name);
        return found == globals.end() ? nullptr : &found->second;
    }
    NumericValue Evaluate::evaluate(ExprNode &expression) {
        size_t count = values.size();
        try {
            expression.accept(*this);
            auto value = values.back();
            values.resize(count);
            return value;
        } catch (...) {
            values.resize(count);
            throw;
        }
    }
    void Evaluate::execute(ASTNode &statement) {
        size_t count = values.size();
        try {
            statement.accept(*this);
        } catch (...) {
            values.resize(count);
            throw;
        }
        values.resize(count);
    }
    void Evaluate::store(const std::string &name, NumericValue value) {
        number(value);
        if (auto *binding = lookup(name)) {
            if (binding->type && binding->type->qualifiers.is_const)
                throw std::runtime_error("Cannot assign const variable: " + name);
            binding->value = binding->type ? convert(value, binding->type) : value;
        } else {
            if (!frames.empty())
                throw std::runtime_error("Undeclared variable: " + name);
            globals[name] = {value, {}};
        }
    }
    void Evaluate::visit(NumberNode &node) {
        const auto &text = node.value.get_token();
        try {
            values.push_back(std::int64_t(text.starts_with("0b") || text.starts_with("0B") ? std::stoll(text.substr(2), nullptr, 2) : std::stoll(text, nullptr, 0)));
        } catch (const std::exception &) {
            throw std::runtime_error("Integer literal out of range: " + text);
        }
    }
    void Evaluate::visit(LiteralNode &node) {
        if (node.value.get_type() != TOKEN_TYPE::FLOAT_VALUE)
            throw std::runtime_error("Evaluator supports numeric literals only");
        auto text = node.value.get_token();
        char suffix = text.back();
        if (suffix == 'f' || suffix == 'F' || suffix == 'l' || suffix == 'L')
            text.pop_back();
        double value;
        size_t consumed = 0;
        try {
            value = std::stod(text, &consumed);
        } catch (const std::exception &) {
            throw std::runtime_error("Floating literal out of range: " + text);
        }
        if (consumed != text.size())
            throw std::runtime_error("Invalid floating literal");
        if (suffix == 'f' || suffix == 'F')
            value = static_cast<float>(value);
        values.push_back(value);
    }
    void Evaluate::visit(VariableNode &node) {
        auto *binding = lookup(node.var.get_token());
        if (!binding)
            throw std::runtime_error("Variable " + node.var.get_token() + " not found!");
        if (std::holds_alternative<std::monostate>(binding->value))
            throw std::runtime_error("Uninitialized variable: " + node.var.get_token());
        values.push_back(binding->value);
    }
    void Evaluate::visit(BinaryNode &node) {
        auto op = node.op.get_token();
        if (assignment_operator(op)) {
            auto *variable = dynamic_cast<VariableNode *>(node.left.get());
            if (!variable)
                throw std::runtime_error("Evaluator requires a variable assignment target");
            NumericValue left;
            if (op != "=")
                left = evaluate(*node.left);
            auto value = evaluate(*node.right);
            if (op != "=")
                value = binary_value(op.substr(0, op.size() - 1), left, value);
            auto name = variable->var.get_token();
            store(name, value);
            values.push_back(lookup(name)->value);
            return;
        }
        auto left = evaluate(*node.left);
        if ((op == "&&" && !truth(left)) || (op == "||" && truth(left))) {
            values.push_back(boolean(op == "||"));
            return;
        }
        auto right = evaluate(*node.right);
        values.push_back(op == "," ? right : binary_value(op, left, right));
    }
    void Evaluate::visit(UnaryNode &node) {
        auto op = node.op.get_token();
        auto value = evaluate(*node.value);
        if (op == "++" || op == "--") {
            auto *variable = dynamic_cast<VariableNode *>(node.value.get());
            if (!variable)
                throw std::runtime_error("Evaluator requires a variable increment target");
            auto name = variable->var.get_token();
            store(name, binary_value(op == "++" ? "+" : "-", value, std::int64_t(1)));
            values.push_back(node.postfix ? value : lookup(name)->value);
        } else if (op == "+") {
            number(value);
            values.push_back(value);
        } else if (op == "-")
            values.push_back(binary_value("-", std::int64_t(0), value));
        else if (op == "!")
            values.push_back(boolean(!truth(value)));
        else if (op == "~")
            values.push_back(~integer(value));
        else
            throw std::runtime_error("Evaluator does not support unary operator: " + op);
    }
    void Evaluate::visit(ConditionalNode &node) {
        auto condition = evaluate(*node.condition);
        auto value = evaluate(*(truth(condition) ? node.then_expr : node.else_expr));
        if (floating_expression(*node.then_expr) || floating_expression(*node.else_expr))
            value = static_cast<double>(number(value));
        values.push_back(value);
    }
    bool Evaluate::floating_expression(ExprNode &expression) {
        if (auto *literal = dynamic_cast<LiteralNode *>(&expression))
            return literal->value.get_type() == TOKEN_TYPE::FLOAT_VALUE;
        if (auto *variable = dynamic_cast<VariableNode *>(&expression)) {
            auto *binding = lookup(variable->var.get_token());
            if (!binding)
                return false;
            if (!binding->type)
                return std::holds_alternative<double>(binding->value);
            auto kind = scalar_type(binding->type);
            return kind == Scalar::Float || kind == Scalar::Double;
        }
        if (auto *cast = dynamic_cast<CastNode *>(&expression)) {
            auto kind = scalar_type(cast->declared_type());
            return kind == Scalar::Float || kind == Scalar::Double;
        }
        if (auto *call = dynamic_cast<CallNode *>(&expression)) {
            auto *callee = dynamic_cast<VariableNode *>(call->callee.get());
            auto function = callee ? functions.find(callee->var.get_token()) : functions.end();
            if (function == functions.end())
                return false;
            auto kind = scalar_type(function->second->result_type());
            return kind == Scalar::Float || kind == Scalar::Double;
        }
        if (auto *conditional = dynamic_cast<ConditionalNode *>(&expression))
            return floating_expression(*conditional->then_expr) || floating_expression(*conditional->else_expr);
        if (auto *unary = dynamic_cast<UnaryNode *>(&expression)) {
            auto op = unary->op.get_token();
            return op != "!" && op != "~" && floating_expression(*unary->value);
        }
        if (auto *binary = dynamic_cast<BinaryNode *>(&expression)) {
            auto op = binary->op.get_token();
            if (op == ",")
                return floating_expression(*binary->right);
            if (assignment_operator(op))
                return floating_expression(*binary->left);
            if (op == "+" || op == "-" || op == "*" || op == "/")
                return floating_expression(*binary->left) || floating_expression(*binary->right);
        }
        return false;
    }

    void Evaluate::visit(AssignmentNode &node) {
        auto value = evaluate(*node.expression);
        store(node.var.get_token(), value);
        values.push_back(lookup(node.var.get_token())->value);
    }
    void Evaluate::visit(CastNode &node) { values.push_back(convert(evaluate(*node.value), node.declared_type())); }
    void Evaluate::visit(CallNode &node) {
        auto *callee = dynamic_cast<VariableNode *>(node.callee.get());
        if (!callee)
            throw std::runtime_error("Only direct function calls are supported");
        if (lookup(callee->var.get_token()))
            throw std::runtime_error("Called name is a variable");
        std::vector<NumericValue> arguments;
        for (auto &argument : node.arguments)
            arguments.push_back(evaluate(*argument));
        values.push_back(invoke(callee->var.get_token(), arguments));
    }
    void Evaluate::visit(ProgramNode &node) {
        for (auto &declaration : node.declarations) {
            auto *function = dynamic_cast<FunctionNode *>(declaration.get());
            if (function)
                function->accept(*this);
        }
        for (auto &declaration : node.declarations) {
            if (!dynamic_cast<FunctionNode *>(declaration.get()))
                execute(*declaration);
        }
    }
    void Evaluate::visit(FunctionNode &node) {
        scalar_type(node.result_type());
        for (auto &parameter : node.parameters) {
            if (scalar_type(parameter.declared_type()) == Scalar::Void)
                throw std::runtime_error("Void parameter");
        }
        auto name = node.name.get_token();
        if (globals.contains(name))
            throw std::runtime_error("Function conflicts with variable: " + name);
        auto found = functions.find(name);
        if (found != functions.end()) {
            auto *previous = found->second;
            bool compatible = scalar_type(previous->result_type()) == scalar_type(node.result_type()) && previous->parameters.size() == node.parameters.size();
            for (size_t i = 0; compatible && i < node.parameters.size(); ++i)
                compatible = scalar_type(previous->parameters[i].declared_type()) == scalar_type(node.parameters[i].declared_type());
            if (!compatible)
                throw std::runtime_error("Conflicting function declaration: " + name);
            if (previous->body && node.body)
                throw std::runtime_error("Duplicate function definition: " + name);
            if (previous->body)
                return;
        }
        functions[name] = &node;
    }
    NumericValue Evaluate::invoke(const std::string &name, const std::vector<NumericValue> &arguments) {
        auto found = functions.find(name);
        if (found == functions.end() || !found->second->body)
            throw std::runtime_error("Undefined function: " + name);
        auto *function = found->second;
        if (function->parameters.size() != arguments.size())
            throw std::runtime_error("Wrong argument count for " + name);
        if (frames.size() >= 256)
            throw std::runtime_error("Function recursion limit exceeded");
        Frame frame;
        frame.scopes.emplace_back();
        for (size_t i = 0; i < arguments.size(); ++i) {
            const auto &parameter = function->parameters[i];
            frame.scopes.back()[parameter.name.get_token()] = {convert(arguments[i], parameter.declared_type()), parameter.declared_type()};
        }
        frames.push_back(std::move(frame));
        NumericValue result;
        bool returned = false;
        try {
            try {
                for (auto &statement : function->body->statements)
                    execute(*statement);
            } catch (const Returned &signal) {
                result = signal.value;
                returned = true;
            }
        } catch (...) {
            frames.pop_back();
            throw;
        }
        frames.pop_back();
        auto type = scalar_type(function->result_type());
        if (type == Scalar::Void) {
            if (returned && !std::holds_alternative<std::monostate>(result))
                throw std::runtime_error("Void function returned a value: " + name);
            return std::monostate{};
        }
        if (!returned && name == "main")
            result = std::int64_t(0);
        else if (!returned || std::holds_alternative<std::monostate>(result))
            throw std::runtime_error("Missing return value in " + name);
        return convert(result, function->result_type());
    }
    void Evaluate::visit(BlockNode &node) {
        if (frames.empty())
            throw std::runtime_error("Block requires a function frame");
        frames.back().scopes.emplace_back();
        try {
            for (auto &statement : node.statements)
                execute(*statement);
        } catch (...) {
            frames.back().scopes.pop_back();
            throw;
        }
        frames.back().scopes.pop_back();
    }
    void Evaluate::visit(DeclarationNode &node) {
        if (scalar_type(node.declared_type()) == Scalar::Void)
            throw std::runtime_error("Variable cannot have type void");
        for (auto &declarator : node.declarators) {
            auto name = declarator.name.get_token();
            auto &scope = frames.empty() ? globals : frames.back().scopes.back();
            if (scope.contains(name) || (frames.empty() && functions.contains(name)))
                throw std::runtime_error("Duplicate declaration: " + name);
            scope[name] = {frames.empty() ? convert(std::int64_t(0), node.declared_type()) : NumericValue(std::monostate{}), node.declared_type()};
            if (declarator.initializer) {
                auto value = convert(evaluate(*declarator.initializer), node.declared_type());
                lookup(name)->value = value;
            }
        }
    }
    void Evaluate::visit(ReturnNode &node) { throw Returned{node.expression ? evaluate(*node.expression) : NumericValue(std::monostate{})}; }
    void Evaluate::visit(IfNode &node) {
        if (truth(evaluate(*node.condition)))
            execute(*node.then_statement);
        else if (node.else_statement)
            execute(*node.else_statement);
    }
    void Evaluate::visit(WhileNode &node) {
        while (truth(evaluate(*node.condition))) {
            try {
                execute(*node.body);
            } catch (const Jumped &signal) {
                if (signal.is_break)
                    break;
            }
        }
    }
    void Evaluate::visit(ForNode &node) {
        if (frames.empty())
            throw std::runtime_error("For loop requires a function frame");
        frames.back().scopes.emplace_back();
        try {
            if (node.initializer)
                execute(*node.initializer);
            while (!node.condition || truth(evaluate(*node.condition))) {
                try {
                    execute(*node.body);
                } catch (const Jumped &signal) {
                    if (signal.is_break)
                        break;
                }
                if (node.update)
                    evaluate(*node.update);
            }
        } catch (...) {
            frames.back().scopes.pop_back();
            throw;
        }
        frames.back().scopes.pop_back();
    }
    void Evaluate::visit(JumpNode &node) { throw Jumped{node.is_break}; }
    void Evaluate::call(const std::string &name, const std::vector<NumericValue> &arguments) { values.push_back(invoke(name, arguments)); }
    bool Evaluate::has_function(const std::string &name) const {
        auto found = functions.find(name);
        return found != functions.end() && found->second->body;
    }
    NumericValue Evaluate::result_value() const {
        if (values.empty())
            throw std::runtime_error("No values");
        return values.back();
    }
    long double Evaluate::result() const { return number(result_value()); }
    void Evaluate::clear() { values.clear(); }
} // namespace mx
