#include "MXParser/semantic.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>

namespace mx {
    namespace {
        using S = ScalarKind;
        using K = TypeKind;
        std::atomic_size_t next_tag_identity{1};
        TypePtr bare(TypePtr type) {
            while (type && (type->kind == K::Qualified || (type->kind == K::Named && type->element)))
                type = type->element;
            return type;
        }
        bool floating(const TypePtr &type) {
            auto t = bare(type);
            return t && t->kind == K::Scalar && (t->scalar == S::Float || t->scalar == S::Double || t->scalar == S::LongDouble);
        }
        bool integer(const TypePtr &type) {
            auto t = bare(type);
            return t && ((t->kind == K::Tagged && t->tag == TagKind::Enum) || (t->kind == K::Scalar && t->scalar != S::Void && !floating(t)));
        }
        bool arithmetic(const TypePtr &type) { return integer(type) || floating(type); }
        bool void_type(const TypePtr &type) {
            auto t = bare(type);
            return t && t->kind == K::Scalar && t->scalar == S::Void;
        }
        bool pointer(const TypePtr &type) {
            auto t = bare(type);
            return t && t->kind == K::Pointer;
        }
        bool scalar(const TypePtr &type) { return arithmetic(type) || pointer(type); }
        bool unsigned_type(S type) { return type == S::Bool || type == S::UnsignedChar || type == S::UnsignedShort || type == S::UnsignedInt || type == S::UnsignedLong || type == S::UnsignedLongLong; }
        int rank(S type) {
            switch (type) {
            case S::Bool:
                return 0;
            case S::Char:
            case S::SignedChar:
            case S::UnsignedChar:
                return 1;
            case S::Short:
            case S::UnsignedShort:
                return 2;
            case S::Int:
            case S::UnsignedInt:
                return 3;
            case S::Long:
            case S::UnsignedLong:
                return 4;
            case S::LongLong:
            case S::UnsignedLongLong:
                return 5;
            case S::Float:
                return 6;
            case S::Double:
                return 7;
            case S::LongDouble:
                return 8;
            default:
                return -1;
            }
        }
        S unsigned_version(S type) {
            switch (type) {
            case S::LongLong:
                return S::UnsignedLongLong;
            case S::Long:
                return S::UnsignedLong;
            default:
                return S::UnsignedInt;
            }
        }
        bool complete(const TypePtr &type) {
            auto t = bare(type);
            if (!t)
                return false;
            if (t->kind == K::Scalar)
                return t->scalar != S::Void;
            if (t->kind == K::Pointer)
                return true;
            if (t->kind == K::Tagged)
                return t->layout && t->layout->complete;
            return t->kind == K::Array && t->bound && *t->bound > 0 && complete(t->element);
        }
        TypePtr decay(const TypePtr &type) {
            auto t = bare(type);
            return t->kind == K::Array ? Type::pointer(Type::qualified(t->element, type->top_qualifiers())) : t;
        }
        bool compatible(const TypePtr &left, const TypePtr &right, bool ignore_top = false) {
            if (!ignore_top && left->top_qualifiers() != right->top_qualifiers())
                return false;
            auto a = bare(left), b = bare(right);
            if (a->kind != b->kind)
                return false;
            switch (a->kind) {
            case K::Scalar:
                return a->scalar == b->scalar;
            case K::Tagged:
                return a->tag == b->tag && a->tag_identity == b->tag_identity;
            case K::Pointer:
                return compatible(a->element, b->element);
            case K::Array:
                return (!a->bound || !b->bound || a->bound == b->bound) && compatible(a->element, b->element);
            case K::Function:
                if (a->has_prototype != b->has_prototype || !compatible(a->element, b->element, true) || a->parameters.size() != b->parameters.size())
                    return false;
                for (size_t i = 0; i < a->parameters.size(); ++i)
                    if (!compatible(a->parameters[i], b->parameters[i], true))
                        return false;
                return true;
            default:
                return false;
            }
        }
        bool qualifiers_include(TypeQualifiers destination, TypeQualifiers source) { return (!source.is_const || destination.is_const) && (!source.is_volatile || destination.is_volatile) && (!source.is_atomic || destination.is_atomic); }
        bool pointer_conversion(const TypePtr &destination, const TypePtr &source) {
            if (!pointer(destination) || !pointer(source))
                return false;
            auto target = bare(destination)->element, value = bare(source)->element;
            if (!qualifiers_include(target->top_qualifiers(), value->top_qualifiers()))
                return false;
            return compatible(target, value, true) || void_type(target) || void_type(value);
        }
        size_t bits(const TypePtr &type) { return ScalarDataModel::size_of(type) * 8; }
        uint64_t mask(const TypePtr &type) {
            auto width = bits(type);
            return width == 64 ? UINT64_MAX : (uint64_t(1) << width) - 1;
        }
        int64_t signed_value(uint64_t value, const TypePtr &type) {
            auto width = bits(type);
            value &= mask(type);
            if (width < 64 && (value & (uint64_t(1) << (width - 1))))
                value |= ~mask(type);
            // bit_cast avoids an implementation-defined unsigned-to-signed conversion.
            return std::bit_cast<int64_t>(value);
        }
        uint64_t converted(uint64_t value, const TypePtr &source, const TypePtr &destination) {
            if (bare(destination)->scalar == S::Bool)
                return value != 0;
            if (!unsigned_type(bare(source)->scalar))
                value = std::bit_cast<uint64_t>(signed_value(value, source));
            return value & mask(destination);
        }
        bool assignment_operator(const std::string &op) { return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" || op == "|="; }
        TypeQualifiers combined_qualifiers(TypeQualifiers a, TypeQualifiers b) { return {a.is_const || b.is_const, a.is_volatile || b.is_volatile, a.is_restrict || b.is_restrict, a.is_atomic || b.is_atomic}; }
    } // namespace

    SemanticException::SemanticException(const std::string &message, SourceLocation location) : runtime_error(std::format("{} at line {}{}, found '{}'", message, location.line, location.column ? std::format(", column {}", location.column) : "", location.token)), location(std::move(location)) {}

    size_t ScalarDataModel::size_of(const TypePtr &type) {
        auto t = bare(type);
        if (!t)
            throw std::invalid_argument("Missing type");
        if (t->kind == K::Pointer)
            return 8;
        if (t->kind == K::Tagged && t->layout && t->layout->complete)
            return t->layout->size;
        if (t->kind == K::Array && t->bound && *t->bound != 0) {
            auto element = size_of(t->element);
            if (*t->bound > SIZE_MAX / element)
                throw std::invalid_argument("Array size exceeds the data model");
            return *t->bound * element;
        }
        if (t->kind != K::Scalar || t->scalar == S::Void)
            throw std::invalid_argument("Size requires a complete type with known extent");
        switch (t->scalar) {
        case S::Bool:
        case S::Char:
        case S::SignedChar:
        case S::UnsignedChar:
            return 1;
        case S::Short:
        case S::UnsignedShort:
            return 2;
        case S::Int:
        case S::UnsignedInt:
        case S::Float:
            return 4;
        default:
            return 8;
        }
    }
    TypePtr ScalarDataModel::integer_promotion(const TypePtr &type) {
        if (!integer(type))
            throw std::invalid_argument("Integer promotion requires integer type");
        return bare(type)->kind == K::Tagged || rank(bare(type)->scalar) < rank(S::Int) ? Type::scalar_type(S::Int) : bare(type);
    }
    TypePtr ScalarDataModel::arithmetic_type(const TypePtr &left, const TypePtr &right) {
        if (!arithmetic(left) || !arithmetic(right))
            throw std::invalid_argument("Arithmetic conversion requires arithmetic operands");
        if (floating(left) || floating(right))
            return rank(bare(left)->scalar) > rank(bare(right)->scalar) ? bare(left) : bare(right);
        auto a = integer_promotion(left), b = integer_promotion(right);
        if (a->scalar == b->scalar)
            return a;
        if (unsigned_type(a->scalar) == unsigned_type(b->scalar))
            return rank(a->scalar) > rank(b->scalar) ? a : b;
        auto u = unsigned_type(a->scalar) ? a : b;
        auto s = unsigned_type(a->scalar) ? b : a;
        if (rank(u->scalar) >= rank(s->scalar))
            return u;
        return size_of(s) > size_of(u) ? s : Type::scalar_type(unsigned_version(s->scalar));
    }

    struct SemanticAnalyzer::Implementation {
        SemanticSymbolTable table;
        std::unordered_map<const ExprNode *, ExpressionInfo> facts;
        std::unordered_map<std::string, SemanticSymbolTable::Entry> external_functions;
        struct Member {
            std::string name;
            TypePtr type;
            SourceLocation location;
            size_t offset;
        };
        std::unordered_map<size_t, std::vector<Member>> members;
        std::unordered_map<const Type *, TypePtr> definitions;
        size_t loop_depth = 0;
        struct SwitchContext {
            TypePtr promoted_type;
            std::set<uint64_t> values;
            bool default_seen = false;
        };
        std::vector<SwitchContext> switches;
        TypePtr return_type;
        struct Scope {
            SemanticSymbolTable &table;
            Scope(SemanticSymbolTable &table) : table(table) { table.push_scope(); }
            ~Scope() { table.pop_scope(); }
        };
        [[noreturn]] void fail(const SourceLocation &where, const std::string &message) const { throw SemanticException(message, where); }
        void require(bool condition, const SourceLocation &where, const std::string &message) const {
            if (!condition)
                fail(where, message);
        }
        SemanticSymbolTable::Entry entry(SemanticSymbolKind kind, TypePtr type, SourceLocation location) { return std::make_shared<SemanticSymbol>(kind, std::move(type), std::move(location)); }
        bool storage(const std::vector<std::string> &specifiers, const SourceLocation &where, bool typedef_declaration = false) {
            require(specifiers.size() <= 1, where, "Conflicting or duplicate storage classes");
            if (specifiers.empty()) {
                require(!typedef_declaration, where, "Missing typedef storage class");
                return false;
            }
            const auto &specifier = specifiers.front();
            require(specifier == "static" || specifier == "extern" || specifier == "typedef", where, "Unsupported storage class: " + specifier);
            require((specifier == "typedef") == typedef_declaration, where, "Invalid typedef storage class");
            return specifier == "static";
        }
        size_t alignment(const TypePtr &type) {
            auto t = bare(type);
            if (t->kind == K::Array)
                return alignment(t->element);
            if (t->kind == K::Tagged)
                return t->layout->alignment;
            return std::min(size_t(8), ScalarDataModel::size_of(t));
        }
        bool contains_const(const TypePtr &type) {
            auto t = bare(type);
            if (type->top_qualifiers().is_const)
                return true;
            if (t->kind == K::Array)
                return contains_const(t->element);
            return t->kind == K::Tagged && t->layout && t->layout->const_member;
        }
        size_t aligned(size_t size, size_t align, const SourceLocation &where) {
            require(size <= SIZE_MAX - (align - 1), where, "Struct layout exceeds the data model");
            return (size + align - 1) / align * align;
        }
        TypePtr tag(const TypePtr &raw, const SourceLocation &where, bool introduce = false) {
            auto type = bare(raw);
            if (type->tag_identity)
                return type;
            require(type->tag != TagKind::Union, where, "Unsupported feature: union types");
            if (auto cached = definitions.find(type.get()); cached != definitions.end())
                return cached->second;
            bool defining = bool(type->definition);
            auto found = (introduce || defining) ? table.lookup_current(type->name, true) : table.lookup(type->name, true);
            if (type->name.empty())
                found = {};
            if (found)
                require(bare(found->type)->tag == type->tag, where, "Conflicting tag kind: " + type->name);
            require(type->tag != TagKind::Enum || defining || found, where, "Unsupported feature: enum forward declaration");
            TypePtr canonical;
            if (found)
                canonical = found->type;
            else {
                canonical = Type::tagged(type->tag, type->name, next_tag_identity.fetch_add(1, std::memory_order_relaxed), {}, std::make_shared<TagLayout>());
                if (!type->name.empty())
                    table.enter(table.depth() - 1, type->name, entry(SemanticSymbolKind::Tag, canonical, where), true);
            }
            if (!defining)
                return canonical;
            require(!canonical->layout->complete && !canonical->layout->defining, where, "Duplicate tag definition: " + type->name);
            canonical->layout->defining = true;
            definitions[type.get()] = canonical;
            const auto &definition = *type->definition;
            if (type->tag == TagKind::Enum) {
                int64_t next_value = 0;
                for (const auto &enumerator : definition.enumerators) {
                    auto location = SourceLocation::from_token(enumerator.name);
                    int64_t value = next_value;
                    if (enumerator.value) {
                        auto info = infer(*enumerator.value);
                        require(integer(info.type) && info.constant_expression && info.integer_constant.has_value(), enumerator.value->location, "Enumerator requires an integer constant expression");
                        if (unsigned_type(bare(info.type)->scalar)) {
                            require(*info.integer_constant <= INT32_MAX, location, "Enumerator value must fit int");
                            value = static_cast<int64_t>(*info.integer_constant);
                        } else
                            value = signed_value(*info.integer_constant, info.type);
                    }
                    require(value >= INT32_MIN && value <= INT32_MAX, location, "Enumerator value must fit int");
                    auto name = enumerator.name.get_token();
                    require(!table.lookup_current(name), location, "Duplicate enumerator or ordinary identifier: " + name);
                    auto symbol = entry(SemanticSymbolKind::Enumerator, Type::scalar_type(S::Int), location);
                    symbol->integer_constant = uint64_t(value) & UINT32_MAX;
                    table.enter(table.depth() - 1, name, symbol);
                    next_value = value + 1;
                }
                canonical->layout->size = canonical->layout->alignment = 4;
            } else {
                std::vector<Member> fields;
                std::set<std::string> names;
                size_t size = 0, align = 1;
                for (const auto &declaration : definition.members) {
                    require(declaration->storage.empty() && !declaration->is_typedef, declaration->location, "Struct members cannot have storage classes");
                    require(!declaration->declarators.empty(), declaration->location, "Unsupported feature: anonymous struct member");
                    for (const auto &field : declaration->declarators) {
                        require(!field.initializer, field.location, "Struct members cannot have initializers");
                        auto member_type = resolve(field.type, field.location);
                        require(complete(member_type), field.location, "Struct member requires a complete object type; recursive objects and flexible arrays are unsupported");
                        auto name = field.name.get_token();
                        require(names.insert(name).second, field.location, "Duplicate struct member: " + name);
                        auto member_align = alignment(member_type);
                        size = aligned(size, member_align, field.location);
                        fields.push_back({name, member_type, field.location, size});
                        auto extent = ScalarDataModel::size_of(member_type);
                        require(size <= SIZE_MAX - extent, field.location, "Struct layout exceeds the data model");
                        size += extent;
                        align = std::max(align, member_align);
                        canonical->layout->const_member |= contains_const(member_type);
                    }
                }
                require(!fields.empty(), where, "Empty structs are unsupported");
                canonical->layout->size = aligned(size, align, where);
                canonical->layout->alignment = align;
                members[canonical->tag_identity] = std::move(fields);
            }
            canonical->layout->defining = false;
            canonical->layout->complete = true;
            return canonical;
        }
        TypePtr resolve(const TypePtr &raw, const SourceLocation &where, const std::vector<Parameter> *parameters = nullptr) {
            require(bool(raw), where, "Missing declared type");
            switch (raw->kind) {
            case K::Named: {
                auto symbol = table.lookup(raw->name);
                require(symbol && symbol->kind == SemanticSymbolKind::Typedef, where, "Unknown typedef name: " + raw->name);
                return symbol->type;
            }
            case K::Scalar:
                return Type::scalar_type(raw->scalar);
            case K::Tagged:
                return raw->tag_identity ? raw : tag(raw, where);
            case K::Qualified: {
                auto type = resolve(raw->element, where);
                require(!raw->qualifiers.is_atomic, where, "Unsupported feature: atomic types");
                require(!raw->qualifiers.is_restrict || (pointer(type) && !void_type(bare(type)->element)), where, "restrict requires a pointer to an object type");
                return Type::qualified(type, raw->qualifiers);
            }
            case K::Pointer: {
                auto target = resolve(raw->element, where);
                require(bare(target)->kind != K::Function, where, "Unsupported feature: function pointers");
                return Type::pointer(target);
            }
            case K::Array: {
                auto element = resolve(raw->element, where);
                require(!void_type(element) && bare(element)->kind != K::Function, where, "Array element must have object type");
                require(complete(element), where, "Array element requires a complete object type");
                auto extent = raw->bound;
                if (raw->bound_expression) {
                    auto bound = infer(*raw->bound_expression);
                    require(integer(bound.type), raw->bound_expression->location, "Array bound requires integer type");
                    require(bound.constant_expression && bound.integer_constant.has_value(), raw->bound_expression->location, "Array bound requires an integer constant expression; variable length arrays are unsupported");
                    require(*bound.integer_constant != 0 && (unsigned_type(bare(bound.type)->scalar) || signed_value(*bound.integer_constant, bound.type) > 0), raw->bound_expression->location, "Array bound must be positive");
                    require(*bound.integer_constant <= SIZE_MAX, where, "Array bound exceeds the data model");
                    extent = static_cast<size_t>(*bound.integer_constant);
                }
                if (extent) {
                    require(*extent > 0, where, "Array bound must be positive");
                    require(*extent <= SIZE_MAX / ScalarDataModel::size_of(element), where, "Array size exceeds the data model");
                }
                return Type::array(element, extent, raw->bound_expression);
            }
            case K::Function: {
                require(raw->has_prototype, where, "Use (void) or typed parameters; unprototyped functions are unsupported");
                auto result = resolve(raw->element, where);
                require(bare(result)->kind != K::Array && bare(result)->kind != K::Function, where, "Invalid function return type");
                Scope scope(table);
                return Type::function(result, resolve_parameters(raw, parameters, where));
            }
            }
            fail(where, "Unsupported type");
        }
        std::vector<TypePtr> resolve_parameters(const TypePtr &signature, const std::vector<Parameter> *parameters, const SourceLocation &where) {
            std::vector<TypePtr> types;
            for (size_t i = 0; i < signature->parameters.size(); ++i) {
                const auto *parameter = parameters && i < parameters->size() ? &(*parameters)[i] : nullptr;
                auto source = parameter ? parameter->location : where;
                if (parameter && parameter->declaration_type)
                    resolve(parameter->declaration_type, source);
                auto type = resolve(signature->parameters[i], source);
                if (bare(type)->kind == K::Array)
                    type = Type::pointer(bare(type)->element);
                require(!void_type(type) && bare(type)->kind != K::Function && complete(type), source, "Parameter requires a complete object type");
                types.push_back(type);
                if (parameter && parameter->name.get_type() != TOKEN_TYPE::TOKEN_NULL) {
                    auto name = parameter->name.get_token();
                    require(!table.lookup_current(name), SourceLocation::from_token(parameter->name), "Duplicate parameter: " + name);
                    table.enter(table.depth() - 1, name, entry(SemanticSymbolKind::Object, type, SourceLocation::from_token(parameter->name)));
                }
            }
            return types;
        }
        void assignable(const TypePtr &destination, const ExpressionInfo &value, const SourceLocation &where) {
            auto source = decay(value.type);
            bool valid = arithmetic(destination) && arithmetic(source);
            valid |= pointer(destination) && (value.null_pointer_constant || pointer_conversion(destination, source));
            valid |= bare(destination)->kind == K::Tagged && bare(destination)->tag == TagKind::Struct && complete(destination) && compatible(destination, source, true);
            valid |= bare(destination)->kind == K::Scalar && bare(destination)->scalar == S::Bool && pointer(source);
            require(valid, where, "Incompatible assignment or conversion from " + value.type->to_string() + " to " + destination->to_string());
        }
        void modifiable(const ExpressionInfo &value, const SourceLocation &where) { require(value.lvalue && value.modifiable, where, "Assignment or increment requires a modifiable lvalue (const and array objects cannot be modified)"); }
        TypePtr binary_type(const std::string &op, const ExpressionInfo &left, const ExpressionInfo &right, const SourceLocation &where) {
            auto a = decay(left.type), b = decay(right.type);
            if (op == "&&" || op == "||") {
                require(scalar(a) && scalar(b), where, "Logical operators require scalar operands");
                return Type::scalar_type(S::Int);
            }
            if (op == "==" || op == "!=") {
                bool valid = arithmetic(a) && arithmetic(b);
                valid |= pointer(a) && (right.null_pointer_constant || (pointer(b) && (compatible(bare(a)->element, bare(b)->element, true) || void_type(bare(a)->element) || void_type(bare(b)->element))));
                valid |= pointer(b) && left.null_pointer_constant;
                require(valid, where, "Equality operands have incompatible types");
                return Type::scalar_type(S::Int);
            }
            if (op == "<" || op == "<=" || op == ">" || op == ">=") {
                require((arithmetic(a) && arithmetic(b)) || (pointer(a) && pointer(b) && complete(bare(a)->element) && compatible(bare(a)->element, bare(b)->element, true)), where, "Relational operands require arithmetic or compatible object pointers");
                return Type::scalar_type(S::Int);
            }
            if ((op == "+" || op == "-") && (pointer(a) || pointer(b))) {
                if (pointer(a) && integer(b)) {
                    require(complete(bare(a)->element), where, "Pointer arithmetic requires a complete object type");
                    return a;
                }
                if (op == "+" && integer(a) && pointer(b)) {
                    require(complete(bare(b)->element), where, "Pointer arithmetic requires a complete object type");
                    return b;
                }
                require(op == "-" && pointer(a) && pointer(b) && complete(bare(a)->element) && compatible(bare(a)->element, bare(b)->element, true), where, "Invalid pointer arithmetic operands");
                return Type::scalar_type(S::Long);
            }
            if (op == "%" || op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
                require(integer(a) && integer(b), where, "Operator " + op + " requires integer operands");
                return op == "<<" || op == ">>" ? ScalarDataModel::integer_promotion(a) : ScalarDataModel::arithmetic_type(a, b);
            }
            require(arithmetic(a) && arithmetic(b), where, "Operator " + op + " requires arithmetic operands");
            return ScalarDataModel::arithmetic_type(a, b);
        }
        std::optional<uint64_t> fold_binary(const std::string &op, const ExpressionInfo &left, const ExpressionInfo &right, const TypePtr &result) {
            if (integer(left.type) && integer(right.type) && left.integer_constant) {
                if (op == "&&" && *left.integer_constant == 0)
                    return 0;
                if (op == "||" && *left.integer_constant != 0)
                    return 1;
            }
            if (!integer(result) || !integer(left.type) || !integer(right.type) || !left.integer_constant || !right.integer_constant)
                return {};
            auto common = op == "<<" || op == ">>" ? ScalarDataModel::integer_promotion(left.type) : ScalarDataModel::arithmetic_type(left.type, right.type);
            auto a = converted(*left.integer_constant, left.type, common), b = converted(*right.integer_constant, right.type, common);
            bool u = unsigned_type(common->scalar);
            auto x = signed_value(a, common), y = signed_value(b, common);
            if (op == "==")
                return a == b;
            if (op == "!=")
                return a != b;
            if (op == "<")
                return u ? a < b : x < y;
            if (op == "<=")
                return u ? a <= b : x <= y;
            if (op == ">")
                return u ? a > b : x > y;
            if (op == ">=")
                return u ? a >= b : x >= y;
            if (op == "&&")
                return a != 0 && b != 0;
            if (op == "||")
                return a != 0 || b != 0;
            if (op == "&")
                return (a & b) & mask(result);
            if (op == "|")
                return (a | b) & mask(result);
            if (op == "^")
                return (a ^ b) & mask(result);
            if (op == "<<" || op == ">>") {
                auto count = *right.integer_constant;
                if (count >= bits(common) || (!u && x < 0))
                    return {};
                if (op == ">>")
                    return a >> count;
                if (!u && a > (mask(common) >> 1) >> count)
                    return {};
                return (a << count) & mask(result);
            }
            if (op == "/" || op == "%") {
                auto minimum = bits(common) == 64 ? INT64_MIN : -(int64_t(1) << (bits(common) - 1));
                if (!b || (!u && x == minimum && y == -1))
                    return {};
                return (u ? (op == "/" ? a / b : a % b) : std::bit_cast<uint64_t>(op == "/" ? x / y : x % y)) & mask(result);
            }
            if (op != "+" && op != "-" && op != "*")
                return {};
            if (u)
                return (op == "+" ? a + b : op == "-" ? a - b : a * b) & mask(result);
            int64_t value = 0;
            bool overflow = op == "+" ? __builtin_add_overflow(x, y, &value) : op == "-" ? __builtin_sub_overflow(x, y, &value) : __builtin_mul_overflow(x, y, &value);
            auto width = bits(result);
            if (overflow || (width < 64 && (value < -(int64_t(1) << (width - 1)) || value >= (int64_t(1) << (width - 1)))))
                return {};
            return std::bit_cast<uint64_t>(value) & mask(result);
        }
        ExpressionInfo number(const NumberNode &node) {
            auto text = node.value.get_token();
            bool u = false;
            int longs = 0;
            while (!text.empty() && (text.back() == 'u' || text.back() == 'U' || text.back() == 'l' || text.back() == 'L')) {
                if (text.back() == 'u' || text.back() == 'U')
                    u = true;
                else
                    ++longs;
                text.pop_back();
            }
            uint64_t value = 0;
            int base = 10;
            size_t skip = 0;
            if (text.starts_with("0x") || text.starts_with("0X")) {
                base = 16;
                skip = 2;
            } else if (text.starts_with("0b") || text.starts_with("0B")) {
                base = 2;
                skip = 2;
            } else if (text.size() > 1 && text[0] == '0')
                base = 8;
            auto parsed = std::from_chars(text.data() + skip, text.data() + text.size(), value, base);
            require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(), node.location, "Integer literal exceeds the scalar data model");
            std::vector<S> candidates;
            if (u)
                candidates = longs == 2 ? std::vector<S>{S::UnsignedLongLong} : longs == 1 ? std::vector<S>{S::UnsignedLong, S::UnsignedLongLong} : std::vector<S>{S::UnsignedInt, S::UnsignedLong, S::UnsignedLongLong};
            else if (base == 10)
                candidates = longs == 2 ? std::vector<S>{S::LongLong} : longs == 1 ? std::vector<S>{S::Long, S::LongLong} : std::vector<S>{S::Int, S::Long, S::LongLong};
            else
                candidates = longs == 2 ? std::vector<S>{S::LongLong, S::UnsignedLongLong} : longs == 1 ? std::vector<S>{S::Long, S::UnsignedLong, S::LongLong, S::UnsignedLongLong} : std::vector<S>{S::Int, S::UnsignedInt, S::Long, S::UnsignedLong, S::LongLong, S::UnsignedLongLong};
            for (auto candidate : candidates) {
                auto type = Type::scalar_type(candidate);
                if (value <= (unsigned_type(candidate) ? mask(type) : mask(type) >> 1)) {
                    ExpressionInfo result;
                    result.type = type;
                    result.constant_expression = true;
                    result.integer_constant = value;
                    return result;
                }
            }
            fail(node.location, "Integer literal exceeds its permitted C types");
        }
        std::vector<unsigned char> literal_units(const std::string &text, const SourceLocation &where) {
            std::vector<unsigned char> result;
            for (size_t i = 1; i + 1 < text.size(); ++i) {
                unsigned value = static_cast<unsigned char>(text[i]);
                if (text[i] == '\\') {
                    require(++i + 1 < text.size(), where, "Invalid literal escape");
                    char escape = text[i];
                    if (escape >= '0' && escape <= '7') {
                        value = escape - '0';
                        for (int count = 1; count < 3 && i + 2 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '7'; ++count)
                            value = value * 8 + text[++i] - '0';
                    } else if (escape == 'x') {
                        value = 0;
                        size_t digits = 0;
                        while (i + 2 < text.size()) {
                            char c = text[i + 1];
                            unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
                            if (digit == 16)
                                break;
                            require(value <= 15, where, "Literal escape exceeds 8-bit character range");
                            value = value * 16 + digit;
                            ++i;
                            ++digits;
                        }
                        require(digits != 0, where, "Hexadecimal escape requires digits");
                    } else {
                        const std::string escapes = "abfnrtv\\'\"?";
                        const std::vector<unsigned> values = {7, 8, 12, 10, 13, 9, 11, 92, 39, 34, 63};
                        auto index = escapes.find(escape);
                        require(index != std::string::npos, where, "Unsupported literal escape");
                        value = values[index];
                    }
                }
                require(value <= 255, where, "Literal escape exceeds 8-bit character range");
                result.push_back(static_cast<unsigned char>(value));
            }
            return result;
        }
        ExpressionInfo infer(const ExprNode &expression) {
            if (auto found = facts.find(&expression); found != facts.end())
                return found->second;
            ExpressionInfo result;
            const auto &where = expression.location;
            if (auto node = dynamic_cast<const NumberNode *>(&expression))
                result = number(*node);
            else if (auto node = dynamic_cast<const LiteralNode *>(&expression)) {
                const auto &text = node->value.get_token();
                if (node->value.get_type() == TOKEN_TYPE::FLOAT_VALUE) {
                    char suffix = text.back();
                    result.type = Type::scalar_type(suffix == 'f' || suffix == 'F' ? S::Float : suffix == 'l' || suffix == 'L' ? S::LongDouble : S::Double);
                    long double value = 0;
                    try {
                        value = std::stold(text);
                    } catch (const std::exception &) {
                        fail(where, "Floating literal exceeds the scalar data model");
                    }
                    require(std::isfinite(value) && std::abs(value) <= (bare(result.type)->scalar == S::Float ? std::numeric_limits<float>::max() : std::numeric_limits<double>::max()), where, "Floating literal exceeds the scalar data model");
                    result.constant_expression = true;
                } else if (node->value.get_type() == TOKEN_TYPE::CHARACTER_VALUE) {
                    require(text.starts_with("'"), where, "Unsupported feature: wide character literals");
                    result.type = Type::scalar_type(S::Int);
                    result.constant_expression = true;
                    auto units = literal_units(text, where);
                    if (units.size() == 1)
                        result.integer_constant = units[0] < 128 ? uint64_t(units[0]) : uint64_t(0xffffff00u | units[0]);
                } else {
                    require(text.starts_with("\""), where, "Unsupported feature: wide string literals");
                    std::istringstream input(text);
                    Scanner scanner(input);
                    Token token;
                    size_t length = 1;
                    while (scanner.lex(token) != TOKEN_TYPE::TOKEN_NULL) {
                        require(token.get_type() == TOKEN_TYPE::STRING_VALUE && token.get_token().starts_with("\""), where, "Unsupported feature: wide string literals");
                        length += literal_units(token.get_token(), where).size();
                    }
                    result.type = Type::array(Type::scalar_type(S::Char), length);
                    result.lvalue = true;
                }
            } else if (auto node = dynamic_cast<const VariableNode *>(&expression)) {
                auto name = node->var.get_token();
                auto symbol = table.lookup(name);
                require(symbol && symbol->kind != SemanticSymbolKind::Typedef, SourceLocation::from_token(node->var), "Undeclared identifier: " + name);
                result.type = symbol->type;
                result.symbol = symbol;
                if (symbol->kind == SemanticSymbolKind::Enumerator) {
                    result.constant_expression = true;
                    result.integer_constant = symbol->integer_constant;
                }
                result.lvalue = symbol->kind == SemanticSymbolKind::Object;
            } else if (auto node = dynamic_cast<const UnaryNode *>(&expression)) {
                auto value = infer(*node->value);
                auto type = decay(value.type);
                auto op = node->op.get_token();
                auto source = SourceLocation::from_token(node->op);
                if (op == "&") {
                    require(bare(value.type)->kind != K::Function, source, "Unsupported feature: function pointers");
                    require(value.lvalue, source, "Address-of requires an lvalue");
                    result.type = Type::pointer(value.type);
                } else if (op == "*") {
                    require(pointer(type) && !void_type(bare(type)->element), source, "Dereference requires a pointer to an object");
                    result.type = bare(type)->element;
                    result.lvalue = true;
                } else if (op == "++" || op == "--") {
                    modifiable(value, source);
                    require(arithmetic(type) || (pointer(type) && complete(bare(type)->element)), source, "Increment requires arithmetic or complete object pointer type");
                    result.type = bare(value.type);
                } else {
                    require(op == "!" ? scalar(type) : op == "~" ? integer(type) : arithmetic(type), source, "Invalid operand type for unary " + op);
                    result.type = op == "!" ? Type::scalar_type(S::Int) : integer(type) ? ScalarDataModel::integer_promotion(type) : type;
                    result.constant_expression = value.constant_expression;
                    if (value.integer_constant && integer(result.type)) {
                        auto raw = converted(*value.integer_constant, value.type, result.type);
                        if (op == "!")
                            result.integer_constant = *value.integer_constant == 0;
                        else if (op == "+")
                            result.integer_constant = raw;
                        else if (op == "~")
                            result.integer_constant = (~raw) & mask(result.type);
                        else if (unsigned_type(bare(result.type)->scalar) || signed_value(raw, result.type) != (bits(result.type) == 64 ? INT64_MIN : -(int64_t(1) << (bits(result.type) - 1))))
                            result.integer_constant = (uint64_t(0) - raw) & mask(result.type);
                    }
                }
            } else if (auto node = dynamic_cast<const BinaryNode *>(&expression)) {
                auto left = infer(*node->left), right = infer(*node->right);
                auto op = node->op.get_token();
                auto source = SourceLocation::from_token(node->op);
                if (assignment_operator(op)) {
                    modifiable(left, source);
                    if (op == "=")
                        assignable(left.type, right, source);
                    else {
                        auto intermediate = right;
                        intermediate.type = binary_type(op.substr(0, op.size() - 1), left, right, source);
                        intermediate.null_pointer_constant = false;
                        assignable(left.type, intermediate, source);
                    }
                    result.type = bare(left.type);
                } else if (op == ",")
                    result.type = decay(right.type);
                else {
                    result.type = binary_type(op, left, right, source);
                    result.constant_expression = left.constant_expression && right.constant_expression;
                    if (result.constant_expression)
                        result.integer_constant = fold_binary(op, left, right, result.type);
                }
            } else if (auto node = dynamic_cast<const ConditionalNode *>(&expression)) {
                auto condition = infer(*node->condition), left = infer(*node->then_expr), right = infer(*node->else_expr);
                require(scalar(decay(condition.type)), node->condition->location, "Conditional test requires scalar type");
                auto a = decay(left.type), b = decay(right.type);
                if (arithmetic(a) && arithmetic(b))
                    result.type = ScalarDataModel::arithmetic_type(a, b);
                else if (bare(a)->kind == K::Tagged && bare(a)->tag == TagKind::Struct && compatible(a, b, true))
                    result.type = bare(a);
                else if (void_type(a) && void_type(b))
                    result.type = Type::scalar_type(S::Void);
                else if (pointer(a) && right.null_pointer_constant)
                    result.type = a;
                else if (pointer(b) && left.null_pointer_constant)
                    result.type = b;
                else {
                    require(pointer(a) && pointer(b) && (compatible(bare(a)->element, bare(b)->element, true) || void_type(bare(a)->element) || void_type(bare(b)->element)), where, "Conditional branches have incompatible types");
                    auto x = bare(a)->element, y = bare(b)->element;
                    auto target = void_type(x) || void_type(y) ? Type::scalar_type(S::Void) : bare(x);
                    result.type = Type::pointer(Type::qualified(target, combined_qualifiers(x->top_qualifiers(), y->top_qualifiers())));
                }
                result.constant_expression = condition.constant_expression && left.constant_expression && right.constant_expression;
                if (result.constant_expression && condition.integer_constant && integer(result.type)) {
                    const auto &selected = *condition.integer_constant ? left : right;
                    if (selected.integer_constant)
                        result.integer_constant = converted(*selected.integer_constant, selected.type, result.type);
                }
            } else if (auto node = dynamic_cast<const CastNode *>(&expression)) {
                auto value = infer(*node->value);
                result.type = resolve(node->declared_type(), where);
                auto source = decay(value.type);
                bool valid = void_type(result.type) || (arithmetic(result.type) && arithmetic(source));
                valid |= pointer(result.type) && (pointer(source) || value.null_pointer_constant);
                valid |= bare(result.type)->kind == K::Scalar && bare(result.type)->scalar == S::Bool && pointer(source);
                require(valid, where, "Invalid cast; pointer/integer casts require a null pointer constant");
                result.type = bare(result.type);
                result.constant_expression = value.constant_expression && arithmetic(result.type) && arithmetic(source);
                if (integer(result.type) && value.integer_constant)
                    result.integer_constant = converted(*value.integer_constant, value.type, result.type);
                if (integer(result.type) && floating(source) && dynamic_cast<const LiteralNode *>(node->value.get())) {
                    auto numeric = std::stold(dynamic_cast<const LiteralNode &>(*node->value).value.get_token());
                    if (bare(result.type)->scalar == S::Bool)
                        result.integer_constant = numeric != 0;
                    else {
                        bool u = unsigned_type(bare(result.type)->scalar);
                        auto limit = std::ldexp(1.0L, static_cast<int>(bits(result.type)) - (u ? 0 : 1));
                        if (numeric >= (u ? 0 : -limit) && numeric < limit)
                            result.integer_constant = (u ? static_cast<uint64_t>(numeric) : std::bit_cast<uint64_t>(static_cast<int64_t>(numeric))) & mask(result.type);
                    }
                }
                if (pointer(result.type) && void_type(bare(result.type)->element) && value.null_pointer_constant)
                    result.null_pointer_constant = true;
            } else if (auto node = dynamic_cast<const SizeofNode *>(&expression)) {
                auto type = node->value ? infer(*node->value).type : resolve(node->declared_type(), where);
                require(complete(type), where, "sizeof requires a complete object type");
                result.type = Type::scalar_type(S::UnsignedLong);
                result.constant_expression = true;
                try {
                    result.integer_constant = ScalarDataModel::size_of(type);
                } catch (const std::invalid_argument &) {
                }
            } else if (auto node = dynamic_cast<const CallNode *>(&expression)) {
                auto callee = infer(*node->callee);
                require(dynamic_cast<const VariableNode *>(node->callee.get()) != nullptr, where, "Only direct calls to declared functions are supported");
                auto signature = bare(callee.type);
                require(signature->kind == K::Function && signature->has_prototype, where, "Call requires a visible typed function prototype");
                require(signature->parameters.size() == node->arguments.size(), where, "Function call argument count does not match prototype");
                for (size_t i = 0; i < node->arguments.size(); ++i)
                    assignable(signature->parameters[i], infer(*node->arguments[i]), node->arguments[i]->location);
                result.type = bare(signature->element);
            } else if (auto node = dynamic_cast<const SubscriptNode *>(&expression)) {
                auto left = infer(*node->base), right = infer(*node->subscript);
                auto a = decay(left.type), b = decay(right.type);
                if (integer(a) && pointer(b))
                    std::swap(a, b);
                require(pointer(a) && integer(b) && complete(bare(a)->element), where, "Subscript requires an integer and a pointer to a complete object");
                result.type = bare(a)->element;
                result.lvalue = true;
            } else if (auto node = dynamic_cast<const MemberNode *>(&expression)) {
                auto base = infer(*node->base);
                auto type = node->op.get_token() == "->" ? decay(base.type) : base.type;
                if (node->op.get_token() == "->") {
                    require(pointer(type), SourceLocation::from_token(node->op), "Arrow member access requires a pointer to a struct");
                    type = bare(type)->element;
                }
                require(bare(type)->kind == K::Tagged && bare(type)->tag == TagKind::Struct, SourceLocation::from_token(node->member), "Member access requires a struct type");
                require(complete(type), SourceLocation::from_token(node->member), "Member access on incomplete struct");
                const auto &fields = members.at(bare(type)->tag_identity);
                auto field = std::find_if(fields.begin(), fields.end(), [&](const Member &member) { return member.name == node->member.get_token(); });
                require(field != fields.end(), SourceLocation::from_token(node->member), "Struct has no member named " + node->member.get_token());
                result.type = Type::qualified(field->type, type->top_qualifiers());
                result.lvalue = node->op.get_token() == "->" || base.lvalue;
                result.member_offset = field->offset;
            } else
                fail(where, "Unsupported expression in semantic validation");
            result.modifiable = result.lvalue && !contains_const(result.type) && bare(result.type)->kind != K::Array && complete(result.type);
            result.null_pointer_constant |= integer(result.type) && result.constant_expression && result.integer_constant && *result.integer_constant == 0;
            facts[&expression] = result;
            return result;
        }

        void declare_function(const std::string &name, TypePtr signature, const SourceLocation &where, bool definition, bool internal, size_t scope) {
            auto current = table.at_scope(scope, name);
            auto linked = external_functions.find(name);
            auto symbol = current ? current : linked == external_functions.end() ? SemanticSymbolTable::Entry{} : linked->second;
            auto global = table.at_scope(0, name);
            require(!global || global->kind != SemanticSymbolKind::Object, where, "Function conflicts with object: " + name);
            if (symbol) {
                require(symbol->kind == SemanticSymbolKind::Function && compatible(symbol->type, signature), where, "Incompatible function declaration: " + name);
                require(!definition || !symbol->defined, where, "Duplicate function definition: " + name);
                require(!internal || symbol->internal_linkage, where, "Conflicting function linkage: " + name);
                symbol->defined |= definition;
            } else {
                symbol = entry(SemanticSymbolKind::Function, signature, where);
                symbol->defined = definition;
                symbol->internal_linkage = internal;
                external_functions[name] = symbol;
            }
            if (!current)
                table.enter(scope, name, symbol);
        }
        void function(const FunctionNode &node) {
            bool internal = storage(node.storage, node.location);
            auto declared = node.signature_type();
            auto raw = declared->kind == K::Function ? declared : bare(resolve(declared, node.location));
            require(raw->kind == K::Function && raw->has_prototype, node.location, "Use (void) or typed parameters; unprototyped functions are unsupported");
            auto result = resolve(raw->element, node.location);
            require(void_type(result) || (bare(result)->kind != K::Array && complete(result)), node.location, "Function return requires void or complete object type");
            auto outer = table.depth() - 1;
            Scope scope(table);
            auto types = resolve_parameters(raw, &node.parameters, node.location);
            auto signature = Type::function(result, std::move(types));
            declare_function(node.name.get_token(), signature, SourceLocation::from_token(node.name), bool(node.body), internal, outer);
            if (node.body) {
                require(outer == 0, node.location, "Nested function definitions are unsupported");
                for (const auto &parameter : node.parameters)
                    require(parameter.name.get_type() != TOKEN_TYPE::TOKEN_NULL, parameter.location, "Function definition requires parameter names");
                auto previous = return_type;
                return_type = result;
                for (const auto &statement : node.body->statements)
                    analyze(*statement);
                return_type = previous;
            }
        }
        TypePtr initialize(TypePtr type, const ExprNode &expression, bool constant) {
            auto t = bare(type);
            auto list = dynamic_cast<const InitializerListNode *>(&expression);
            if (t->kind == K::Array) {
                const ExprNode *string = &expression;
                if (list && list->elements.size() == 1)
                    string = list->elements.front().get();
                auto literal = dynamic_cast<const LiteralNode *>(string);
                auto element = bare(t->element);
                bool character = element->kind == K::Scalar && (element->scalar == S::Char || element->scalar == S::SignedChar || element->scalar == S::UnsignedChar);
                if (character && literal && literal->value.get_type() == TOKEN_TYPE::STRING_VALUE) {
                    auto value = infer(*literal);
                    auto length = *bare(value.type)->bound;
                    require(!t->bound || *t->bound >= length - 1, expression.location, "String initializer is too long for character array");
                    return Type::qualified(Type::array(t->element, t->bound.value_or(length)), type->top_qualifiers());
                }
                require(list != nullptr, expression.location, "Array initializer requires positional braces; brace elision is unsupported");
                require(!list->elements.empty(), expression.location, "Empty initializer is unsupported");
                for (const auto &value : list->elements)
                    initialize(t->element, *value, constant);
                require(!t->bound || list->elements.size() <= *t->bound, expression.location, "Too many elements in array initializer");
                auto result = Type::array(t->element, t->bound.value_or(list->elements.size()));
                require(*result->bound <= SIZE_MAX / ScalarDataModel::size_of(t->element), expression.location, "Array size exceeds the data model");
                return Type::qualified(result, type->top_qualifiers());
            }
            if (t->kind == K::Tagged && t->tag == TagKind::Struct) {
                require(complete(type), expression.location, "Initializer requires a complete struct type");
                require(list != nullptr, expression.location, "Unsupported feature: aggregate copy initializer; use positional braces");
                const auto &fields = members.at(t->tag_identity);
                require(list->elements.size() <= fields.size(), expression.location, "Too many elements in struct initializer");
                for (size_t i = 0; i < list->elements.size(); ++i)
                    initialize(fields[i].type, *list->elements[i], constant);
                return type;
            }
            if (list) {
                require(list->elements.size() == 1, expression.location, "Scalar initializer requires exactly one element");
                require(!dynamic_cast<const InitializerListNode *>(list->elements.front().get()), expression.location, "Excess braces in scalar initializer");
                return initialize(type, *list->elements.front(), constant);
            }
            auto value = infer(expression);
            assignable(type, value, expression.location);
            if (constant)
                require(pointer(type) ? value.null_pointer_constant : value.constant_expression, expression.location, "File-scope initializer requires an arithmetic constant or null pointer; address constants are not implemented");
            if (constant && integer(value.type))
                require(value.integer_constant.has_value(), expression.location, "Invalid arithmetic in constant initializer");
            return type;
        }
        void declaration(const DeclarationNode &node) {
            bool internal = storage(node.storage, node.location, node.is_typedef);
            if (bare(node.declared_type())->kind == K::Tagged && bare(node.declared_type())->definition)
                resolve(node.declared_type(), node.location);
            if (node.declarators.empty()) {
                require(bare(node.declared_type())->kind == K::Tagged && !node.is_typedef, node.location, "Declaration requires a declarator");
                tag(node.declared_type(), node.location, true);
                return;
            }
            for (const auto &declarator : node.declarators) {
                auto raw = declarator.type ? declarator.type : node.declared_type();
                if (!node.is_typedef && bare(raw)->kind == K::Function) {
                    FunctionNode prototype;
                    prototype.location = node.location;
                    prototype.name = declarator.name;
                    prototype.type = raw;
                    prototype.parameters = declarator.parameters;
                    prototype.storage = node.storage;
                    function(prototype);
                    continue;
                }
                auto type = resolve(raw, declarator.location, &declarator.parameters);
                auto name = declarator.name.get_token();
                auto previous = table.lookup_current(name);
                if (node.is_typedef) {
                    require(!declarator.initializer, declarator.location, "Typedef cannot have an initializer");
                    require(!previous, declarator.location, "Duplicate typedef name: " + name);
                    table.enter(table.depth() - 1, name, entry(SemanticSymbolKind::Typedef, type, declarator.location));
                    continue;
                }
                require(!void_type(type) && bare(type)->kind != K::Function, declarator.location, "Object cannot have void or function type");
                require(complete(type) || (bare(type)->kind == K::Array && !bare(type)->bound && (bool(declarator.initializer) || (table.depth() == 1 && !node.storage.empty() && node.storage.front() == "extern"))), declarator.location, "Object has incomplete struct type or array extent");
                if (previous) {
                    require(table.depth() == 1 && previous->kind == SemanticSymbolKind::Object && compatible(previous->type, type), declarator.location, "Duplicate or incompatible object declaration: " + name);
                    require(!declarator.initializer || !previous->defined, declarator.location, "Duplicate object definition: " + name);
                    bool is_extern = !node.storage.empty() && node.storage.front() == "extern";
                    require((!internal || previous->internal_linkage) && (!previous->internal_linkage || internal || is_extern), declarator.location, "Conflicting object linkage: " + name);
                    previous->defined |= bool(declarator.initializer);
                    if (bare(type)->kind == K::Array) {
                        if (!bare(previous->type)->bound && bare(type)->bound)
                            previous->type = type;
                        if (!bare(type)->bound && bare(previous->type)->bound)
                            type = previous->type;
                    }
                } else {
                    require(table.depth() != 1 || !external_functions.contains(name), declarator.location, "Object conflicts with external function: " + name);
                    auto symbol = entry(SemanticSymbolKind::Object, type, declarator.location);
                    symbol->defined = bool(declarator.initializer);
                    symbol->internal_linkage = internal;
                    table.enter(table.depth() - 1, name, symbol);
                }
                if (declarator.initializer) {
                    type = initialize(type, *declarator.initializer, table.depth() == 1);
                    table.lookup_current(name)->type = type;
                }
            }
        }
        void analyze(const ASTNode &node) {
            if (auto expression = dynamic_cast<const ExprNode *>(&node)) {
                infer(*expression);
                return;
            }
            if (auto declaration_node = dynamic_cast<const DeclarationNode *>(&node)) {
                declaration(*declaration_node);
                return;
            }
            if (auto function_node = dynamic_cast<const FunctionNode *>(&node)) {
                function(*function_node);
                return;
            }
            if (auto block = dynamic_cast<const BlockNode *>(&node)) {
                Scope scope(table);
                for (const auto &statement : block->statements)
                    analyze(*statement);
                return;
            }
            if (auto ret = dynamic_cast<const ReturnNode *>(&node)) {
                require(bool(return_type), node.location, "return requires a function");
                if (ret->expression) {
                    auto value = infer(*ret->expression);
                    require(!void_type(return_type), node.location, "Void function must not return a value");
                    assignable(return_type, value, ret->expression->location);
                } else
                    require(void_type(return_type), node.location, "Non-void function must return a value");
                return;
            }
            if (auto branch = dynamic_cast<const IfNode *>(&node)) {
                require(scalar(decay(infer(*branch->condition).type)), branch->condition->location, "if condition requires scalar type");
                analyze(*branch->then_statement);
                if (branch->else_statement)
                    analyze(*branch->else_statement);
                return;
            }
            if (auto loop = dynamic_cast<const WhileNode *>(&node)) {
                require(scalar(decay(infer(*loop->condition).type)), loop->condition->location, "while condition requires scalar type");
                ++loop_depth;
                analyze(*loop->body);
                --loop_depth;
                return;
            }
            if (auto loop = dynamic_cast<const DoWhileNode *>(&node)) {
                Scope scope(table);
                ++loop_depth;
                {
                    Scope body_scope(table);
                    analyze(*loop->body);
                }
                --loop_depth;
                require(scalar(decay(infer(*loop->condition).type)), loop->condition->location, "do/while condition requires scalar type");
                return;
            }
            if (auto selection = dynamic_cast<const SwitchNode *>(&node)) {
                Scope scope(table);
                auto control = decay(infer(*selection->condition).type);
                require(integer(control), selection->condition->location, "switch condition requires integer type");
                switches.push_back({ScalarDataModel::integer_promotion(control), {}, false});
                {
                    Scope body_scope(table);
                    analyze(*selection->body);
                }
                switches.pop_back();
                return;
            }
            if (auto label = dynamic_cast<const CaseNode *>(&node)) {
                require(!switches.empty(), node.location, "case requires an enclosing switch");
                auto value = infer(*label->value);
                require(integer(value.type) && value.constant_expression && value.integer_constant.has_value(), label->value->location, "Case label requires an integer constant expression");
                auto key = converted(*value.integer_constant, value.type, switches.back().promoted_type);
                require(switches.back().values.insert(key).second, node.location, "Duplicate case value after conversion to the promoted switch type");
                analyze(*label->statement);
                return;
            }
            if (auto label = dynamic_cast<const DefaultNode *>(&node)) {
                require(!switches.empty(), node.location, "default requires an enclosing switch");
                require(!switches.back().default_seen, node.location, "Multiple default labels in one switch");
                switches.back().default_seen = true;
                analyze(*label->statement);
                return;
            }
            if (auto loop = dynamic_cast<const ForNode *>(&node)) {
                Scope scope(table);
                if (loop->initializer)
                    analyze(*loop->initializer);
                if (loop->condition)
                    require(scalar(decay(infer(*loop->condition).type)), loop->condition->location, "for condition requires scalar type");
                if (loop->update)
                    infer(*loop->update);
                ++loop_depth;
                analyze(*loop->body);
                --loop_depth;
                return;
            }
            if (auto jump = dynamic_cast<const JumpNode *>(&node)) {
                require(loop_depth != 0 || (jump->is_break && !switches.empty()), node.location, jump->is_break ? "break requires an enclosing loop or switch" : "continue requires an enclosing loop");
                return;
            }
            if (auto assignment = dynamic_cast<const AssignmentNode *>(&node)) {
                auto symbol = table.lookup(assignment->var.get_token());
                require(symbol && symbol->kind == SemanticSymbolKind::Object, node.location, "Assignment to undeclared object");
                require(!symbol->type->top_qualifiers().is_const && bare(symbol->type)->kind != K::Array, node.location, "Assignment requires a modifiable lvalue");
                assignable(symbol->type, infer(*assignment->expression), node.location);
                return;
            }
            fail(node.location, "Unsupported statement in semantic validation");
        }
    };

    SemanticAnalyzer::SemanticAnalyzer() : implementation(std::make_unique<Implementation>()) {}
    SemanticAnalyzer::~SemanticAnalyzer() = default;
    void SemanticAnalyzer::validate(const ProgramNode &program) {
        implementation = std::make_unique<Implementation>();
        for (const auto &declaration : program.declarations)
            implementation->analyze(*declaration);
    }
    const ExpressionInfo &SemanticAnalyzer::info(const ExprNode &expression) const { return implementation->facts.at(&expression); }
    const SemanticSymbolTable &SemanticAnalyzer::symbols() const { return implementation->table; }
} // namespace mx
