#pragma once
#include "MXParser/semantic.hpp"
#include <cstdint>
#include <memory>
#include <variant>

namespace mx {
    using ExecutionValue = std::variant<std::monostate, int64_t, uint64_t, double>;
    class ExecutionException : public std::runtime_error {
      public:
        ExecutionException(const std::string &message, SourceLocation location);
        SourceLocation location;
    };
    struct ExecutionLimits {
        size_t instructions = 1000000;
        size_t recursion = 256;
        size_t object_bytes = 1024 * 1024;
    };
    /** Validated LP64 interpreter. Keep the loaded AST alive. Public calls exchange scalar values. */
    class Interpreter {
      public:
        explicit Interpreter(ExecutionLimits limits = {});
        ~Interpreter();
        Interpreter(const Interpreter &) = delete;
        Interpreter &operator=(const Interpreter &) = delete;
        void load(const ProgramNode &program);
        ExecutionValue call(const std::string &name = "main", const std::vector<ExecutionValue> &arguments = {});

      private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
        ExecutionLimits limits;
    };
} // namespace mx
