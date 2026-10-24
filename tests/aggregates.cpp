#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    int failures = 0;
    void check(bool value, const std::string &message) {
        if (!value) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream stream(source);
        mx::Parser parser(stream);
        return parser.parse_program();
    }
    void accepts(const std::string &source) {
        try {
            auto ast = parse(source);
            const auto printed = ast->to_string();
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            check(ast->to_string() == printed, "Validation mutated syntax");
            auto second = parse(printed);
            analyzer.validate(*second);
            check(second->to_string() == printed, "Unstable aggregate round trip: " + source);
        } catch (const std::exception &error) {
            check(false, "Rejected valid input: " + source + "\n" + error.what());
        }
    }
    void rejects(const std::string &source, const std::string &diagnostic) {
        try {
            auto ast = parse(source);
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            check(false, "Accepted invalid input: " + source);
        } catch (const mx::SemanticException &error) {
            check(error.location.line && error.location.column && !error.location.token.empty(), "Missing semantic location");
            check(std::string(error.what()).find(diagnostic) != std::string::npos, "Unexpected error: " + std::string(error.what()));
        } catch (const mx::ParserException &error) {
            check(error.location.line && (error.location.column || error.location.token == "end of input") && !error.location.token.empty(), "Missing parser location");
            check(std::string(error.what()).find(diagnostic) != std::string::npos, "Unexpected error: " + std::string(error.what()));
        }
    }
    void facts() {
        auto ast = parse("struct S; struct S *p; struct S { char c; long n; short a[3]; }; struct S objects[2]; char text[]=\"a\\n\" \"b\"; enum E { A=-2, B, C=sizeof(struct S), D }; enum E e=C; void inspect(void) { p->n; sizeof objects; sizeof text; D; }");
        mx::SemanticAnalyzer analyzer;
        analyzer.validate(*ast);
        auto symbol = analyzer.symbols().lookup("objects");
        check(symbol && mx::ScalarDataModel::size_of(symbol->type) == 48, "Struct padding and array size");
        check(symbol->type->element->layout->alignment == 8, "Struct natural alignment");
        check(analyzer.symbols().lookup("text")->type->bound == 4, "String escape decoding and inferred bound");
        auto &function = dynamic_cast<mx::FunctionNode &>(*ast->declarations.back());
        const auto &statements = function.body->statements;
        auto info = [&](size_t i) -> const mx::ExpressionInfo & { return analyzer.info(dynamic_cast<mx::ExprNode &>(*statements[i])); };
        check(info(0).member_offset == 8 && info(0).lvalue && info(0).modifiable, "Member offset and category");
        check(info(1).integer_constant == 48 && info(2).integer_constant == 4, "sizeof complete aggregates and inferred arrays");
        check(info(3).integer_constant == 25 && !info(3).lvalue, "Enum successor constant");
        auto &definition = dynamic_cast<mx::DeclarationNode &>(*ast->declarations[2]);
        check(definition.type->definition && definition.type->definition->members.size() == 3, "Retain struct member syntax");
        const auto &member = definition.type->definition->members.front()->declarators.front();
        check(member.location.line == 1 && member.location.column > 0 && member.location.token == "c", "Retain member source location");
        auto layout = symbol->type->element;
        auto bad = parse("struct Bad { struct Bad self; };");
        try {
            analyzer.validate(*bad);
            check(false, "Expected recursive member error");
        } catch (const mx::SemanticException &) {
        }
        analyzer.validate(*ast);
        check(mx::ScalarDataModel::size_of(layout) == 24, "Resolved layout survives analyzer reuse");
        auto init = parse("int a[2]={1,2};");
        auto &decl = dynamic_cast<mx::DeclarationNode &>(*init->declarations.front());
        auto &list = dynamic_cast<mx::InitializerListNode &>(*decl.declarators.front().initializer);
        check(list.location.column == 10 && list.elements.size() == 2, "Initializer AST and location");
        for (int visitor = 0; visitor < 3; ++visitor) {
            try {
                mx::Evaluate evaluator;
                mx::ICode code;
                mx::HTMLCode html;
                if (visitor == 0)
                    list.accept(evaluator);
                if (visitor == 1)
                    list.accept(code);
                if (visitor == 2)
                    list.accept(html);
                check(false, "Unsupported brace execution accepted");
            } catch (const std::runtime_error &error) {
                check(std::string(error.what()).find("brace initializers") != std::string::npos, "Explicit unsupported initializer visitor");
            }
        }
    }
} // namespace

int main() {
    const std::vector<std::string> valid = {"struct Node; struct Node { int value; struct Node *next; }; enum Kind { FIRST=1, SECOND=FIRST+1, }; struct Node nodes[SECOND]; int main(void) { struct Node *p=&nodes[0]; p->value=SECOND; return nodes[0].value; }",
                                            "typedef struct { int x; int y; } Pair; Pair pairs[]={{1,2},{3}}; int main(void) { return pairs[1].x; }",
                                            "struct S { char c; int a[2]; struct { long n; } nested; }; struct S s={'a',{1},{2}};",
                                            "struct Inner { int x; }; struct Outer { struct Inner inner; int z; }; struct Outer a[]={{{1},2},{{3}}};",
                                            "enum { A, B=4, C=B*2, D='\\n', E=sizeof(int[2+1]) }; int a[C];",
                                            "enum E { A=-2147483647-1, B, C=2147483647 }; enum E e=A; int main(void) { return e+B; }",
                                            "enum { A=2147483647, B=0 };",
                                            "enum { A=(int)3.5, B=1?4:1/0 }; int a[A+B];",
                                            "int values[]={1,2,3,}; int matrix[][2]={{1},{2,3}};",
                                            "char s[]=\"a\\n\\x42\\101\"; signed char t[2]=\"hi\"; unsigned char u[]={\"hi\"};",
                                            "char s[8]=\"a\\0b\"; char t[3]={\"abc\"};",
                                            "int x={3}; int *p={0}; int a[2]={0};",
                                            "struct S { int x; }; struct S f(struct S s) { struct S result={0}; result=s; return result; }",
                                            "struct S { const int x; int y; }; struct S a={1,2}; int main(void) { a.y=3; return a.x; }",
                                            "struct S { int x; }; const struct S s={1}; int main(void) { return s.x; }",
                                            "struct S; typedef struct S S; struct S { int x; }; S items[]={{1},{2}};",
                                            "struct S { int x; }; int main(void) { struct S { long y; } s={2}; return sizeof s; }",
                                            "struct S { int x; }; int main(void) { struct S; struct S *p=0; return p==0; }",
                                            "typedef int T; struct S { int T; T value; }; T x;",
                                            "enum E { A=2 }; int main(void) { enum E { A=3 }; int a[A]; return sizeof a; }",
                                            "enum E { A=2 }; enum E e; enum E f(enum E e) { return e; }",
                                            "int main(void) { int n=2; int a[2]={n,n+1}; struct S { int n; } s={n}; return s.n+a[0]; }",
                                            "typedef int A[3]; const A a={1,2}; int main(void) { return a[1]; }",
                                            "int f(int a[]) { return a[0]; } int g(int a[2+3]) { return sizeof a; }",
                                            "extern int a[]; int a[2]; int main(void) { return sizeof a; }",
                                            "struct S; struct S { struct S *next; }; struct S s={0};",
                                            "struct S { int n; }; int main(void) { struct S s={1}; s=(s); return s.n; }",
                                            "enum { N=sizeof(struct { char c; long n; }) }; int a[N];",
                                            "int a[sizeof(char[3])]; int b['\\x03']; int c[(int)2.9];",
                                            "int a[1<<4]; int b[0xffffffffu+2u]; int c[1?3:1/0];",
                                            "int a[1 || (1/0)]; int b[1+ (0 && (1/0))];",
                                            "extern int a[3]; int a[]={1}; int main(void) { return sizeof a; }",
                                            "struct S { int x; }; struct S f(int c, struct S a, struct S b) { return c?a:b; }",
                                            "struct S { int x; } a, f(void), b; struct S f(void) { return a; }",
                                            "typedef enum { A=2, B } E; E e=B;",
                                            "typedef int A; int main(void) { enum { A=sizeof(A) }; return A; }",
                                            "typedef int A[]; A a={1,2}, b={3}; int main(void) { return sizeof a+sizeof b; }",
                                            "enum { N=2 }; int main(void) { switch(N) { case N: return 1; default: return 0; } }",
                                            "char rows[][3]={\"hi\",{\"ok\"}};",
                                            "struct S { enum { A=2 } e; int a[A]; }; struct S s={A,{1,2}};",
                                            "struct S { char text[3]; int values[2]; }; struct S s={\"hi\",{1}};",
                                            "struct S { int x; }; int main(void) { struct S s={1}; const struct S *p=&s; return p->x; }"};
    for (const auto &source : valid)
        accepts(source);
    const std::vector<std::pair<std::string, std::string>> invalid = {{"struct S { struct S { int x; } field; };", "Duplicate tag definition"},
                                                                      {"struct S { struct S self; };", "complete object"},
                                                                      {"struct S { struct S children[2]; };", "complete object"},
                                                                      {"struct A; struct B { struct A a; };", "complete object"},
                                                                      {"struct S { int x; int x; };", "Duplicate struct member"},
                                                                      {"struct S { void x; };", "complete object"},
                                                                      {"struct S { int f(void); };", "complete object"},
                                                                      {"struct S { int x[]; };", "complete object"},
                                                                      {"struct S { int x=1; };", "cannot have initializers"},
                                                                      {"struct S { typedef int T; };", "storage classes"},
                                                                      {"struct S {};", "Empty aggregate"},
                                                                      {"struct S { int x; }; struct S { int y; };", "Duplicate tag definition"},
                                                                      {"struct S; struct S s;", "incomplete struct"},
                                                                      {"struct S { int x; }; int main(void) { struct S s; return s.missing; }", "no member named missing"},
                                                                      {"struct S { int x; }; const struct S s={1}; int main(void) { s.x=2; return 0; }", "modifiable lvalue"},
                                                                      {"struct S { int a[2]; }; const struct S s={{1}}; int main(void) { s.a[0]=2; return 0; }", "modifiable lvalue"},
                                                                      {"struct S { const int x; }; struct S a={1}, b={2}; int main(void) { a=b; return 0; }", "modifiable lvalue"},
                                                                      {"enum E;", "enum forward"},
                                                                      {"enum E e;", "enum forward"},
                                                                      {"enum { A=1, A=2 };", "Duplicate enumerator"},
                                                                      {"int A; enum { A };", "Duplicate enumerator"},
                                                                      {"enum { A=2147483647, B };", "fit int"},
                                                                      {"enum { A=0xffffffffu };", "fit int"},
                                                                      {"int n=2; enum { A=n };", "integer constant expression"},
                                                                      {"enum { A=1/0 };", "integer constant expression"},
                                                                      {"enum { A=2147483647+1 };", "integer constant expression"},
                                                                      {"enum { A=B, B=1 };", "Undeclared identifier"},
                                                                      {"enum { A=A };", "Undeclared identifier"},
                                                                      {"enum { A=1.0 };", "integer constant expression"},
                                                                      {"typedef int A[2]; const A a={1}; int main(void) { a[0]=2; return 0; }", "modifiable lvalue"},
                                                                      {"int a[0];", "positive"},
                                                                      {"int a[-1];", "positive"},
                                                                      {"int n; int a[n];", "variable length"},
                                                                      {"int a[1/0];", "integer constant expression"},
                                                                      {"int a[2147483647+1];", "integer constant expression"},
                                                                      {"int a[1.5];", "integer type"},
                                                                      {"int a[];", "array extent"},
                                                                      {"int a[2][];", "complete object"},
                                                                      {"int a[18446744073709551615UL];", "exceeds the data model"},
                                                                      {"int a[2]={1,2,3};", "Too many elements"},
                                                                      {"extern int a[2]; int a[]={1,2,3};", "Too many elements"},
                                                                      {"struct S { int x; }; struct S s={1,2};", "Too many elements"},
                                                                      {"int a={1,2};", "exactly one element"},
                                                                      {"int a={{1}};", "Excess braces"},
                                                                      {"int a[2][2]={1,2};", "brace elision"},
                                                                      {"struct S { int x; }; struct S a={1}, b=a;", "aggregate copy initializer"},
                                                                      {"char s[2]=\"abc\";", "too long"},
                                                                      {"int a[2]=\"hi\";", "positional braces"},
                                                                      {"int n; int a[2]={n};", "File-scope initializer"},
                                                                      {"int n; int *a[1]={&n};", "address constants"},
                                                                      {"int a[2]={.x=1};", "designated initializer"},
                                                                      {"struct S { int x:2; };", "bit field"},
                                                                      {"struct S { int x; ", "Expected"},
                                                                      {"int a[2]={1,", "Expected"},
                                                                      {"enum E { A=", "Expected"}};
    for (const auto &[source, diagnostic] : invalid) {
        rejects(source, diagnostic);
    }
    rejects("struct S { int x; }; struct S *p; int main(void) { struct S { int x; } s; p=&s; return 0; }", "Incompatible assignment");
    try {
        facts();
    } catch (const std::exception &error) {
        check(false, error.what());
    }
    std::cout << valid.size() << " aggregate round trips and " << invalid.size() << " rejection cases checked\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
