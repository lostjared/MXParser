#pragma once
#include <cstdint>
#include <limits>

namespace mx {
    inline bool add_overflow(int64_t left, int64_t right, int64_t *result) {
        constexpr auto MIN = std::numeric_limits<int64_t>::min();
        constexpr auto MAX = std::numeric_limits<int64_t>::max();
        if ((right > 0 && left > MAX - right) || (right < 0 && left < MIN - right))
            return true;
        *result = left + right;
        return false;
    }
    inline bool subtract_overflow(int64_t left, int64_t right, int64_t *result) {
        constexpr auto MIN = std::numeric_limits<int64_t>::min();
        constexpr auto MAX = std::numeric_limits<int64_t>::max();
        if ((right > 0 && left < MIN + right) || (right < 0 && left > MAX + right))
            return true;
        *result = left - right;
        return false;
    }
    inline bool multiply_overflow(int64_t left, int64_t right, int64_t *result) {
        constexpr auto MIN = std::numeric_limits<int64_t>::min();
        constexpr auto MAX = std::numeric_limits<int64_t>::max();
        if (left != 0 && right != 0) {
            if (left > 0 ? (right > 0 ? left > MAX / right : right < MIN / left) : (right > 0 ? left < MIN / right : left < MAX / right))
                return true;
        }
        *result = left * right;
        return false;
    }
} // namespace mx
