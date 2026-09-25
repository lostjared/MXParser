#pragma once
#include "MXParser/sym_tab.hpp"
#include <MXLex/scanner.hpp>
#include <format>
#include <print>
#include <string>
#include <utility>

namespace mx {

    class ParserException : public std::runtime_error {
      public:
        ParserException(const std::string &msg) : runtime_error(msg) {}

      private:
        std::string message;
    };

    class Parser {
      public:
        Parser(std::istream &stream) : scanner{stream}, error_count{0} {}
        bool parse();
        void error_message(const std::string &message, int line);

      protected:
        Scanner scanner;
        SymbolTable table;
        size_t error_count;
    };
} // namespace mx
