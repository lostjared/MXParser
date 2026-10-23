#pragma once
#include "MXParser/source.hpp"
#include "MXParser/type.hpp"
#include <MXLex/scanner.hpp>
#include <memory>
#include <optional>
#include <print>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mx {

    enum class SemanticSymbolKind { Object, Function, Typedef, Tag };
    struct SemanticSymbol {
        SemanticSymbolKind kind;
        TypePtr type;
        SourceLocation location;
        bool defined = false;
        bool internal_linkage = false;
    };

    /** Typed lexical scopes with independent ordinary-identifier and tag namespaces. */
    class SemanticSymbolTable {
      public:
        using Entry = std::shared_ptr<SemanticSymbol>;
        void push_scope();
        void pop_scope();
        size_t depth() const;
        Entry lookup(const std::string &name, bool tag = false) const;
        Entry lookup_current(const std::string &name, bool tag = false) const;
        Entry at_scope(size_t scope, const std::string &name, bool tag = false) const;
        bool enter(size_t scope, const std::string &name, Entry symbol, bool tag = false);

      private:
        struct Scope {
            std::unordered_map<std::string, Entry> ordinary;
            std::unordered_map<std::string, Entry> tags;
        };
        std::vector<Scope> scopes{1};
    };

    enum class SYMBOL_TYPE { SYMBOL_NOT_FOUND, SYMBOL_IDENTIFER, SYMBOL_FLOAT_VARIABLE, SYMBOL_INTEGER_VARIABLE, SYMBOL_FUNCTION, SYMBOL_KEYWORD };
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
        void print(std::ostream &out);

      private:
        std::vector<std::vector<Symbol>> scope;
        std::string indent;
    };
} // namespace mx
