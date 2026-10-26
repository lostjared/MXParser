#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace {
    int failures = 0;
    void check(bool value, const std::string &message) {
        if (!value) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    void value(mx::Interpreter &interpreter, const std::string &source, const std::string &expected) {
        auto result = interpreter.submit(source);
        check(result && *result == expected, "Wrong REPL result for " + source + ": " + result.value_or("<none>"));
    }
    void quiet(mx::Interpreter &interpreter, const std::string &source) { check(!interpreter.submit(source), "Expected quiet REPL input: " + source); }
    void error(mx::Interpreter &interpreter, const std::string &source, const std::string &expected) {
        try {
            interpreter.submit(source);
            check(false, "Expected REPL error: " + source);
        } catch (const std::exception &exception) {
            check(std::string(exception.what()).find(expected) != std::string::npos, "Unexpected REPL error: " + std::string(exception.what()));
        }
    }
} // namespace
int main() {
    try {
        mx::Interpreter interpreter;
        quiet(interpreter, "");
        quiet(interpreter, "// comment");
        quiet(interpreter, ";;;");
        quiet(interpreter, "int commented=3 // comment");
        value(interpreter, "commented // comment", "3");
        quiet(interpreter, "commented=4; // comment");
        value(interpreter, "commented", "4");
        value(interpreter, "1+2*3", "7");
        value(interpreter, "0.1", "0.1");
        quiet(interpreter, "int x=2");
        value(interpreter, "x+=3", "5");
        value(interpreter, "x++", "5");
        value(interpreter, "x", "6");
        quiet(interpreter, "x=8;");
        value(interpreter, "x", "8");
        quiet(interpreter, "double half(double x) { return x/2.0; }");
        value(interpreter, "half(x)", "4");
        value(interpreter, "half(5)", "2.5");
        quiet(interpreter, "double y=half(7)");
        value(interpreter, "y", "3.5");
        quiet(interpreter, "int calls=0; int bump(void) { return ++calls; }");
        quiet(interpreter, "int once=bump();");
        value(interpreter, "bump()", "2");
        value(interpreter, "once", "1");
        value(interpreter, "calls", "2");
        value(interpreter, "calls", "2");
        quiet(interpreter, "void increment(void) { ++calls; }");
        quiet(interpreter, "increment()");
        value(interpreter, "calls", "3");
        quiet(interpreter, "typedef unsigned long Count");
        quiet(interpreter, "Count n=18446744073709551615UL");
        value(interpreter, "n", "18446744073709551615");
        value(interpreter, "n+1", "0");
        quiet(interpreter, "struct Item { int x; struct Item *next; };");
        quiet(interpreter, "struct Item a={3,0}; struct Item b={5,&a};");
        value(interpreter, "b.next->x", "3");
        value(interpreter, "b", "<struct Item>");
        quiet(interpreter, "int *p=&a.x;");
        value(interpreter, "*p+=4", "7");
        value(interpreter, "a.x", "7");
        quiet(interpreter, "int values[3]={1};");
        value(interpreter, "values[0]+values[1]", "1");
        value(interpreter, "sizeof values", "12");
        auto pointer = interpreter.submit("p");
        check(pointer && pointer->starts_with("0x"), "REPL pointer display");
        quiet(interpreter, "enum { A=2, B=A+3 };");
        value(interpreter, "B", "5");
        error(interpreter, "missing()", "Undeclared");
        error(interpreter, "int bad=unknown;", "Undeclared");
        error(interpreter, "int x=100;", "Duplicate object");
        error(interpreter, "1/0", "division by zero");
        value(interpreter, "x", "8");
        value(interpreter, "calls", "3");
        error(interpreter, "int broken=1/(calls-calls)", "division by zero");
        value(interpreter, "broken", "0");
        value(interpreter, "a.x", "7");
        error(interpreter, "x=", "Expected");
        quiet(interpreter, "if(1) x=3;");
        value(interpreter, "x", "3");
        quiet(interpreter, "if(0) x=100; else x+=2;");
        value(interpreter, "x", "5");
        quiet(interpreter, "while(x<8) ++x;");
        value(interpreter, "x", "8");
        quiet(interpreter, "for(int i=0;i<5;++i) { if(i==1) continue; if(i==4) break; x+=i; }");
        value(interpreter, "x", "13");
        error(interpreter, "i", "Undeclared");
        quiet(interpreter, "switch(x) { case 13: x+=2; case 15: x+=3; break; default: x=0; }");
        value(interpreter, "x", "18");
        quiet(interpreter, "switch(x) { case 0: break; default: x=4; }");
        quiet(interpreter, "do { ++x; } while(x<6);");
        value(interpreter, "x", "6");
        quiet(interpreter, "{ int x=100; increment(); x++; }");
        value(interpreter, "x", "6");
        value(interpreter, "calls", "4");
        quiet(interpreter, "x=1; x+1");
        value(interpreter, "x", "1");
        error(interpreter, "if(1) { x=100; missing(); }", "Undeclared");
        value(interpreter, "x", "1");
        error(interpreter, "{ x=2; x=1/0; }", "division by zero");
        value(interpreter, "x", "2");
        quiet(interpreter, "x=8;");
        value(interpreter, "x", "8");
        quiet(interpreter, "struct Item copy(struct Item item) { return item; }");
        value(interpreter, "copy(a).x", "7");
        error(interpreter, "double half(double x){return x;}", "Duplicate function");
        value(interpreter, "half(4)", "2");
        std::ostringstream captured;
        auto previous = std::cout.rdbuf(captured.rdbuf());
        try {
            quiet(interpreter, "lsprintf(\"hello %d\\n\", x);");
            check(interpreter.submit("lsprintf(\"ok\")") == "2", "REPL built-in return value");
        } catch (...) {
            std::cout.rdbuf(previous);
            throw;
        }
        std::cout.rdbuf(previous);
        check(captured.str() == "hello 8\nok", "REPL stdout or side-effect replay");
        mx::Interpreter loaded;
        std::istringstream input("int total=1; int add(int n){total+=n;return total;} int main(void){return add(2);}");
        mx::Parser parser(input);
        auto program = parser.parse_program();
        loaded.load(*program);
        loaded.call();
        value(loaded, "total", "3");
        value(loaded, "add(2)", "5");
        quiet(loaded, "int after=add(1)");
        value(loaded, "total", "6");
        mx::Interpreter strict;
        std::istringstream invalid("int f(void){return 3;} int n=f();");
        mx::Parser invalid_parser(invalid);
        auto invalid_program = invalid_parser.parse_program();
        try {
            strict.load(*invalid_program);
            check(false, "REPL relaxation leaked into file validation");
        } catch (const mx::SemanticException &) {
        }
        mx::Interpreter limited({200, 20, 1024});
        quiet(limited, "int forever(void){while(1){}return 0;}");
        error(limited, "forever()", "instruction limit");
        error(limited, "while(1) {}", "instruction limit");
        value(limited, "2+2", "4");
        mx::Interpreter cancellable;
        quiet(cancellable, "int progress=0;");
        size_t polls = 0;
        cancellable.set_interrupt_handler([&] { return ++polls > 100; });
        error(cancellable, "while(1) { int local=1; progress+=local; }", "execution interrupted");
        cancellable.set_interrupt_handler({});
        value(cancellable, "progress>0", "1");
        value(cancellable, "2+2", "4");
        bool cancel = false;
        cancellable.set_interrupt_handler([&] { return cancel; });
        cancellable.load(*program);
        cancel = true;
        try {
            cancellable.call();
            check(false, "Expected interruption after load");
        } catch (const mx::ExecutionException &exception) {
            check(std::string(exception.what()).find("execution interrupted") != std::string::npos, "Wrong interruption after load");
        }
        cancel = false;
        check(std::get<int64_t>(cancellable.call()) == 3, "Execution did not recover after interruption");
    } catch (const std::exception &exception) {
        check(false, exception.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
