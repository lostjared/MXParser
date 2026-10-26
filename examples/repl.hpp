#pragma once
#include <MXParser/interpreter.hpp>

namespace mx {
    int run_repl(const char *filename = nullptr, const char *module_path = nullptr, const char *imports_path = nullptr, ExecutionLimits limits = {});
}
