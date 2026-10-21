#include <MXParser/parser.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    int failures = 0;
    void check(bool condition, const std::string &message) {
        if (!condition) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    std::unique_ptr<mx::ProgramNode> program(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    long double evaluate(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        auto expression = parser.parse_expression();
        mx::Evaluate evaluator;
        expression->accept(evaluator);
        return evaluator.result();
    }
    void runtime_error(const std::string &source, const std::string &name = "main") {
        try {
            auto ast = program(source);
            mx::Evaluate evaluator;
            ast->accept(evaluator);
            evaluator.call(name);
            check(false, "Expected runtime error: " + source);
        } catch (const std::runtime_error &) {
        }
    }
} // namespace

int main() {
    const std::vector<std::pair<std::string, long double>> expressions = {{"1.5+2.25*2", 6}, {"5/2", 2}, {"5/2.0", 2.5}, {"5.0/2", 2.5}, {"-1.25", -1.25}, {"x=1.5,x*=2,x/4", 0.75}, {"x=0.25,x++ + ++x", 2.5}, {"1e-3+2e-3", 0.003}, {"0x1.8p+1+0.5", 3.5}, {".5f+1.0L", 1.5}, {"2.5>2 && 1.5!=0", 1}, {"!0.0", 1}, {"(int)3.9 + (double)5/2", 5.5}, {"0.0 && (1.0/0.0)", 0}, {"1.0 || missing()", 1}, {"(1 ? 1 : 2.0)/2", 0.5}, {"(0 ? 2.0 : 1)/2", 0.5}, {"(1 ? 2 : (double)1)/4", 0.5}, {"9007199254740993 > 9007199254740992", 1}};
    for (const auto &[source, expected] : expressions) {
        try {
            check(std::abs(evaluate(source) - expected) < 1e-12L, "Wrong result: " + source);
        } catch (const std::exception &error) {
            check(false, source + ": " + error.what());
        }
    }
    for (const auto &source : {"1.5%1", "1|2.0", "~1.0", "1.0<<2", "1<<1.5", "1.0/0.0", "(int)1e100", "9223372036854775807+1", "9223372036854775808", "1e999"}) {
        try {
            evaluate(source);
            check(false, std::string("Expected numeric error: ") + source);
        } catch (const std::runtime_error &) {
        }
    }
    const std::string source = R"(
        double offset = 0.25;
        double average(double, double);
        double average(double a, double b) { return (a+b)/2.0; }
        int factorial(int n) { if(n<=1) return 1; return n*factorial(n-1); }
        int odd(int n);
        int even(int n) { if(n==0) return 1; return odd(n-1); }
        int odd(int n) { if(n==0) return 0; return even(n-1); }
        double compute(double x) {
            double sum=0.0, step=0.5;
            for(int i=0; i<6; ++i) {
                if(i==1) continue;
                if(i==5) break;
                sum += step;
            }
            int n=3;
            while(n>0) { --n; if(n==1) continue; sum+=0.25; }
            { double x=100.0; sum += x/100.0; }
            return average(sum,x)+offset;
        }
        int convert(int n) { return n; }
        float narrow(float x) { return x; }
        int counter;
        void bump(void) { ++counter; return; }
        int main(void) {
            bump();
            return factorial(5)+convert(3.9)+even(6)+counter;
        }
    )";
    try {
        auto ast = program(source);
        auto round_trip = program(ast->to_string());
        check(round_trip->to_string() == ast->to_string(), "Program round trip");
        mx::Evaluate evaluator;
        ast->accept(evaluator);
        evaluator.call("main");
        check(evaluator.result() == 125, "Recursion, conversions, prototypes, void calls, globals");
        evaluator.clear();
        evaluator.call("compute", {4.0});
        check(evaluator.result() == 4.0, "Loop execution and local scope");
        evaluator.clear();
        evaluator.call("narrow", {1.234567890123});
        check(evaluator.result() == static_cast<double>(static_cast<float>(1.234567890123)), "Float parameter and return conversion");
        try {
            evaluator.call("average", {1.0});
            check(false, "Wrong arity accepted");
        } catch (const std::runtime_error &) {
        }
        evaluator.clear();
        evaluator.call("factorial", {std::int64_t(3)});
        check(evaluator.result() == 6, "Evaluator usable after call error");
        mx::ICode code;
        mx::HTMLCode html;
        ast->accept(code);
        ast->accept(html);
        check(code.code().find("FUNC average, double") != std::string::npos, "Function code generation");
        check(code.code().find("CALL ") != std::string::npos && code.code().find("RETURN ") != std::string::npos, "Call and return instructions");
        check(html.code().find("<b>FUNC</b> average, double") != std::string::npos, "HTML function generation");
    } catch (const std::exception &error) {
        check(false, error.what());
    }

    runtime_error("int f(){return 1;} int f(){return 2;}", "f");
    runtime_error("double f(int); int f(int x){return x;}", "f");
    runtime_error("int f(int); int main(){return f(1);}");
    runtime_error("int main(){return missing();}");
    runtime_error("int main(){int x; return x;}");
    runtime_error("int main(){const double x=1.5; x=2.0; return 0;}");
    runtime_error("int main(){int x=1; int x=2; return x;}");
    runtime_error("int f(int x){int x=2;return x;} int main(){return f(1);}");
    runtime_error("int f(){return hidden;} int main(){int hidden=42;return f();}");
    runtime_error("int main(){while(1){double x=2.5; return 1.0/0.0;}}");
    runtime_error("void f(){return 1;}", "f");
    runtime_error("int main(){return;}");
    runtime_error("int f(){}", "f");
    runtime_error("int main(){return main();}");
    {
        auto ast = program("int bad(){for(int i=0;i<1;++i){int x=0;return 1/x;}} int good(){return 42;}");
        mx::Evaluate evaluator;
        ast->accept(evaluator);
        try {
            evaluator.call("bad");
            check(false, "Expected exception in nested scopes");
        } catch (const std::runtime_error &) {
        }
        evaluator.call("good");
        check(evaluator.result() == 42, "Frames and scopes restored after runtime error");
    }
    for (const auto &invalid : {"double f(double x,){return x;}", "int f(int x, int x){return x;}", "int f(int){return 0;}", "int f(){return 1}", "int f(){return 1;", "int f(){break;}", "int f(){continue;}", "int f(){for(;;)}", "int f(void x){return 0;}", "int f(){double = 2.0;}", "int f(){if(1) int x=2;}"}) {
        try {
            program(invalid);
            check(false, std::string("Accepted invalid program: ") + invalid);
        } catch (const mx::ParserException &) {
        }
    }
    {
        std::istringstream input("x=1.5; x/=2; x+0.25;");
        mx::Parser parser(input);
        std::ostringstream output;
        auto *previous = std::cout.rdbuf(output.rdbuf());
        bool success = parser.parse();
        std::cout.rdbuf(previous);
        check(success && output.str().find("Result: 1") != std::string::npos, "Floating point through parse()");
    }
    {
        std::istringstream input("double f(double x){return x/2.0;} int main(){return (int)(f(5.0)*2);}");
        mx::Parser parser(input);
        std::ostringstream output;
        auto *previous = std::cout.rdbuf(output.rdbuf());
        bool success = parser.parse();
        std::cout.rdbuf(previous);
        check(success && output.str().find("Result: 5") != std::string::npos, "Program through parse()");
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
