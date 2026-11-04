#pragma once
#include <string_view>

namespace otter {
    namespace Utils {

        [[nodiscard]] constexpr inline char LowercaseCharacter(char c) {
            return c >= 'A' && c <= 'Z' ? (c | 0x20) : c;
        }
        [[nodiscard]] inline bool InplaceLowercaseComparison(std::string_view x, std::string_view y) {
            size_t len = x.length();
            if (len != y.length()) return false;

            for (size_t pointer = 0; pointer < len; pointer++) {
                if (LowercaseCharacter(x[pointer]) != LowercaseCharacter(y[pointer])) return false;
            }

            return true;
        }

        [[nodiscard]] inline bool InplaceComparison(std::string_view x, std::string_view y) {
            size_t len = x.length();
            if (len != y.length()) return false;

            for (size_t pointer = 0; pointer < len; pointer++) {
                if (x[pointer] != y[pointer]) return false;
            }

            return true;
        }

    } // namespace Utils

}