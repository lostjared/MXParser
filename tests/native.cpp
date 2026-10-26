#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <iostream>
#include <optional>
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
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    double scale(int n, double factor) { return n * factor; }
    unsigned long long identity(unsigned long long n) { return n; }
    char character(char c) { return c; }
    int length(const std::string &text) { return static_cast<int>(text.size()); }
    void update(mx::NativeObject pointer) {
        auto object = pointer.dereference();
        object.member("count").set(int64_t{7});
        object.member("items").at(1).set(int64_t{9});
    }
} // namespace

int main() {
    try {
        mx::Interpreter interpreter;
        interpreter.register_function("scale", &scale);
        interpreter.register_function("identity", &identity);
        interpreter.register_function("character", &character);
        interpreter.register_function("length", &length);
        interpreter.register_function("update", &update);
        int invocations = 0;
        std::optional<mx::NativeObject> saved;
        interpreter.register_function("inspect", [&](const std::vector<mx::NativeObject> &args) -> mx::ExecutionValue {
            ++invocations;
            check(args[0].type()->unqualified().scalar == mx::ScalarKind::Int, "declared parameter type");
            saved = args[0];
            auto n = args[0].as<int>();
            args[0].set(int64_t{99});
            args[1].at(1).set(int64_t{8});
            return int64_t{n};
        });
        auto program = parse(R"(
            struct Data { int count; int items[2]; };
            double scale(int n, double factor);
            unsigned long long identity(unsigned long long n);
            char character(char c);
            int length(const char *text);
            void update(struct Data *data);
            int inspect(int n, int *items);
            int main(void) {
                int n=3; int a[2]={1,2}; struct Data d={1,{2,3}};
                update(&d);
                return inspect(n,a)+n+a[1]+d.count+d.items[1]+length("hello")+(int)scale(2,1.5);
            }
        )");
        interpreter.load(*program);
        check(std::get<int64_t>(interpreter.call()) == 38, "callbacks and C value semantics");
        check(invocations == 1, "capturing callback invoked");
        rejects([&] { saved->value(); }, "outside its callback");
        check(std::get<uint64_t>(interpreter.call("identity", {UINT64_MAX})) == UINT64_MAX, "full unsigned result");
        check(std::get<int64_t>(interpreter.call("character", {int64_t{'a'}})) == 'a', "C++ char adapter");
        check(std::get<double>(interpreter.call("scale", {int64_t{4}, 0.5})) == 2, "direct native call");
        rejects([&] { interpreter.call("scale"); }, "wrong argument count");
        rejects([&] { interpreter.register_function("scale", &scale); }, "already registered");
        rejects([&] { interpreter.register_function("lsprintf", &length); }, "reserved");
        rejects([&] { interpreter.register_function("empty", mx::NativeCallback{}); }, "must not be empty");
        rejects([&] { interpreter.register_function("null", static_cast<int (*)(const std::string &)>(nullptr)); }, "must not be null");
        auto conflict = parse("double scale(int n,double d){return d;} int main(void){return 0;}");
        rejects([&] { interpreter.load(*conflict); }, "conflicts with C definition");
        check(std::get<double>(interpreter.call("scale", {int64_t{6}, 0.5})) == 3, "failed load preserves runtime");
        interpreter.load(*program);
        check(std::get<int64_t>(interpreter.call()) == 38, "registrations survive load");

        mx::Interpreter interactive;
        interactive.register_function("length", &length);
        interactive.submit("int length(const char *text);");
        check(interactive.submit("length(\"abc\")") == "3", "interactive callback");
        rejects([&] { interactive.submit("int length(const char *p){return 0;}"); }, "conflicts with C definition");
        check(interactive.submit("length(\"abcd\")") == "4", "rejected definition preserves interactive state");
        auto block = parse("int main(void){int length(const char *p);return length(\"abc\");}");
        interactive.load(*block);
        check(std::get<int64_t>(interactive.call()) == 3, "block-scope prototype");

        mx::Interpreter structs;
        structs.register_function("copy", [](const auto &args) -> mx::ExecutionValue {
            args[0].member("items").at(0).at(1).set(int64_t{7});
            return args[0].member("items").at(0).at(1).value();
        });
        auto struct_ast = parse("struct S{int items[1][2];};int copy(struct S s);int main(void){struct S s={{{1,2}}};return copy(s)+s.items[0][1];}");
        structs.load(*struct_ast);
        check(std::get<int64_t>(structs.call()) == 9, "struct arguments copy nested arrays");

        auto error_case = [&](const std::string &source, mx::NativeCallback callback, const std::string &message) {
            mx::Interpreter engine;
            engine.register_function("native", std::move(callback));
            auto ast = parse(source);
            engine.load(*ast);
            rejects([&] { engine.call(); }, message);
        };
        auto write = [](const auto &args) -> mx::ExecutionValue {
            args[0].dereference().set(int64_t{4});
            return {};
        };
        error_case("void native(const int *p); int main(void){int n=1;native(&n);return n;}", write, "write to const object");
        error_case("void native(int *p); int main(void){native(0);return 0;}", write, "null or expired");
        error_case("void native(int *p); int main(void){int a[1]={1};native(a+1);return 0;}", write, "one-past");
        error_case("void native(int *p); int main(void){int n;native(&n);return 0;}", [](const auto &args) -> mx::ExecutionValue { return args[0].dereference().value(); }, "uninitialized");
        error_case("void native(char *p); int main(void){native(\"a\");return 0;}", write, "write to const");
        error_case("int native(void); int main(void){return native();}", [](const auto &) -> mx::ExecutionValue { return {}; }, "returned void");
        error_case("int native(void); int main(void){return native();}", [](const auto &) -> mx::ExecutionValue { throw std::runtime_error("host failure"); }, "host failure");
        error_case(
            "void native(int *p); int main(void){int a[1]={0};native(a);return 0;}",
            [](const auto &args) -> mx::ExecutionValue {
                args[0].at(SIZE_MAX);
                return {};
            },
            "out-of-bounds");
        error_case("int native(double x); int main(void){return native(1e30);}", [](const auto &args) -> mx::ExecutionValue { return int64_t{args[0].template as<int>()}; }, "conversion out of range");
        error_case(
            "struct S{int x;};void native(const struct S *p);int main(void){struct S s={1};native(&s);return 0;}",
            [](const auto &args) -> mx::ExecutionValue {
                args[0].dereference().member("x").set(int64_t{3});
                return {};
            },
            "write to const");
        error_case("struct S{int x;};void native(struct S s);int main(void){struct S s={1};native(s);return 0;}", [](const auto &args) -> mx::ExecutionValue { return args[0].member("missing").value(); }, "unknown native object member");
        error_case("int native(void);int main(void){return native();}", [](const auto &) -> mx::ExecutionValue { throw 1; }, "unknown exception");

        mx::Interpreter recovery;
        int attempts = 0;
        recovery.register_function("native", [&](const auto &) -> mx::ExecutionValue {
            if (++attempts == 1) {
                rejects([&] { recovery.submit("1"); }, "cannot reenter");
                throw std::runtime_error("retry");
            }
            return int64_t{5};
        });
        auto retry = parse("int native(void);int main(void){return native();}");
        recovery.load(*retry);
        rejects([&] { recovery.call(); }, "retry");
        check(std::get<int64_t>(recovery.call()) == 5, "frames recover after callback failure");
        rejects([&] { saved->member("x"); }, "outside its callback");
    } catch (const std::exception &error) {
        check(false, error.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
