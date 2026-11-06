#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>

#include "utils.hpp"
namespace otter {

    enum class LineKind : uint8_t {
        Blank, // tombstone: a deleted line. dropped from `lines` on the next rebuild of the hash table btw
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

    using name_hash_t = uint64_t;

    constexpr uint32_t kNone = 0xFFFFFFFFu;

    namespace hash {
        constexpr name_hash_t kOffset = 14695981039346656037ULL;
        constexpr name_hash_t kPrime = 1099511628211ULL;

        [[nodiscard]] inline constexpr name_hash_t get(std::string_view string) noexcept {
            name_hash_t hash = kOffset;
            for (char c : string) {
                hash ^= static_cast<name_hash_t>(static_cast<unsigned char>(c));
                hash *= kPrime;
            }
            return hash;
        }

        [[nodiscard]] inline constexpr name_hash_t get_lowercase(std::string_view string) noexcept {
            name_hash_t hash = kOffset;
            for (char c : string) {
                hash ^= static_cast<name_hash_t>(static_cast<unsigned char>(utils::lowercase_character(c)));
                hash *= kPrime;
            }
            return hash;
        }

        [[nodiscard]] inline constexpr name_hash_t get_lowercase(std::string_view string1, std::string_view string2) noexcept {
            name_hash_t hash = kOffset;
            // first string
            for (char c : string1) {
                hash ^= static_cast<name_hash_t>(static_cast<unsigned char>(utils::lowercase_character(c)));
                hash *= kPrime;
            }

            // ensure a seperator because otherwise ("ab", "c") and ("a", "bc") will hash identically
            hash ^= static_cast<name_hash_t>(static_cast<unsigned char>('\0'));
            hash *= kPrime;

            // second string
            for (char c : string2) {
                hash ^= static_cast<name_hash_t>(static_cast<unsigned char>(utils::lowercase_character(c)));
                hash *= kPrime;
            }
            return hash;
        }


        [[nodiscard]] inline constexpr std::tuple<name_hash_t, name_hash_t> get_lowercase_ex(std::string_view string1, std::string_view string2) noexcept {
            name_hash_t hash1 = kOffset;
            name_hash_t hash2 = 0;
            // first string
            for (char c : string1) {
                hash1 ^= static_cast<name_hash_t>(static_cast<unsigned char>(utils::lowercase_character(c)));
                hash1 *= kPrime;
            }
            hash2 = hash1; // for later use

            // ensure a seperator because otherwise ("ab", "c") and ("a", "bc") will hash identically
            hash1 ^= static_cast<name_hash_t>(static_cast<unsigned char>('\0'));
            hash1 *= kPrime;

            // second string
            for (char c : string2) {
                hash1 ^= static_cast<name_hash_t>(static_cast<unsigned char>(utils::lowercase_character(c)));
                hash1 *= kPrime;
            }
            return { hash2, hash1 };
        }

        // fnv-1a step over one case-folded character
        [[nodiscard]] inline constexpr name_hash_t step(name_hash_t h, char c) noexcept {
            return (h ^ static_cast<name_hash_t>(static_cast<unsigned char>(utils::lowercase_character(c)))) * kPrime;
        }

        // abseil takes H1 (upper 57 bits, picks the group) and H2 (lower 7 bits, the control byte) from the hash.
        // fnv-1a alone doesn't mix both halves well, so scramble once at the end (murmur3 finalizer)
        [[nodiscard]] inline constexpr name_hash_t finalize(name_hash_t h) noexcept {
            h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;
            return h;
        }

        // input: kind tag, folded section, then (keys only) a '\0' separator and the folded key.
        // the tag keeps section "x" apart from a key lookup, the separator keeps ("ab","c") apart from ("a","bc")
        [[nodiscard]] inline constexpr name_hash_t entry(LineKind kind, std::string_view section,
            std::string_view key = {}) noexcept {
            name_hash_t h = (kOffset ^ static_cast<name_hash_t>(kind)) * kPrime;
            for (char c : section) h = step(h, c);
            if (kind != LineKind::Section) {
                h = (h ^ 0u) * kPrime; // the '\0' separator
                for (char c : key) h = step(h, c);
            }
            return finalize(h);
        }
    } // namespace hash


    struct Line {
        size_t offset = 0;
        size_t name_len = 0, value_len = 0;
        size_t name_off = 0, value_off = 0;
        LineKind kind = LineKind::Blank;
    };


}