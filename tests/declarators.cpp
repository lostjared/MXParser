#include <MXParser/parser.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    int failures = 0;
    using K = mx::TypeKind;
    using S = mx::ScalarKind;
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
    std::unique_ptr<mx::ExprNode> expression(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_expression();
    }
    mx::DeclarationNode &declaration(mx::ProgramNode &program, size_t index) { return dynamic_cast<mx::DeclarationNode &>(*program.declarations.at(index)); }
    void round_trip(const std::string &source) {
        auto first = parse(source)->to_string();
        auto second = parse(first)->to_string();
        check(first == second, "Unstable program print: " + first + "\nThen: " + second);
    }
    void rejects(const std::string &source, const std::string &diagnostic = {}, bool is_expression = false) {
        try {
            if (is_expression)
                expression(source);
            else
                parse(source);
            check(false, "Accepted invalid or excluded declarator: " + source);
        } catch (const mx::ParserException &error) {
            check(error.location.line > 0 && !error.location.token.empty(), "Missing diagnostic location");
            if (!diagnostic.empty())
                check(std::string(error.what()).find(diagnostic) != std::string::npos, "Wrong diagnostic: " + std::string(error.what()));
        }
    }
    long double evaluate(const std::string &source) {
        auto program = parse(source);
        mx::Evaluate evaluator;
        program->accept(evaluator);
        evaluator.call("main");
        return evaluator.result();
    }

    void binding() {
        auto ast = parse("const int *p, count=3, * const q=0; int *items[4]; int (*row)[4]; int matrix[2][3];");
        auto &mixed = declaration(*ast, 0);
        check(mixed.declarators[0].type->kind == K::Pointer && mixed.declarators[0].type->element->top_qualifiers().is_const, "Pointee const belongs to p");
        check(mixed.declarators[1].type->kind == K::Qualified && mixed.declarators[1].type->unqualified().scalar == S::Int, "count must be an independent scalar declarator");
        check(mixed.declarators[2].type->kind == K::Qualified && mixed.declarators[2].type->qualifiers.is_const && mixed.declarators[2].type->unqualified().kind == K::Pointer, "q must be a const pointer");
        auto items = declaration(*ast, 1).declarators[0].type;
        auto row = declaration(*ast, 2).declarators[0].type;
        auto matrix = declaration(*ast, 3).declarators[0].type;
        check(items->kind == K::Array && items->bound == 4 && items->element->kind == K::Pointer, "Array of pointers");
        check(row->kind == K::Pointer && row->element->kind == K::Array && row->element->bound == 4, "Pointer to array");
        check(matrix->bound == 2 && matrix->element->bound == 3, "Multidimensional array binding");
        check(items->declaration("items") == "int *items[4]" && row->declaration("row") == "int (*row)[4]", "Named declarator printing");
        auto bounds = parse("int values[1+2]; int hex[0x10UL]; int binary[0b10]; int incomplete[];");
        auto expression_bound = declaration(*bounds, 0).declarators[0].type;
        check(!expression_bound->bound && expression_bound->bound_expression && expression_bound->bound_expression->to_string() == "(1+2)", "Preserve unevaluated bound AST");
        check(declaration(*bounds, 1).declarators[0].type->bound == 16 && declaration(*bounds, 2).declarators[0].type->bound == 2, "Literal array extents");
        check(!declaration(*bounds, 3).declarators[0].type->bound_expression, "Omitted array extent");
        auto explicit_size = expression("sizeof(int (*)[1+2])");
        auto type = dynamic_cast<mx::SizeofNode &>(*explicit_size).type;
        check(type->kind == K::Pointer && type->element->kind == K::Array && type->element->bound_expression, "Abstract pointer-to-array");
        auto cast = expression("(int (*)[4])p");
        check(dynamic_cast<mx::CastNode &>(*cast).type->element->bound == 4, "Cast abstract declarator");
        auto size = expression("sizeof(const int * const [3])");
        auto size_type = dynamic_cast<mx::SizeofNode &>(*size).type;
        check(size_type->kind == K::Array && size_type->element->qualifiers.is_const && size_type->element->unqualified().element->top_qualifiers().is_const, "Abstract array qualifiers");
    }

    void functions() {
        auto zero = parse("int f(void); int g(); typedef int F(void); F h;");
        check(dynamic_cast<mx::FunctionNode &>(*zero->declarations[0]).type->has_prototype && !dynamic_cast<mx::FunctionNode &>(*zero->declarations[1]).type->has_prototype, "Preserve typed versus unspecified parameter lists");
        check(dynamic_cast<mx::FunctionNode &>(*zero->declarations[3]).to_string() == "int h(void);", "Function typedef prototype survives printing");
        auto ast = parse("int *f(int), value, *g(void); int (*row(void))[4]; int sum(int values[][3], int n) { return values[0][0]+n; }");
        auto &f = dynamic_cast<mx::FunctionNode &>(*ast->declarations[0]);
        auto &g = dynamic_cast<mx::FunctionNode &>(*ast->declarations[2]);
        auto &row = dynamic_cast<mx::FunctionNode &>(*ast->declarations[3]);
        auto &sum = dynamic_cast<mx::FunctionNode &>(*ast->declarations[4]);
        check(f.result_type()->kind == K::Pointer && g.result_type()->kind == K::Pointer && declaration(*ast, 1).declarators[0].type->kind == K::Scalar, "Mixed function/object declarators");
        check(row.result_type()->kind == K::Pointer && row.result_type()->element->kind == K::Array, "Function returning pointer to array");
        check(sum.parameters[0].type->kind == K::Pointer && sum.parameters[0].type->element->kind == K::Array && sum.parameters[0].type->element->bound == 3, "Array parameter adjustment");
        check(sum.parameters[0].declaration_type->kind == K::Array && sum.signature_type()->parameters[0] == sum.parameters[0].type, "Preserve source parameter type and adjusted signature");
        auto qualified_array = parse("typedef int A[4]; int f(const A values);");
        auto &qualified_parameter = dynamic_cast<mx::FunctionNode &>(*qualified_array->declarations[1]).parameters[0];
        check(qualified_parameter.type->kind == K::Pointer && qualified_parameter.type->element->top_qualifiers().is_const, "Array typedef qualification survives parameter adjustment");
        auto local = parse("int main(void) { int f(int), value=0; return value; }");
        auto &main = dynamic_cast<mx::FunctionNode &>(*local->declarations[0]);
        auto &group = dynamic_cast<mx::DeclarationNode &>(*main.body->statements[0]);
        check(group.declarators[0].type->kind == K::Function && group.declarators[1].type->kind == K::Scalar, "Block prototypes use declaration grammar");
        auto printed_local = parse("int main(void) { int f(int n, int a[4]); return 0; }")->to_string();
        check(printed_local.find("int f(int n, int a[4]);") != std::string::npos, "Block prototype printing preserves declared parameters");
        round_trip(printed_local);

        for (const auto &source : {"int *f(int), value, *g(void);", "int (*row(void))[4];", "int sum(int values[][3], int n) { return values[0][0]+n; }", "int main(void) { int f(int), value=0; return value; }", "int (f)(int n) { return n; }"})
            round_trip(source);
    }

    void typedefs() {
        const std::vector<std::string> sources = {"typedef int T; int main(void) { T n=2; { int T=3; n+=T; } return (T)n; }", "typedef int T; int main(void) { for(int T=0;T<2;++T) {} T n=4; return n; }", "typedef double T; int f(int T) { return T; } T value; int main(void) { return f(3); }", "typedef int T; int main(void) { int T=2; return sizeof(T); }", "typedef int T; int main(void) { int (T)=2; return (T)+1; }", "typedef int T; typedef T U; int main(void) { typedef double T; U n=3; return n; }", "typedef int *P, A[4]; P p; const P q=0; A values; A *row;", "typedef int F(int); F f; int f(int x) { return x; }", "typedef void V; int f(V);", "typedef int T; int f(int T); T value;", "typedef int T; int f(int T, int a[sizeof(T)]);"};
        for (const auto &source : sources)
            round_trip(source);
        check(evaluate(sources[0]) == 5, "Ordinary block shadowing and typedef casts");
        check(evaluate(sources[1]) == 4, "For declaration scope must end after the loop");
        check(evaluate(sources[2]) == 3, "Parameters shadow typedefs only inside their function");
        check(evaluate(sources[4]) == 3, "Parenthesized identifier can shadow a typedef");
        check(evaluate(sources[5]) == 3, "Typedef definitions retain their original binding");
        auto point = parse(sources[3]);
        auto &main = dynamic_cast<mx::FunctionNode &>(*point->declarations[1]);
        auto &ret = dynamic_cast<mx::ReturnNode &>(*main.body->statements[1]);
        auto &size = dynamic_cast<mx::SizeofNode &>(*ret.expression);
        check(size.value && dynamic_cast<mx::VariableNode *>(size.value.get()), "Ordinary names disambiguate sizeof operands");
        auto init = parse("typedef int T; int main(void) { int T=sizeof(T); return T; }");
        auto &function = dynamic_cast<mx::FunctionNode &>(*init->declarations[1]);
        auto &decl = dynamic_cast<mx::DeclarationNode &>(*function.body->statements[0]);
        check(dynamic_cast<mx::SizeofNode &>(*decl.declarators[0].initializer).value != nullptr, "Name becomes visible before its initializer");
        auto frozen = parse(sources[5]);
        auto &last = dynamic_cast<mx::FunctionNode &>(*frozen->declarations[2]);
        auto &object = dynamic_cast<mx::DeclarationNode &>(*last.body->statements[1]);
        check(object.declarators[0].type->unqualified().scalar == S::Int, "Typedef alias retains concrete definition despite later shadowing");
        auto aliases = parse(sources[6]);
        check(declaration(*aliases, 0).is_typedef && declaration(*aliases, 1).declarators[0].type->unqualified().kind == K::Pointer, "Typedef declaration metadata");
        auto qualified_pointer = declaration(*aliases, 2).declarators[0].type;
        check(qualified_pointer->top_qualifiers().is_const && !qualified_pointer->unqualified().element->top_qualifiers().is_const, "Qualifying a pointer typedef qualifies the pointer");
        auto parameters = parse(sources[10]);
        auto &prototype = dynamic_cast<mx::FunctionNode &>(*parameters->declarations[1]);
        auto &bound = dynamic_cast<const mx::SizeofNode &>(*prototype.parameters[1].declaration_type->bound_expression);
        check(bool(bound.value), "Earlier parameter names shadow typedefs inside later array bounds");
        check(evaluate("typedef double Real; typedef int Count; Real half(Real n) { Real a=n/2; return a; } int main(void) { Count n=3; return (Count)half(n); }") == 1, "Numeric typedefs execute through resolved types");
        try {
            evaluate("typedef const int C; int main(void) { C n=1; n=2; return n; }");
            check(false, "Const typedef write accepted by evaluator");
        } catch (const std::runtime_error &) {
        }
    }

    void invalid() {
        for (const auto &source : {"int a[;", "int (*row[4];", "int f(int x,);", "int f(int, int) { return 0; }", "typedef int T=2;", "typedef int T; double T;", "int T; typedef int T;", "int main(void) { { typedef int T; } T n; }", "typedef int T; int f(int T, T n);", "typedef int T; int f(int T) { typedef int T; return 0; }", "int main(void) { for(typedef int T;;) {} }", "int f(int x, int x);", "int f()=1;", "int main(void) { int f(void) { return 0; } }", "int a[2](int);", "int f(void)[2];", "int f(void)(int);"})
            rejects(source);
        for (const auto &source : {"int (*callback)(int);", "typedef int F(int); F *callback;", "int f(int callback(int));", "int f(int (*callback)(int));"})
            rejects(source, "function pointer");
        rejects("int f(int, ...);", "variadic");
        rejects("sizeof(int (*)(int))", "function pointer", true);
        rejects("sizeof(int named)", {}, true);
        rejects("sizeof(static int)", "storage class", true);
        rejects("int main(void) { static int n; return n; }", "block-scope storage");
        for (const auto &source : {"int values[4]; int main(void) { return 0; }", "int *p; int main(void) { return 0; }"}) {
            try {
                evaluate(source);
                check(false, "Unsupported object type executed");
            } catch (const std::runtime_error &) {
            }
        }
    }
} // namespace

int main() {
    try {
        binding();
        functions();
        typedefs();
        invalid();
    } catch (const std::exception &error) {
        check(false, std::string("Unexpected exception: ") + error.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
