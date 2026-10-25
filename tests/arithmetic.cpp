#include "source/arithmetic.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    constexpr auto MIN = std::numeric_limits<int64_t>::min();
    constexpr auto MAX = std::numeric_limits<int64_t>::max();
    struct Case {
        int64_t left, right, expected;
        bool overflow;
    };
    const std::vector<std::vector<Case>> cases = {{{MAX, 1, 0, true}, {MIN, -1, 0, true}, {MAX, -1, MAX - 1, false}, {MIN, 1, MIN + 1, false}, {-5, 2, -3, false}, {MIN, MAX, -1, false}}, {{MIN, 1, 0, true}, {MAX, -1, 0, true}, {MIN, -1, MIN + 1, false}, {MAX, 1, MAX - 1, false}, {-5, 2, -7, false}, {MIN, MIN, 0, false}}, {{MIN, -1, 0, true}, {-1, MIN, 0, true}, {MAX, 2, 0, true}, {MIN, 2, 0, true}, {MIN, 1, MIN, false}, {MAX, 1, MAX, false}, {0, MIN, 0, false}, {MIN, 0, 0, false}, {-3, -4, 12, false}, {-3, 4, -12, false}, {3, -4, -12, false}, {MAX / 2, 2, MAX - 1, false}}};
    bool (*const operations[])(int64_t, int64_t, int64_t *) = {mx::add_overflow, mx::subtract_overflow, mx::multiply_overflow};
    for (size_t i = 0; i < cases.size(); ++i)
        for (auto test : cases[i]) {
            int64_t result = 0;
            bool overflow = operations[i](test.left, test.right, &result);
            if (overflow != test.overflow || (!overflow && result != test.expected)) {
                std::cerr << "Incorrect portable overflow check\n";
                return 1;
            }
        }
    return 0;
}
