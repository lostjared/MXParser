#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mx {
    enum class TypeKind { Scalar, Qualified, Pointer, Array, Function, Tagged, Named };
    enum class ScalarKind { Void, Bool, Char, SignedChar, UnsignedChar, Short, UnsignedShort, Int, UnsignedInt, Long, UnsignedLong, LongLong, UnsignedLongLong, Float, Double, LongDouble };
    enum class TagKind { Struct, Union, Enum };

    struct TypeQualifiers {
        bool is_const = false;
        bool is_volatile = false;
        bool is_restrict = false;
        bool is_atomic = false;
        bool operator==(const TypeQualifiers &) const = default;
    };

    class Type;
    class ExprNode;
    using TypePtr = std::shared_ptr<const Type>;

    /** Immutable, shared C type structure. Named types retain an optional typedef definition. */
    class Type {
      public:
        const TypeKind kind;
        const ScalarKind scalar;
        const TagKind tag;
        const TypeQualifiers qualifiers;
        const std::string name;
        /** Qualified base, pointer target, array element, or function result. */
        const TypePtr element;
        const std::optional<size_t> bound;
        const std::shared_ptr<const ExprNode> bound_expression;
        const std::vector<TypePtr> parameters;
        const bool has_prototype;
        /** Zero for syntax-only tags; resolved identities distinguish semantic scopes. */
        const size_t tag_identity;

        static TypePtr scalar_type(ScalarKind scalar);
        static TypePtr qualified(TypePtr base, TypeQualifiers qualifiers);
        static TypePtr pointer(TypePtr target);
        static TypePtr array(TypePtr element, std::optional<size_t> bound = std::nullopt, std::shared_ptr<const ExprNode> expression = {});
        static TypePtr function(TypePtr result, std::vector<TypePtr> parameters, bool has_prototype = true);
        static TypePtr tagged(TagKind tag, std::string name, size_t identity = 0);
        static TypePtr named(std::string name, TypePtr definition = {});
        /** Read the scalar/tag/typedef and pointer type-name syntax supported by the parser. */
        static TypePtr from_name(const std::string &spelling);
        /** Print a C abstract declarator; parsed names retain their original specifier order. */
        std::string to_string() const;
        /** Print this type with a named declarator, or only the modifiers above base. */
        std::string declaration(const std::string &name) const;
        std::string declarator(const std::string &name, const TypePtr &base) const;
        const Type &unqualified() const;
        TypeQualifiers top_qualifiers() const;
        bool equals(const Type &other) const;

      private:
        Type(TypeKind kind, ScalarKind scalar = ScalarKind::Int, TypePtr element = {}, TypeQualifiers qualifiers = {}, TagKind tag = TagKind::Struct, std::string name = {}, std::optional<size_t> bound = {}, std::vector<TypePtr> parameters = {}, std::string spelling = {}, std::shared_ptr<const ExprNode> expression = {}, bool has_prototype = true, size_t tag_identity = 0);
        const std::string spelling;
        std::string render(std::string declarator, const Type *stop = nullptr) const;
    };
} // namespace mx
