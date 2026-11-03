#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <iomanip>

#include <tuple>
#include <cstdint>

// TODO: empty value vs missing key

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
    namespace Utils {

        [[nodiscard]] inline char LowercaseCharacter(char c) {
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
    }

    namespace Hash {
        [[nodiscard]] inline constexpr xhash_t Get(std::string_view string) noexcept {
            xhash_t hash = 14695981039346656037ULL;
            for (char c : string) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(c));
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        [[nodiscard]] inline constexpr xhash_t GetLowercase(std::string_view string) noexcept {
            xhash_t hash = 14695981039346656037ULL;
            for (char c : string) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        [[nodiscard]] inline constexpr xhash_t GetLowercase(std::string_view string1, std::string_view string2) noexcept {
            xhash_t hash = 14695981039346656037ULL;
            // first string
            for (char c : string1) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash *= 1099511628211ULL;
            }

            // ensure a seperator because otherwise ("ab", "c") and ("a", "bc") will hash identically
            hash ^= static_cast<xhash_t>(static_cast<unsigned char>('\0'));
            hash *= 1099511628211ULL;

            // second string
            for (char c : string2) {
                hash ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash *= 1099511628211ULL;
            }
            return hash;
        }


        [[nodiscard]] inline constexpr std::tuple<xhash_t, xhash_t> GetLowercaseEx(std::string_view string1, std::string_view string2) noexcept {
            xhash_t hash1 = 14695981039346656037ULL;
            xhash_t hash2 = 0;
            // first string
            for (char c : string1) {
                hash1 ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash1 *= 1099511628211ULL;
            }
            hash2 = hash1; // for later use

            // ensure a seperator because otherwise ("ab", "c") and ("a", "bc") will hash identically
            hash1 ^= static_cast<xhash_t>(static_cast<unsigned char>('\0'));
            hash1 *= 1099511628211ULL;

            // second string
            for (char c : string2) {
                hash1 ^= static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)));
                hash1 *= 1099511628211ULL;
            }
            return {hash2, hash1};
        }
    }


    struct Line {
        size_t offset = 0;
        size_t name_len = 0, value_len = 0;
        size_t name_off = 0, value_off = 0;
        LineKind kind = LineKind::Blank;

    };

    class INI {
    public:
        // clean container alias
        template <typename K, typename V>
        using Map = std::unordered_map<K, V>;


        Map<std::string, Map<std::string, std::string>> ini{}; // TODO: should we remove?

        // line index lookups
        Map<xhash_t, size_t> section_line_indices;
        Map<xhash_t, size_t> key_line_indices;

        std::string content = "";

        std::vector<Line> lines; // fast metadata data

        void parse_lines(const std::string& test_content) {
            const char* character = test_content.c_str();
            content = test_content; // TODO: move to a constructor or some loader
            std::string_view current_section = "";

            while (*character != '\0') {
                if (*character == ' ' || *character == '\t' || *character == '\r' || *character == '\n') {
                    character++;
                    continue;
                }

                Line line;
                line.offset = character - content.c_str();

                if (*character == '[') {
                    // TODO: trim section names
                    line.kind = LineKind::Section;
                    const char* section_start = ++character;
                    while (*character != ']' && *character != '\n' && *character != '\r' && *character != '\0') {
                        character++;
                    }
                    const char* section_end = character;
                    if (*character == ']') {
                        character++; // skip ']'
                    }
                    line.name_off = section_start - content.c_str();
                    line.name_len = section_end - section_start;

                    current_section = std::string_view(section_start, section_end - section_start);
                    //sections[Hash::GetLowercase(current_section)] = lines.size(); // here the last one wins
                    section_line_indices.try_emplace(Hash::GetLowercase(current_section), lines.size()); // to ensure the first one wins

                    lines.push_back(line);
                    while (*character != '\n' && *character != '\r' && *character != '\0') {
                        character++;
                    }
                }
                else if (*character == ';') {
                    line.kind = LineKind::Comment;
                    const char* comment_start = ++character;
                    while (*character != '\n' && *character != '\r' && *character != '\0') {
                        character++;
                    }
                    line.name_off = comment_start - content.c_str();
                    line.name_len = character - comment_start;
                    lines.push_back(line);
                }
                else {
                    const char* key_start = character;
                    while (*character != '=' && *character != '\n' && *character != '\r' && *character != '\0') {
                        character++;
                    }

                    const char* key_end = character;
                    while (key_end > key_start && (*(key_end - 1) == ' ' || *(key_end - 1) == '\t')) {
                        key_end--;
                    }
                    std::string_view key(key_start, key_end - key_start);

                    if (*character == '=') {
                        line.kind = LineKind::KeyValue;
                        line.name_off = key_start - content.c_str();
                        line.name_len = key_end - key_start;

                        character++; // skip '='
                        while (*character == ' ' || *character == '\t') {
                            character++;
                        }
                        const char* value_start = character;
                        while (*character != '\n' && *character != '\r' && *character != '\0') {
                            character++;
                        }
                        const char* value_end = character;

                        line.value_off = value_start - test_content.c_str();
                        line.value_len = value_end - value_start;

                        std::string_view value(value_start, value_end - value_start);
                        if (!current_section.empty() && !key.empty()) {
                            ini[std::string(current_section)][std::string(key)] = std::string(value);
                            // keyValue[Hash::GetLowercase(current_section, key)] = lines.size();
                            key_line_indices.try_emplace(Hash::GetLowercase(current_section, key), lines.size());  // to ensure the first one wins
                        }
                        lines.push_back(line);
                    }
                    else {
                        line.kind = LineKind::Bare;
                        line.name_off = key_start - test_content.c_str();
                        line.name_len = key_end - key_start;

                        while (*character != '\n' && *character != '\r' && *character != '\0') {
                            character++;
                        }
                        // hmm should i add it to the key line indices map?
                        key_line_indices.try_emplace(Hash::GetLowercase(current_section, std::string_view(key_start, key_end - key_start)), lines.size());  // to ensure the first one wins
                        lines.push_back(line);
                    }
                }
            }
        }

        std::string_view get(std::string_view section, std::string_view key, std::string_view default = "") {
            // hash
            // TODO: finish it
            auto [section_hash, hash] = Hash::GetLowercaseEx(section, key);
            auto it = key_line_indices.find(hash);
            if (it == key_line_indices.end())
                return default;
            auto index = it->second;
            // TODO: handle and check the kind to ensure its the exact key
            const auto& line = lines[index];
            std::string_view key_name = this->GetName(line);

            // huh, now that we do double hashing, i wonder the collision chance lowered to what, think about it, one hash is 0.048 possible collision, two, is it twice or how
            // just realized that we would gotta compare the section as well- just to double check-
            if (Utils::InplaceLowercaseComparison(key_name, key)) {
                return this->GetValue(line);
            }
            // TODO: handle the slow path
            // NOTE: first match wins
            // TODO: use the section hash to find the section in lines then start iterating from there
            auto it = section_line_indices[section_hash];
            // check the validdity, failure return
            // get the index & the line
            // get the section name, then compare the inputted section
            // two paths
            // if the compersion fails, iterate from the start of the file
            // if the compersion sccesss, iterate from that section till the next section, if not found, then fail, return the default

            //size_t index;

        }




    private:
        std::string_view inline GetName(const Line& line) {
            const char* key_start = content.c_str() + line.name_off;
            return { key_start, line.name_len };
        }

        std::string_view inline GetValue(const Line& line) {
            const char* value_start = content.c_str() + line.value_off;
            return { value_start, line.value_len };
        }
    };

}