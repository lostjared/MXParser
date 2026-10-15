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

    std::unique_ptr<ASTNode> Parser::statement() {
        if (top().get_type() == TOKEN_TYPE::IDENTIFIER && peek(1).get_token() == "=") {
            return assignment();
        }
        return expr();
    }
    std::unique_ptr<AssignmentNode> Parser::assignment() {
        if (top().get_type() == TOKEN_TYPE::IDENTIFIER) {
            Token var = top();
            advance();
            advance();
            auto ex = expr();
            return std::make_unique<AssignmentNode>(var, std::move(ex));
        }
        throw ParserException("Expected Identifier.");
    }

    std::unique_ptr<ExprNode> Parser::factor() {
        Token &current = top();

        if (current.get_token() == "-") {
            Token value = current;
            advance();
            auto factor_value = factor();
            return std::make_unique<UnaryNode>(value, std::move(factor_value));
        }

        if (current.get_type() == TOKEN_TYPE::IDENTIFIER) {
            Token value = current;
            advance();
            return std::make_unique<VariableNode>(value);
        }

        if (current.get_type() == TOKEN_TYPE::INTEGER_VALUE) {
            Token value = current;
            advance();
            return std::make_unique<NumberNode>(value);
        }
        if (current.get_token() == "(") {
            advance();
            auto node = expr();
            if (top().get_token() != ")") {
                throw ParserException("Expecting closing ) on Lne: " + std::to_string(top().get_line()));
            }
            advance();
            return node;
        }
        throw ParserException("Parser error on factor function: " + current.get_token());
    }
    std::unique_ptr<ExprNode> Parser::term() {
        auto left = factor();
        while (top().get_token() == "*" || top().get_token() == "/") {
            Token op = top();
            advance();
            auto right = factor();
            left = std::make_unique<BinaryNode>(std::move(left), op, std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> Parser::expr() {
        auto left = term();
        while (top().get_token() == "+" || top().get_token() == "-") {
            Token op = top();
            advance();
            auto right = term();
            left = std::make_unique<BinaryNode>(std::move(left), op, std::move(right));
        }
        return left;
    }

    void Parser::error_message(const std::string &message, int line) { std::println("MXParser: Error: {} on Line: {}", message, line); }
} // namespace mx
