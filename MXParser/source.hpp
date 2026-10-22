#pragma once
#include <MXLex/token.hpp>
#include <cstddef>
#include <string>

namespace mx {
    /** One-based lexer coordinates. Zero denotes an unavailable line or column. */
    struct SourceLocation {
        size_t line = 0;
        std::string token;
        size_t column = 0;
        static SourceLocation from_token(const Token &token) { return {token.get_line(), token.get_type() == TOKEN_TYPE::TOKEN_NULL ? "end of input" : token.get_token(), token.get_column()}; }
    };
} // namespace mx
