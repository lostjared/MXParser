#pragma once
#include "MXParser/interpreter.hpp"

namespace mx {
    struct NativeImport {
        std::string name;
        TypePtr signature;
        NativeCallback callback;
    };
    struct NativeModuleImports {
        std::string declarations;
        std::vector<NativeImport> functions;
    };
    NativeModuleImports load_native_module(const std::string &module_path, const std::string &declarations_path);
} // namespace mx
