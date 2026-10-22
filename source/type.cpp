#include "MXParser/type.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace mx {
    namespace {
        std::string scalar_name(ScalarKind scalar) {
            switch (scalar) {
            case ScalarKind::Void:
                return "void";
            case ScalarKind::Bool:
                return "_Bool";
            case ScalarKind::Char:
                return "char";
            case ScalarKind::SignedChar:
                return "signed char";
            case ScalarKind::UnsignedChar:
                return "unsigned char";
            case ScalarKind::Short:
                return "short";
            case ScalarKind::UnsignedShort:
                return "unsigned short";
            case ScalarKind::Int:
                return "int";
            case ScalarKind::UnsignedInt:
                return "unsigned int";
            case ScalarKind::Long:
                return "long";
            case ScalarKind::UnsignedLong:
                return "unsigned long";
            case ScalarKind::LongLong:
                return "long long";
            case ScalarKind::UnsignedLongLong:
                return "unsigned long long";
            case ScalarKind::Float:
                return "float";
            case ScalarKind::Double:
                return "double";
            case ScalarKind::LongDouble:
                return "long double";
            }
            throw std::invalid_argument("Unknown scalar kind");
        }
        std::string qualifier_names(TypeQualifiers qualifiers) {
            std::string result;
            if (qualifiers.is_const)
                result += "const ";
            if (qualifiers.is_volatile)
                result += "volatile ";
            if (qualifiers.is_restrict)
                result += "restrict ";
            if (qualifiers.is_atomic)
                result += "_Atomic ";
            if (!result.empty())
                result.pop_back();
            return result;
        }
        bool read_qualifier(const std::string &word, TypeQualifiers &qualifiers) {
            if (word == "const")
                qualifiers.is_const = true;
            else if (word == "volatile")
                qualifiers.is_volatile = true;
            else if (word == "restrict")
                qualifiers.is_restrict = true;
            else if (word == "_Atomic")
                qualifiers.is_atomic = true;
            else
                return false;
            return true;
        }
        void require_type(const TypePtr &type) {
            if (!type)
                throw std::invalid_argument("A derived type requires a base type");
        }
        bool identifier(const std::string &name) {
            auto initial = [](unsigned char c) { return std::isalpha(c) || c == '_'; };
            return !name.empty() && initial(name.front()) && std::all_of(name.begin() + 1, name.end(), [&](unsigned char c) { return initial(c) || std::isdigit(c); });
        }
    } // namespace

    Type::Type(TypeKind kind, ScalarKind scalar, TypePtr element, TypeQualifiers qualifiers, TagKind tag, std::string name, std::optional<size_t> bound, std::vector<TypePtr> parameters, std::string spelling) : kind(kind), scalar(scalar), tag(tag), qualifiers(qualifiers), name(std::move(name)), element(std::move(element)), bound(bound), parameters(std::move(parameters)), spelling(std::move(spelling)) {}

    TypePtr Type::scalar_type(ScalarKind scalar) { return TypePtr(new Type(TypeKind::Scalar, scalar)); }
    TypePtr Type::qualified(TypePtr base, TypeQualifiers qualifiers) {
        require_type(base);
        if (qualifiers == TypeQualifiers{})
            return base;
        if (base->kind == TypeKind::Qualified) {
            qualifiers.is_const |= base->qualifiers.is_const;
            qualifiers.is_volatile |= base->qualifiers.is_volatile;
            qualifiers.is_restrict |= base->qualifiers.is_restrict;
            qualifiers.is_atomic |= base->qualifiers.is_atomic;
            base = base->element;
        }
        return TypePtr(new Type(TypeKind::Qualified, ScalarKind::Int, std::move(base), qualifiers));
    }
    TypePtr Type::pointer(TypePtr target) {
        require_type(target);
        return TypePtr(new Type(TypeKind::Pointer, ScalarKind::Int, std::move(target)));
    }
    TypePtr Type::array(TypePtr element, std::optional<size_t> bound) {
        require_type(element);
        return TypePtr(new Type(TypeKind::Array, ScalarKind::Int, std::move(element), {}, TagKind::Struct, {}, bound));
    }
    TypePtr Type::function(TypePtr result, std::vector<TypePtr> parameters) {
        require_type(result);
        for (const auto &parameter : parameters)
            require_type(parameter);
        return TypePtr(new Type(TypeKind::Function, ScalarKind::Int, std::move(result), {}, TagKind::Struct, {}, {}, std::move(parameters)));
    }
    TypePtr Type::tagged(TagKind tag, std::string name) {
        if (!identifier(name))
            throw std::invalid_argument("Expected type tag");
        return TypePtr(new Type(TypeKind::Tagged, ScalarKind::Int, {}, {}, tag, std::move(name)));
    }
    TypePtr Type::named(std::string name) {
        if (!identifier(name))
            throw std::invalid_argument("Expected typedef name");
        return TypePtr(new Type(TypeKind::Named, ScalarKind::Int, {}, {}, TagKind::Struct, std::move(name)));
    }
    const Type &Type::unqualified() const { return kind == TypeKind::Qualified ? element->unqualified() : *this; }
    bool Type::equals(const Type &other) const {
        if (kind != other.kind)
            return false;
        switch (kind) {
        case TypeKind::Scalar:
            return scalar == other.scalar;
        case TypeKind::Qualified:
            return qualifiers == other.qualifiers && element->equals(*other.element);
        case TypeKind::Pointer:
            return element->equals(*other.element);
        case TypeKind::Array:
            return bound == other.bound && element->equals(*other.element);
        case TypeKind::Tagged:
            return tag == other.tag && name == other.name;
        case TypeKind::Named:
            return name == other.name;
        case TypeKind::Function:
            if (!element->equals(*other.element) || parameters.size() != other.parameters.size())
                return false;
            for (size_t i = 0; i < parameters.size(); ++i)
                if (!parameters[i]->equals(*other.parameters[i]))
                    return false;
            return true;
        }
        return false;
    }
    std::string Type::render(std::string declarator) const {
        if (kind == TypeKind::Qualified) {
            if (element->kind == TypeKind::Pointer) {
                declarator = "* " + qualifier_names(qualifiers) + (declarator.empty() ? "" : " " + declarator);
                if (element->element->unqualified().kind == TypeKind::Array || element->element->unqualified().kind == TypeKind::Function)
                    declarator = "(" + declarator + ")";
                return element->element->render(declarator);
            }
            return qualifier_names(qualifiers) + " " + element->render(declarator);
        }
        if (kind == TypeKind::Pointer) {
            declarator = "*" + declarator;
            if (element->unqualified().kind == TypeKind::Array || element->unqualified().kind == TypeKind::Function)
                declarator = "(" + declarator + ")";
            return element->render(declarator);
        }
        if (kind == TypeKind::Array)
            return element->render(declarator + "[" + (bound ? std::to_string(*bound) : "") + "]");
        if (kind == TypeKind::Function) {
            declarator += "(";
            if (parameters.empty())
                declarator += "void";
            for (size_t i = 0; i < parameters.size(); ++i) {
                if (i)
                    declarator += ", ";
                declarator += parameters[i]->to_string();
            }
            return element->render(declarator + ")");
        }
        std::string base = kind == TypeKind::Scalar ? scalar_name(scalar) : kind == TypeKind::Named ? name : (tag == TagKind::Struct ? "struct " : tag == TagKind::Union ? "union " : "enum ") + name;
        return base + (declarator.empty() ? "" : " " + declarator);
    }
    std::string Type::to_string() const { return spelling.empty() ? render("") : spelling; }

    TypePtr Type::from_name(const std::string &spelling) {
        std::string separated;
        for (char c : spelling)
            separated += c == '*' ? " * " : std::string(1, c);
        std::istringstream input(separated);
        std::vector<std::string> words;
        std::string word;
        while (input >> word)
            words.push_back(word);
        size_t index = 0;
        TypeQualifiers qualifiers;
        std::unordered_map<std::string, int> counts;
        while (index < words.size()) {
            if (read_qualifier(words[index], qualifiers)) {
                ++index;
                continue;
            }
            const auto &candidate = words[index];
            if (candidate != "signed" && candidate != "unsigned" && candidate != "void" && candidate != "_Bool" && candidate != "char" && candidate != "short" && candidate != "int" && candidate != "long" && candidate != "float" && candidate != "double")
                break;
            ++counts[words[index++]];
        }
        TypePtr result;
        if (counts.empty()) {
            if (index == words.size())
                throw std::invalid_argument("Expected type name");
            word = words[index++];
            if (word == "struct" || word == "union" || word == "enum") {
                if (index == words.size())
                    throw std::invalid_argument("Expected type tag");
                result = tagged(word == "struct" ? TagKind::Struct : word == "union" ? TagKind::Union : TagKind::Enum, words[index++]);
            } else
                result = named(word);
        } else {
            int total = 0;
            for (const auto &[name, count] : counts) {
                if (count > (name == "long" ? 2 : 1))
                    throw std::invalid_argument("Duplicate type specifier: " + name);
                total += count;
            }
            bool valid = !(counts["signed"] && counts["unsigned"]);
            int sign = counts["signed"] + counts["unsigned"];
            ScalarKind scalar;
            if (counts["void"] || counts["_Bool"] || counts["float"]) {
                valid = valid && total == 1;
                scalar = counts["void"] ? ScalarKind::Void : counts["_Bool"] ? ScalarKind::Bool : ScalarKind::Float;
            } else if (counts["double"]) {
                valid = valid && total == 1 + counts["long"] && counts["long"] <= 1;
                scalar = counts["long"] ? ScalarKind::LongDouble : ScalarKind::Double;
            } else if (counts["char"]) {
                valid = valid && total == 1 + sign;
                scalar = counts["unsigned"] ? ScalarKind::UnsignedChar : counts["signed"] ? ScalarKind::SignedChar : ScalarKind::Char;
            } else {
                valid = valid && !(counts["short"] && counts["long"]);
                scalar = counts["short"] ? (counts["unsigned"] ? ScalarKind::UnsignedShort : ScalarKind::Short) : counts["long"] == 2 ? (counts["unsigned"] ? ScalarKind::UnsignedLongLong : ScalarKind::LongLong) : counts["long"] ? (counts["unsigned"] ? ScalarKind::UnsignedLong : ScalarKind::Long) : counts["unsigned"] ? ScalarKind::UnsignedInt : ScalarKind::Int;
            }
            if (!valid)
                throw std::invalid_argument("Invalid type specifiers: " + spelling);
            result = scalar_type(scalar);
        }
        while (index < words.size() && read_qualifier(words[index], qualifiers))
            ++index;
        result = qualified(result, qualifiers);
        while (index < words.size() && words[index] == "*") {
            ++index;
            qualifiers = {};
            while (index < words.size() && read_qualifier(words[index], qualifiers))
                ++index;
            result = qualified(pointer(result), qualifiers);
        }
        if (index != words.size())
            throw std::invalid_argument("Unexpected token in type name: " + words[index]);
        return TypePtr(new Type(result->kind, result->scalar, result->element, result->qualifiers, result->tag, result->name, result->bound, result->parameters, spelling));
    }
} // namespace mx
