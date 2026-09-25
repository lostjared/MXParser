#include "MXParser/parser.hpp"
#include "MXLex/token.hpp"
#include<iostream>
#include<string>
#include<fstream>

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
        Token token;
        try {
            mx::Token token;
            mx::TOKEN_TYPE token_type;
            while ((token_type = scanner.lex(token)) != mx::TOKEN_TYPE::TOKEN_NULL) {
                if (token_type == mx::TOKEN_TYPE::TOKEN_ERROR) {
                    std::cerr << token << "\n";
                    return false;
                }
                tokens.push_back(token);
            }
        } catch (const mx::ScannerError &e) {
            std::cerr << "MXLex: Token Error Exception: " << e.what() << "\n";
            return false;
        }

        if(tokens.empty()) {
            std::cerr << "MXLex: Error: no token returned.\n";
            return false;
        }
        return  true;
    }

    bool Parser::parse() {
        if(!scan())
            return false;

        return true;

    }

    void Parser::error_message(const std::string &message, int line) { std::println("MXParser: Error: {} on Line: {}", message, line); }
} // namespace mx
