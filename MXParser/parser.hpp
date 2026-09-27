#pragma once
#include "MXParser/sym_tab.hpp"
#include <MXLex/scanner.hpp>
#include <MXLex/token.hpp>
#include <format>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace mx {

    std::string html_escape(const std::string &s);
    std::string js_escape(const std::string &s);

    class ParserException : public std::runtime_error {
      public:
        ParserException(const std::string &msg) : runtime_error(msg) {}

      private:
        std::string message;
    };

    class ParserEOF {};

    class Parser {
      public:
        Parser(std::istream &stream) : scanner{stream}, error_count{0} {}
        bool scan();
        bool parse();
        void error_message(const std::string &message, int line);
        void print_tokens(std::ostream &out);
        bool advance();
        Token &peek(size_t advance) const;
        Token &at(size_t index);
        Token &next();

      protected:
        Scanner scanner;
        SymbolTable table;
        size_t error_count = 0;
        size_t index = 0;
        std::vector<Token> tokens;
    };
} // namespace mx
