#include "MXParser/parser.hpp"
#include "MXLex/token.hpp"
#include "MXParser/ast.hpp"
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace mx {

    std::string html_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size() * 12 / 10 + 8);
        for (char c : s) {
            switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out.push_back(c);
                break;
            }
        }
        return out;
    }
    std::string js_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size() * 12 / 10 + 8);
        for (char c : s) {
            switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '\'':
                out += "\\'";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\0':
                out += "\\0";
                break;
            case '`':
                out += "\\`";
                break;
            case '$':
                out += "\\$";
                break;
            case '/':
                out += "\\/";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
            }
        }
        return html_escape(out);
    }

    bool Parser::scan() {
        tokens.clear();
        index = 0;
        scan_failure.reset();
        try {
            Token token;
            TOKEN_TYPE token_type;
            while ((token_type = scanner.lex(token)) != mx::TOKEN_TYPE::TOKEN_NULL) {
                if (token_type == mx::TOKEN_TYPE::TOKEN_ERROR) {
                    scan_failure.emplace("Expected a valid token", SourceLocation::from_token(token));
                    std::cerr << scan_failure->what() << "\n";
                    return false;
                }
                if (token.get_token() == "<:")
                    token.set_token("[", TOKEN_TYPE::OPERATOR, CHAR_TYPE::CHAR_LBRACKET);
                else if (token.get_token() == ":>")
                    token.set_token("]", TOKEN_TYPE::OPERATOR, CHAR_TYPE::CHAR_RBRACKET);
                tokens.push_back(token);
            }
            if (token.get_line() == 0)
                token.set_line(scanner.get_line());
            tokens.push_back(token);
        } catch (const mx::ScannerError &e) {
            scan_failure.emplace(std::string("Expected valid lexical input: ") + e.what(), SourceLocation{scanner.get_line(), "invalid lexical input"});
            std::cerr << scan_failure->what() << "\n";
            return false;
        }
        return true;
    }

    bool Parser::parse() {
        if (!scan()) {
            std::cerr << "Scanning failed.\n";
            return false;
        }
        Evaluate eval;
        ICode icode;
        HTMLCode html;
        std::fstream fout;
        fout.open("out.html", std::ios::out);

        if (!fout.is_open()) {
            throw ParserException("Error could not open output file for writing.\n");
        }

        std::vector<std::string> line_state;
        std::string gen_icode, gen_html_code;
        try {
            if (starts_type_name(index) || top().get_token() == "static" || top().get_token() == "extern" || top().get_token() == "inline") {
                auto unit = program();
                line_state.push_back(unit->to_string());
                unit->accept(eval);
                unit->accept(icode);
                unit->accept(html);
                if (eval.has_function("main")) {
                    eval.call("main");
                    if (!std::holds_alternative<std::monostate>(eval.result_value()))
                        std::cout << "Result: " << eval.result() << "\n";
                }
                gen_icode = icode.code();
                gen_html_code = html.code();
            } else {
                while (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
                    if (match(";"))
                        continue;
                    eval.clear();
                    icode.clear();
                    html.clear();
                    auto e = statement();
                    if (top().get_type() != TOKEN_TYPE::TOKEN_NULL)
                        expect(";");
                    auto representation = e->to_string();
                    std::cout << representation << "\n";
                    line_state.push_back(representation);
                    e->accept(eval);
                    e->accept(icode);
                    e->accept(html);
                    gen_icode += icode.code();
                    gen_html_code += html.code();
                }
                std::cout << "Result: " << eval.result() << "\n";
            }
            std::cout << "ICode:\n" << gen_icode << "\n";

            output_html(fout, gen_html_code, line_state);

        } catch (std::runtime_error &e) {
            std::cerr << "Exception: " << e.what() << "\n";
            return false;
        }
        fout.close();
        if (!fout) {
            throw ParserException("Error writing to output file.");
        }
        return true;
    }

    void Parser::output_html(std::ostream &out, const std::string &html, std::vector<std::string> &state) {
        out << "<html><head><title>HTML Test</title></head><body>\n";
        for (const auto &s : state) {
            out << "<span style=\"color: red;\">" << html_escape(s) << "</span><br>\n";
        }
        out << html << "<br>";
        out << "</body></html>";
    }

    bool Parser::advance() {
        if (index + 1 >= tokens.size())
            return false;
        ++index;
        return true;
    }

    Token &Parser::peek(size_t offset) {
        if (index + offset >= tokens.size()) {
            throw ParserException(std::format("peek index {} out of bounds.", index + offset));
        }

        return tokens[index + offset];
    }

    Token &Parser::next() {
        if (advance())
            return tokens[index];

        throw ParserEOF();
    }

    Token &Parser::at(size_t index) {
        if (index < tokens.size()) {
            return tokens[index];
        }
        throw ParserException(std::format("Index: {}  out of bounds for token loop up", index));
    }

    Token &Parser::top() {
        if (index < tokens.size())
            return tokens[index];
        throw ParserException(std::format("index out of range for top at index:{}.", index));
    }

    void Parser::print_tokens(std::ostream &out) {
        out << "Tokens: \n";
        for (const auto &i : tokens) {
            out << i << std::endl;
        }
    }

    namespace {
        int precedence(const std::string &op) {
            if (op == "||")
                return 1;
            if (op == "&&")
                return 2;
            if (op == "|")
                return 3;
            if (op == "^")
                return 4;
            if (op == "&")
                return 5;
            if (op == "==" || op == "!=")
                return 6;
            if (op == "<" || op == ">" || op == "<=" || op == ">=")
                return 7;
            if (op == "<<" || op == ">>")
                return 8;
            if (op == "+" || op == "-")
                return 9;
            if (op == "*" || op == "/" || op == "%")
                return 10;
            return 0;
        }

        bool assignment_operator(const std::string &op) { return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" || op == "|="; }

        bool qualifier(const std::string &text) { return text == "const" || text == "volatile" || text == "restrict" || text == "_Atomic"; }

        bool builtin_type(const std::string &text) { return text == "void" || text == "char" || text == "short" || text == "int" || text == "long" || text == "float" || text == "double" || text == "signed" || text == "unsigned" || text == "_Bool"; }

        bool keyword(const std::string &text) {
            static const std::set<std::string> words = {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"};
            return words.contains(text);
        }
    } // namespace

    bool Parser::match(const std::string &text) {
        if (top().get_token() != text)
            return false;
        advance();
        return true;
    }

    [[noreturn]] void Parser::fail_at(const Token &token, const std::string &message) { throw ParserException(message, SourceLocation::from_token(token)); }
    [[noreturn]] void Parser::fail(const std::string &message) { fail_at(top(), message); }

    void Parser::expect(const std::string &text) {
        if (!match(text))
            fail("Expected '" + text + "'");
    }

    void Parser::add_type_name(const std::string &name) { type_names.insert(name); }

    std::unique_ptr<ExprNode> Parser::parse_expression() {
        if (!scan())
            throw *scan_failure;
        auto result = expr();
        match(";");
        if (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            fail("Expected end of input after expression");
        }
        return result;
    }

    std::vector<std::unique_ptr<ExprNode>> Parser::parse_expressions() {
        if (!scan())
            throw *scan_failure;
        std::vector<std::unique_ptr<ExprNode>> result;
        while (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            if (match(";"))
                continue;
            result.push_back(expr());
            if (top().get_type() != TOKEN_TYPE::TOKEN_NULL)
                expect(";");
        }
        return result;
    }

    std::unique_ptr<ASTNode> Parser::statement() { return expr(); }

    std::unique_ptr<AssignmentNode> Parser::assignment() {
        Token var = top();
        advance();
        expect("=");
        return std::make_unique<AssignmentNode>(var, expr());
    }

    std::unique_ptr<ExprNode> Parser::expr() {
        auto left = assignment_expr();
        while (top().get_token() == ",") {
            Token op = top();
            advance();
            left = std::make_unique<BinaryNode>(std::move(left), op, assignment_expr());
        }
        return left;
    }

    std::unique_ptr<ExprNode> Parser::assignment_expr() {
        size_t start = index;
        auto left = conditional();
        if (assignment_operator(top().get_token())) {
            size_t end = index;
            index = start;
            unary();
            if (index != end)
                fail("Expected unary expression before assignment");
            Token op = top();
            advance();
            return std::make_unique<BinaryNode>(std::move(left), op, assignment_expr());
        }
        return left;
    }

    std::unique_ptr<ExprNode> Parser::conditional() {
        auto condition = binary(1);
        if (!match("?"))
            return condition;
        auto then_expr = expr();
        expect(":");
        auto else_expr = conditional();
        return std::make_unique<ConditionalNode>(std::move(condition), std::move(then_expr), std::move(else_expr));
    }

    std::unique_ptr<ExprNode> Parser::binary(int min_precedence) {
        auto left = cast();
        while (precedence(top().get_token()) >= min_precedence) {
            Token op = top();
            int level = precedence(op.get_token());
            advance();
            auto right = binary(level + 1);
            left = std::make_unique<BinaryNode>(std::move(left), op, std::move(right));
        }
        return left;
    }

    bool Parser::starts_type_name(size_t position) const {
        if (position >= tokens.size())
            return false;
        const auto &text = tokens[position].get_token();
        return builtin_type(text) || qualifier(text) || text == "struct" || text == "union" || text == "enum" || type_names.contains(text);
    }

    TypePtr Parser::type_name() {
        std::string spelling;
        Token last_specifier = top();
        bool have_type = false;
        auto append = [&] {
            if (!spelling.empty())
                spelling += " ";
            spelling += top().get_token();
            advance();
        };
        while (qualifier(top().get_token()) || builtin_type(top().get_token())) {
            if (builtin_type(top().get_token())) {
                have_type = true;
                last_specifier = top();
            }
            append();
        }
        if (!have_type && (top().get_token() == "struct" || top().get_token() == "union" || top().get_token() == "enum")) {
            append();
            if (top().get_type() != TOKEN_TYPE::IDENTIFIER || keyword(top().get_token()))
                fail("Expected type tag");
            append();
            have_type = true;
        } else if (!have_type && type_names.contains(top().get_token())) {
            append();
            have_type = true;
        }
        if (!have_type)
            fail("Expected type name");
        while (qualifier(top().get_token()))
            append();
        while (top().get_token() == "*") {
            append();
            while (qualifier(top().get_token()))
                append();
        }
        try {
            return Type::from_name(spelling);
        } catch (const std::invalid_argument &error) {
            fail_at(last_specifier, std::string("Expected valid type specifiers: ") + error.what());
        }
    }

    std::unique_ptr<ExprNode> Parser::cast() {
        if (top().get_token() == "(" && starts_type_name(index + 1)) {
            Token start = top();
            advance();
            auto type = type_name();
            expect(")");
            auto result = std::make_unique<CastNode>(std::move(type), cast());
            result->location = SourceLocation::from_token(start);
            return result;
        }
        return unary();
    }

    std::unique_ptr<ExprNode> Parser::unary() {
        Token op = top();
        const auto text = op.get_token();
        if (text == "sizeof") {
            advance();
            if (top().get_token() == "(" && starts_type_name(index + 1)) {
                advance();
                auto type = type_name();
                expect(")");
                auto result = std::make_unique<SizeofNode>(std::move(type));
                result->location = SourceLocation::from_token(op);
                return result;
            }
            auto result = std::make_unique<SizeofNode>(unary());
            result->location = SourceLocation::from_token(op);
            return result;
        }
        if (text == "++" || text == "--") {
            advance();
            return std::make_unique<UnaryNode>(op, unary());
        }
        if (text == "+" || text == "-" || text == "!" || text == "~" || text == "*" || text == "&") {
            advance();
            return std::make_unique<UnaryNode>(op, cast());
        }
        return postfix();
    }

    std::unique_ptr<ExprNode> Parser::postfix() {
        auto result = primary();
        for (;;) {
            if (match("(")) {
                std::vector<std::unique_ptr<ExprNode>> args;
                if (!match(")")) {
                    do {
                        args.push_back(assignment_expr());
                    } while (match(","));
                    expect(")");
                }
                result = std::make_unique<CallNode>(std::move(result), std::move(args));
            } else if (match("[")) {
                auto subscript = expr();
                expect("]");
                result = std::make_unique<SubscriptNode>(std::move(result), std::move(subscript));
            } else if (top().get_token() == "." || top().get_token() == "->") {
                Token op = top();
                advance();
                Token member = top();
                if (member.get_type() != TOKEN_TYPE::IDENTIFIER || keyword(member.get_token()))
                    fail("Expected member name");
                advance();
                result = std::make_unique<MemberNode>(std::move(result), op, member);
            } else if (top().get_token() == "++" || top().get_token() == "--") {
                Token op = top();
                advance();
                result = std::make_unique<UnaryNode>(op, std::move(result), true);
            } else
                return result;
        }
    }

    std::unique_ptr<ExprNode> Parser::primary() {
        Token current = top();
        if (match("(")) {
            auto result = expr();
            expect(")");
            result->location = SourceLocation::from_token(current);
            return result;
        }
        if (current.get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(current.get_token()) && !type_names.contains(current.get_token())) {
            advance();
            return std::make_unique<VariableNode>(current);
        }
        if (current.get_type() == TOKEN_TYPE::INTEGER_VALUE) {
            advance();
            return std::make_unique<NumberNode>(current);
        }
        if (current.get_type() == TOKEN_TYPE::FLOAT_VALUE || current.get_type() == TOKEN_TYPE::CHARACTER_VALUE || current.get_type() == TOKEN_TYPE::STRING_VALUE) {
            auto location = SourceLocation::from_token(current);
            advance();
            if (current.get_type() == TOKEN_TYPE::STRING_VALUE) {
                std::string text = current.get_token();
                while (top().get_type() == TOKEN_TYPE::STRING_VALUE) {
                    text += " " + top().get_token();
                    advance();
                }
                current.set_token(text, TOKEN_TYPE::STRING_VALUE);
            }
            auto result = std::make_unique<LiteralNode>(current);
            result->location = std::move(location);
            return result;
        }
        fail("Expected expression");
    }

    std::unique_ptr<ExprNode> Parser::factor() { return unary(); }
    std::unique_ptr<ExprNode> Parser::term() { return binary(10); }

    std::unique_ptr<ProgramNode> Parser::parse_program() {
        if (!scan())
            throw *scan_failure;
        return program();
    }

    Token Parser::identifier() {
        Token name = top();
        if (name.get_type() != TOKEN_TYPE::IDENTIFIER || keyword(name.get_token()) || type_names.contains(name.get_token())) {
            fail("Expected identifier");
        }
        advance();
        return name;
    }

    std::unique_ptr<ProgramNode> Parser::program() {
        auto result = std::make_unique<ProgramNode>();
        result->location = SourceLocation::from_token(top());
        while (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            if (match(";"))
                continue;
            auto location = SourceLocation::from_token(top());
            while (top().get_token() == "static" || top().get_token() == "extern" || top().get_token() == "inline")
                advance();
            auto type = type_name();
            auto name = identifier();
            if (!match("(")) {
                result->declarations.push_back(declaration(std::move(type), name, location));
                continue;
            }
            auto function = std::make_unique<FunctionNode>();
            function->return_type = type->to_string();
            function->location = location;
            function->name = name;
            std::set<std::string> names;
            if (top().get_token() == "void" && peek(1).get_token() == ")")
                advance();
            else if (top().get_token() != ")") {
                do {
                    Parameter parameter;
                    parameter.location = SourceLocation::from_token(top());
                    parameter.type = type_name();
                    parameter.type_name = parameter.type->to_string();
                    if (parameter.type->unqualified().kind == TypeKind::Scalar && parameter.type->unqualified().scalar == ScalarKind::Void)
                        throw ParserException("Expected a non-void parameter type", parameter.location);
                    if (top().get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(top().get_token())) {
                        parameter.name = identifier();
                        if (!names.insert(parameter.name.get_token()).second)
                            fail_at(parameter.name, "Expected unique parameter name");
                    }
                    function->parameters.push_back(std::move(parameter));
                } while (match(","));
            }
            expect(")");
            if (!match(";")) {
                for (const auto &parameter : function->parameters) {
                    if (parameter.name.get_type() == TOKEN_TYPE::TOKEN_NULL)
                        fail("Expected named parameters in function definition");
                }
                function->body = block();
            }
            std::vector<TypePtr> parameter_types;
            for (const auto &parameter : function->parameters)
                parameter_types.push_back(parameter.type);
            function->type = Type::function(std::move(type), std::move(parameter_types));
            result->declarations.push_back(std::move(function));
        }
        return result;
    }

    std::unique_ptr<DeclarationNode> Parser::declaration(TypePtr type, Token name, SourceLocation location) {
        auto result = std::make_unique<DeclarationNode>();
        result->type_name = type->to_string();
        result->type = std::move(type);
        result->location = std::move(location);
        for (;;) {
            Declarator declarator;
            declarator.name = name;
            declarator.location = SourceLocation::from_token(name);
            declarator.type = result->type;
            if (match("="))
                declarator.initializer = assignment_expr();
            result->declarators.push_back(std::move(declarator));
            if (!match(","))
                break;
            name = identifier();
        }
        expect(";");
        return result;
    }

    std::unique_ptr<BlockNode> Parser::block() {
        auto location = SourceLocation::from_token(top());
        expect("{");
        auto result = std::make_unique<BlockNode>();
        result->location = location;
        while (!match("}")) {
            if (top().get_type() == TOKEN_TYPE::TOKEN_NULL)
                fail("Expected '}' before end of input");
            result->statements.push_back(function_statement(true));
        }
        return result;
    }

    std::unique_ptr<ASTNode> Parser::function_statement(bool allow_declaration) {
        auto location = SourceLocation::from_token(top());
        if (top().get_token() == "{")
            return block();
        if (match(";")) {
            auto result = std::make_unique<BlockNode>();
            result->location = location;
            return result;
        }
        if (match("return")) {
            auto result = std::make_unique<ReturnNode>();
            result->location = location;
            if (top().get_token() != ";")
                result->expression = expr();
            expect(";");
            return result;
        }
        if (match("if")) {
            auto result = std::make_unique<IfNode>();
            result->location = location;
            expect("(");
            result->condition = expr();
            expect(")");
            result->then_statement = function_statement();
            if (match("else"))
                result->else_statement = function_statement();
            return result;
        }
        if (match("while")) {
            auto result = std::make_unique<WhileNode>();
            result->location = location;
            expect("(");
            result->condition = expr();
            expect(")");
            ++loop_depth;
            try {
                result->body = function_statement();
            } catch (...) {
                --loop_depth;
                throw;
            }
            --loop_depth;
            return result;
        }
        if (match("for")) {
            auto result = std::make_unique<ForNode>();
            result->location = location;
            expect("(");
            if (starts_type_name(index)) {
                auto declaration_location = SourceLocation::from_token(top());
                auto type = type_name();
                auto name = identifier();
                result->initializer = declaration(std::move(type), name, declaration_location);
            } else {
                if (top().get_token() != ";")
                    result->initializer = expr();
                expect(";");
            }
            if (top().get_token() != ";")
                result->condition = expr();
            expect(";");
            if (top().get_token() != ")")
                result->update = expr();
            expect(")");
            ++loop_depth;
            try {
                result->body = function_statement();
            } catch (...) {
                --loop_depth;
                throw;
            }
            --loop_depth;
            return result;
        }
        if (top().get_token() == "break" || top().get_token() == "continue") {
            if (!loop_depth)
                fail("Expected an enclosing loop for break/continue");
            auto result = std::make_unique<JumpNode>();
            result->location = location;
            result->is_break = top().get_token() == "break";
            advance();
            expect(";");
            return result;
        }
        if (starts_type_name(index)) {
            if (!allow_declaration)
                fail("Expected statement; a declaration requires a block");
            auto type = type_name();
            auto name = identifier();
            return declaration(std::move(type), name, location);
        }
        auto result = expr();
        expect(";");
        return result;
    }

    void Parser::error_message(const std::string &message, int line) { std::println("MXParser: Error: {} on Line: {}", message, line); }
} // namespace mx
