#pragma once
namespace otter {

    enum LineKind {
        Blank, // never used
        Section = 1,
        KeyValue = 2,
        Bare = 3,
        Comment = 4,
        Invalid
    };

    enum class ValueKind {
        None,
        String,
        Integer,
        // those don't exist lol. i wonder if it worth to implement them
        //Float,
        //Boolean
    };

    // SIMD is possible btw

    using xhash_t = uint64_t;

    constexpr uint32_t kNone = 0xFFFFFFFFu;

    namespace Hash {
        constexpr xhash_t kOffset = 14695981039346656037ULL;
        constexpr xhash_t kPrime = 1099511628211ULL;

        [[nodiscard]] inline constexpr xhash_t Get(std::string_view string) noexcept {
            xhash_t hash = kOffset;
            for (char c : string) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(c));
                hash *= kPrime;
            }
            return hash;
        }

        [[nodiscard]] inline constexpr xhash_t GetLowercase(std::string_view string) noexcept {
            xhash_t hash = kOffset;
            for (char c : string) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash *= kPrime;
            }
            return hash;
        }

        [[nodiscard]] inline constexpr xhash_t GetLowercase(std::string_view string1, std::string_view string2) noexcept {
            xhash_t hash = kOffset;
            // first string
            for (char c : string1) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash *= kPrime;
            }

            // ensure a seperator because otherwise ("ab", "c") and ("a", "bc") will hash identically
            hash ^= static_cast<xhash_t>(static_cast<unsigned char>('\0'));
            hash *= kPrime;

            // second string
            for (char c : string2) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash *= kPrime;
            }
            return hash;
        }


        [[nodiscard]] inline constexpr std::tuple<xhash_t, xhash_t> GetLowercaseEx(std::string_view string1, std::string_view string2) noexcept {
            xhash_t hash1 = kOffset;
            xhash_t hash2 = 0;
            // first string
            for (char c : string1) {
                hash1 ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash1 *= kPrime;
            }
            hash2 = hash1; // for later use

            // ensure a seperator because otherwise ("ab", "c") and ("a", "bc") will hash identically
            hash1 ^= static_cast<xhash_t>(static_cast<unsigned char>('\0'));
            hash1 *= kPrime;

            // second string
            for (char c : string2) {
                hash1 ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash1 *= kPrime;
            }
            return { hash2, hash1 };
        }
    } // namespace Hash


    struct Line {
        size_t offset = 0;
        size_t name_len = 0, value_len = 0;
        size_t name_off = 0, value_off = 0;
        LineKind kind = LineKind::Blank;
    };


}