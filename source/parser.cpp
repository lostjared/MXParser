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
        try {
            Token token;
            TOKEN_TYPE token_type;
            while ((token_type = scanner.lex(token)) != mx::TOKEN_TYPE::TOKEN_NULL) {
                if (token_type == mx::TOKEN_TYPE::TOKEN_ERROR) {
                    std::cerr << "MXParser:" << token << "\n";
                    return false;
                }
                if (token.get_token() == "<:")
                    token.set_token("[", TOKEN_TYPE::OPERATOR, CHAR_TYPE::CHAR_LBRACKET);
                else if (token.get_token() == ":>")
                    token.set_token("]", TOKEN_TYPE::OPERATOR, CHAR_TYPE::CHAR_RBRACKET);
                tokens.push_back(token);
            }
            token.clear();
            token.set_line(scanner.get_line());
            tokens.push_back(token);
        } catch (const mx::ScannerError &e) {
            std::cerr << "MXLex: Token Error Exception: " << e.what() << "\n";
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
        ;
        std::string gen_icode, gen_html_code;
        try {
            while (index < tokens.size() - 1) {
                if (top().get_token() == ";") {
                    advance();
                    continue;
                }

                eval.clear();
                icode.clear();
                html.clear();
                auto e = statement();

                if (index < tokens.size() - 1) {
                    if (top().get_token() != ";" && top().get_token() != ",") {
                        throw ParserException(std::format("Error expected: ; found: {} at line: {}", top().get_token(), top().get_line()));
                    }
                }

                if (top().get_token() == ",") {
                    if (index + 1 >= tokens.size() - 1 || peek(1).get_token() == ";" || peek(1).get_token() == ",") {
                        throw ParserException("Expected statement after comma");
                    }
                }

                std::string string_rep = e->to_string();
                std::cout << string_rep << "\n";
                line_state.push_back(string_rep);

                e->accept(eval);
                e->accept(icode);
                e->accept(html);

                gen_icode += icode.code();
                gen_html_code += html.code();

                if (top().get_token() == ";" || top().get_token() == ",") {
                    advance();
                }
            }
            std::cout << "Result: " << eval.result() << "\n";
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

    void Parser::expect(const std::string &text) {
        if (!match(text)) {
            throw ParserException(std::format("Expected '{}' at line {}, found '{}'", text, top().get_line(), top().get_type() == TOKEN_TYPE::TOKEN_NULL ? "end of input" : top().get_token()));
        }
    }

    void Parser::add_type_name(const std::string &name) { type_names.insert(name); }

    std::unique_ptr<ExprNode> Parser::parse_expression() {
        if (!scan())
            throw ParserException("Scanning failed");
        auto result = expr();
        match(";");
        if (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            throw ParserException(std::format("Unexpected '{}' after expression at line {}", top().get_token(), top().get_line()));
        }
        return result;
    }

    std::vector<std::unique_ptr<ExprNode>> Parser::parse_expressions() {
        if (!scan())
            throw ParserException("Scanning failed");
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
                throw ParserException(std::format("Expected unary expression before assignment at line {}", top().get_line()));
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

    std::string Parser::type_name() {
        std::string result;
        bool have_type = false;
        std::unordered_map<std::string, int> specifiers;
        while (qualifier(top().get_token()) || builtin_type(top().get_token())) {
            if (builtin_type(top().get_token())) {
                have_type = true;
                ++specifiers[top().get_token()];
            }
            if (!result.empty())
                result += " ";
            result += top().get_token();
            advance();
        }
        if (have_type) {
            for (const auto &[name, count] : specifiers) {
                if (count > (name == "long" ? 2 : 1))
                    throw ParserException("Duplicate type specifier: " + name);
            }
            bool valid = !(specifiers["signed"] && specifiers["unsigned"]);
            int sign = specifiers["signed"] + specifiers["unsigned"];
            int total = 0;
            for (const auto &[name, count] : specifiers)
                total += count;
            if (specifiers["void"] || specifiers["_Bool"] || specifiers["float"])
                valid = valid && total == 1;
            else if (specifiers["double"])
                valid = valid && total == 1 + specifiers["long"] && specifiers["long"] <= 1;
            else if (specifiers["char"])
                valid = valid && total == 1 + sign;
            else
                valid = valid && !(specifiers["short"] && specifiers["long"]);
            if (!valid)
                throw ParserException("Invalid type specifiers: " + result);
        }
        if (!have_type && (top().get_token() == "struct" || top().get_token() == "union" || top().get_token() == "enum")) {
            if (!result.empty())
                result += " ";
            result += top().get_token();
            advance();
            if (top().get_type() != TOKEN_TYPE::IDENTIFIER || keyword(top().get_token()))
                throw ParserException("Expected type tag");
            result += " " + top().get_token();
            advance();
            have_type = true;
        } else if (!have_type && type_names.contains(top().get_token())) {
            if (!result.empty())
                result += " ";
            result += top().get_token();
            advance();
            have_type = true;
        }
        if (!have_type)
            throw ParserException("Expected type name");
        while (qualifier(top().get_token())) {
            result += " " + top().get_token();
            advance();
        }
        while (match("*")) {
            result += " *";
            while (qualifier(top().get_token())) {
                result += " " + top().get_token();
                advance();
            }
        }
        return result;
    }

    std::unique_ptr<ExprNode> Parser::cast() {
        if (top().get_token() == "(" && starts_type_name(index + 1)) {
            advance();
            auto type = type_name();
            expect(")");
            return std::make_unique<CastNode>(std::move(type), cast());
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
                return std::make_unique<SizeofNode>(std::move(type));
            }
            return std::make_unique<SizeofNode>(unary());
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
                    throw ParserException("Expected member name");
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
            advance();
            if (current.get_type() == TOKEN_TYPE::STRING_VALUE) {
                std::string text = current.get_token();
                while (top().get_type() == TOKEN_TYPE::STRING_VALUE) {
                    text += " " + top().get_token();
                    advance();
                }
                current.set_token(text, TOKEN_TYPE::STRING_VALUE);
            }
            return std::make_unique<LiteralNode>(current);
        }
        throw ParserException(std::format("Expected expression at line {}, found '{}'", current.get_line(), current.get_type() == TOKEN_TYPE::TOKEN_NULL ? "end of input" : current.get_token()));
    }

    std::unique_ptr<ExprNode> Parser::factor() { return unary(); }
    std::unique_ptr<ExprNode> Parser::term() { return binary(10); }

    void Parser::error_message(const std::string &message, int line) { std::println("MXParser: Error: {} on Line: {}", message, line); }
} // namespace mx
