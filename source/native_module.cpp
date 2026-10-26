#include "native_module.hpp"
#include "MXParser/parser.hpp"
#include <fstream>
#include <sstream>

#ifdef MXPARSER_NATIVE_MODULES
#include <array>
#include <cstring>
#include <ffi.h>
#include <filesystem>
#include <set>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace mx {
    namespace {
        using S = ScalarKind;
        std::filesystem::path native_path(const std::string &text) { return std::filesystem::path(std::u8string(text.begin(), text.end())); }
        const Type &bare(const TypePtr &type) { return type->unqualified(); }
        bool input_string(const TypePtr &type) {
            const auto &t = bare(type);
            return t.kind == TypeKind::Pointer && bare(t.element).kind == TypeKind::Scalar && bare(t.element).scalar == S::Char && t.element->top_qualifiers().is_const;
        }
        ffi_type *abi_type(const TypePtr &type, bool result) {
            const auto &t = bare(type);
            if (!result && input_string(type))
                return &ffi_type_pointer;
            if (t.kind == TypeKind::Scalar) {
                switch (t.scalar) {
                case S::Void:
                    if (result)
                        return &ffi_type_void;
                    break;
                case S::Bool:
                    return &ffi_type_uint8;
                case S::Char:
                    return std::is_signed_v<char> ? &ffi_type_schar : &ffi_type_uchar;
                case S::SignedChar:
                    return &ffi_type_schar;
                case S::UnsignedChar:
                    return &ffi_type_uchar;
                case S::Short:
                    return &ffi_type_sshort;
                case S::UnsignedShort:
                    return &ffi_type_ushort;
                case S::Int:
                    return &ffi_type_sint;
                case S::UnsignedInt:
                    return &ffi_type_uint;
                case S::Long:
                    return &ffi_type_slong;
                case S::UnsignedLong:
                    return &ffi_type_ulong;
                case S::LongLong:
                    return &ffi_type_sint64;
                case S::UnsignedLongLong:
                    return &ffi_type_uint64;
                case S::Float:
                    return &ffi_type_float;
                case S::Double:
                    return &ffi_type_double;
                case S::LongDouble:
                    return &ffi_type_longdouble;
                }
            }
            throw std::runtime_error("unsupported module import type: " + type->to_string() + "; use numeric types, void results, or const char * input strings");
        }
        class Module {
          public:
            explicit Module(const std::string &path) {
#ifdef _WIN32
                auto wide = native_path(path).wstring();
                handle = LoadLibraryW(wide.c_str());
                if (!handle)
                    throw std::runtime_error("cannot load module " + path + ": Windows error " + std::to_string(GetLastError()));
#else
                handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
                if (!handle)
                    throw std::runtime_error("cannot load module " + path + ": " + dlerror());
#endif
            }
            ~Module() {
#ifdef _WIN32
                FreeLibrary(handle);
#else
                dlclose(handle);
#endif
            }
            Module(const Module &) = delete;
            Module &operator=(const Module &) = delete;
            void (*symbol(const std::string &name))() {
#ifdef _WIN32
                auto address = GetProcAddress(handle, name.c_str());
                if (!address)
                    throw std::runtime_error("cannot find module symbol: " + name);
                return reinterpret_cast<void (*)()>(address);
#else
                dlerror();
                auto address = dlsym(handle, name.c_str());
                auto error = dlerror();
                if (error || !address)
                    throw std::runtime_error("cannot find module symbol " + name + (error ? ": " + std::string(error) : ""));
                return reinterpret_cast<void (*)()>(address);
#endif
            }

          private:
#ifdef _WIN32
            HMODULE handle;
#else
            void *handle;
#endif
        };
        struct Buffer {
            alignas(std::max_align_t) std::array<std::byte, sizeof(long double) + sizeof(ffi_arg)> data{};
            template <typename T> void put(T value) { std::memcpy(data.data(), &value, sizeof(value)); }
            template <typename T> T get() const {
                T value;
                std::memcpy(&value, data.data(), sizeof(value));
                return value;
            }
            template <typename T> ExecutionValue result() const {
                T value;
                if constexpr (std::is_integral_v<T> && sizeof(T) < sizeof(ffi_arg))
                    value = static_cast<T>(get<ffi_arg>());
                else
                    value = get<T>();
                if constexpr (std::is_floating_point_v<T>)
                    return static_cast<double>(value);
                else if constexpr (std::is_unsigned_v<T>)
                    return static_cast<uint64_t>(value);
                else
                    return static_cast<int64_t>(value);
            }
        };
        void argument(Buffer &buffer, const NativeObject &object, S kind) {
            switch (kind) {
            case S::Bool:
                buffer.put(object.as<bool>());
                break;
            case S::Char:
                buffer.put(object.as<char>());
                break;
            case S::SignedChar:
                buffer.put(object.as<signed char>());
                break;
            case S::UnsignedChar:
                buffer.put(object.as<unsigned char>());
                break;
            case S::Short:
                buffer.put(object.as<short>());
                break;
            case S::UnsignedShort:
                buffer.put(object.as<unsigned short>());
                break;
            case S::Int:
                buffer.put(object.as<int>());
                break;
            case S::UnsignedInt:
                buffer.put(object.as<unsigned int>());
                break;
            case S::Long:
                buffer.put(object.as<long>());
                break;
            case S::UnsignedLong:
                buffer.put(object.as<unsigned long>());
                break;
            case S::LongLong:
                buffer.put(object.as<long long>());
                break;
            case S::UnsignedLongLong:
                buffer.put(object.as<unsigned long long>());
                break;
            case S::Float:
                buffer.put(object.as<float>());
                break;
            case S::Double:
                buffer.put(object.as<double>());
                break;
            case S::LongDouble:
                buffer.put(object.as<long double>());
                break;
            case S::Void:
                throw std::runtime_error("void module argument");
            }
        }
        ExecutionValue result(const Buffer &buffer, S kind) {
            switch (kind) {
            case S::Void:
                return {};
            case S::Bool:
                return buffer.result<bool>();
            case S::Char:
                return buffer.result<char>();
            case S::SignedChar:
                return buffer.result<signed char>();
            case S::UnsignedChar:
                return buffer.result<unsigned char>();
            case S::Short:
                return buffer.result<short>();
            case S::UnsignedShort:
                return buffer.result<unsigned short>();
            case S::Int:
                return buffer.result<int>();
            case S::UnsignedInt:
                return buffer.result<unsigned int>();
            case S::Long:
                return buffer.result<long>();
            case S::UnsignedLong:
                return buffer.result<unsigned long>();
            case S::LongLong:
                return buffer.result<long long>();
            case S::UnsignedLongLong:
                return buffer.result<unsigned long long>();
            case S::Float:
                return buffer.result<float>();
            case S::Double:
                return buffer.result<double>();
            case S::LongDouble:
                return buffer.result<long double>();
            }
            throw std::runtime_error("invalid module result");
        }
        struct Function {
            std::shared_ptr<Module> module;
            TypePtr signature;
            void (*address)();
            ffi_cif cif{};
            std::vector<ffi_type *> arguments;
            Function(std::shared_ptr<Module> module, TypePtr signature, const std::string &name) : module(std::move(module)), signature(std::move(signature)), address(this->module->symbol(name)) {
                for (const auto &type : this->signature->parameters)
                    arguments.push_back(abi_type(type, false));
                if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, static_cast<unsigned>(arguments.size()), abi_type(this->signature->element, true), arguments.data()) != FFI_OK)
                    throw std::runtime_error("cannot prepare module function: " + name);
            }
            ExecutionValue call(const std::vector<NativeObject> &objects) {
                if (objects.size() != signature->parameters.size())
                    throw std::runtime_error("module function argument count does not match imported signature");
                std::vector<Buffer> buffers(objects.size());
                std::vector<std::string> strings(objects.size());
                std::vector<void *> pointers(objects.size());
                for (size_t i = 0; i < objects.size(); ++i) {
                    if (!objects[i].type()->equals(*signature->parameters[i]))
                        throw std::runtime_error("module function prototype does not match imported signature");
                    if (input_string(signature->parameters[i])) {
                        strings[i] = objects[i].string();
                        buffers[i].put(strings[i].c_str());
                    } else
                        argument(buffers[i], objects[i], bare(signature->parameters[i]).scalar);
                    pointers[i] = buffers[i].data.data();
                }
                Buffer returned;
                ffi_call(&cif, address, returned.data.data(), pointers.data());
                return result(returned, bare(signature->element).scalar);
            }
        };
    } // namespace
    NativeModuleImports load_native_module(const std::string &module_path, const std::string &declarations_path) {
        std::ifstream file(native_path(declarations_path));
        if (!file)
            throw std::runtime_error("cannot open import declarations: " + declarations_path);
        Scanner scanner(file);
        Token token;
        std::string source;
        size_t count = 0;
        bool declaration = false;
        for (auto kind = scanner.lex(token); kind != TOKEN_TYPE::TOKEN_NULL; kind = scanner.lex(token)) {
            if (kind == TOKEN_TYPE::TOKEN_ERROR)
                throw ParserException("invalid import token", SourceLocation::from_token(token));
            auto word = token.get_token();
            if (!declaration) {
                if (word != "__import__")
                    throw ParserException("expected __import__ before each function prototype", SourceLocation::from_token(token));
                declaration = true;
                ++count;
            } else {
                if (word == "__import__" || word == "{" || word == "}")
                    throw ParserException("expected an imported function prototype ending in ;", SourceLocation::from_token(token));
                source += word + " ";
                if (word == ";") {
                    source += '\n';
                    declaration = false;
                }
            }
        }
        if (declaration || !count)
            throw std::runtime_error("import file must contain __import__ function prototypes ending in ;");
        std::istringstream stream(source);
        Parser parser(stream);
        auto program = parser.parse_program();
        if (program->declarations.size() != count)
            throw std::runtime_error("use one function prototype per __import__ declaration");
        std::set<std::string> names;
        for (const auto &node : program->declarations) {
            auto function = dynamic_cast<FunctionNode *>(node.get());
            if (!function || function->body || !function->storage.empty())
                throw std::runtime_error("__import__ requires a function prototype without storage specifiers");
            if (!names.insert(function->name.get_token()).second)
                throw std::runtime_error("duplicate module import: " + function->name.get_token());
            if (!function->type->has_prototype)
                function->type = Type::function(function->type->element, {});
        }
        SemanticAnalyzer analyzer;
        analyzer.validate(*program);
        auto module = std::make_shared<Module>(module_path);
        NativeModuleImports imports;
        imports.declarations = program->to_string();
        for (const auto &node : program->declarations) {
            auto &function = dynamic_cast<FunctionNode &>(*node);
            auto name = function.name.get_token();
            auto signature = analyzer.symbols().lookup(name)->type;
            auto bound = std::make_shared<Function>(module, signature, name);
            imports.functions.push_back({name, signature, [bound](const auto &arguments) { return bound->call(arguments); }});
        }
        return imports;
    }
} // namespace mx
#else
namespace mx {
    NativeModuleImports load_native_module(const std::string &, const std::string &) { throw std::runtime_error("native modules are disabled; rebuild with WITH_NATIVE_MODULES=ON and libffi"); }
} // namespace mx
#endif
