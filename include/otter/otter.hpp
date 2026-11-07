#pragma once
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>
#include <iomanip>

#include <tuple>
#include <cstdint>

#include "utils.hpp"
#include "defines.hpp"
#include "detail.hpp"

// TODO: empty value vs missing key

namespace otter {

    class INI {
    public:
        std::string content = "";

        std::vector<Line> lines; // fast metadata data. the source of truth, the table is derived from it.

        void parse_lines(const std::string& test_content) {
            content = test_content; // TODO: move to a constructor or some loader method
            lines.clear();
            const char* character = content.c_str();

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
                        lines.push_back(line);
                    }
                    else {
                        line.kind = LineKind::Bare;
                        line.name_off = key_start - content.c_str();
                        line.name_len = key_end - key_start;

                        while (*character != '\n' && *character != '\r' && *character != '\0') {
                            character++;
                        }
                        // bare lines can't be read by key name, so they are not in the table
                        lines.push_back(line);
                    }
                }
            }
            rebuild();
        }

        // NOTE: the returned view points into `content`; insert_key_value() may move that buffer.
        std::string_view get(std::string_view section, std::string_view key, std::string_view default_value = "") {
            if (table_.dirty()) rebuild(); // a delete left tombstones behind
            const uint32_t index = table_.find(detail::make_query(source(), LineKind::KeyValue, section, key));
            if (index == kNone) return default_value;
            return value_of(lines[index]);
        }

        bool has_section(std::string_view name) {
            if (table_.dirty()) rebuild();
            return table_.find(detail::make_query(source(), LineKind::Section, name, {})) != kNone;
        }

        // insert "name=value" so it lands at lines[index] (index == lines.size() appends).
        // indices refer to the compacted `lines`, so any pending delete is flushed first.
        // the text is appended to `content`; file order lives in `lines`, not in the buffer.
        bool insert_key_value(size_t index, std::string_view name, std::string_view value) {
            if (table_.dirty()) rebuild();
            if (index > lines.size() || name.empty()) return false;

            // which section owns the new line? the nearest header above it.
            uint32_t section_line = kNone;
            for (size_t i = index; i-- > 0;) {
                if (lines[i].kind == LineKind::Section) { section_line = static_cast<uint32_t>(i); break; }
            }

            Line line;
            line.kind = LineKind::KeyValue;
            line.offset = line.name_off = content.size();
            line.name_len = name.size();
            content.append(name);
            content.push_back('=');
            line.value_off = content.size();
            line.value_len = value.size();
            content.append(value);
            content.push_back('\n');

            // the order matters: patch the table, then add the line, then add the new entry.
            table_.shift_from(static_cast<uint32_t>(index));
            lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(index), line);

            const Source src = source(); // after the append: the buffer may have moved
            const std::string_view section = section_line == kNone ? std::string_view{} : src.name(lines[section_line]);
            table_.insert(detail::make_query(src, LineKind::KeyValue, section, src.name(lines[index])),
                static_cast<uint32_t>(index), section_line);

            table_.debug_check(src);
            return true;
        }

        // tombstone + lazy rebuild. if the deleted line was the winner of a duplicate,
        // the runner-up has no table entry, and only a rebuild can see that.
        void erase_line(size_t index) {
            if (index >= lines.size()) return;
            lines[index].kind = LineKind::Blank;
            table_.mark_dirty();
        }

        // compact the dead records, then re-derive the whole table from `lines`
        void rebuild() {
            lines.erase(std::remove_if(lines.begin(), lines.end(),
                [](const Line& l) { return l.kind == LineKind::Blank; }),
                lines.end());
            table_.rebuild(source());
        }


        using Source = detail::Source;
        using Query = detail::Query;
        using Slot = detail::Slot;
        using LineTable = detail::LineTable;

        Source source() const { return Source{ content.c_str(), &lines }; }

        std::string_view inline name_of(const Line& line) {
            const char* key_start = content.c_str() + line.name_off;
            return { key_start, line.name_len };
        }

        std::string_view inline value_of(const Line& line) {
            const char* value_start = content.c_str() + line.value_off;
            return { value_start, line.value_len };
        }

        LineTable table_;
    };

}
