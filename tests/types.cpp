#include <MXParser/parser.hpp>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
    int failures = 0;
    void check(bool condition, const std::string &message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    }
    std::unique_ptr<mx::ExprNode> expression(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_expression();
    }
    std::unique_ptr<mx::ProgramNode> program(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    void location(const mx::SourceLocation &source, size_t line, const std::string &token, std::optional<size_t> column = {}) {
        if (column)
            check(source.column == *column, "Expected column " + std::to_string(*column) + ", got " + std::to_string(source.column));
        check(source.line == line && source.token == token, "Expected line " + std::to_string(line) + " token '" + token + "', got line " + std::to_string(source.line) + " token '" + source.token + "'");
    }
    void rejects(const std::string &source, bool is_program, size_t line, const std::string &token, const std::string &expected, size_t column) {
        try {
            if (is_program)
                program(source);
            else
                expression(source);
            check(false, "Accepted invalid input: " + source);
        } catch (const mx::ParserException &error) {
            location(error.location, line, token, column);
            const std::string text = error.what();
            check(column ? text.find(", column " + std::to_string(column) + ",") != std::string::npos : text.find(", column ") == std::string::npos, "Incorrect diagnostic column: " + text);
            check(text.find(expected) != std::string::npos && text.find("at line " + std::to_string(line)) != std::string::npos && text.find("found '" + token + "'") != std::string::npos, "Incomplete diagnostic: " + text);
        }
    }

    class Locations : public mx::ASTVisitor {
      public:
        void walk(mx::ASTNode &node) {
            check(node.location.line != 0 && node.location.column != 0 && !node.location.token.empty(), "Missing source on " + node.to_string());
            node.accept(*this);
        }
        void visit(mx::NumberNode &) override {}
        void visit(mx::LiteralNode &) override {}
        void visit(mx::VariableNode &) override {}
        void visit(mx::BinaryNode &node) override {
            walk(*node.left);
            walk(*node.right);
        }
        void visit(mx::UnaryNode &node) override { walk(*node.value); }
        void visit(mx::AssignmentNode &node) override { walk(*node.expression); }
        void visit(mx::ConditionalNode &node) override {
            walk(*node.condition);
            walk(*node.then_expr);
            walk(*node.else_expr);
        }
        void visit(mx::CallNode &node) override {
            walk(*node.callee);
            for (auto &argument : node.arguments)
                walk(*argument);
        }
        void visit(mx::SubscriptNode &node) override {
            walk(*node.base);
            walk(*node.subscript);
        }
        void visit(mx::MemberNode &node) override { walk(*node.base); }
        void visit(mx::CastNode &node) override { walk(*node.value); }
        void visit(mx::SizeofNode &node) override {
            if (node.value)
                walk(*node.value);
        }
        void visit(mx::ProgramNode &node) override {
            for (auto &declaration : node.declarations)
                walk(*declaration);
        }
        void visit(mx::FunctionNode &node) override {
            for (auto &parameter : node.parameters)
                check(parameter.location.line > 0 && parameter.location.column > 0, "Missing parameter source");
            if (node.body)
                walk(*node.body);
        }
        void visit(mx::DeclarationNode &node) override {
            for (auto &declarator : node.declarators) {
                check(declarator.location.line > 0 && declarator.location.column > 0 && declarator.location.token == declarator.name.get_token(), "Missing declarator source");
                if (declarator.initializer)
                    walk(*declarator.initializer);
            }
        }
        void visit(mx::BlockNode &node) override {
            for (auto &statement : node.statements)
                walk(*statement);
        }
        void visit(mx::ReturnNode &node) override {
            if (node.expression)
                walk(*node.expression);
        }
        void visit(mx::IfNode &node) override {
            walk(*node.condition);
            walk(*node.then_statement);
            if (node.else_statement)
                walk(*node.else_statement);
        }
        void visit(mx::WhileNode &node) override {
            walk(*node.condition);
            walk(*node.body);
        }
        void visit(mx::ForNode &node) override {
            if (node.initializer)
                walk(*node.initializer);
            if (node.condition)
                walk(*node.condition);
            if (node.update)
                walk(*node.update);
            walk(*node.body);
        }
        void visit(mx::JumpNode &) override {}
    };

    void types() {
        using S = mx::ScalarKind;
        using K = mx::TypeKind;
        const std::vector<std::pair<std::string, S>> scalars = {{"void", S::Void}, {"_Bool", S::Bool}, {"char", S::Char}, {"signed char", S::SignedChar}, {"unsigned char", S::UnsignedChar}, {"short int", S::Short}, {"unsigned short", S::UnsignedShort}, {"signed", S::Int}, {"unsigned", S::UnsignedInt}, {"long int", S::Long}, {"unsigned long", S::UnsignedLong}, {"long long int", S::LongLong}, {"unsigned long long", S::UnsignedLongLong}, {"float", S::Float}, {"double", S::Double}, {"long double", S::LongDouble}};
        for (const auto &[name, scalar] : scalars) {
            auto type = mx::Type::from_name(name);
            check(type->kind == K::Scalar && type->scalar == scalar && type->to_string() == name, "Scalar type: " + name);
            check(type->equals(*mx::Type::scalar_type(scalar)), "Scalar equality: " + name);
            auto ast = expression("sizeof(" + name + ")");
            auto &size = dynamic_cast<mx::SizeofNode &>(*ast);
            check(size.type && size.type->equals(*type), "Parsed sizeof type: " + name);
        }
        auto pointer = mx::Type::from_name("const unsigned long * volatile * const restrict");
        check(pointer->kind == K::Qualified && pointer->qualifiers.is_const && pointer->qualifiers.is_restrict, "Outermost pointer qualifiers");
        auto inner = pointer->element->element;
        check(inner->kind == K::Qualified && inner->qualifiers.is_volatile && inner->element->kind == K::Pointer, "Inner pointer qualifiers");
        check(inner->element->element->qualifiers.is_const && inner->element->element->unqualified().scalar == S::UnsignedLong, "Pointee qualifiers");
        check(!mx::Type::from_name("const int *")->equals(*mx::Type::from_name("int * const")), "Qualifier levels must be distinct");
        check(mx::Type::from_name("long unsigned int")->equals(*mx::Type::from_name("unsigned long")), "Equivalent specifier order");
        for (const auto &[name, tag] : std::vector<std::pair<std::string, mx::TagKind>>{{"struct", mx::TagKind::Struct}, {"union", mx::TagKind::Union}, {"enum", mx::TagKind::Enum}}) {
            auto ast = expression("(" + name + " item *)p");
            auto &cast = dynamic_cast<mx::CastNode &>(*ast);
            check(cast.type->kind == K::Pointer && cast.type->element->kind == K::Tagged && cast.type->element->tag == tag && cast.type->element->name == "item", "Tagged cast: " + name);
        }
        {
            std::istringstream input("(size_t)n + sizeof(size_t *)");
            mx::Parser parser(input);
            parser.add_type_name("size_t");
            auto ast = parser.parse_expression();
            auto &sum = dynamic_cast<mx::BinaryNode &>(*ast);
            check(dynamic_cast<mx::CastNode &>(*sum.left).type->kind == K::Named, "Registered typedef type");
            check(dynamic_cast<mx::SizeofNode &>(*sum.right).type->element->name == "size_t", "Registered typedef pointer target");
        }
        auto integer = mx::Type::scalar_type(S::Int);
        auto array = mx::Type::array(integer, 4);
        auto function = mx::Type::function(integer, {mx::Type::from_name("double"), mx::Type::pointer(integer)});
        check(array->to_string() == "int [4]" && array->bound == 4, "Array representation");
        check(mx::Type::pointer(array)->to_string() == "int (*)[4]", "Pointer-to-array printing");
        check(mx::Type::pointer(mx::Type::qualified(array, {.is_const = true}))->to_string() == "const int (*)[4]", "Pointer-to-qualified-array printing");
        check(mx::Type::qualified(mx::Type::pointer(array), {.is_const = true})->to_string() == "int (* const)[4]", "Qualified pointer-to-array printing");
        check(mx::Type::array(mx::Type::pointer(integer), 4)->to_string() == "int *[4]", "Array-of-pointer printing");
        check(function->to_string() == "int (double, int *)" && function->parameters.size() == 2, "Function representation");
        check(mx::Type::pointer(function)->to_string() == "int (*)(double, int *)", "Pointer-to-function printing");
        check(mx::Type::array(integer)->to_string() == "int []" && !mx::Type::array(integer)->equals(*array), "Incomplete and complete array types");
        check(!mx::Type::function(integer, {})->equals(*function), "Function signature equality");
        check(!mx::Type::tagged(mx::TagKind::Struct, "T")->equals(*mx::Type::tagged(mx::TagKind::Enum, "T")), "Tag kind equality");
        for (const auto &name : {"unsigned float", "long long double", "int int", "signed unsigned", "short long", "void int", "int garbage", "struct", ""}) {
            try {
                mx::Type::from_name(name);
                check(false, std::string("Accepted invalid type: ") + name);
            } catch (const std::invalid_argument &) {
            }
        }
    }

    void ast_and_sources() {
        auto ast = program("\n// skipped\nconst double global=1.5;\nfloat f(\n const int n, double x) {\n int a=1,b=2;\n if(n) { a++; } else ;\n while(a<3) { ++a; if(a==2) continue; break; }\n for(int i=0;i<n;++i) x+=i;\n return (float)(x+global);\n}\n");
        Locations locations;
        locations.walk(*ast);
        location(ast->location, 3, "const", 1);
        auto &global = dynamic_cast<mx::DeclarationNode &>(*ast->declarations[0]);
        location(global.location, 3, "const", 1);
        location(global.declarators[0].location, 3, "global", 14);
        check(global.type->kind == mx::TypeKind::Qualified && global.type->unqualified().scalar == mx::ScalarKind::Double, "Global structured type");
        auto &function = dynamic_cast<mx::FunctionNode &>(*ast->declarations[1]);
        location(function.location, 4, "float", 1);
        location(function.parameters[0].location, 5, "const", 2);
        location(function.body->location, 5, "{", 25);
        auto &loop = dynamic_cast<mx::ForNode &>(*function.body->statements[3]);
        location(loop.location, 9, "for", 2);
        location(loop.initializer->location, 9, "int", 6);
        check(function.type->kind == mx::TypeKind::Function && function.type->element->scalar == mx::ScalarKind::Float && function.type->parameters[0] == function.parameters[0].type, "Shared function signature types");
        auto printed = ast->to_string();
        check(program(printed)->to_string() == printed, "Typed program round trip");

        auto tree = expression("\n/* comment */\nf(\n(int)1,\n\"hello\"\n\"world\")[0].field->next++ ? sizeof(int *) : sizeof x");
        locations.walk(*tree);
        auto &conditional = dynamic_cast<mx::ConditionalNode &>(*tree);
        location(tree->location, 3, "f", 1);
        location(conditional.then_expr->location, 6, "sizeof", 29);
        auto &postfix = dynamic_cast<mx::UnaryNode &>(*conditional.condition);
        auto &arrow = dynamic_cast<mx::MemberNode &>(*postfix.value);
        auto &member = dynamic_cast<mx::MemberNode &>(*arrow.base);
        auto &subscript = dynamic_cast<mx::SubscriptNode &>(*member.base);
        auto &call = dynamic_cast<mx::CallNode &>(*subscript.base);
        location(call.arguments[0]->location, 4, "(", 1);
        location(call.arguments[1]->location, 5, "\"hello\"", 1);
        auto parenthesized = expression("\n(\n a+b)");
        location(parenthesized->location, 2, "(", 1);
        auto indented = expression("\r\n\t/* skip */  (const int *)p");
        location(indented->location, 2, "(", 14);
        auto &indented_cast = dynamic_cast<mx::CastNode &>(*indented);
        location(indented_cast.value->location, 2, "p", 27);
        std::istringstream digraph_input("  a<: 2 :>");
        mx::Parser digraph_parser(digraph_input);
        check(digraph_parser.scan(), "Digraph scanning failed");
        check(digraph_parser.peek(1).get_token() == "[" && digraph_parser.peek(1).get_column() == 4 && digraph_parser.peek(3).get_token() == "]" && digraph_parser.peek(3).get_column() == 9, "Digraph normalization must preserve columns");

        auto numeric = program("double half(double x) { double value=x/2; return (double)value; }");
        auto &half = dynamic_cast<mx::FunctionNode &>(*numeric->declarations[0]);
        half.return_type = "void";
        half.parameters[0].type_name = "void";
        dynamic_cast<mx::DeclarationNode &>(*half.body->statements[0]).type_name = "void";
        auto &ret = dynamic_cast<mx::ReturnNode &>(*half.body->statements[1]);
        dynamic_cast<mx::CastNode &>(*ret.expression).type_name = "void";
        mx::Evaluate evaluator;
        numeric->accept(evaluator);
        evaluator.call("half", {5.0});
        check(evaluator.result() == 2.5, "Execution must use structured types, not spelling snapshots");
        mx::ICode code;
        numeric->accept(code);
        check(code.code().find("FUNC half, double") != std::string::npos && code.code().find("CAST ") != std::string::npos && code.code().find("void") == std::string::npos, "Code generation must use structured types");
        auto legacy = mx::CastNode("double", expression("2"));
        evaluator.clear();
        legacy.accept(evaluator);
        check(evaluator.result() == 2 && legacy.type->scalar == mx::ScalarKind::Double, "Legacy string constructor");
    }

    void diagnostics() {
        rejects("\nint main(void) {\n return 1\n}\n", true, 4, "}", "Expected ';'", 1);
        rejects("\nint main(void) {\n return 1;\n", true, 4, "end of input", "Expected '}'", 0);
        rejects("\n(unsigned float)x", false, 2, "float", "Expected valid type specifiers", 11);
        rejects("\n(int int)x", false, 2, "int", "Duplicate type specifier", 6);
        rejects("\nsizeof(struct )", false, 2, ")", "Expected type tag", 15);
        rejects("\nx->int", false, 2, "int", "Expected member name", 4);
        rejects("\na b", false, 2, "b", "Expected end of input", 3);
        rejects("\nf(a,)", false, 2, ")", "Expected expression", 5);
        rejects("int f(int x,\nint x);", true, 2, "x", "Expected unique parameter name", 5);
        rejects("\nint f(void x);", true, 2, "void", "Expected a non-void parameter type", 7);
        rejects("\nint f(void){break;}", true, 2, "break", "Expected an enclosing loop", 13);
        rejects("\n`", false, 2, "`", "Expected a valid token", 1);
        rejects("\n1e+", false, 2, "invalid lexical input", "Invalid numeric literal", 0);
        rejects("\n\"unterminated", false, 2, "invalid lexical input", "Unterminated literal", 0);
        rejects("", false, 1, "end of input", "Expected expression", 0);
        rejects("\r\n  a b", false, 2, "b", "Expected end of input", 5);
        rejects("\n\t/* skip */  a b", false, 2, "b", "Expected end of input", 16);
        rejects("\n a +   ", false, 2, "end of input", "Expected expression", 0);
    }

    void lexer_coverage() {
        const std::vector<std::string> keywords = {"typedef", "struct", "enum", "do", "switch", "case", "default", "unsigned", "signed", "char", "short", "int", "long", "float", "double", "restrict", "const", "volatile", "void", "_Bool", "static", "extern", "if", "else", "while", "for", "return", "break", "continue", "sizeof"};
        const std::vector<std::string> symbols = {"0xffUL", ".5f", "'a'", "\"text\"", "->", "++", "--", "<<=", ">>=", "...", "<:", ":>", "(", ")", "[", "]", "{", "}", ";", ",", "?", ":", ".", "*", "&", "+", "-", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "|", "^", "~", "!", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="};
        std::string source;
        for (const auto &keyword : keywords)
            source += keyword + " ";
        source += "\n";
        for (const auto &symbol : symbols)
            source += symbol + " ";
        source += "/*skip*/";
        std::istringstream input(source);
        mx::Scanner scanner(input);
        mx::Token token;
        size_t column = 1;
        for (const auto &keyword : keywords) {
            check(scanner.lex(token) == mx::TOKEN_TYPE::IDENTIFIER && token.get_token() == keyword, "Lexer keyword coverage: " + keyword);
            check(token.get_line() == 1, "Lexer keyword line: " + keyword);
            check(token.get_column() == column, "Lexer keyword column: " + keyword);
            column += keyword.size() + 1;
        }
        column = 1;
        for (const auto &symbol : symbols) {
            auto kind = scanner.lex(token);
            check(kind != mx::TOKEN_TYPE::TOKEN_ERROR && kind != mx::TOKEN_TYPE::TOKEN_NULL && token.get_token() == symbol, "Lexer symbol coverage: " + symbol);
            check(token.get_line() == 2, "Lexer symbol line: " + symbol);
            check(token.get_column() == column, "Lexer symbol column: " + symbol);
            column += symbol.size() + 1;
        }
        check(scanner.lex(token) == mx::TOKEN_TYPE::TOKEN_NULL, "Lexer comment and EOF coverage");
    }

} // namespace

int main() {
    try {
        types();
        ast_and_sources();
        diagnostics();
        lexer_coverage();
    } catch (const std::exception &error) {
        check(false, std::string("Unexpected exception: ") + error.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
