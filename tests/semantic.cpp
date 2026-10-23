#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    int failures = 0;
    using S = mx::ScalarKind;
    using K = mx::TypeKind;
    void check(bool value, const std::string &message) {
        if (!value) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    void accepts(const std::string &source) {
        try {
            auto ast = parse(source);
            auto before = ast->to_string();
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            check(ast->to_string() == before, "Semantic analysis changed the AST");
        } catch (const std::exception &error) {
            check(false, "Rejected valid program: " + source + "\n" + error.what());
        }
    }
    void rejects(const std::string &source, const std::string &message = {}) {
        auto ast = parse(source);
        try {
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            check(false, "Accepted invalid semantics: " + source);
        } catch (const mx::SemanticException &error) {
            check(error.location.line > 0 && error.location.column > 0 && !error.location.token.empty(), "Missing semantic source location");
            check(std::string(error.what()).find("column ") != std::string::npos, "Missing diagnostic column");
            if (!message.empty())
                check(std::string(error.what()).find(message) != std::string::npos, "Unexpected diagnostic: " + std::string(error.what()));
        }
    }
    mx::ExpressionInfo expression(const std::string &source, const std::string &prefix = {}) {
        auto ast = parse(prefix + " void inspect(void) { " + source + "; }");
        mx::SemanticAnalyzer analyzer;
        analyzer.validate(*ast);
        auto &function = dynamic_cast<mx::FunctionNode &>(*ast->declarations.back());
        auto &expr = dynamic_cast<mx::ExprNode &>(*function.body->statements[0]);
        return analyzer.info(expr);
    }
    void model() {
        using M = mx::ScalarDataModel;
        for (auto type : {S::Bool, S::Char, S::SignedChar, S::UnsignedChar})
            check(M::size_of(mx::Type::scalar_type(type)) == 1, "One-byte scalar size");
        check(M::size_of(mx::Type::from_name("short")) == 2 && M::size_of(mx::Type::from_name("int")) == 4, "Short and int sizes");
        for (const auto &type : {"long", "unsigned long long", "double", "long double", "int *"})
            check(M::size_of(mx::Type::from_name(type)) == 8, "LP64 size: " + std::string(type));
        check(M::size_of(mx::Type::from_name("float")) == 4, "Float size");
        check(M::size_of(mx::Type::array(mx::Type::scalar_type(S::Int), 3)) == 12, "Array size");
        for (const auto &type : {"char", "unsigned short", "_Bool"})
            check(M::integer_promotion(mx::Type::from_name(type))->scalar == S::Int, "Integer promotion");
        check(M::arithmetic_type(mx::Type::from_name("unsigned int"), mx::Type::from_name("long"))->scalar == S::Long, "Long represents every unsigned int");
        check(M::arithmetic_type(mx::Type::from_name("unsigned long"), mx::Type::from_name("long long"))->scalar == S::UnsignedLongLong, "Signedness and rank conversion");
        check(M::arithmetic_type(mx::Type::from_name("long double"), mx::Type::from_name("float"))->scalar == S::LongDouble, "Floating rank conversion");
        const std::vector<std::pair<std::string, S>> cases = {{"1+2", S::Int}, {"2147483648", S::Long}, {"0xffffffff", S::UnsignedInt}, {"18446744073709551615UL", S::UnsignedLong}, {"1.0f+2", S::Float}, {"1.0L+2.0", S::LongDouble}, {"(unsigned short)1+(short)2", S::Int}, {"1U+2L", S::Long}, {"1UL+2LL", S::UnsignedLongLong}, {"(unsigned char)1<<2UL", S::Int}, {"1<2.0", S::Int}, {"1?2:3.0", S::Double}, {"sizeof(int)", S::UnsignedLong}, {"'x'", S::Int}, {"~(unsigned char)0", S::Int}};
        for (const auto &[text, kind] : cases)
            check(expression(text).type->unqualified().scalar == kind, "Expression type: " + text);
        for (const auto &text : {"0", "1-1", "(int)0.0", "sizeof(int)-4", "'\\0'", "!1", "(void*)0"})
            check(expression(text).null_pointer_constant, "Null pointer constant: " + std::string(text));
        check(!expression("1?0:0.0").null_pointer_constant, "Conditional expression type must determine null status");
        check(expression("0&&(2147483647+1)").null_pointer_constant, "Uncomputed constant expression can short circuit");
        check(!expression("((-2147483647-1)/-1)-(-2147483647-1)").null_pointer_constant, "Signed division overflow must not create a null pointer constant");
        check(expression("sizeof(\"hi\" \"!\")").integer_constant == 4, "Adjacent string extent");
        check(expression("sizeof(\"a\\x42\\101\")").integer_constant == 4, "Escaped string extent");
        auto literal = expression("\"hi\"");
        check(literal.type->kind == K::Array && literal.type->bound == 3 && literal.lvalue && !literal.modifiable, "String literal array type");
        check(expression("p-q", "int *p, *q;").type->scalar == S::Long, "Pointer difference type");
        check(expression("p+1", "int *p;").type->kind == K::Pointer, "Pointer arithmetic type");
        auto conditional = expression("1?p:q", "int *p; const int *q;");
        check(conditional.type->element->top_qualifiers().is_const, "Conditional pointer qualification");
    }
    void valid() {
        for (const auto &source : {"int f(int); int f(const int n) { return n; } int main(void) { return f(3.9); }",
                                   "typedef int T; int main(void) { T n=1; { double n=2; n+=1; } for(int n=0;n<3;++n) {} return n; }",
                                   "int factorial(int n) { if(n<2) return 1; return n*factorial(n-1); }",
                                   "void done(void) { return; } int main(void) { done(); return 0; }",
                                   "int main(void) { int n=0; while(1) { ++n; if(n==2) continue; break; } return n; }",
                                   "int main(void) { while(1) {} return 0; }",
                                   "int main(void) { int n=0; return sizeof(++n); }",
                                   "int *p=0; const int *q=(void*)0; int main(void) { int n=2; p=&n; q=p; return *q; }",
                                   "int main(void) { int values[4]; int (*p)[4]=&values; return (*p)[2]; }",
                                   "int main(void) { int values[4]; return 2[values]; }",
                                   "int main(void) { int *p=0; void *v=p; p=v; _Bool b=p; return b; }",
                                   "int main(void) { int **p=0; int * const *q=p; return q==p; }",
                                   "int main(void) { char *p=\"hi\"; const char *q=p; return q[0]; }",
                                   "extern int n; int n; int n; int n=2; extern int n;",
                                   "extern int values[]; int values[2]; int main(void) { return sizeof values; }",
                                   "static int n; extern int n; static int f(int); int f(int n) { return n; }",
                                   "int main(void) { int f(int); return f(1); } int f(int n) { return n; }",
                                   "struct S; int S; struct S *p; typedef struct S *P; int main(void) { P q=p; return q==p; }",
                                   "struct S; typedef struct S *P; int main(void) { struct S; P p=0; return p==0; }",
                                   "typedef const int C; int f(C n) { return n; } int main(void) { return f(1); }",
                                   "typedef int A[3]; int f(const A a) { return a[0]; }",
                                   "int f(void); void g(void) { f; (void)f; }",
                                   "typedef int T; typedef T F(T); int main(void) { typedef double T; F f; return f(1); } int f(int n) { return n; }",
                                   "struct S; typedef struct S *F(struct S *); struct S *p; int main(void) { struct S; F f; (void)f(p); return 0; } struct S *f(struct S *p) { return p; }"})
            accepts(source);
    }
    void invalid() {
        const std::vector<std::pair<std::string, std::string>> cases = {{"int main(void) { if(0) return missing; return 0; }", "Undeclared identifier"},
                                                                        {"int main(void) { return 0 && missing; }", "Undeclared identifier"},
                                                                        {"int main(void) { return 1?0:missing; }", "Undeclared identifier"},
                                                                        {"int main(void) { int n=1; int n=2; return n; }", "Duplicate"},
                                                                        {"int f(int n) { int n; return n; }", "Duplicate"},
                                                                        {"int f(void) { return hidden; } int main(void) { int hidden=1; return f(); }", "Undeclared"},
                                                                        {"int main(void) { for(int i=0;i<1;++i) {} return i; }", "Undeclared"},
                                                                        {"int main(void) { {int i=0;} return i; }", "Undeclared"},
                                                                        {"int main(void) { return later(); } int later(void) { return 0; }", "Undeclared"},
                                                                        {"double f(int); int f(int n) { return n; }", "Incompatible function"},
                                                                        {"int f(unsigned int); int f(long n) { return n; }", "Incompatible function"},
                                                                        {"int f(void) { return 1; } int f(void) { return 2; }", "Duplicate function"},
                                                                        {"int f(void); int f;", "Duplicate or incompatible"},
                                                                        {"int n; int n(void);", "conflicts with object"},
                                                                        {"int f(int); int main(void) { return f(); }", "argument count"},
                                                                        {"int f(int); int main(void) { int *p=0; return f(p); }", "Incompatible assignment"},
                                                                        {"int f(void); int main(void) { int f=1; return f(); }", "visible typed function"},
                                                                        {"void f(void) { if(0) return 1; }", "Void function"},
                                                                        {"int f(void) { return; }", "Non-void function"},
                                                                        {"void f(void); int main(void) { return f(); }", "Incompatible assignment"},
                                                                        {"int main(void) { 1=2; return 0; }", "modifiable lvalue"},
                                                                        {"int main(void) { const int n=1; if(0) n=2; return n; }", "modifiable lvalue"},
                                                                        {"typedef const int C; int main(void) { C n=1; ++n; return n; }", "modifiable lvalue"},
                                                                        {"typedef int A[3]; int main(void) { const A a; a[0]=1; return 0; }", "modifiable lvalue"},
                                                                        {"typedef int A[2][3]; int main(void) { const A a; a[0][0]=1; return 0; }", "modifiable lvalue"},
                                                                        {"int main(void) { int a[2],b[2]; a=b; return 0; }", "modifiable lvalue"},
                                                                        {"int main(void) { return 1.5%2; }", "integer operands"},
                                                                        {"int main(void) { return ~1.0; }", "Invalid operand"},
                                                                        {"int main(void) { return 1<<1.0; }", "integer operands"},
                                                                        {"int main(void) { int *p=0; return p*2; }", "arithmetic operands"},
                                                                        {"int main(void) { int *p=0; return p+p; }", "pointer arithmetic"},
                                                                        {"int main(void) { void *p=0; p++; return 0; }", "Increment"},
                                                                        {"int main(void) { const int *p=0; int *q=p; return 0; }", "Incompatible assignment"},
                                                                        {"int main(void) { int **p=0; const int **q=p; return 0; }", "Incompatible assignment"},
                                                                        {"int main(void) { int *p=0; double *q=p; return 0; }", "Incompatible assignment"},
                                                                        {"int main(void) { int *p=1; return 0; }", "Incompatible assignment"},
                                                                        {"int main(void) { int *p=(int*)1; return 0; }", "Invalid cast"},
                                                                        {"int main(void) { int *p=0; return (long)p; }", "Invalid cast"},
                                                                        {"int main(void) { const int *p=0; *p=1; return 0; }", "modifiable lvalue"},
                                                                        {"int main(void) { int * const p=0; p=0; return 0; }", "modifiable lvalue"},
                                                                        {"void f(void); int main(void) { if(f()) return 1; return 0; }", "scalar type"},
                                                                        {"int main(void) { return sizeof(void); }", "complete object"},
                                                                        {"struct S; int main(void) { return sizeof(struct S); }", "complete object"},
                                                                        {"struct S *p; int main(void) { p++; return 0; }", "Increment"},
                                                                        {"struct S; int main(void) { struct S item; return 0; }", "incomplete struct"},
                                                                        {"int main(void) { return (1).field; }", "struct type"},
                                                                        {"struct S *p; int main(void) { return p->field; }", "incomplete struct"},
                                                                        {"int main(void) { int n=0; return n[1]; }", "Subscript"},
                                                                        {"struct S; struct S items[2];", "complete object"},
                                                                        {"int main(void) { int a[2]; return a[1.0]; }", "Subscript"},
                                                                        {"int main(void) { return &1; }", "lvalue"},
                                                                        {"int main(void) { return *1; }", "Dereference"},
                                                                        {"restrict int n;", "restrict requires"},
                                                                        {"static extern int n;", "Conflicting"},
                                                                        {"int n=1; int n=2;", "Duplicate object definition"},
                                                                        {"int n; static int n;", "linkage"},
                                                                        {"static int n; int n;", "linkage"},
                                                                        {"int f(void); static int f(void);", "linkage"},
                                                                        {"int n=1; int m=n;", "File-scope initializer"},
                                                                        {"int n; int *p=&n;", "address constants"},
                                                                        {"int f();", "unprototyped"},
                                                                        {"int main() { return 0; }", "unprototyped"},
                                                                        {"int f(struct S *); int f(struct S *);", "Incompatible function"},
                                                                        {"struct S; struct S *p; int main(void) { struct S; struct S *q=0; p=q; return 0; }", "Incompatible assignment"},
                                                                        {"void a(void) { int f(int); } void b(void) { f(1); }", "Undeclared"},
                                                                        {"void a(void) { int f(int); } double f(int);", "Incompatible function"},
                                                                        {"int main(void) { return 18446744073709551615; }", "permitted C types"},
                                                                        {"int main(void) { return 18446744073709551616ULL; }", "exceeds"},
                                                                        {"double f(void) { return 1e999; }", "Floating literal"}};
        for (const auto &[source, message] : cases)
            rejects(source, message);
    }
    void facts_and_reuse() {
        auto ast = parse("int n; int main(void) {\n const int local=1;\n return local+n;\n}");
        mx::SemanticAnalyzer analyzer;
        analyzer.validate(*ast);
        auto &function = dynamic_cast<mx::FunctionNode &>(*ast->declarations[1]);
        auto &ret = dynamic_cast<mx::ReturnNode &>(*function.body->statements[1]);
        auto &sum = dynamic_cast<mx::BinaryNode &>(*ret.expression);
        check(analyzer.info(*sum.left).symbol && analyzer.info(*sum.left).symbol->kind == mx::SemanticSymbolKind::Object && !analyzer.info(*sum.left).modifiable, "Resolved const symbol fact");
        check(analyzer.info(*sum.right).symbol == analyzer.symbols().lookup("n"), "Reference resolves to translation-unit symbol");
        check(!analyzer.symbols().lookup("local"), "Local names must not leak into translation-unit symbols");
        auto bad = parse("int main(void) {\n  return unknown;\n}");
        try {
            analyzer.validate(*bad);
            check(false, "Missing semantic error");
        } catch (const mx::SemanticException &error) {
            check(error.location.line == 2 && error.location.column == 10 && error.location.token == "unknown", "Exact semantic error location");
        }
        analyzer.validate(*ast);
        check(analyzer.info(sum).type->scalar == S::Int, "Analyzer reusable after failure");
        auto jump_ast = parse("int main(void) { return 0; }");
        auto &main = dynamic_cast<mx::FunctionNode &>(*jump_ast->declarations[0]);
        auto jump = std::make_unique<mx::JumpNode>();
        jump->location = {1, "break", 18};
        main.body->statements.insert(main.body->statements.begin(), std::move(jump));
        try {
            analyzer.validate(*jump_ast);
            check(false, "AST jump context must be validated independently of parser");
        } catch (const mx::SemanticException &) {
        }
    }
} // namespace

int main() {
    try {
        model();
        valid();
        invalid();
        facts_and_reuse();
    } catch (const std::exception &error) {
        check(false, std::string("Unexpected exception: ") + error.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
