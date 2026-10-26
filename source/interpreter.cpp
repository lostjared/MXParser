#include "MXParser/interpreter.hpp"
#include "MXParser/parser.hpp"
#include "arithmetic.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace mx {
    namespace {
        using K = TypeKind;
        using S = ScalarKind;
        TypePtr bare(TypePtr type) {
            while (type->kind == K::Qualified || (type->kind == K::Named && type->element))
                type = type->element;
            return type;
        }
        bool floating(TypePtr type) {
            auto t = bare(type);
            return t->kind == K::Scalar && (t->scalar == S::Float || t->scalar == S::Double || t->scalar == S::LongDouble);
        }
        bool unsigned_type(TypePtr type) {
            auto s = bare(type)->scalar;
            return s == S::Bool || s == S::UnsignedChar || s == S::UnsignedShort || s == S::UnsignedInt || s == S::UnsignedLong || s == S::UnsignedLongLong;
        }
        bool aggregate(TypePtr type) {
            auto t = bare(type);
            return t->kind == K::Array || (t->kind == K::Tagged && t->tag == TagKind::Struct);
        }
        size_t size_of(TypePtr type) { return ScalarDataModel::size_of(type); }
        uint64_t mask(TypePtr type) {
            auto bits = size_of(type) * 8;
            return bits == 64 ? UINT64_MAX : (uint64_t(1) << bits) - 1;
        }
        int64_t signed_bits(uint64_t value, TypePtr type) {
            auto bits = size_of(type) * 8;
            value &= mask(type);
            if (bits < 64 && (value & (uint64_t(1) << (bits - 1))))
                value |= ~mask(type);
            return std::bit_cast<int64_t>(value);
        }
        struct Allocation;
        struct Pointer {
            Allocation *allocation = nullptr;
            size_t offset = 0, lower = 0, upper = 0;
        };
        struct Snapshot;
        struct Value {
            TypePtr type = Type::scalar_type(S::Void);
            std::variant<std::monostate, uint64_t, double, Pointer, std::shared_ptr<Snapshot>> data;
        };
        struct Cell {
            Value value;
            bool initialized = false;
            bool readonly = false;
        };
        struct Allocation {
            uintptr_t address = 0;
            bool alive = true;
            size_t size = 0;
            std::map<size_t, Cell> cells;
        };
        struct Snapshot {
            std::map<size_t, Cell> cells;
        };
        struct Reference {
            Pointer pointer;
            TypePtr type;
        };
        bool assignment(const std::string &op) { return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" || op == "|="; }
        std::vector<unsigned char> decode(const std::string &text) {
            std::vector<unsigned char> result;
            for (size_t i = 1; i + 1 < text.size(); ++i) {
                unsigned value = static_cast<unsigned char>(text[i]);
                if (text[i] == '\\') {
                    char c = text[++i];
                    if (c >= '0' && c <= '7') {
                        value = c - '0';
                        for (int n = 1; n < 3 && i + 2 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '7'; ++n)
                            value = value * 8 + text[++i] - '0';
                    } else if (c == 'x') {
                        value = 0;
                        while (i + 2 < text.size()) {
                            auto c = text[i + 1];
                            unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
                            if (digit == 16)
                                break;
                            value = value * 16 + digit;
                            ++i;
                        }
                    } else {
                        const std::string escapes = "abfnrtv\\'\"?";
                        const std::vector<unsigned> values = {7, 8, 12, 10, 13, 9, 11, 92, 39, 34, 63};
                        value = values.at(escapes.find(c));
                    }
                }
                result.push_back(static_cast<unsigned char>(value));
            }
            return result;
        }
    } // namespace
    struct NativeObject::State {
        std::weak_ptr<int> lifetime;
        TypePtr type;
        std::function<ExecutionValue()> read;
        std::function<void(const ExecutionValue &)> write;
        std::function<NativeObject(size_t)> element;
        std::function<NativeObject(const std::string &)> field;
        std::function<std::string()> text;
        void check() const {
            if (lifetime.expired())
                throw std::runtime_error("native object used outside its callback");
        }
    };
    NativeObject::NativeObject(std::shared_ptr<State> state) : state(std::move(state)) {}
    TypePtr NativeObject::type() const {
        state->check();
        return state->type;
    }
    ExecutionValue NativeObject::value() const {
        state->check();
        return state->read();
    }
    void NativeObject::set(const ExecutionValue &value) const {
        state->check();
        state->write(value);
    }
    NativeObject NativeObject::dereference() const {
        state->check();
        if (state->type->unqualified().kind != K::Pointer)
            throw std::runtime_error("dereference requires a pointer");
        return state->element(0);
    }
    NativeObject NativeObject::at(size_t index) const {
        state->check();
        return state->element(index);
    }
    NativeObject NativeObject::member(const std::string &name) const {
        state->check();
        return state->field(name);
    }
    std::string NativeObject::string() const {
        state->check();
        return state->text();
    }

    struct Interpreter::Implementation {
        SemanticAnalyzer analyzer;
        std::unordered_map<std::string, NativeCallback> callbacks;
        bool in_callback = false;
        ExecutionLimits limits;
        size_t remaining = 0;
        SourceLocation location;
        explicit Implementation(ExecutionLimits limits) : limits(limits) {}
        [[noreturn]] void fail(const std::string &message) const { throw ExecutionException(message, location); }
        void require(bool condition, const std::string &message) const {
            if (!condition)
                fail(message);
        }
        void tick() {
            require(remaining > 0, "instruction limit exceeded");
            --remaining;
        }
        enum class Action { Nop, Expression, Declaration, Test, Jump, Selection, Return };
        struct Instruction {
            Action action = Action::Nop;
            const ASTNode *node = nullptr;
            const ExprNode *expression = nullptr;
            size_t target = 0;
            std::vector<size_t> path;
            std::vector<std::pair<uint64_t, size_t>> cases;
            TypePtr control_type;
        };
        struct Plan {
            const FunctionNode *function = nullptr;
            std::vector<Instruction> code;
            std::vector<size_t> labels;
            std::vector<std::vector<const SemanticSymbol *>> locals{1};
        };
        std::unordered_map<std::string, Plan> functions;
        std::vector<std::unique_ptr<ProgramNode>> submissions;
        std::string session_source;
        size_t declaration_count = 0;
        size_t query_count = 0;
        using Objects = std::unordered_map<const SemanticSymbol *, Reference>;
        Objects globals;
        std::vector<std::unique_ptr<Allocation>> allocations;
        uintptr_t next_address = 0x10000;
        std::unordered_map<const LiteralNode *, Reference> strings;
        struct Scope {
            size_t id;
            Objects objects;
        };
        struct Frame {
            Plan *plan;
            std::vector<Scope> scopes;
            std::vector<Allocation *> temporaries;
        };
        std::vector<std::unique_ptr<Frame>> frames;
        ~Implementation() {
            for (auto &[symbol, ref] : globals) {
                (void)symbol;
                ref.pointer.allocation->alive = false;
            }
        }
        Value integer(uint64_t bits, TypePtr type = Type::scalar_type(S::Int)) { return {type, bits & mask(type)}; }
        Value from_public(const ExecutionValue &value) {
            if (auto n = std::get_if<int64_t>(&value))
                return integer(std::bit_cast<uint64_t>(*n), Type::scalar_type(S::LongLong));
            if (auto n = std::get_if<uint64_t>(&value))
                return integer(*n, Type::scalar_type(S::UnsignedLongLong));
            if (auto n = std::get_if<double>(&value))
                return {Type::scalar_type(S::Double), *n};
            return {};
        }
        ExecutionValue to_public(const Value &value) {
            if (auto n = std::get_if<uint64_t>(&value.data))
                return unsigned_type(value.type) ? ExecutionValue(*n) : ExecutionValue(signed_bits(*n, value.type));
            if (auto n = std::get_if<double>(&value.data))
                return *n;
            if (std::holds_alternative<std::monostate>(value.data))
                return std::monostate{};
            fail("object is not a scalar value");
        }
        NativeObject native_object(Reference ref, const std::shared_ptr<int> &lifetime) {
            auto state = std::make_shared<NativeObject::State>();
            state->lifetime = lifetime;
            state->type = ref.type;
            state->read = [this, ref] { return to_public(load(ref)); };
            state->write = [this, ref](const ExecutionValue &value) {
                require(!ref.type->top_qualifiers().is_const, "write to const object");
                require(!std::holds_alternative<std::monostate>(value), "cannot store void value");
                require(!aggregate(ref.type), "set requires a scalar object");
                store(ref, from_public(value));
            };
            std::weak_ptr<int> weak = lifetime;
            state->element = [this, ref, weak](size_t index) {
                auto t = bare(ref.type);
                require(t->kind == K::Array || t->kind == K::Pointer, "index requires an array or pointer");
                Reference child;
                if (t->kind == K::Array) {
                    require(index < *t->bound, "out-of-bounds native array access");
                    child = array_element(ref, index);
                    child.type = Type::qualified(child.type, ref.type->top_qualifiers());
                } else {
                    auto pointer = std::get<Pointer>(load(ref).data);
                    auto width = size_of(t->element);
                    require(width && pointer.offset <= pointer.upper && index <= (pointer.upper - pointer.offset) / width, "out-of-bounds native pointer access");
                    pointer.offset += index * width;
                    child = {pointer, t->element};
                }
                access(child);
                return native_object(child, weak.lock());
            };
            state->field = [this, ref, weak](const std::string &name) {
                auto t = bare(ref.type);
                require(t->kind == K::Tagged && t->tag == TagKind::Struct, "member requires a struct object");
                for (const auto &field : analyzer.members(t))
                    if (field.name == name) {
                        auto child = member(ref, Type::qualified(field.type, ref.type->top_qualifiers()), field.offset);
                        access(child);
                        return native_object(child, weak.lock());
                    }
                fail("unknown native object member: " + name);
            };
            state->text = [this, ref] { return c_string(load(ref)); };
            return NativeObject(std::move(state));
        }
        Value invoke_native(const std::string &name, const std::vector<Value> &arguments, const NativeCallback &callback, TypePtr signature) {
            if (!signature) {
                auto symbol = analyzer.symbols().lookup(name);
                require(symbol && symbol->kind == SemanticSymbolKind::Function, "native function requires a C prototype: " + name);
                signature = symbol->type;
            }
            signature = bare(signature);
            require(signature->has_prototype, "native function requires a C prototype: " + name);
            require(arguments.size() == signature->parameters.size(), "wrong argument count for " + name);
            require(bare(signature->element)->kind == K::Scalar, "native callbacks return scalars or void only");
            tick();
            auto lifetime = std::make_shared<int>(0);
            std::vector<Allocation *> temporaries;
            std::vector<NativeObject> objects;
            try {
                for (size_t i = 0; i < arguments.size(); ++i) {
                    auto ref = allocate(signature->parameters[i], false);
                    temporaries.push_back(ref.pointer.allocation);
                    store(ref, arguments[i], true);
                    objects.push_back(native_object(ref, lifetime));
                }
                in_callback = true;
                auto result = callback(objects);
                in_callback = false;
                lifetime.reset();
                for (auto allocation : temporaries)
                    allocation->alive = false;
                require(bare(signature->element)->scalar == S::Void || !std::holds_alternative<std::monostate>(result), "native callback returned void for a non-void function");
                return convert(from_public(result), signature->element);
            } catch (const std::exception &error) {
                in_callback = false;
                lifetime.reset();
                for (auto allocation : temporaries)
                    allocation->alive = false;
                fail("native callback " + name + ": " + error.what());
            } catch (...) {
                in_callback = false;
                lifetime.reset();
                for (auto allocation : temporaries)
                    allocation->alive = false;
                fail("native callback " + name + " threw an unknown exception");
            }
        }
        uint64_t bits(const Value &value) {
            require(std::holds_alternative<uint64_t>(value.data), "integer operand required");
            return std::get<uint64_t>(value.data);
        }
        long double numeric(const Value &value) {
            if (auto p = std::get_if<double>(&value.data))
                return *p;
            auto n = bits(value);
            return unsigned_type(value.type) ? static_cast<long double>(n) : static_cast<long double>(signed_bits(n, value.type));
        }
        bool truth(const Value &value) {
            if (auto p = std::get_if<Pointer>(&value.data)) {
                if (p->allocation)
                    require(p->allocation->alive, "pointer to expired object");
                return bool(p->allocation);
            }
            return numeric(value) != 0;
        }
        Value convert(Value value, TypePtr type) {
            auto t = bare(type);
            if (t->kind == K::Scalar && t->scalar == S::Void)
                return {};
            if (aggregate(type)) {
                require(aggregate(value.type) && bare(value.type)->tag_identity == t->tag_identity, "incompatible aggregate value");
                value.type = type;
                return value;
            }
            if (t->kind == K::Pointer) {
                if (auto p = std::get_if<Pointer>(&value.data))
                    return {type, *p};
                require(std::holds_alternative<uint64_t>(value.data) && bits(value) == 0, "unsupported integer-to-pointer conversion");
                return {type, Pointer{}};
            }
            if (t->scalar == S::Bool)
                return integer(truth(value), type);
            if (floating(type)) {
                double n = static_cast<double>(numeric(value));
                if (t->scalar == S::Float)
                    n = static_cast<float>(n);
                require(std::isfinite(n), "floating conversion overflow");
                return {type, n};
            }
            uint64_t n;
            if (auto floating = std::get_if<double>(&value.data)) {
                auto number = std::trunc(static_cast<long double>(*floating));
                auto limit = std::ldexp(1.0L, static_cast<int>(size_of(type) * 8) - (unsigned_type(type) ? 0 : 1));
                require(std::isfinite(number) && number >= (unsigned_type(type) ? 0 : -limit) && number < limit, "floating-to-integer conversion out of range");
                n = unsigned_type(type) ? static_cast<uint64_t>(number) : std::bit_cast<uint64_t>(static_cast<int64_t>(number));
            } else {
                n = bits(value);
                if (!unsigned_type(value.type))
                    n = std::bit_cast<uint64_t>(signed_bits(n, value.type));
            }
            return integer(n, type);
        }
        void access(const Reference &ref) {
            const auto &p = ref.pointer;
            require(p.allocation && p.allocation->alive, "null or expired object pointer");
            auto size = size_of(ref.type);
            require(p.offset >= p.lower && p.offset < p.upper && size <= p.upper - p.offset && p.offset <= p.allocation->size && size <= p.allocation->size - p.offset, "out-of-bounds or one-past object access");
        }
        void layout(Allocation *allocation, TypePtr type, size_t offset, bool zero, bool readonly = false) {
            readonly |= type->top_qualifiers().is_const;
            auto t = bare(type);
            if (t->kind == K::Array) {
                for (size_t i = 0; i < *t->bound; ++i)
                    layout(allocation, t->element, offset + i * size_of(t->element), zero, readonly);
                return;
            }
            if (aggregate(t)) {
                for (const auto &member : analyzer.members(t))
                    layout(allocation, member.type, offset + member.offset, zero, readonly);
                return;
            }
            Value value = t->kind == K::Pointer ? Value{type, Pointer{}} : floating(t) ? Value{type, 0.0} : integer(0, type);
            allocation->cells[offset] = {value, zero, readonly};
        }
        Reference allocate(TypePtr type, bool zero, bool readonly = false) {
            size_t size = size_of(type);
            require(size <= limits.object_bytes, "object allocation limit exceeded");
            auto allocation = std::make_unique<Allocation>();
            allocation->size = size;
            require(size <= UINTPTR_MAX - 16 && next_address <= UINTPTR_MAX - size - 16, "virtual address space exhausted");
            allocation->address = next_address;
            next_address += size + 16;
            auto pointer = allocation.get();
            allocations.push_back(std::move(allocation));
            layout(pointer, type, 0, zero, readonly);
            return {{pointer, 0, 0, size}, type};
        }
        Reference member(Reference ref, TypePtr type, size_t offset) {
            ref.pointer.offset += offset;
            ref.type = type;
            ref.pointer.lower = ref.pointer.offset;
            ref.pointer.upper = ref.pointer.offset + size_of(type);
            return ref;
        }
        Reference array_element(Reference ref, size_t index) {
            auto t = bare(ref.type);
            ref.pointer.lower = ref.pointer.offset;
            ref.pointer.upper = ref.pointer.offset + size_of(t);
            ref.pointer.offset += index * size_of(t->element);
            ref.type = t->element;
            return ref;
        }
        Value load(Reference ref) {
            access(ref);
            auto t = bare(ref.type);
            if (t->kind == K::Array) {
                auto p = ref.pointer;
                p.lower = p.offset;
                p.upper = p.offset + size_of(t);
                return {Type::pointer(Type::qualified(t->element, ref.type->top_qualifiers())), p};
            }
            if (aggregate(t)) {
                auto snapshot = std::make_shared<Snapshot>();
                auto first = ref.pointer.allocation->cells.lower_bound(ref.pointer.offset);
                for (auto it = first; it != ref.pointer.allocation->cells.end() && it->first < ref.pointer.offset + size_of(t); ++it)
                    snapshot->cells[it->first - ref.pointer.offset] = it->second;
                return {ref.type, snapshot};
            }
            auto found = ref.pointer.allocation->cells.find(ref.pointer.offset);
            require(found != ref.pointer.allocation->cells.end(), "unaligned or unsupported object representation access");
            require(found->second.initialized, "read of uninitialized object");
            require(bare(found->second.value.type)->kind == t->kind && size_of(found->second.value.type) == size_of(t) && floating(found->second.value.type) == floating(t), "unsupported aliasing access");
            return convert(found->second.value, ref.type);
        }
        void store(Reference ref, Value value, bool initializing = false) {
            access(ref);
            value = convert(std::move(value), ref.type);
            auto &cells = ref.pointer.allocation->cells;
            if (aggregate(ref.type)) {
                auto snapshot = std::get<std::shared_ptr<Snapshot>>(value.data);
                for (const auto &[offset, cell] : snapshot->cells) {
                    auto found = cells.find(ref.pointer.offset + offset);
                    require(found != cells.end(), "incompatible aggregate layout");
                    require(initializing || !found->second.readonly, "write to const object");
                    auto readonly = found->second.readonly;
                    found->second = cell;
                    found->second.readonly = readonly;
                }
            } else {
                auto found = cells.find(ref.pointer.offset);
                require(found != cells.end(), "unaligned object store");
                require(initializing || !found->second.readonly, "write to const object");
                require(bare(found->second.value.type)->kind == bare(ref.type)->kind && size_of(found->second.value.type) == size_of(ref.type) && floating(found->second.value.type) == floating(ref.type), "unsupported aliasing store");
                found->second.value = std::move(value);
                found->second.initialized = true;
            }
        }
        Reference object(const SemanticSymbol *symbol) {
            if (!frames.empty())
                for (auto it = frames.back()->scopes.rbegin(); it != frames.back()->scopes.rend(); ++it) {
                    auto found = it->objects.find(symbol);
                    if (found != it->objects.end())
                        return found->second;
                }
            auto found = globals.find(symbol);
            require(found != globals.end(), "undefined external object or object outside lifetime");
            return found->second;
        }
        Reference literal(const LiteralNode &node) {
            if (auto found = strings.find(&node); found != strings.end())
                return found->second;
            auto type = analyzer.info(node).type;
            auto ref = allocate(type, true, true);
            std::istringstream input(node.value.get_token());
            Scanner scanner(input);
            Token token;
            size_t offset = 0;
            while (scanner.lex(token) != TOKEN_TYPE::TOKEN_NULL)
                for (auto unit : decode(token.get_token()))
                    store(array_element(ref, offset++), integer(unit, Type::scalar_type(S::Char)), true);
            strings[&node] = ref;
            return ref;
        }
        Reference reference(const ExprNode &expression) {
            location = expression.location;
            if (auto n = dynamic_cast<const VariableNode *>(&expression))
                return object(analyzer.info(*n).symbol.get());
            if (auto n = dynamic_cast<const LiteralNode *>(&expression))
                return literal(*n);
            if (auto n = dynamic_cast<const UnaryNode *>(&expression); n && n->op.get_token() == "*") {
                auto value = eval(*n->value);
                return {std::get<Pointer>(value.data), analyzer.info(expression).type};
            }
            if (auto n = dynamic_cast<const SubscriptNode *>(&expression)) {
                auto left = eval(*n->base), right = eval(*n->subscript);
                if (!std::holds_alternative<Pointer>(left.data))
                    std::swap(left, right);
                auto value = binary("+", left, right, left.type);
                return {std::get<Pointer>(value.data), analyzer.info(expression).type};
            }
            if (auto n = dynamic_cast<const MemberNode *>(&expression)) {
                Reference base;
                if (n->op.get_token() == "->") {
                    auto value = eval(*n->base);
                    base = {std::get<Pointer>(value.data), bare(value.type)->element};
                } else if (analyzer.info(*n->base).lvalue)
                    base = reference(*n->base);
                else {
                    auto value = eval(*n->base);
                    base = allocate(value.type, false);
                    store(base, value, true);
                    require(!frames.empty(), "aggregate temporary outside function");
                    frames.back()->temporaries.push_back(base.pointer.allocation);
                }
                access(base);
                return member(base, analyzer.info(expression).type, *analyzer.info(expression).member_offset);
            }
            fail("expression is not an object reference");
        }
        Value binary(const std::string &op, Value left, Value right, TypePtr result) {
            if (op == "&&" || op == "||")
                return integer(op == "&&" ? (truth(left) && truth(right)) : (truth(left) || truth(right)));
            if (std::holds_alternative<Pointer>(left.data) || std::holds_alternative<Pointer>(right.data)) {
                if (!std::holds_alternative<Pointer>(left.data) && op == "+")
                    std::swap(left, right);
                if (op == "==" || op == "!=") {
                    if (!std::holds_alternative<Pointer>(left.data))
                        left = convert(left, right.type);
                    if (!std::holds_alternative<Pointer>(right.data))
                        right = convert(right, left.type);
                    auto a = std::get<Pointer>(left.data), b = std::get<Pointer>(right.data);
                    require((!a.allocation || a.allocation->alive) && (!b.allocation || b.allocation->alive), "comparison using expired pointer");
                    bool equal = a.allocation == b.allocation && a.offset == b.offset;
                    return integer(op == "==" ? equal : !equal);
                }
                auto a = std::get<Pointer>(left.data);
                require(a.allocation && a.allocation->alive, "arithmetic on null or expired pointer");
                auto stride = size_of(bare(left.type)->element);
                if (auto b = std::get_if<Pointer>(&right.data)) {
                    require(b->allocation == a.allocation && a.lower == b->lower && a.upper == b->upper, "pointer operation requires the same array object");
                    if (op == "-") {
                        auto difference = static_cast<int64_t>(a.offset) - static_cast<int64_t>(b->offset);
                        return integer(std::bit_cast<uint64_t>(difference / static_cast<int64_t>(stride)), Type::scalar_type(S::Long));
                    }
                    bool value = op == "<" ? a.offset < b->offset : op == "<=" ? a.offset <= b->offset : op == ">" ? a.offset > b->offset : a.offset >= b->offset;
                    return integer(value);
                }
                auto number = numeric(right);
                if (op == "-")
                    number = -number;
                auto offset = static_cast<long double>(a.offset) + number * stride;
                require(offset >= a.lower && offset <= a.upper && offset <= SIZE_MAX, "pointer arithmetic outside array bounds");
                a.offset = static_cast<size_t>(offset);
                return {left.type, a};
            }
            auto common = (op == "<<" || op == ">>") ? ScalarDataModel::integer_promotion(left.type) : ScalarDataModel::arithmetic_type(left.type, right.type);
            left = convert(left, common);
            right = convert(right, (op == "<<" || op == ">>") ? ScalarDataModel::integer_promotion(right.type) : common);
            bool comparison = op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
            if (floating(common)) {
                double a = static_cast<double>(numeric(left)), b = static_cast<double>(numeric(right));
                if (comparison)
                    return integer(op == "==" ? a == b : op == "!=" ? a != b : op == "<" ? a < b : op == "<=" ? a <= b : op == ">" ? a > b : a >= b);
                require(op != "/" || b != 0, "division by zero");
                auto value = op == "+" ? a + b : op == "-" ? a - b : op == "*" ? a * b : a / b;
                return convert({common, value}, result);
            }
            auto a = bits(left), b = bits(right);
            auto x = signed_bits(a, common), y = signed_bits(b, right.type);
            bool u = unsigned_type(common);
            if (comparison) {
                bool value = op == "==" ? a == b : op == "!=" ? a != b : op == "<" ? (u ? a < b : x < y) : op == "<=" ? (u ? a <= b : x <= y) : op == ">" ? (u ? a > b : x > y) : (u ? a >= b : x >= y);
                return integer(value);
            }
            uint64_t value = 0;
            if (op == "&")
                value = a & b;
            else if (op == "|")
                value = a | b;
            else if (op == "^")
                value = a ^ b;
            else if (op == "<<" || op == ">>") {
                require(numeric(right) >= 0 && b < size_of(common) * 8, "invalid shift count");
                if (op == "<<") {
                    require(u || x >= 0, "left shift of negative signed value");
                    require(u || a <= ((mask(common) >> 1) >> b), "signed shift overflow");
                    value = a << b;
                } else
                    value = u ? a >> b : std::bit_cast<uint64_t>(x >> b);
            } else if (op == "/" || op == "%") {
                require(b != 0, "division by zero");
                auto min = size_of(common) == 8 ? INT64_MIN : -(int64_t(1) << (size_of(common) * 8 - 1));
                require(u || x != min || y != -1, "signed division overflow");
                value = u ? (op == "/" ? a / b : a % b) : std::bit_cast<uint64_t>(op == "/" ? x / y : x % y);
            } else if (u)
                value = op == "+" ? a + b : op == "-" ? a - b : a * b;
            else {
                int64_t n = 0;
                bool overflow = op == "+" ? add_overflow(x, y, &n) : op == "-" ? subtract_overflow(x, y, &n) : multiply_overflow(x, y, &n);
                auto limit = mask(common) >> 1;
                require(!overflow && n >= -static_cast<int64_t>(limit) - 1 && n <= static_cast<int64_t>(limit), "signed arithmetic overflow");
                value = std::bit_cast<uint64_t>(n);
            }
            return convert(integer(value, common), result);
        }
        Value eval(const ExprNode &expression) {
            location = expression.location;
            tick();
            const auto &info = analyzer.info(expression);
            if (auto n = dynamic_cast<const NumberNode *>(&expression)) {
                (void)n;
                return integer(*info.integer_constant, info.type);
            }
            if (auto n = dynamic_cast<const LiteralNode *>(&expression)) {
                if (bare(info.type)->kind == K::Array)
                    return load(literal(*n));
                if (info.integer_constant)
                    return integer(*info.integer_constant, info.type);
                if (floating(info.type))
                    return convert({info.type, std::stod(n->value.get_token())}, info.type);
                fail("unsupported multicharacter literal");
            }
            if (dynamic_cast<const VariableNode *>(&expression) && info.symbol->kind == SemanticSymbolKind::Enumerator)
                return integer(*info.integer_constant, info.type);
            if (dynamic_cast<const VariableNode *>(&expression) || dynamic_cast<const SubscriptNode *>(&expression) || dynamic_cast<const MemberNode *>(&expression))
                return load(reference(expression));
            if (auto n = dynamic_cast<const SizeofNode *>(&expression)) {
                (void)n;
                require(info.integer_constant.has_value(), "sizeof needs a known size");
                return integer(*info.integer_constant, info.type);
            }
            if (auto n = dynamic_cast<const CastNode *>(&expression))
                return convert(eval(*n->value), info.type);
            if (auto n = dynamic_cast<const ConditionalNode *>(&expression))
                return convert(eval(truth(eval(*n->condition)) ? *n->then_expr : *n->else_expr), info.type);
            if (auto n = dynamic_cast<const CallNode *>(&expression)) {
                auto callee = dynamic_cast<const VariableNode *>(n->callee.get());
                require(callee, "only direct calls are supported");
                std::vector<Value> args;
                for (const auto &arg : n->arguments)
                    args.push_back(eval(*arg));
                location = n->location;
                return invoke(callee->var.get_token(), args, analyzer.info(*callee).type);
            }
            if (auto n = dynamic_cast<const UnaryNode *>(&expression)) {
                auto op = n->op.get_token();
                if (op == "&")
                    return {info.type, reference(*n->value).pointer};
                if (op == "*")
                    return load(reference(expression));
                if (op == "++" || op == "--") {
                    auto ref = reference(*n->value);
                    auto old = load(ref);
                    auto value = convert(binary(op == "++" ? "+" : "-", old, integer(1), old.type), ref.type);
                    store(ref, value);
                    return n->postfix ? old : value;
                }
                auto value = eval(*n->value);
                if (op == "!")
                    return integer(!truth(value));
                value = convert(value, info.type);
                if (op == "+")
                    return value;
                if (op == "~")
                    return integer(~bits(value), info.type);
                if (floating(value.type))
                    return convert({info.type, -std::get<double>(value.data)}, info.type);
                return binary("-", integer(0, info.type), value, info.type);
            }
            if (auto n = dynamic_cast<const BinaryNode *>(&expression)) {
                auto op = n->op.get_token();
                if (assignment(op)) {
                    auto ref = reference(*n->left);
                    Value left;
                    if (op != "=")
                        left = load(ref);
                    auto right = eval(*n->right);
                    if (op != "=") {
                        TypePtr type;
                        if (bare(left.type)->kind == K::Pointer)
                            type = left.type;
                        else if (op == "<<=" || op == ">>=")
                            type = ScalarDataModel::integer_promotion(left.type);
                        else
                            type = ScalarDataModel::arithmetic_type(left.type, right.type);
                        right = binary(op.substr(0, op.size() - 1), left, right, type);
                    }
                    auto value = convert(right, ref.type);
                    store(ref, value);
                    return value;
                }
                auto left = eval(*n->left);
                if (op == ",")
                    return eval(*n->right);
                if ((op == "&&" && !truth(left)) || (op == "||" && truth(left)))
                    return integer(op == "||");
                return binary(op, left, eval(*n->right), info.type);
            }
            fail("unsupported expression: " + expression.to_string());
        }
        void initialize(Reference ref, const ExprNode &expression) {
            auto t = bare(ref.type);
            auto list = dynamic_cast<const InitializerListNode *>(&expression);
            if (t->kind == K::Array) {
                const ExprNode *string = &expression;
                if (list && list->elements.size() == 1)
                    string = list->elements.front().get();
                if (auto text = dynamic_cast<const LiteralNode *>(string); text && text->value.get_type() == TOKEN_TYPE::STRING_VALUE && bare(t->element)->kind == K::Scalar) {
                    auto source = literal(*text);
                    auto bound = *bare(source.type)->bound;
                    for (size_t i = 0; i < std::min(*t->bound, bound); ++i)
                        store(array_element(ref, i), load(array_element(source, i)), true);
                    for (size_t i = bound; i < *t->bound; ++i) {
                        auto element = array_element(ref, i);
                        layout(element.pointer.allocation, element.type, element.pointer.offset, true);
                    }
                    return;
                }
                require(list, "array initializer requires braces");
                for (size_t i = 0; i < list->elements.size(); ++i)
                    initialize(array_element(ref, i), *list->elements[i]);
                for (size_t i = list->elements.size(); i < *t->bound; ++i) {
                    auto element = array_element(ref, i);
                    layout(element.pointer.allocation, element.type, element.pointer.offset, true);
                }
                return;
            }
            if (aggregate(t)) {
                require(list, "struct initializer requires braces");
                const auto &fields = analyzer.members(t);
                for (size_t i = 0; i < list->elements.size(); ++i)
                    initialize(member(ref, fields[i].type, fields[i].offset), *list->elements[i]);
                for (size_t i = list->elements.size(); i < fields.size(); ++i) {
                    auto field = member(ref, fields[i].type, fields[i].offset);
                    layout(field.pointer.allocation, field.type, field.pointer.offset, true);
                }
                return;
            }
            if (list)
                initialize(ref, *list->elements.front());
            else
                store(ref, eval(expression), true);
        }
        void declaration(const DeclarationNode &node) {
            if (node.is_typedef)
                return;
            for (const auto &d : node.declarators) {
                if (bare(d.type ? d.type : node.declared_type())->kind == K::Function)
                    continue;
                const auto &symbol = analyzer.symbol(d);
                if (d.initializer) {
                    auto ref = object(&symbol);
                    initialize(ref, *d.initializer);
                }
            }
        }
        struct Builder {
            Implementation &runtime;
            Plan &plan;
            std::vector<size_t> path{0};
            struct Context {
                size_t done, next;
                bool loop;
                size_t selection;
            };
            std::vector<Context> contexts;
            size_t label() {
                plan.labels.push_back(0);
                return plan.labels.size() - 1;
            }
            size_t emit(Action action, const ASTNode *node = nullptr, const ExprNode *expr = nullptr, size_t target = 0) {
                Instruction i;
                i.action = action;
                i.node = node;
                i.expression = expr;
                i.target = target;
                i.path = path;
                plan.code.push_back(std::move(i));
                return plan.code.size() - 1;
            }
            void mark(size_t label) {
                plan.labels[label] = plan.code.size();
                emit(Action::Nop);
            }
            void scope() {
                path.push_back(plan.locals.size());
                plan.locals.emplace_back();
            }
            void end_scope() {
                path.pop_back();
                emit(Action::Nop);
            }
            void build(const ASTNode &node) {
                if (auto n = dynamic_cast<const ExprNode *>(&node)) {
                    emit(Action::Expression, n, n);
                    return;
                }
                if (auto n = dynamic_cast<const DeclarationNode *>(&node)) {
                    if (!n->is_typedef)
                        for (const auto &d : n->declarators)
                            if (bare(d.type ? d.type : n->declared_type())->kind != K::Function)
                                plan.locals[path.back()].push_back(&runtime.analyzer.symbol(d));
                    emit(Action::Declaration, n);
                    return;
                }
                if (auto n = dynamic_cast<const BlockNode *>(&node)) {
                    scope();
                    for (const auto &s : n->statements)
                        build(*s);
                    end_scope();
                    return;
                }
                if (auto n = dynamic_cast<const ReturnNode *>(&node)) {
                    emit(Action::Return, n, n->expression.get());
                    return;
                }
                if (auto n = dynamic_cast<const IfNode *>(&node)) {
                    auto otherwise = label(), done = label();
                    emit(Action::Test, n, n->condition.get(), otherwise);
                    build(*n->then_statement);
                    emit(Action::Jump, n, nullptr, done);
                    mark(otherwise);
                    if (n->else_statement)
                        build(*n->else_statement);
                    mark(done);
                    return;
                }
                if (auto n = dynamic_cast<const ForNode *>(&node)) {
                    scope();
                    auto start = label(), next = label(), done = label();
                    if (n->initializer)
                        build(*n->initializer);
                    mark(start);
                    if (n->condition)
                        emit(Action::Test, n, n->condition.get(), done);
                    contexts.push_back({done, next, true, 0});
                    build(*n->body);
                    contexts.pop_back();
                    mark(next);
                    if (n->update)
                        emit(Action::Expression, n, n->update.get());
                    emit(Action::Jump, n, nullptr, start);
                    mark(done);
                    end_scope();
                    return;
                }
                if (auto n = dynamic_cast<const WhileNode *>(&node)) {
                    auto start = label(), done = label();
                    mark(start);
                    emit(Action::Test, n, n->condition.get(), done);
                    contexts.push_back({done, start, true, 0});
                    build(*n->body);
                    contexts.pop_back();
                    emit(Action::Jump, n, nullptr, start);
                    mark(done);
                    return;
                }
                if (auto n = dynamic_cast<const DoWhileNode *>(&node)) {
                    auto start = label(), test = label(), done = label();
                    mark(start);
                    contexts.push_back({done, test, true, 0});
                    build(*n->body);
                    contexts.pop_back();
                    mark(test);
                    emit(Action::Test, n, n->condition.get(), done);
                    emit(Action::Jump, n, nullptr, start);
                    mark(done);
                    return;
                }
                if (auto n = dynamic_cast<const SwitchNode *>(&node)) {
                    scope();
                    auto done = label();
                    auto index = emit(Action::Selection, n, n->condition.get(), done);
                    plan.code[index].control_type = ScalarDataModel::integer_promotion(runtime.analyzer.info(*n->condition).type);
                    contexts.push_back({done, 0, false, index});
                    build(*n->body);
                    contexts.pop_back();
                    mark(done);
                    end_scope();
                    return;
                }
                if (auto n = dynamic_cast<const CaseNode *>(&node)) {
                    auto current = std::find_if(contexts.rbegin(), contexts.rend(), [](auto c) { return !c.loop; });
                    runtime.require(current != contexts.rend(), "case outside switch");
                    auto target = label();
                    mark(target);
                    auto &instruction = plan.code[current->selection];
                    auto info = runtime.analyzer.info(*n->value);
                    auto value = runtime.convert(runtime.integer(*info.integer_constant, info.type), instruction.control_type);
                    instruction.cases.push_back({runtime.bits(value), target});
                    build(*n->statement);
                    return;
                }
                if (auto n = dynamic_cast<const DefaultNode *>(&node)) {
                    auto current = std::find_if(contexts.rbegin(), contexts.rend(), [](auto c) { return !c.loop; });
                    runtime.require(current != contexts.rend(), "default outside switch");
                    auto target = label();
                    mark(target);
                    plan.code[current->selection].target = target;
                    build(*n->statement);
                    return;
                }
                if (auto n = dynamic_cast<const JumpNode *>(&node)) {
                    auto current = std::find_if(contexts.rbegin(), contexts.rend(), [&](auto c) { return n->is_break || c.loop; });
                    runtime.require(current != contexts.rend(), "jump outside context");
                    emit(Action::Jump, n, nullptr, n->is_break ? current->done : current->next);
                    return;
                }
                runtime.fail("unsupported execution statement");
            }
        };
        void pop(Frame &frame) {
            for (auto &[symbol, ref] : frame.scopes.back().objects) {
                (void)symbol;
                ref.pointer.allocation->alive = false;
                ref.pointer.allocation->cells.clear();
            }
            frame.scopes.pop_back();
        }
        void sync(Frame &frame, const std::vector<size_t> &path) {
            size_t common = 0;
            while (common < path.size() && common < frame.scopes.size() && path[common] == frame.scopes[common].id)
                ++common;
            while (frame.scopes.size() > common)
                pop(frame);
            for (size_t i = common; i < path.size(); ++i) {
                frame.scopes.push_back({path[i], {}});
                for (auto symbol : frame.plan->locals[path[i]])
                    frame.scopes.back().objects[symbol] = allocate(symbol->type, false);
            }
        }
        void clear_temporaries(Frame &frame) {
            for (auto &allocation : frame.temporaries) {
                allocation->alive = false;
                allocation->cells.clear();
            }
            frame.temporaries.clear();
        }
        bool character(TypePtr type) {
            auto t = bare(type);
            return t->kind == K::Scalar && (t->scalar == S::Char || t->scalar == S::SignedChar || t->scalar == S::UnsignedChar);
        }
        std::string c_string(const Value &value, std::optional<size_t> precision = {}) {
            require(bare(value.type)->kind == K::Pointer && character(bare(value.type)->element), "lsprintf requires a character pointer for strings");
            auto p = std::get<Pointer>(value.data);
            require(p.allocation && p.allocation->alive, "lsprintf string uses a null or expired pointer");
            auto bound = precision.value_or(limits.output_bytes);
            require(bound <= limits.output_bytes, "lsprintf string exceeds output limit");
            std::string result;
            for (size_t i = 0; i < bound; ++i) {
                tick();
                auto unit = bits(load({p, Type::scalar_type(S::Char)})) & 255;
                if (!unit)
                    return result;
                result += static_cast<char>(unit);
                ++p.offset;
            }
            if (precision)
                return result;
            if ((bits(load({p, Type::scalar_type(S::Char)})) & 255) == 0)
                return result;
            fail("lsprintf unterminated string or output limit exceeded");
        }
        Value print(const std::vector<Value> &input) {
            require(!input.empty(), "lsprintf requires a format string");
            auto format_type = Type::pointer(Type::qualified(Type::scalar_type(S::Char), {.is_const = true}));
            auto format = c_string(convert(input.front(), format_type));
            std::vector<Value> arguments;
            for (size_t i = 1; i < input.size(); ++i) {
                auto value = input[i];
                if (bare(value.type)->kind == K::Pointer)
                    arguments.push_back(value);
                else if (floating(value.type))
                    arguments.push_back(convert(value, Type::scalar_type(bare(value.type)->scalar == S::Float ? S::Double : bare(value.type)->scalar)));
                else
                    arguments.push_back(convert(value, ScalarDataModel::integer_promotion(value.type)));
            }
            size_t argument = 0;
            auto next = [&]() -> Value {
                require(argument < arguments.size(), "lsprintf has too few format arguments");
                return arguments[argument++];
            };
            auto integral = [&](Value value, size_t width) -> Value {
                require(!floating(value.type) && (bare(value.type)->kind == K::Scalar || (bare(value.type)->kind == K::Tagged && bare(value.type)->tag == TagKind::Enum)) && !std::holds_alternative<std::monostate>(value.data) && size_of(value.type) == width, "lsprintf integer argument type does not match format");
                return value;
            };
            std::string output;
            auto append = [&](const std::string &text) {
                require(text.size() <= limits.output_bytes - output.size() && text.size() <= size_t(INT32_MAX) - output.size(), "lsprintf output limit exceeded");
                output += text;
            };
            auto formatted = [&](const std::string &specification, auto value) {
                int count = std::snprintf(nullptr, 0, specification.c_str(), value);
                require(count >= 0, "lsprintf formatting failed");
                require(size_t(count) <= limits.output_bytes - output.size() && size_t(count) <= size_t(INT32_MAX) - output.size(), "lsprintf output limit exceeded");
                std::vector<char> buffer(size_t(count) + 1);
                int written = std::snprintf(buffer.data(), buffer.size(), specification.c_str(), value);
                require(written == count, "lsprintf formatting failed");
                append(std::string(buffer.data(), size_t(count)));
            };
            for (size_t i = 0; i < format.size();) {
                tick();
                if (format[i] != '%') {
                    append(format.substr(i++, 1));
                    continue;
                }
                ++i;
                require(i < format.size(), "lsprintf incomplete format conversion");
                if (format[i] == '%') {
                    append("%");
                    ++i;
                    continue;
                }
                std::string flags;
                while (i < format.size() && std::string("-+ #0").find(format[i]) != std::string::npos)
                    flags += format[i++];
                auto digits = [&]() {
                    size_t value = 0;
                    while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
                        auto digit = size_t(format[i++] - '0');
                        require(value <= limits.output_bytes / 10 && value * 10 <= limits.output_bytes && digit <= limits.output_bytes - value * 10, "lsprintf field exceeds output limit");
                        value = value * 10 + digit;
                    }
                    return value;
                };
                std::optional<size_t> width, precision;
                if (i < format.size() && format[i] == '*') {
                    ++i;
                    auto value = convert(integral(next(), 4), Type::scalar_type(S::Int));
                    auto count = signed_bits(bits(value), value.type);
                    if (count < 0) {
                        flags += '-';
                        count = -count;
                    }
                    require(uint64_t(count) <= limits.output_bytes, "lsprintf field exceeds output limit");
                    width = size_t(count);
                } else if (i < format.size() && format[i] >= '0' && format[i] <= '9')
                    width = digits();
                if (i < format.size() && format[i] == '.') {
                    ++i;
                    if (i < format.size() && format[i] == '*') {
                        ++i;
                        auto value = convert(integral(next(), 4), Type::scalar_type(S::Int));
                        auto count = signed_bits(bits(value), value.type);
                        if (count >= 0) {
                            require(uint64_t(count) <= limits.output_bytes, "lsprintf field exceeds output limit");
                            precision = size_t(count);
                        }
                    } else
                        precision = digits();
                }
                std::string length;
                if (i < format.size() && std::string("hljztL").find(format[i]) != std::string::npos) {
                    length += format[i++];
                    if (i < format.size() && (length == "h" || length == "l") && format[i] == length.front())
                        length += format[i++];
                }
                require(i < format.size(), "lsprintf incomplete format conversion");
                char conversion = format[i++];
                require(std::string("diouxXfFeEgGaAcspn").find(conversion) != std::string::npos, "lsprintf unsupported format conversion");
                std::string specification = "%" + flags;
                if (width)
                    specification += std::to_string(*width);
                if (precision)
                    specification += "." + std::to_string(*precision);
                if (std::string("diouxXn").find(conversion) != std::string::npos) {
                    require(length != "L", "lsprintf invalid integer length modifier");
                    S signed_type = length == "hh" ? S::SignedChar : length == "h" ? S::Short : length == "ll" ? S::LongLong : length.empty() ? S::Int : S::Long;
                    S unsigned_kind = length == "hh" ? S::UnsignedChar : length == "h" ? S::UnsignedShort : length == "ll" ? S::UnsignedLongLong : length.empty() ? S::UnsignedInt : S::UnsignedLong;
                    if (conversion == 'n') {
                        require(flags.empty() && !width && !precision, "lsprintf %n does not allow flags, width, or precision");
                        auto value = next();
                        require(bare(value.type)->kind == K::Pointer, "lsprintf %n requires an integer pointer");
                        auto target = bare(value.type)->element;
                        require(bare(target)->kind == K::Scalar && bare(target)->scalar == signed_type && !target->top_qualifiers().is_const, "lsprintf %n pointer type does not match format");
                        require(output.size() <= (mask(target) >> 1), "lsprintf %n count does not fit its target");
                        store({std::get<Pointer>(value.data), target}, integer(output.size(), target));
                    } else {
                        bool signed_conversion = conversion == 'd' || conversion == 'i';
                        auto type = Type::scalar_type(signed_conversion ? signed_type : unsigned_kind);
                        auto value = convert(integral(next(), length.empty() || length == "h" || length == "hh" ? 4 : 8), type);
                        if (signed_conversion)
                            formatted(specification + "ll" + conversion, static_cast<long long>(signed_bits(bits(value), type)));
                        else
                            formatted(specification + "ll" + conversion, static_cast<unsigned long long>(bits(value)));
                    }
                } else if (std::string("fFeEgGaA").find(conversion) != std::string::npos) {
                    require(length.empty() || length == "l" || length == "L", "lsprintf invalid floating length modifier");
                    auto value = next();
                    require(floating(value.type) && ((length == "L") == (bare(value.type)->scalar == S::LongDouble)), "lsprintf floating argument type does not match format");
                    if (length == "L")
                        formatted(specification + "L" + conversion, static_cast<long double>(numeric(value)));
                    else
                        formatted(specification + conversion, static_cast<double>(numeric(value)));
                } else if (conversion == 'c') {
                    require(length.empty() && !precision, "lsprintf wide characters and character precision are unsupported");
                    auto value = integral(next(), 4);
                    formatted(specification + "c", static_cast<int>(bits(value) & 255));
                } else if (conversion == 's') {
                    require(length.empty(), "lsprintf wide strings are unsupported");
                    auto text = c_string(next(), precision);
                    formatted(specification + "s", text.c_str());
                } else {
                    require(length.empty() && !precision, "lsprintf invalid pointer format");
                    auto value = next();
                    require(bare(value.type)->kind == K::Pointer, "lsprintf %p requires an object pointer");
                    auto p = std::get<Pointer>(value.data);
                    require(!p.allocation || p.allocation->alive, "lsprintf %p uses an expired pointer");
                    auto address = p.allocation ? p.allocation->address + p.offset : uintptr_t(0);
                    formatted(specification + "p", address ? reinterpret_cast<void *>(address) : nullptr);
                }
            }
            try {
                std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
            } catch (const std::ios_base::failure &) {
                return integer(UINT32_MAX);
            }
            return std::cout ? integer(output.size()) : integer(UINT32_MAX);
        }
        Value invoke(const std::string &name, const std::vector<Value> &arguments, TypePtr signature = {}) {
            if (name == "lsprintf")
                return print(arguments);
            if (auto callback = callbacks.find(name); callback != callbacks.end()) {
                require(!functions.contains(name), "native callback conflicts with C definition: " + name);
                return invoke_native(name, arguments, callback->second, std::move(signature));
            }
            auto found = functions.find(name);
            require(found != functions.end(), "undefined function: " + name);
            auto &plan = found->second;
            require(arguments.size() == plan.function->parameters.size(), "wrong argument count for " + name);
            require(frames.size() < limits.recursion, "recursion limit exceeded");
            auto frame = std::make_unique<Frame>();
            frame->plan = &plan;
            auto *active = frame.get();
            frames.push_back(std::move(frame));
            try {
                sync(*active, {0});
                for (size_t i = 0; i < arguments.size(); ++i) {
                    auto symbol = &analyzer.symbol(plan.function->parameters[i]);
                    store(object(symbol), arguments[i], true);
                }
                size_t pc = 0;
                while (pc < plan.code.size()) {
                    const auto &instruction = plan.code[pc++];
                    if (instruction.node)
                        location = instruction.node->location;
                    tick();
                    sync(*active, instruction.path);
                    if (instruction.action == Action::Expression)
                        eval(*instruction.expression);
                    else if (instruction.action == Action::Declaration)
                        declaration(dynamic_cast<const DeclarationNode &>(*instruction.node));
                    else if (instruction.action == Action::Test) {
                        if (!truth(eval(*instruction.expression)))
                            pc = plan.labels[instruction.target];
                    } else if (instruction.action == Action::Jump)
                        pc = plan.labels[instruction.target];
                    else if (instruction.action == Action::Selection) {
                        auto value = bits(convert(eval(*instruction.expression), instruction.control_type));
                        auto label = instruction.target;
                        for (auto [constant, target] : instruction.cases)
                            if (constant == value) {
                                label = target;
                                break;
                            }
                        pc = plan.labels[label];
                    } else if (instruction.action == Action::Return) {
                        auto result_type = analyzer.symbols().lookup(name)->type->element;
                        Value value;
                        if (instruction.expression)
                            value = eval(*instruction.expression);
                        else if (instruction.node == nullptr && name == "main")
                            value = integer(0);
                        else
                            require(bare(result_type)->kind == K::Scalar && bare(result_type)->scalar == S::Void, "missing return value in " + name);
                        value = convert(value, result_type);
                        while (!active->scopes.empty())
                            pop(*active);
                        clear_temporaries(*active);
                        frames.pop_back();
                        return value;
                    }
                    clear_temporaries(*active);
                }
                fail("function plan has no return");
            } catch (...) {
                while (!active->scopes.empty())
                    pop(*active);
                clear_temporaries(*active);
                frames.pop_back();
                throw;
            }
        }
        void check_native_definitions(const ProgramNode &program) {
            for (const auto &node : program.declarations)
                if (auto f = dynamic_cast<const FunctionNode *>(node.get()); f && f->body)
                    require(!callbacks.contains(f->name.get_token()), "native callback conflicts with C definition: " + f->name.get_token());
        }
        void install(const ProgramNode &program) {
            check_native_definitions(program);
            for (const auto &node : program.declarations) {
                if (auto f = dynamic_cast<const FunctionNode *>(node.get()); f && f->body) {
                    auto &plan = functions[f->name.get_token()];
                    plan.function = f;
                    for (const auto &param : f->parameters)
                        plan.locals[0].push_back(&analyzer.symbol(param));
                    Builder builder{*this, plan, {0}, {}};
                    for (const auto &s : f->body->statements)
                        builder.build(*s);
                    builder.emit(Action::Return);
                }
                if (auto d = dynamic_cast<const DeclarationNode *>(node.get()); d && !d->is_typedef)
                    for (const auto &decl : d->declarators) {
                        if (bare(decl.type ? decl.type : d->declared_type())->kind == K::Function)
                            continue;
                        if (!d->storage.empty() && d->storage.front() == "extern" && !decl.initializer)
                            continue;
                        const auto &symbol = analyzer.symbol(decl);
                        if (!globals.contains(&symbol))
                            globals[&symbol] = allocate(symbol.type, true);
                    }
            }
            for (const auto &node : program.declarations)
                if (auto d = dynamic_cast<const DeclarationNode *>(node.get()))
                    declaration(*d);
        }
        void load(const ProgramNode &program) {
            remaining = limits.instructions;
            analyzer.validate(program);
            install(program);
            session_source = program.to_string();
            declaration_count = program.declarations.size();
        }
        std::optional<std::string> display(const Value &value) {
            if (std::holds_alternative<std::monostate>(value.data))
                return {};
            if (auto n = std::get_if<uint64_t>(&value.data))
                return unsigned_type(value.type) ? std::to_string(*n) : std::to_string(signed_bits(*n, value.type));
            if (auto n = std::get_if<double>(&value.data)) {
                char buffer[128];
                auto result = bare(value.type)->scalar == S::Float ? std::to_chars(buffer, buffer + sizeof(buffer), static_cast<float>(*n)) : std::to_chars(buffer, buffer + sizeof(buffer), *n);
                require(result.ec == std::errc{}, "cannot display floating result");
                return std::string(buffer, result.ptr);
            }
            if (auto pointer = std::get_if<Pointer>(&value.data)) {
                require(!pointer->allocation || pointer->allocation->alive, "result points to an expired object");
                std::ostringstream text;
                text << "0x" << std::hex << (pointer->allocation ? pointer->allocation->address + pointer->offset : uintptr_t(0));
                return text.str();
            }
            auto name = bare(value.type)->name;
            return name.empty() ? "<struct>" : "<struct " + name + ">";
        }
        std::optional<std::string> query(const FunctionNode &function) {
            auto &plan = functions.at(function.name.get_token());
            auto frame = std::make_unique<Frame>();
            frame->plan = &plan;
            auto *active = frame.get();
            frames.push_back(std::move(frame));
            try {
                sync(*active, {0});
                auto value = eval(dynamic_cast<const ExprNode &>(*function.body->statements.front()));
                auto text = display(value);
                while (!active->scopes.empty())
                    pop(*active);
                clear_temporaries(*active);
                frames.pop_back();
                return text;
            } catch (...) {
                while (!active->scopes.empty())
                    pop(*active);
                clear_temporaries(*active);
                frames.pop_back();
                throw;
            }
        }
        std::optional<std::string> submit(const std::string &input) {
            remaining = limits.instructions;
            auto end = input.find_last_not_of(" \t\r\n");
            if (end == std::string::npos)
                return {};
            auto text = input.substr(0, end + 1);
            std::istringstream probe(text);
            Scanner scanner(probe);
            Token token;
            auto token_type = scanner.lex(token);
            if (token_type == TOKEN_TYPE::TOKEN_NULL)
                return {};
            if (token_type == TOKEN_TYPE::TOKEN_ERROR)
                throw ParserException("Expected valid interactive input", SourceLocation::from_token(token));
            const auto first_name = token.get_token();
            auto last_token = first_name;
            bool empty_statements = first_name == ";";
            while ((token_type = scanner.lex(token)) != TOKEN_TYPE::TOKEN_NULL) {
                if (token_type == TOKEN_TYPE::TOKEN_ERROR)
                    throw ParserException("Expected valid interactive input", SourceLocation::from_token(token));
                last_token = token.get_token();
                empty_statements &= last_token == ";";
            }
            if (empty_statements)
                return {};
            static const std::set<std::string> specifiers = {"void", "_Bool", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "const", "volatile", "restrict", "typedef", "static", "extern", "struct", "enum", "union", "inline", "auto", "register", "_Atomic", "_Complex", "_Static_assert"};
            auto symbol = analyzer.symbols().lookup(first_name);
            bool declaration = specifiers.contains(first_name) || (symbol && symbol->kind == SemanticSymbolKind::Typedef);
            bool suppress = last_token == ";";
            std::string query_name;
            if (declaration) {
                if (last_token != ";")
                    text += "\n;";
            } else {
                do {
                    query_name = "__ls_repl_query_" + std::to_string(query_count++);
                } while (analyzer.symbols().lookup(query_name));
                text = "void " + query_name + "(void){\n" + text + (suppress ? "\n}\n" : "\n;}\n");
            }
            std::istringstream source(session_source + "\n" + text);
            Parser parser(source);
            auto candidate = parser.parse_program();
            SemanticAnalyzer verification;
            verification.validate(*candidate, true);
            require(candidate->declarations.size() >= declaration_count, "interactive declaration history changed unexpectedly");
            candidate->declarations.erase(candidate->declarations.begin(), candidate->declarations.begin() + declaration_count);
            FunctionNode *expression_function = nullptr;
            if (!declaration) {
                expression_function = dynamic_cast<FunctionNode *>(candidate->declarations.back().get());
                require(expression_function && expression_function->body && expression_function->body->statements.size() == 1 && dynamic_cast<ExprNode *>(expression_function->body->statements.front().get()), "REPL input must be one expression, declaration, or function definition");
            }
            auto accepted = candidate.get();
            check_native_definitions(*accepted);
            auto canonical = candidate->to_string();
            submissions.push_back(std::move(candidate));
            analyzer.append(*accepted, true);
            session_source += "\n" + canonical;
            declaration_count += accepted->declarations.size();
            install(*accepted);
            if (expression_function) {
                auto result = query(*expression_function);
                if (!suppress)
                    return result;
            }
            return {};
        }
    };
    ExecutionException::ExecutionException(const std::string &message, SourceLocation location) : std::runtime_error(SemanticException(message, location).what()), location(std::move(location)) {}
    Interpreter::Interpreter(ExecutionLimits limits) : implementation(std::make_unique<Implementation>(limits)), limits(limits) {}
    Interpreter::~Interpreter() = default;
    void Interpreter::load(const ProgramNode &program) {
        implementation->require(!implementation->in_callback, "cannot reenter interpreter from native callback");
        auto next = std::make_unique<Implementation>(limits);
        next->callbacks = implementation->callbacks;
        next->load(program);
        implementation = std::move(next);
    }
    std::optional<std::string> Interpreter::submit(const std::string &source) {
        implementation->require(!implementation->in_callback, "cannot reenter interpreter from native callback");
        return implementation->submit(source);
    }
    void Interpreter::register_function(const std::string &name, NativeCallback callback) {
        auto &runtime = *implementation;
        runtime.require(!runtime.in_callback, "cannot reenter interpreter from native callback");
        runtime.require(!name.empty() && name != "lsprintf", "invalid or reserved native function name");
        auto identifier_start = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
        runtime.require(identifier_start(name.front()) && std::all_of(name.begin(), name.end(), [&](char c) { return identifier_start(c) || (c >= '0' && c <= '9'); }), "invalid native function name");
        runtime.require(bool(callback), "native callback must not be empty");
        runtime.require(!runtime.callbacks.contains(name), "native function already registered: " + name);
        runtime.require(!runtime.functions.contains(name), "native callback conflicts with C definition: " + name);
        auto symbol = runtime.analyzer.symbols().lookup(name);
        runtime.require(!symbol || symbol->kind == SemanticSymbolKind::Function, "native callback conflicts with C object: " + name);
        runtime.callbacks.emplace(name, std::move(callback));
    }
    ExecutionValue Interpreter::call(const std::string &name, const std::vector<ExecutionValue> &arguments) {
        auto &runtime = *implementation;
        runtime.require(!runtime.in_callback, "cannot reenter interpreter from native callback");
        runtime.remaining = limits.instructions;
        std::vector<Value> args;
        for (const auto &arg : arguments)
            args.push_back(runtime.from_public(arg));
        auto value = runtime.invoke(name, args);
        if (auto n = std::get_if<uint64_t>(&value.data))
            return unsigned_type(value.type) ? ExecutionValue(*n) : ExecutionValue(signed_bits(*n, value.type));
        if (auto n = std::get_if<double>(&value.data))
            return *n;
        if (std::holds_alternative<std::monostate>(value.data))
            return std::monostate{};
        runtime.fail("public calls return scalar values only; use a C wrapper for object results");
    }
} // namespace mx
