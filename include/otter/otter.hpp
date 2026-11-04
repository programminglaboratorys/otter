#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <iomanip>

#include <tuple>
#include <cstdint>

#include "utils.hpp"
#include "defines.hpp"

// TODO: empty value vs missing key

namespace otter {

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
            content = test_content; // TODO: move to a constructor or some loader
            const char* character = content.c_str();
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

                        line.value_off = value_start - content.c_str();
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
                        line.name_off = key_start - content.c_str();
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

        std::string_view get(std::string_view section, std::string_view key, std::string_view defaultValue = "") {
            // hash
            // TODO: finish it
            auto [section_hash, hash] = Hash::GetLowercaseEx(section, key);
            auto it = key_line_indices.find(hash);
            if (it == key_line_indices.end())
                return defaultValue;
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
            auto it2 = section_line_indices[section_hash];
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