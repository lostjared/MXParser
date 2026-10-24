#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
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
    long double number(const mx::ExecutionValue &value) {
        return std::visit(
            [](auto value) -> long double {
                using T = decltype(value);
                if constexpr (std::is_same_v<T, std::monostate>)
                    throw std::runtime_error("void result");
                else
                    return value;
            },
            value);
    }
    void accepts(const std::string &source, long double expected, const std::string &name = "main") {
        try {
            auto ast = parse(source);
            mx::Interpreter interpreter;
            interpreter.load(*ast);
            auto value = number(interpreter.call(name));
            check(std::abs(value - expected) < 1e-8L, "Wrong execution result " + std::to_string(static_cast<double>(value)) + ": " + source);
        } catch (const std::exception &error) {
            check(false, "Rejected execution: " + source + "\n" + error.what());
        }
    }
    void rejects(const std::string &source, const std::string &diagnostic, mx::ExecutionLimits limits = {}) {
        try {
            auto ast = parse(source);
            mx::Interpreter interpreter(limits);
            interpreter.load(*ast);
            interpreter.call();
            check(false, "Expected execution error: " + source);
        } catch (const std::exception &error) {
            check(std::string(error.what()).find(diagnostic) != std::string::npos, "Unexpected error: " + std::string(error.what()));
        }
    }
} // namespace
int main(int argc, char **argv) {
    const std::vector<std::pair<std::string, long double>> programs = {{"int main(void){unsigned int x=4294967295u;return x+2u;}", 1},
                                                                       {"int main(void){unsigned char x=255;x++;return x;}", 0},
                                                                       {"int main(void){signed char x=(signed char)255;return x;}", -1},
                                                                       {"int main(void){return -1 < 1u;}", 0},
                                                                       {"int main(void){int x=-8;return x>>2;}", -2},
                                                                       {"int main(void){unsigned int x=4294967295u;return (x>>31)+(x<<1==4294967294u);}", 2},
                                                                       {"int main(void){unsigned short x=65535;return x+1;}", 65536},
                                                                       {"int main(void){return sizeof(long double)+sizeof(void*);}", 16},
                                                                       {"int main(void){unsigned long x=18446744073709551615UL;return x+1;}", 0},
                                                                       {"unsigned long f(void){return 18446744073709551615UL;}", 18446744073709551615.0L},
                                                                       {"int main(void){float x=16777216.0f;x+=1;return x==16777216.0f;}", 1},
                                                                       {"int main(void){int a[3]={1};return a[0]+a[1]+a[2];}", 1},
                                                                       {"struct S{int a;int b;};int main(void){struct S s={2};return s.a+s.b;}", 2},
                                                                       {"int main(void){char s[8]=\"a\\n\\x42\\101\";return s[1]+s[2]+s[3]+s[7];}", 141},
                                                                       {"int main(void){char s[3]=\"abc\";return sizeof s+s[2];}", 102},
                                                                       {"int main(void){const char *p=\"ab\" \"c\";return p[2]+p[3];}", 99},
                                                                       {"enum {A=2,B=A+3};int main(void){int a[B]={A};return sizeof a+a[0];}", 22},
                                                                       {"int main(void){int a[3]={1,2,3};int *p=a;return *(p+2)+(p+3-p);}", 6},
                                                                       {"int main(void){int a[3]={1,2,3};return &a[0]<&a[2];}", 1},
                                                                       {"int main(void){int a[2][2]={{1,2},{3,4}};int (*p)[2]=a;return p[1][0]+(*p)[1];}", 5},
                                                                       {"int main(void){int x=2;void *p=&x;int *q=p;*q=4;return x;}", 4},
                                                                       {"int main(void){int x=1;int *p=&x;return (p&&1)+(0||p)+(p!=0)+(p==0);}", 3},
                                                                       {"int main(void){int a[2]={1,2};int i=0;a[i++]+=3;return i*10+a[0];}", 14},
                                                                       {"int main(void){int a[2]={1,2};int *p=a;int first=*p++;return first+*p;}", 3},
                                                                       {"int calls;int *f(int *p){++calls;return p;}int main(void){int x=1;*f(&x)+=2;return 10*calls+x;}", 13},
                                                                       {"int main(void){int n=1;return sizeof(n++)+n;}", 5},
                                                                       {"int main(void){int a[3]={1,2,3};int *p=&a[3];return (p-a)+(a-p);}", 0},
                                                                       {"int main(void){int x=1;int *p=&x;int **q=&p;**q=6;return x;}", 6},
                                                                       {"struct S{int x;};int main(void){struct S a={1},b={2},c={0};c=(1?a:b);return c.x;}", 1},
                                                                       {"int main(void){int *p=0;return sizeof *p;}", 4},
                                                                       {"int main(void){int a=0;0&&(a=1);1||(a=2);return a;}", 0},
                                                                       {"struct S{int x;};struct S f(struct S s){s.x+=2;return s;}int main(void){struct S a={1},b={0};b=f(a);return a.x*10+b.x;}", 13},
                                                                       {"struct S{int x;};struct S f(void){struct S s={7};return s;}int main(void){return f().x;}", 7},
                                                                       {"struct S{int x;int a[2];};int main(void){struct S a={1,{2,3}},b={0,{0}};b=a;b.a[0]=7;return a.a[0]*10+b.a[0];}", 27},
                                                                       {"struct N{int x;struct N *next;};int main(void){struct N a={1,0},b={2,0};a.next=&b;return a.next->x;}", 2},
                                                                       {"int x;extern int x;int x;int main(void){return ++x;}", 1},
                                                                       {"extern int a[];int a[]={1,2};int main(void){return a[1];}", 2},
                                                                       {"int f(int n){if(n<2)return 1;return n*f(n-1);}int main(void){return f(5);}", 120},
                                                                       {"int main(void){int n=0;do{++n;if(n<3)continue;}while(n<4);return n;}", 4},
                                                                       {"int main(void){int x=0;switch(2){case 1:x=1;break;case 2:x=2;default:x+=3;}return x;}", 5},
                                                                       {"int main(void){int x=0;switch(9){default:x=4;case 1:x+=2;}return x;}", 6},
                                                                       {"int main(void){int x=0;switch(9){case 1:x=4;}return x;}", 0},
                                                                       {"int main(void){int x=0;for(int i=0;i<4;++i){switch(i){case 1:continue;case 2:break;default:x++;}x+=10;}return x;}", 32},
                                                                       {"int main(void){int x=0;switch(2){case 2:switch(1){case 1:x=3;break;}x++;break;default:x=9;}return x;}", 4},
                                                                       {"int main(void){int x=0;switch(1){if(0){case 1:x=4;}x++;}return x;}", 5},
                                                                       {"int main(void){int x=0;switch(1){do{case 1:++x;}while(x<3);}return x;}", 3},
                                                                       {"int main(void){int x=0;switch(1){int skipped=9;case 1:skipped=2;x=skipped;}return x;}", 2},
                                                                       {"int main(void){int x=3;{int x=9;++x;}return x;}", 3},
                                                                       {"int main(void){unsigned int x=4294967295u;switch(x){case -1:return 4;default:return 0;}}", 4},
                                                                       {"int main(void){int x=7;_Bool b=&x;return b;}", 1}};
    if (argc == 3 && std::string(argv[1]) == "--export") {
        std::filesystem::create_directories(argv[2]);
        std::ofstream manifest(std::filesystem::path(argv[2]) / "manifest.tsv");
        size_t index = 0;
        for (const auto &[source, expected] : programs) {
            if (source.starts_with("unsigned long f") || source.find("sizeof(long double)") != std::string::npos)
                continue;
            auto name = "case-" + std::to_string(index++) + ".c";
            std::ofstream output(std::filesystem::path(argv[2]) / name);
            output << source << '\n';
            manifest << name << '\t' << std::fixed << std::setprecision(0) << expected << '\n';
        }
        return manifest ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    for (const auto &[source, expected] : programs)
        accepts(source, expected, source.starts_with("unsigned long f") ? "f" : "main");
    const std::vector<std::pair<std::string, std::string>> errors = {{"int main(void){int x=2147483647;return x+1;}", "overflow"},
                                                                     {"int main(void){int x=-2147483647-1;return x/-1;}", "overflow"},
                                                                     {"int main(void){int x=0;return 1/x;}", "division by zero"},
                                                                     {"int main(void){int x=32;return 1<<x;}", "shift count"},
                                                                     {"int main(void){int x;return x;}", "uninitialized"},
                                                                     {"int main(void){int x=x;return x;}", "uninitialized"},
                                                                     {"int main(void){int a[2]={a[0],1};return a[0];}", "uninitialized"},
                                                                     {"int main(void){int a[2];return a[0];}", "uninitialized"},
                                                                     {"int main(void){int *p=0;return *p;}", "null or expired"},
                                                                     {"int main(void){int a[2]={0};return a[2];}", "one-past"},
                                                                     {"int main(void){int a[2]={0};return *(a+3);}", "outside array bounds"},
                                                                     {"int main(void){int a[2][2]={{1},{2}};return a[0][2];}", "one-past"},
                                                                     {"int *f(void){int x=1;return &x;}int main(void){return *f();}", "expired"},
                                                                     {"int main(void){int *p=0;{int x=3;p=&x;}return *p;}", "expired"},
                                                                     {"int main(void){int a=1,b=2;return &a-&b;}", "same array"},
                                                                     {"int main(void){char *p=\"x\";p[0]='y';return 0;}", "const object"},
                                                                     {"int main(void){const int x=3;int *p=(int*)&x;*p=4;return x;}", "const object"},
                                                                     {"extern int x;int main(void){return x;}", "undefined external object"},
                                                                     {"int f(void);int main(void){return f();}", "undefined function"},
                                                                     {"int main(void){switch(1){int x=2;case 1:return x;}return 0;}", "uninitialized"},
                                                                     {"int main(void){double x=1e100;return (int)x;}", "out of range"},
                                                                     {"int main(void){return main();}", "recursion limit"}};
    for (const auto &[source, error] : errors)
        rejects(source, error);
    rejects("int main(void){while(1){}return 0;}", "instruction limit", {100, 256, 1024});
    rejects("int main(void){int a[1000];return 0;}", "allocation limit", {10000, 256, 100});
    try {
        auto ast = parse("int n;int bad(void){int x=0;return 1/x;}int good(void){return ++n;}");
        mx::Interpreter interpreter;
        interpreter.load(*ast);
        try {
            interpreter.call("bad");
            check(false, "Missing error");
        } catch (const std::runtime_error &) {
        }
        check(number(interpreter.call("good")) == 1, "Frame recovery after error");
        check(number(interpreter.call("good")) == 2, "Globals preserved between calls");
        auto broken = parse("int f(void){return unknown;}");
        try {
            interpreter.load(*broken);
            check(false, "Invalid load accepted");
        } catch (const std::runtime_error &) {
        }
        check(number(interpreter.call("good")) == 3, "Failed load destroyed previous program");
        auto next = parse("unsigned long f(unsigned long x){return x+1;}");
        interpreter.load(*next);
        auto value = interpreter.call("f", {uint64_t(18446744073709551614ULL)});
        check(std::get<uint64_t>(value) == UINT64_MAX, "Public unsigned argument/result precision");
    } catch (const std::exception &error) {
        check(false, error.what());
    }
    if (argc == 2) {
        std::ifstream input(argv[1]);
        mx::Parser parser(input);
        auto ast = parser.parse_program();
        mx::Interpreter interpreter;
        interpreter.load(*ast);
        check(number(interpreter.call()) == 2, "Combined subset program execution");
    }
    std::cout << programs.size() << " execution cases and " << errors.size() + 2 << " runtime errors checked\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
