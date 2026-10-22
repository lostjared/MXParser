#pragma once
#include <MXLex/token.hpp>
#include <cstddef>
#include <string>

namespace mx {
    /** MXLex exposes lines, but no columns. Line zero means no source was supplied. */
    struct SourceLocation {
        size_t line = 0;
        std::string token;
        static SourceLocation from_token(const Token &token) { return {token.get_line(), token.get_type() == TOKEN_TYPE::TOKEN_NULL ? "end of input" : token.get_token()}; }
    };
} // namespace mx
