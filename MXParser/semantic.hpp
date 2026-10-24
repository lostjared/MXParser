#pragma once
#include "MXParser/ast.hpp"
#include "MXParser/sym_tab.hpp"
#include <memory>
#include <optional>
#include <stdexcept>

namespace mx {
    class SemanticException : public std::runtime_error {
      public:
        SemanticException(const std::string &message, SourceLocation location);
        SourceLocation location;
    };

    /** Fixed LP64 model: 8-bit bytes, two's complement integers, signed char, binary32 float, binary64 double/long double. */
    class ScalarDataModel {
      public:
        static size_t size_of(const TypePtr &type);
        static TypePtr integer_promotion(const TypePtr &type);
        static TypePtr arithmetic_type(const TypePtr &left, const TypePtr &right);
    };

    struct ExpressionInfo {
        TypePtr type;
        bool lvalue = false;
        bool modifiable = false;
        bool constant_expression = false;
        std::optional<uint64_t> integer_constant;
        bool null_pointer_constant = false;
        std::optional<size_t> member_offset;
        std::shared_ptr<const SemanticSymbol> symbol;
    };

    /** Validate a translation unit without executing it. Keep its AST alive while reading expression facts. */
    class SemanticAnalyzer {
      public:
        SemanticAnalyzer();
        ~SemanticAnalyzer();
        SemanticAnalyzer(const SemanticAnalyzer &) = delete;
        SemanticAnalyzer &operator=(const SemanticAnalyzer &) = delete;
        void validate(const ProgramNode &program);
        const ExpressionInfo &info(const ExprNode &expression) const;
        const SemanticSymbolTable &symbols() const;

      private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
    };
} // namespace mx
