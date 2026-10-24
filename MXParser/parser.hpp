#pragma once
#include "MXParser/ast.hpp"
#include "MXParser/sym_tab.hpp"
#include <MXLex/scanner.hpp>
#include <MXLex/token.hpp>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mx {

    std::string html_escape(const std::string &s);
    std::string js_escape(const std::string &s);

    class ParserException : public std::runtime_error {
      public:
        ParserException(const std::string &msg) : runtime_error(msg) {}
        ParserException(const std::string &message, SourceLocation location) : runtime_error(std::format("{} at line {}{}, found '{}'", message, location.line, location.column ? std::format(", column {}", location.column) : "", location.token)), location(std::move(location)) {}
        SourceLocation location;
    };

    class ParserEOF {};

    class Parser {
      public:
        Parser(std::istream &stream) : scanner{stream}, error_count{0} {}
        bool scan();
        bool parse();
        /** Parse numeric C function definitions, prototypes, and global declarations. */
        std::unique_ptr<ProgramNode> parse_program();
        /** Parse a single C expression, allowing one trailing semicolon. */
        std::unique_ptr<ExprNode> parse_expression();
        /** Parse semicolon-separated C expressions without evaluating them. */
        std::vector<std::unique_ptr<ExprNode>> parse_expressions();
        /** Register a typedef name for cast and sizeof disambiguation. */
        void add_type_name(const std::string &name);
        void error_message(const std::string &message, int line);
        void print_tokens(std::ostream &out);
        void output_html(std::ostream &out, const std::string &html, std::vector<std::string> &state);
        bool advance();
        Token &peek(size_t offset);
        Token &at(size_t index);
        Token &next();
        Token &top();

      protected:
        Scanner scanner;
        SymbolTable table;
        size_t error_count = 0;
        size_t index = 0;
        std::vector<Token> tokens;
        std::optional<ParserException> scan_failure;
        [[noreturn]] void fail(const std::string &message);
        [[noreturn]] void fail_at(const Token &token, const std::string &message);
        std::unique_ptr<ASTNode> statement();
        std::unique_ptr<AssignmentNode> assignment();
        std::unique_ptr<ExprNode> factor();
        std::unique_ptr<ExprNode> term();
        std::unique_ptr<ExprNode> expr();
        std::unique_ptr<ExprNode> assignment_expr();
        std::unique_ptr<ExprNode> conditional();
        std::unique_ptr<ExprNode> binary(int min_precedence);
        std::unique_ptr<ExprNode> cast();
        std::unique_ptr<ExprNode> unary();
        std::unique_ptr<ExprNode> postfix();
        std::unique_ptr<ExprNode> primary();
        bool match(const std::string &text);
        void expect(const std::string &text);
        bool starts_type_name(size_t position) const;
        bool starts_declaration(size_t position) const;
        bool is_type_name(const std::string &name) const;
        TypePtr lookup_type(const std::string &name) const;
        void bind_name(const Token &name, TypePtr type = {});
        std::vector<std::unordered_map<std::string, TypePtr>> name_scopes{1};
        class ScopeGuard {
          public:
            ScopeGuard(Parser &parser, bool active = true) : parser(parser), active(active) {
                if (active)
                    parser.name_scopes.emplace_back();
            }
            ~ScopeGuard() {
                if (active)
                    parser.name_scopes.pop_back();
            }
            ScopeGuard(const ScopeGuard &) = delete;
            ScopeGuard &operator=(const ScopeGuard &) = delete;

          private:
            Parser &parser;
            bool active;
        };
        class DepthGuard {
          public:
            DepthGuard(size_t &depth) : depth(depth) { ++depth; }
            ~DepthGuard() { --depth; }
            DepthGuard(const DepthGuard &) = delete;
            DepthGuard &operator=(const DepthGuard &) = delete;

          private:
            size_t &depth;
        };
        struct DeclarationSpecifiers {
            TypePtr type;
            SourceLocation location;
            std::vector<std::string> storage;
            bool is_typedef = false;
        };
        struct DerivedDeclarator {
            TypeKind kind;
            TypeQualifiers qualifiers;
            std::shared_ptr<const ExprNode> bound;
            std::vector<Parameter> parameters;
            Token token;
            bool has_prototype = true;
        };
        struct ParsedDeclarator {
            Token name;
            std::vector<DerivedDeclarator> modifiers;
        };
        DeclarationSpecifiers declaration_specifiers(bool storage_allowed);
        ParsedDeclarator parse_declarator(bool abstract_allowed, bool names_allowed = true);
        std::vector<Parameter> parameter_list();
        TypePtr derive_type(TypePtr base, const ParsedDeclarator &declarator);
        std::unique_ptr<FunctionNode> function_node(const Declarator &declarator, const std::vector<std::string> &storage, SourceLocation location);
        TypePtr type_name();
        std::unique_ptr<ProgramNode> program();
        std::unique_ptr<BlockNode> block(bool new_scope = true);
        std::unique_ptr<ASTNode> function_statement(bool allow_declaration = false);
        std::unique_ptr<ASTNode> declaration(bool file_scope);
        Token identifier();
        size_t loop_depth = 0;
        size_t switch_depth = 0;
        std::set<std::string> type_names;
    };
} // namespace mx
