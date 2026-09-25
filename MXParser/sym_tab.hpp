#pragma once
#include<string>
#include<print>
#include<set>
#include<vector>
#include<utility>
#include<optional>
#include<MXLex/scanner.hpp>

namespace mx {

    enum class SYMBOL_TYPE { SYMBOL_NOT_FOUND, SYMBOL_IDENTIFER, SYMBOL_FLOAT_VARIABLE, SYMBOL_INTEGER_VARIABLE, SYMBOL_FUNCTION, SYMBOL_KEYWORD};
    class SymbolException : public std::exception {};

    class Symbol {
    public:
        std::string sym_variable_name;
        std::string sym_variable_value;
        float sym_float_value = 0.0;
        int sym_int_value = 0.0;
        SYMBOL_TYPE sym_type = SYMBOL_TYPE::SYMBOL_NOT_FOUND;
        int depth = 0;
    };

    class SymbolTable {
    public:
        bool exists(const std::string &vname);
	bool exists_current_scope(const std::string &vname);
        void enter(const std::string &vname, const Symbol &symbol);
        std::optional<Symbol> lookup(const std::string &vname);
        bool is_keyword(const std::string &key);
        void print();
        void push_scope();
        void pop_scope();
        static std::string sym_to_string(SYMBOL_TYPE symbol);
    private:
        std::vector<std::vector<Symbol>> scope;
        std::string indent;
    };
}
