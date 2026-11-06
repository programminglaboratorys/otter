#pragma once
#include <cassert>
#include <cstdint>
#include <string_view>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "defines.hpp"
#include "utils.hpp"

namespace otter {


    // the abseil glue: Source, Slot, Query, the hash/eq hooks and LineTable
    namespace detail {

        // where the text lives right now. travels inside every Query so the eq functor stays stateless.
        // build it after any change to the text: appending to `content` can move the buffer.
        struct Source {
            const char* base;
            const std::vector<Line>* lines;
            std::string_view name(const Line& l) const { return { base + l.name_off, l.name_len }; }
        };


        // the stored element, 16 bytes (abseil keeps the 1-byte control array separately).
        // `line` and `section_line` are mutable on purpose: they are not part of the key (`hash` is),
        // so the +1 patch and the first-wins takeover can rewrite them in place.
        struct Slot {
            name_hash_t hash = 0;
            mutable uint32_t line = kNone;
            mutable uint32_t section_line = kNone; // keys: their section header. sections: their own line.
        };

        // what a lookup/insert asks for. never stored.
        struct Query {
            Source src;
            name_hash_t hash;
            LineKind kind;
            std::string_view section, key; // key is empty for sections
        };

        // the hash is computed here, once per lookup
        inline Query make_query(const Source& src, LineKind kind,
            std::string_view section, std::string_view key) {
            return Query{ src, Hash::Entry(kind, section, key), kind, section, key };
        }

        // hook 1: a stored slot returns its saved hash (so growing never touches the text), a query carries its own
        struct SlotHash {
            using is_transparent = void;
            size_t operator()(const Slot& s) const { return static_cast<size_t>(s.hash); }
            size_t operator()(const Query& q) const { return static_cast<size_t>(q.hash); }
        };

        // hook 2: abseil calls this only after a hash/control byte match, and false just means keep probing.
        // verify order: line kind, key text, then the section header's text (via section_line), all folded like the hash.
        // both argument orders exist because abseil calls it as (element, key).
        template <bool caseInsensitive>
        struct SlotEq {
            using is_transparent = void;

            bool operator()(const Query& q, const Slot& s) const { return (*this)(s, q); }

            static inline bool inPlaceComparison(std::string_view x, std::string_view y) {
                if constexpr (caseInsensitive) {
                    return Utils::InplaceLowercaseComparison(x, y);
                }
                else {
                    return Utils::InplaceComparison(x, y);
                }
            }

            bool operator()(const Slot& s, const Query& q) const {
                const std::vector<Line>& lines = *q.src.lines;
                assert(s.line < lines.size());
                const Line& ln = lines[s.line];
                if (ln.kind != q.kind) return false; // also rejects tombstones (Blank)

                if (q.kind == LineKind::Section)
                    return inPlaceComparison(q.src.name(ln), q.section);

                if (!inPlaceComparison(q.src.name(ln), q.key)) return false;

                if (s.section_line == kNone) return q.section.empty(); // key before any section
                assert(s.section_line < lines.size());
                const Line& header = lines[s.section_line];
                if (header.kind != LineKind::Section) return false;
                return inPlaceComparison(q.src.name(header), q.section);
            }

            // slot vs slot: used by the debug check, and keeps abseil's own key_type paths well-formed
            bool operator()(const Slot& a, const Slot& b) const {
                return a.hash == b.hash && a.line == b.line && a.section_line == b.section_line;
            }
        };

        // the table is decoupled from INI: it never owns or points into the INI.
        // everything it needs to read the text comes in through a Source.
        class LineTable {
        public:
            using Set = absl::flat_hash_set<Slot, SlotHash, SlotEq<true>>;

            size_t size() const { return set_.size(); }
            size_t capacity() const { return set_.capacity(); }
            const Set& slots() const { return set_; }

            // clear and reserve room for `entries` entries (Abseil picks the real capacity)
            void reset(size_t entries) {
                set_.clear();
                set_.reserve(entries);
            }

            // returns the line index, or kNone.
            uint32_t find(const Query& q) const {
                auto it = set_.find(q);
                return it == set_.end() ? kNone : it->line;
            }

            // first wins in one probe: lazy_emplace only builds the slot if nothing verified, a verified match is a duplicate.
            // except when the existing winner is after the new line (insert in the middle): the new line is earlier, so it takes over.
            void insert(const Query& q, uint32_t line, uint32_t section_line) {
                bool inserted = false;
                auto it = set_.lazy_emplace(q, [&](const auto& construct) {
                    inserted = true;
                    construct(Slot{ q.hash, line, section_line });
                    });
                if (!inserted && line < it->line) {
                    it->line = line;
                    it->section_line = section_line;
                }
            }

            // the +1 patch: a line is about to land at `index`, so every reference at or after it moves down by one.
            // one pass over the slots, no hashing
            void shift_from(uint32_t index) {
                for (const Slot& s : set_) {
                    if (s.line >= index) ++s.line;
                    if (s.section_line != kNone && s.section_line >= index) ++s.section_line;
                }
            }

            // debug helper: is exactly this slot in the table?
            bool contains(const Slot& want) const { return set_.find(want) != set_.end(); }

            // deleted lines are tombstones (kind Blank) until the next rebuild, so the table
            // may be stale on purpose. whoever owns the lines marks it and rebuilds before a lookup.
            bool dirty() const { return dirty_; }
            void mark_dirty() { dirty_ = true; }

            // throw everything away and re-derive the table from the lines in `source`.
            // compacting the lines (dropping the tombstones) is the owner's job: it owns the vector.
            void rebuild(const Source& source) {
                reset(source.lines->size());
                fill(source, *this);
                dirty_ = false;
            }

            // debug only: the patched live table must equal a from-scratch rebuild.
            // skipped while dirty, because a pending delete makes the live table stale on purpose.
            void debug_check(const Source& source) const {
#ifndef NDEBUG
                if (dirty_) return;
                LineTable scratch;
                scratch.reset(source.lines->size());
                fill(source, scratch);
                assert(scratch.size() == size());
                for (const Slot& s : scratch.slots()) assert(contains(s));
#else
                (void)source;
#endif
            }

            // the single place that decides what goes in the table. used by rebuild() and by
            // the debug check, so they cannot disagree. bare lines are deliberately NOT indexed.
            static void fill(const Source& source, LineTable& t) {
                const std::vector<Line>& lines = *source.lines;
                uint32_t section_line = kNone;
                for (uint32_t i = 0; i < lines.size(); ++i) {
                    const Line& ln = lines[i];
                    if (ln.kind == LineKind::Section) {
                        section_line = i;
                        t.insert(make_query(source, LineKind::Section, source.name(ln), {}), i, i);
                    }
                    else if (ln.kind == LineKind::KeyValue && ln.name_len != 0) {
                        const std::string_view section = section_line == kNone ? std::string_view{} : source.name(lines[section_line]);
                        t.insert(make_query(source, LineKind::KeyValue, section, source.name(ln)), i, section_line);
                    }
                }
            }

            Set set_;
            bool dirty_ = false;
        };

    } // namespace detail

}
