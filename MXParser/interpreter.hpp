#pragma once
#include "MXParser/semantic.hpp"
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace mx {
    using ExecutionValue = std::variant<std::monostate, int64_t, uint64_t, double>;
    /** Typed callback argument. Handles and derived views are valid only during their callback. */
    class NativeObject {
      public:
        TypePtr type() const;
        ExecutionValue value() const;
        void set(const ExecutionValue &value) const;
        NativeObject dereference() const;
        NativeObject at(size_t index) const;
        NativeObject member(const std::string &name) const;
        std::string string() const;
        template <typename T> T as() const {
            static_assert(std::is_arithmetic_v<T>, "as<T> requires a numeric C++ type");
            return std::visit(
                [](auto number) -> T {
                    if constexpr (std::is_same_v<decltype(number), std::monostate>)
                        throw std::runtime_error("void object has no numeric value");
                    else {
                        if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                            if constexpr (std::is_floating_point_v<decltype(number)>) {
                                auto n = std::trunc(static_cast<long double>(number));
                                auto bound = std::ldexp(1.0L, std::numeric_limits<T>::digits);
                                if (!std::isfinite(n) || n < (std::is_signed_v<T> ? -bound : 0) || n >= bound)
                                    throw std::runtime_error("native numeric conversion out of range");
                            } else {
                                if constexpr (std::is_signed_v<decltype(number)>) {
                                    if (number < 0) {
                                        if constexpr (std::is_unsigned_v<T>)
                                            throw std::runtime_error("native numeric conversion out of range");
                                        else if (number < std::numeric_limits<T>::lowest())
                                            throw std::runtime_error("native numeric conversion out of range");
                                        return static_cast<T>(number);
                                    }
                                }
                                if (static_cast<uint64_t>(number) > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                                    throw std::runtime_error("native numeric conversion out of range");
                            }
                        } else if constexpr (std::is_floating_point_v<T>) {
                            if (!std::isfinite(number) || std::abs(static_cast<long double>(number)) > std::numeric_limits<T>::max())
                                throw std::runtime_error("native numeric conversion out of range");
                        }
                        return static_cast<T>(number);
                    }
                },
                value());
        }

      private:
        friend class Interpreter;
        struct State;
        explicit NativeObject(std::shared_ptr<State> state);
        std::shared_ptr<State> state;
    };
    using NativeCallback = std::function<ExecutionValue(const std::vector<NativeObject> &)>;
    class ExecutionException : public std::runtime_error {
      public:
        ExecutionException(const std::string &message, SourceLocation location);
        SourceLocation location;
    };
    struct ExecutionLimits {
        /** Instruction budget per load/call/submit; zero disables the budget. */
        size_t instructions = 20000000;
        size_t recursion = 256;
        size_t object_bytes = 1024 * 1024;
        size_t output_bytes = 1024 * 1024;
    };
    /** Validated LP64 interpreter. Keep the loaded AST alive. Public calls exchange scalar values. */
    class Interpreter {
      public:
        explicit Interpreter(ExecutionLimits limits = {});
        ~Interpreter();
        Interpreter(const Interpreter &) = delete;
        Interpreter &operator=(const Interpreter &) = delete;
        void load(const ProgramNode &program);
        /** Add interactive declarations, execute statements, or evaluate an expression, preserving prior objects and functions. */
        std::optional<std::string> submit(const std::string &source);
        ExecutionValue call(const std::string &name = "main", const std::vector<ExecutionValue> &arguments = {});
        /** Poll for cancellation during execution. Returning true interrupts execution with normal scope cleanup. Survives load(). */
        void set_interrupt_handler(std::function<bool()> handler);
        /** Load __import__ prototypes from a text file and bind exported C symbols from a shared library. Imports survive load(). */
        void import_module(const std::string &module_path, const std::string &declarations_path);
        /** Map an external C function prototype to a C++ callback. Registrations survive load().
         * Parameters use C value semantics; pointer views can modify interpreter objects.
         * Return a scalar or monostate for void. Reentering this interpreter is unsupported.
         */
        void register_function(const std::string &name, NativeCallback callback);
        /** Convenience adapter for C++ functions taking numbers, strings, or NativeObject views. */
        template <typename R, typename... Args> void register_function(const std::string &name, R (*function)(Args...)) {
            if (!function)
                throw std::invalid_argument("native function must not be null");
            register_function(name, NativeCallback([function](const std::vector<NativeObject> &arguments) -> ExecutionValue {
                                  if (arguments.size() != sizeof...(Args))
                                      throw std::invalid_argument("wrong C++ native function argument count");
                                  return [&]<size_t... I>(std::index_sequence<I...>) -> ExecutionValue {
                                      if constexpr (std::is_void_v<R>) {
                                          function(native_argument<Args>(arguments[I])...);
                                          return std::monostate{};
                                      } else {
                                          static_assert(std::is_arithmetic_v<R>, "native functions return numbers or void");
                                          auto result = function(native_argument<Args>(arguments[I])...);
                                          if constexpr (std::is_floating_point_v<R>)
                                              return static_cast<double>(result);
                                          else if constexpr (std::is_unsigned_v<R>)
                                              return static_cast<uint64_t>(result);
                                          else
                                              return static_cast<int64_t>(result);
                                      }
                                  }(std::index_sequence_for<Args...>{});
                              }));
        }

      private:
        template <typename T> static auto native_argument(const NativeObject &object) {
            using Value = std::remove_cvref_t<T>;
            static_assert(!std::is_lvalue_reference_v<T> || std::is_const_v<std::remove_reference_t<T>>, "use NativeObject to modify interpreter objects");
            if constexpr (std::is_same_v<Value, NativeObject>)
                return object;
            else if constexpr (std::is_same_v<Value, std::string>)
                return object.string();
            else {
                static_assert(std::is_arithmetic_v<Value>, "native arguments must be numbers, strings, or NativeObject views");
                return object.as<Value>();
            }
        }
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
        ExecutionLimits limits;
    };
} // namespace mx
