#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    int failures = 0;
    void check(bool condition, const std::string &message) {
        if (!condition) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    struct LegacyVisitor : mx::ASTVisitor {
        void visit(mx::NumberNode &) override {}
        void visit(mx::BinaryNode &) override {}
        void visit(mx::UnaryNode &) override {}
        void visit(mx::AssignmentNode &) override {}
        void visit(mx::VariableNode &) override {}
    };
    struct Audit : mx::ASTVisitor {
        std::string shape;
        std::set<std::string> kinds;
        std::unordered_set<const mx::Type *> types;
        std::vector<mx::ASTNode *> unsupported;
        void walk(mx::ASTNode *node) {
            if (node)
                node->accept(*this);
        }
        template <typename T> void walk(const std::unique_ptr<T> &node) { walk(node.get()); }
        void type(const mx::TypePtr &value) {
            if (!value || !types.insert(value.get()).second)
                return;
            shape += "T" + std::to_string(static_cast<int>(value->kind)) + ":" + value->name + "{";
            if (value->kind != mx::TypeKind::Named)
                type(value->element);
            if (value->bound_expression)
                walk(const_cast<mx::ExprNode *>(value->bound_expression.get()));
            for (const auto &parameter : value->parameters)
                type(parameter);
            if (value->definition) {
                for (const auto &member : value->definition->members)
                    walk(member);
                for (const auto &enumerator : value->definition->enumerators) {
                    shape += "enumerator:" + enumerator.name.get_token();
                    walk(enumerator.value);
                }
            }
            shape += "}";
        }
        void parameters(const std::vector<mx::Parameter> &values) {
            for (const auto &value : values) {
                type(value.declared_type());
                type(value.declaration_type);
            }
        }
        template <typename T, typename F> void record(T &node, const std::string &name, F children, bool runtime_unsupported = false) {
            kinds.insert(name);
            check(node.location.line && node.location.column && !node.location.token.empty(), name + " lacks a source location");
            check(!node.to_string().empty(), name + " has no printer");
            shape += name + "[";
            children();
            shape += "]";
            if (runtime_unsupported)
                unsupported.push_back(&node);
            if (name != "Number" && name != "Binary" && name != "Unary" && name != "Assignment" && name != "Variable") {
                LegacyVisitor visitor;
                try {
                    node.accept(visitor);
                    check(false, "Legacy visitor silently accepted " + name);
                } catch (const std::runtime_error &error) {
                    check(std::string(error.what()).starts_with("Visitor does not support"), "Unclear legacy visitor error: " + std::string(error.what()));
                }
            }
        }
        void visit(mx::NumberNode &node) override {
            record(node, "Number", [] {});
        }
        void visit(mx::VariableNode &node) override {
            record(node, "Variable", [] {});
        }
        void visit(mx::LiteralNode &node) override {
            record(node, "Literal", [] {});
        }
        void visit(mx::AssignmentNode &node) override {
            record(node, "Assignment", [&] { walk(node.expression); });
        }
        void visit(mx::BinaryNode &node) override {
            record(node, "Binary", [&] {
                walk(node.left);
                walk(node.right);
            });
        }
        void visit(mx::UnaryNode &node) override {
            record(node, "Unary", [&] { walk(node.value); });
        }
        void visit(mx::ConditionalNode &node) override {
            record(node, "Conditional", [&] {
                walk(node.condition);
                walk(node.then_expr);
                walk(node.else_expr);
            });
        }
        void visit(mx::CallNode &node) override {
            record(node, "Call", [&] {
                walk(node.callee);
                for (const auto &arg : node.arguments)
                    walk(arg);
            });
        }
        void visit(mx::SubscriptNode &node) override {
            record(
                node,
                "Subscript",
                [&] {
                    walk(node.base);
                    walk(node.subscript);
                },
                true);
        }
        void visit(mx::MemberNode &node) override {
            record(node, "Member", [&] { walk(node.base); }, true);
        }
        void visit(mx::CastNode &node) override {
            record(node, "Cast", [&] {
                type(node.declared_type());
                walk(node.value);
            });
        }
        void visit(mx::SizeofNode &node) override {
            record(
                node,
                "Sizeof",
                [&] {
                    if (node.type)
                        type(node.type);
                    walk(node.value);
                },
                true);
        }
        void visit(mx::InitializerListNode &node) override {
            record(
                node,
                "InitializerList",
                [&] {
                    for (const auto &value : node.elements)
                        walk(value);
                },
                true);
        }
        void visit(mx::ProgramNode &node) override {
            record(node, "Program", [&] {
                for (const auto &value : node.declarations)
                    walk(value);
            });
        }
        void visit(mx::FunctionNode &node) override {
            record(node, "Function", [&] {
                type(node.type);
                parameters(node.parameters);
                walk(node.body);
            });
        }
        void visit(mx::BlockNode &node) override {
            record(node, "Block", [&] {
                for (const auto &value : node.statements)
                    walk(value);
            });
        }
        void visit(mx::DeclarationNode &node) override {
            record(node, "Declaration", [&] {
                type(node.type);
                for (const auto &value : node.declarators) {
                    type(value.type);
                    parameters(value.parameters);
                    walk(value.initializer);
                }
            });
        }
        void visit(mx::ReturnNode &node) override {
            record(node, "Return", [&] { walk(node.expression); });
        }
        void visit(mx::IfNode &node) override {
            record(node, "If", [&] {
                walk(node.condition);
                walk(node.then_statement);
                walk(node.else_statement);
            });
        }
        void visit(mx::WhileNode &node) override {
            record(node, "While", [&] {
                walk(node.condition);
                walk(node.body);
            });
        }
        void visit(mx::DoWhileNode &node) override {
            record(
                node,
                "DoWhile",
                [&] {
                    walk(node.body);
                    walk(node.condition);
                },
                true);
        }
        void visit(mx::SwitchNode &node) override {
            record(
                node,
                "Switch",
                [&] {
                    walk(node.condition);
                    walk(node.body);
                },
                true);
        }
        void visit(mx::CaseNode &node) override {
            record(
                node,
                "Case",
                [&] {
                    walk(node.value);
                    walk(node.statement);
                },
                true);
        }
        void visit(mx::DefaultNode &node) override {
            record(node, "Default", [&] { walk(node.statement); }, true);
        }
        void visit(mx::ForNode &node) override {
            record(node, "For", [&] {
                walk(node.initializer);
                walk(node.condition);
                walk(node.update);
                walk(node.body);
            });
        }
        void visit(mx::JumpNode &node) override {
            record(node, "Jump", [] {});
        }
        void runtime_visitors() {
            for (auto node : unsupported) {
                mx::Evaluate evaluator;
                mx::ICode code;
                mx::HTMLCode html;
                for (auto visitor : std::vector<mx::ASTVisitor *>{&evaluator, &code, &html}) {
                    bool control = dynamic_cast<mx::DoWhileNode *>(node) || dynamic_cast<mx::SwitchNode *>(node) || dynamic_cast<mx::CaseNode *>(node) || dynamic_cast<mx::DefaultNode *>(node);
                    if (visitor != &evaluator && control)
                        continue;
                    try {
                        node->accept(*visitor);
                        check(false, "Runtime visitor silently accepted " + node->to_string());
                    } catch (const std::runtime_error &error) {
                        check(std::string(error.what()).starts_with("Visitor does not support"), "Unclear unsupported runtime operation");
                    }
                }
            }
        }
    };
    void malformed() {
        const std::vector<std::string> prefixes = {"int", "int n", "int n=", "int n[", "int n[2", "int (*n)[", "int n[2]={", "int n[2]={1,", "struct", "struct S {", "struct S { int n", "struct S { int n;", "enum E {", "enum E { A=", "enum E { A,", "int f(", "int f(int", "int f(int n,", "int f(void) {", "int f(void) { return", "int f(void) { return 1+", "int f(void) { if (", "int f(void) { if (1)", "int f(void) { if (1) {} else", "int f(void) { while (1)", "int f(void) { for (int n=0;", "int f(void) { do {}", "int f(void) { do {} while (1)", "int f(void) { switch (1) { case", "int f(void) { switch (1) { case 1:", "int f(void) { switch (1) { default:", "int f(void) { return sizeof(", "int f(void) { return (int)", "int f(void) { return f("};
        for (const auto &source : prefixes) {
            try {
                parse(source + "\n");
                check(false, "Accepted truncated input: " + source);
            } catch (const mx::ParserException &error) {
                check(error.location.line > 0 && error.location.token == "end of input", "Expected EOF location: " + std::string(error.what()));
                check(std::string(error.what()).find("Expected") != std::string::npos, "Missing expected syntax in EOF diagnostic");
            }
        }
        for (const auto &source : {"int n=;", "int f(void) { return 1 2; }", "int f(void) { else {} }", "int a[2]={,};"}) {
            try {
                parse(source);
                check(false, "Accepted malformed input");
            } catch (const mx::ParserException &error) {
                check(error.location.line && error.location.column && !error.location.token.empty(), "Missing offending-token location");
            }
        }
    }
} // namespace

int main(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;
    const std::filesystem::path root(argv[1]);
    std::ifstream manifest(root / "tests/subset/manifest.tsv");
    if (!manifest)
        return EXIT_FAILURE;
    std::vector<std::filesystem::path> sources{root / "examples/subset.c"};
    std::string row;
    while (std::getline(manifest, row)) {
        if (row.empty() || row.front() == '#')
            continue;
        std::istringstream fields(row);
        std::vector<std::string> values;
        std::string value;
        while (std::getline(fields, value, '\t'))
            values.push_back(value);
        if (values.size() == 6 && values[4] == "accept")
            sources.push_back(root / "tests/subset" / values[0]);
    }
    std::set<std::string> covered;
    for (const auto &path : sources) {
        try {
            std::ifstream input(path);
            if (!input)
                throw std::runtime_error("Cannot read " + path.string());
            const std::string source((std::istreambuf_iterator<char>(input)), {});
            auto ast = parse(source);
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            if (path.filename() == "subset.c") {
                auto items = analyzer.symbols().lookup("items");
                check(items && items->type->bound == 3 && mx::ScalarDataModel::size_of(items->type) == 72, "Combined example lost its array of structs");
                auto summary = analyzer.symbols().lookup("summarize");
                check(summary && summary->type->parameters.size() == 2 && summary->type->element->layout->size == 16, "Combined example lost its struct-valued function signature");
                auto &main = dynamic_cast<mx::FunctionNode &>(*ast->declarations.back());
                auto &outer = dynamic_cast<mx::IfNode &>(*main.body->statements[5]);
                auto &inner = dynamic_cast<mx::IfNode &>(*outer.then_statement);
                check(!outer.else_statement && inner.else_statement, "Dangling else bound to the wrong if");
            }
            const auto printed = ast->to_string();
            auto again = parse(printed);
            analyzer.validate(*again);
            check(printed == again->to_string(), "Unstable printing: " + path.string());
            Audit first, second;
            first.walk(ast.get());
            second.walk(again.get());
            check(first.shape == second.shape, "AST shape changed: " + path.string());
            covered.insert(first.kinds.begin(), first.kinds.end());
            first.runtime_visitors();
        } catch (const std::exception &error) {
            check(false, path.string() + ": " + error.what());
        }
    }
    mx::Token name, value;
    name.set_token("value", mx::TOKEN_TYPE::IDENTIFIER);
    name.set_line(1);
    name.set_column(1);
    value.set_token("1", mx::TOKEN_TYPE::INTEGER_VALUE);
    value.set_line(1);
    value.set_column(7);
    mx::AssignmentNode legacy(name, std::make_unique<mx::NumberNode>(value));
    Audit original;
    original.walk(&legacy);
    covered.insert(original.kinds.begin(), original.kinds.end());
    const std::set<std::string> expected = {"Number", "Binary", "Unary", "Assignment", "Variable", "Literal", "Conditional", "Call", "Subscript", "Member", "Cast", "Sizeof", "InitializerList", "Program", "Function", "Block", "Return", "Declaration", "If", "While", "DoWhile", "Switch", "Case", "Default", "For", "Jump"};
    check(covered == expected, "AST printer/visitor coverage is incomplete");
    malformed();
    std::cout << sources.size() << " translation-unit round trips; " << covered.size() << " AST kinds audited; 38 malformed inputs checked\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
