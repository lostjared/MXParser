#include <MXParser/interpreter.hpp>
#include <iostream>

int add(int left, int right) { return left + right; }

int main() {
    mx::Interpreter interpreter;
    interpreter.register_function("add", &add);
    interpreter.register_function("update", [](const std::vector<mx::NativeObject> &args) -> mx::ExecutionValue {
        auto record = args.at(0).dereference();
        record.member("total").set(int64_t{42});
        record.member("samples").at(1).set(3.5);
        std::cout << args.at(1).string() << '\n';
        return std::monostate{};
    });
    interpreter.submit(R"(
        struct Record { int total; double samples[2]; };
        int add(int left, int right);
        void update(struct Record *record, const char *label);
        int main(void) {
            struct Record record={0,{1.0,2.0}};
            update(&record,"called from C");
            return add(record.total,(int)record.samples[1]);
        }
    )");
    std::cout << std::get<int64_t>(interpreter.call()) << '\n'; // 45
}
