#pragma once
#include <string_view>

namespace otter {
    namespace utils {

        [[nodiscard]] constexpr inline char lowercase_character(char c) {
            return c >= 'A' && c <= 'Z' ? (c | 0x20) : c;
        }
        [[nodiscard]] inline bool inplace_lowercase_comparison(std::string_view x, std::string_view y) {
            size_t len = x.length();
            if (len != y.length()) return false;

            for (size_t pointer = 0; pointer < len; pointer++) {
                if (lowercase_character(x[pointer]) != lowercase_character(y[pointer])) return false;
            }

            return true;
        }

        [[nodiscard]] inline bool inplace_comparison(std::string_view x, std::string_view y) {
            size_t len = x.length();
            if (len != y.length()) return false;

            for (size_t pointer = 0; pointer < len; pointer++) {
                if (x[pointer] != y[pointer]) return false;
            }

            return true;
        }

    } // namespace utils

}