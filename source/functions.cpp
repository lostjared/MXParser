#include "MXParser/parser.hpp"

namespace mx {
    namespace {
        std::string statement_text(const ASTNode &node) { return node.to_string() + (dynamic_cast<const ExprNode *>(&node) ? ";" : ""); }
        std::string function_declarator(const std::string &name, const std::vector<Parameter> &parameters, bool has_prototype) {
            std::string result = name + "(";
            if (parameters.empty() && has_prototype)
                result += "void";
            for (size_t i = 0; i < parameters.size(); ++i) {
                if (i)
                    result += ", ";
                auto type = parameters[i].declaration_type ? parameters[i].declaration_type : parameters[i].declared_type();
                result += type->declaration(parameters[i].name.get_token());
            }
            return result + ")";
        }

    } // namespace

    void ASTVisitor::visit(InitializerListNode &) { throw std::runtime_error("Visitor does not support brace initializers"); }
    std::string InitializerListNode::to_string() const {
        std::string result = "{";
        for (const auto &element : elements) {
            if (result.size() > 1)
                result += ", ";
            result += element->to_string();
        }
        return result + "}";
    }
    std::string AggregateDefinition::to_string(TagKind kind) const {
        std::string result = "{";
        if (kind == TagKind::Enum) {
            for (size_t i = 0; i < enumerators.size(); ++i) {
                if (i)
                    result += ", ";
                result += enumerators[i].name.get_token();
                if (enumerators[i].value)
                    result += "=" + enumerators[i].value->to_string();
            }
        } else {
            for (const auto &member : members)
                result += member->to_string();
        }
        return result + "}";
    }
    void ASTVisitor::visit(ProgramNode &) { throw std::runtime_error("Visitor does not support programs"); }
    void ASTVisitor::visit(FunctionNode &) { throw std::runtime_error("Visitor does not support function definitions"); }
    void ASTVisitor::visit(BlockNode &) { throw std::runtime_error("Visitor does not support blocks"); }
    void ASTVisitor::visit(ReturnNode &) { throw std::runtime_error("Visitor does not support returns"); }
    void ASTVisitor::visit(DeclarationNode &) { throw std::runtime_error("Visitor does not support declarations"); }
    void ASTVisitor::visit(IfNode &) { throw std::runtime_error("Visitor does not support if statements"); }
    void ASTVisitor::visit(WhileNode &) { throw std::runtime_error("Visitor does not support while statements"); }
    void ASTVisitor::visit(DoWhileNode &) { throw std::runtime_error("Visitor does not support do/while statements"); }
    void ASTVisitor::visit(SwitchNode &) { throw std::runtime_error("Visitor does not support switch statements"); }
    void ASTVisitor::visit(CaseNode &) { throw std::runtime_error("Visitor does not support case labels"); }
    void ASTVisitor::visit(DefaultNode &) { throw std::runtime_error("Visitor does not support default labels"); }
    void ASTVisitor::visit(ForNode &) { throw std::runtime_error("Visitor does not support for statements"); }
    void ASTVisitor::visit(JumpNode &) { throw std::runtime_error("Visitor does not support break/continue"); }

    std::string ProgramNode::to_string() const {
        std::string result;
        for (const auto &declaration : declarations)
            result += declaration->to_string() + "\n";
        return result;
    }
    std::string BlockNode::to_string() const {
        std::string result = "{\n";
        for (const auto &statement : statements)
            result += statement_text(*statement) + "\n";
        return result + "}";
    }
    std::string FunctionNode::to_string() const {
        auto declarator = function_declarator(name.get_token(), parameters, signature_type()->has_prototype);
        std::string prefix;
        for (const auto &specifier : storage)
            prefix += specifier + " ";
        return prefix + result_type()->declaration(declarator) + (body ? body->to_string() : ";");
    }
    std::string ReturnNode::to_string() const { return "return" + (expression ? " " + expression->to_string() : "") + ";"; }
    std::string DeclarationNode::to_string() const {
        std::string result;
        for (const auto &specifier : storage)
            result += specifier + " ";
        result += declared_type()->to_string();
        if (!declarators.empty())
            result += " ";
        for (size_t i = 0; i < declarators.size(); ++i) {
            if (i)
                result += ", ";
            const auto &declarator = declarators[i];
            if (declarator.type && declarator.type->kind == TypeKind::Function && declarator.parameters.size() == declarator.type->parameters.size())
                result += declarator.type->element->declarator(function_declarator(declarator.name.get_token(), declarator.parameters, declarator.type->has_prototype), declared_type());
            else
                result += declarator.type ? declarator.type->declarator(declarator.name.get_token(), declared_type()) : declarator.name.get_token();
            if (declarators[i].initializer)
                result += "=" + declarators[i].initializer->to_string();
        }
        return result + ";";
    }
    std::string IfNode::to_string() const { return "if(" + condition->to_string() + ")" + statement_text(*then_statement) + (else_statement ? " else " + statement_text(*else_statement) : ""); }
    std::string WhileNode::to_string() const { return "while(" + condition->to_string() + ")" + statement_text(*body); }
    std::string DoWhileNode::to_string() const { return "do " + statement_text(*body) + " while(" + condition->to_string() + ");"; }
    std::string SwitchNode::to_string() const { return "switch(" + condition->to_string() + ")" + statement_text(*body); }
    std::string CaseNode::to_string() const { return "case " + value->to_string() + ":" + statement_text(*statement); }
    std::string DefaultNode::to_string() const { return "default:" + statement_text(*statement); }
    std::string ForNode::to_string() const {
        std::string init = initializer ? statement_text(*initializer) : ";";
        return "for(" + init + (condition ? condition->to_string() : "") + ";" + (update ? update->to_string() : "") + ")" + statement_text(*body);
    }

    void ICode::emit_statement(ASTNode &node) {
        size_t count = values.size();
        try {
            node.accept(*this);
        } catch (...) {
            values.resize(count);
            throw;
        }
        values.resize(count);
    }
    void ICode::visit(LiteralNode &node) {
        if (node.value.get_type() != TOKEN_TYPE::FLOAT_VALUE)
            throw std::runtime_error("Code generation supports numeric literals only");
        values.push_back(node.value.get_token());
    }
    void ICode::visit(CastNode &node) {
        node.value->accept(*this);
        auto result = get_temp();
        stream << "CAST " << result << ", " << node.declared_type()->to_string() << ", " << values.back() << "\n";
        values.back() = result;
    }
    void ICode::visit(CallNode &node) {
        auto *callee = dynamic_cast<VariableNode *>(node.callee.get());
        if (!callee)
            throw std::runtime_error("Code generation supports direct calls only");
        std::vector<std::string> arguments;
        for (auto &argument : node.arguments) {
            argument->accept(*this);
            auto temp = get_temp();
            stream << "SET " << temp << ", " << values.back() << "\n";
            values.pop_back();
            arguments.push_back(temp);
        }
        for (const auto &argument : arguments)
            stream << "ARG " << argument << "\n";
        auto result = get_temp();
        stream << "CALL " << result << ", " << callee->var.get_token() << ", " << arguments.size() << "\n";
        values.push_back(result);
    }
    void ICode::visit(ProgramNode &node) {
        for (auto &declaration : node.declarations)
            emit_statement(*declaration);
    }
    void ICode::visit(FunctionNode &node) {
        if (!node.body) {
            stream << "PROTO " << node.name.get_token() << ", " << node.result_type()->to_string() << "\n";
            return;
        }
        stream << "FUNC " << node.name.get_token() << ", " << node.result_type()->to_string() << "\n";
        for (const auto &parameter : node.parameters)
            stream << "PARAM " << parameter.name.get_token() << ", " << parameter.declared_type()->to_string() << "\n";
        node.body->accept(*this);
        if (node.result_type()->unqualified().kind == TypeKind::Scalar && node.result_type()->unqualified().scalar == ScalarKind::Void)
            stream << "RETURN\n";
        else if (node.name.get_token() == "main")
            stream << "RETURN 0\n";
        stream << "END " << node.name.get_token() << "\n";
    }
    void ICode::visit(BlockNode &node) {
        stream << "SCOPE\n";
        for (auto &statement : node.statements)
            emit_statement(*statement);
        stream << "ENDSCOPE\n";
    }
    void ICode::visit(DeclarationNode &node) {
        if (node.declared_type()->unqualified().definition)
            throw std::runtime_error("Code generation does not support aggregate definitions");
        if (node.is_typedef)
            return;
        for (auto &declarator : node.declarators) {
            auto type = declarator.type ? declarator.type : node.declared_type();
            if (type->unqualified().kind == TypeKind::Function)
                throw std::runtime_error("Code generation does not support block-scope function declarations");
            stream << "VAR " << declarator.name.get_token() << ", " << type->to_string() << "\n";
            if (declarator.initializer) {
                declarator.initializer->accept(*this);
                stream << "SET " << declarator.name.get_token() << ", " << values.back() << "\n";
                values.pop_back();
            }
        }
    }
    void ICode::visit(ReturnNode &node) {
        if (node.expression) {
            node.expression->accept(*this);
            stream << "RETURN " << values.back() << "\n";
            values.pop_back();
        } else
            stream << "RETURN\n";
    }
    void ICode::visit(IfNode &node) {
        auto otherwise = get_temp(), done = get_temp();
        node.condition->accept(*this);
        stream << "JZ " << values.back() << ", " << otherwise << "\n";
        values.pop_back();
        emit_statement(*node.then_statement);
        stream << "JMP " << done << "\nLABEL " << otherwise << "\n";
        if (node.else_statement)
            emit_statement(*node.else_statement);
        stream << "LABEL " << done << "\n";
    }
    void ICode::visit(WhileNode &node) {
        auto start = get_temp(), done = get_temp();
        stream << "LABEL " << start << "\n";
        node.condition->accept(*this);
        stream << "JZ " << values.back() << ", " << done << "\n";
        values.pop_back();
        loop_labels.emplace_back(done, start);
        try {
            emit_statement(*node.body);
        } catch (...) {
            loop_labels.pop_back();
            throw;
        }
        loop_labels.pop_back();
        stream << "JMP " << start << "\nLABEL " << done << "\n";
    }
    void ICode::visit(ForNode &node) {
        auto start = get_temp(), update = get_temp(), done = get_temp();
        stream << "SCOPE\n";
        if (node.initializer)
            emit_statement(*node.initializer);
        stream << "LABEL " << start << "\n";
        if (node.condition) {
            node.condition->accept(*this);
            stream << "JZ " << values.back() << ", " << done << "\n";
            values.pop_back();
        }
        loop_labels.emplace_back(done, update);
        try {
            emit_statement(*node.body);
        } catch (...) {
            loop_labels.pop_back();
            throw;
        }
        loop_labels.pop_back();
        stream << "LABEL " << update << "\n";
        if (node.update)
            emit_statement(*node.update);
        stream << "JMP " << start << "\nLABEL " << done << "\nENDSCOPE\n";
    }
    void ICode::visit(JumpNode &node) {
        if (loop_labels.empty())
            throw std::runtime_error("break/continue outside loop");
        stream << "JMP " << (node.is_break ? loop_labels.back().first : loop_labels.back().second) << "\n";
    }
    void ICode::write_to_stream(std::ostream &out) { out << code(); }
    std::string HTMLCode::code() {
        std::istringstream input(generator.code());
        std::string line, result;
        while (std::getline(input, line)) {
            auto separator = line.find(' ');
            result += "<b>" + html_escape(line.substr(0, separator)) + "</b>";
            if (separator != std::string::npos)
                result += " " + html_escape(line.substr(separator + 1));
            result += "<br>\n";
        }
        return result;
    }
    void HTMLCode::write_to_stream(std::ostream &out) { out << code(); }
} // namespace mx
