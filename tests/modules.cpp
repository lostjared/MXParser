#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
    int failures = 0;
    void check(bool condition, const std::string &message) {
        if (!condition) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    template <typename F> void rejects(F action, const std::string &message) {
        try {
            action();
            check(false, "expected error: " + message);
        } catch (const std::exception &error) {
            check(std::string(error.what()).find(message) != std::string::npos, error.what());
        }
    }
    void value(mx::Interpreter &interpreter, const std::string &source, const std::string &expected) { check(interpreter.submit(source) == expected, "module result: " + source); }
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream stream(source);
        mx::Parser parser(stream);
        return parser.parse_program();
    }
} // namespace
int main(int argc, char **argv) {
    if (argc != 3)
        return EXIT_FAILURE;
    try {
        std::string module = argv[1];
        std::filesystem::path directory = argv[2];
        std::filesystem::create_directories(directory);
        auto write = [&](const std::string &name, const std::string &source) {
            auto path = directory / name;
            std::ofstream file(path);
            file << source;
            return path.string();
        };
        auto imports = write("functions.ls-h", R"(
            // Every prototype explicitly opts into native imports.
            __import__ int module_add(int left, int right);
            __import__ double module_scale(int n, double factor, float offset);
            __import__ signed char module_char(signed char n);
            __import__ unsigned char module_byte(unsigned char n);
            __import__ short module_short(short n);
            __import__ unsigned short module_ushort(unsigned short n);
            __import__ unsigned int module_uint(unsigned int n);
            __import__ long module_long(long n);
            __import__ unsigned long module_ulong(unsigned long n);
            __import__ long long module_longlong(long long n);
            __import__ unsigned long long module_unsigned(unsigned long long n);
            __import__ float module_float(float n);
            __import__ long double module_extended(long double n);
            __import__ _Bool module_bool(_Bool n);
            __import__ int module_length(const char *text);
            __import__ void module_bump();
            __import__ int module_count(void);
        )");
        mx::Interpreter interpreter;
        interpreter.submit("int existing=8;");
        interpreter.import_module(module, imports);
        value(interpreter, "existing", "8");
        value(interpreter, "module_add(2,3)", "5");
        value(interpreter, "module_scale(3,2.0,0.5f)", "6.5");
        value(interpreter, "module_char(-12)", "-12");
        value(interpreter, "module_byte(255)", "255");
        value(interpreter, "module_short(-32768)", "-32768");
        value(interpreter, "module_ushort(65535)", "65535");
        value(interpreter, "module_uint(4294967295U)", "4294967295");
        value(interpreter, "module_long(-12)", "-12");
        value(interpreter, "module_ulong(42)", "42");
        value(interpreter, "module_longlong(-9223372036854775807LL-1)", "-9223372036854775808");
        value(interpreter, "module_unsigned(18446744073709551615ULL)", "18446744073709551615");
        value(interpreter, "module_float(1.25f)", "1.25");
        value(interpreter, "module_extended(1.5L)", "1.5");
        value(interpreter, "module_bool(0)", "1");
        value(interpreter, "module_bool(1)", "0");
        value(interpreter, "module_length(\"hello\")", "5");
        interpreter.submit("char text[4]={65,66,67,0};");
        value(interpreter, "module_length(text)", "3");
        check(!interpreter.submit("module_bump()"), "void module result");
        value(interpreter, "module_count()", "1");
        check(std::get<int64_t>(interpreter.call("module_add", {int64_t{4}, int64_t{5}})) == 9, "direct imported call");
        rejects([&] { interpreter.import_module(module, imports); }, "already registered");
        rejects([&] { interpreter.submit("int module_add(int a,int b){return a+b;}"); }, "conflicts with C definition");
        auto replacement = parse("int main(void){return module_add(20,22);}");
        interpreter.load(*replacement);
        check(std::get<int64_t>(interpreter.call()) == 42, "imports and declarations survive load");
        auto wrong = parse("double module_add(double x);int main(void){return 0;}");
        rejects([&] { interpreter.load(*wrong); }, "Incompatible");
        check(std::get<int64_t>(interpreter.call()) == 42, "failed load preserves imports");
        rejects([&] { interpreter.submit("int use(void){double module_add(double x);return (int)module_add(1.0);}"); }, "Incompatible");
        auto attempt = [&](const std::string &source, const std::string &message) {
            auto path = write("invalid.ls-h", source);
            rejects([&] { interpreter.import_module(module, path); }, message);
        };
        attempt("__import__ int module_spare(void); __import__ int missing_symbol(void);", "cannot find module symbol");
        rejects([&] { interpreter.submit("module_spare()"); }, "Undeclared");
        attempt("int module_spare(void);", "expected __import__");
        attempt("__import__ int module_spare(void)", "ending in ;");
        attempt("__import__ int object;", "function prototype");
        attempt("__import__ int module_spare(void) {return 1;}", "function prototype");
        attempt("__import__ int module_spare(void), other(void);", "one function prototype");
        attempt("__import__ int module_spare(void); __import__ int module_spare(void);", "duplicate module import");
        attempt("__import__ int module_spare(int *p);", "unsupported module import type");
        auto valid = write("spare.ls-h", "__import__ int module_spare(void);");
        rejects([&] { interpreter.import_module(module + ".missing", valid); }, "cannot load module");
        rejects([&] { interpreter.import_module(module, valid + ".missing"); }, "cannot open import declarations");
        interpreter.import_module(module, valid);
        value(interpreter, "module_spare()", "42");
    } catch (const std::exception &error) {
        check(false, error.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
