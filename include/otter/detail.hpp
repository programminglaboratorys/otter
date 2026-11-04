#pragma once
#include <algorithm>
#include <cassert>
#include <absl/container/flat_hash_set.h>

#include "defines.hpp"

namespace otter {


    namespace Hash {
        // FNV-1a step over one case-folded character.
        [[nodiscard]] inline constexpr xhash_t Step(xhash_t h, char c) noexcept {
            return (h ^ static_cast<xhash_t>(static_cast<unsigned char>(Utils::LowercaseCharacter(c)))) * kPrime;
        }

        // abseil splits the hash into H1 (the upper 57 bits, picks the group)
        // and H2 (the lower 7 bits, stored in the control byte). Both halves
        // must be well mixed, and FNV-1a alone is not, so scramble once at the
        // end (murmur3 finalizer).
        [[nodiscard]] inline constexpr xhash_t Finalize(xhash_t h) noexcept {
            h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;
            return h;
        }

        // input is: kind tag, folded section, then (keys only) '\0' separator and folded key.
        // The tag keeps a section named "x" apart from a key lookup, the '\0' keeps
        // ("ab","c") apart from ("a","bc").
        [[nodiscard]] inline constexpr xhash_t Entry(LineKind kind, std::string_view section,
            std::string_view key = {}) noexcept {
            xhash_t h = (kOffset ^ static_cast<xhash_t>(kind)) * kPrime;
            for (char c : section) h = Step(h, c);
            if (kind != LineKind::Section) {
                h = (h ^ 0u) * kPrime; // the '\0' separator
                for (char c : key) h = Step(h, c);
            }
            return Finalize(h);
        }
    }

    // otter::detail: the Abseil glue (Source, Slot, Query, SlotHash, SlotEq, LineTable) internal to the parser
    namespace detail {

        // a snapshot of "where the text lives right now" in Query so the table's Eq functor stays stateless
        struct Source {
            const char* base;
            const std::vector<Line>* lines;
            std::string_view name(const Line& l) const { return { base + l.name_off, l.name_len }; }
        };


        // The stored element. This IS the slot of the Swiss table: Abseil keeps the
        // 1-byte control array separately, and this 16-byte struct in the slot array.
        //
        // `line` / `section_line` are `mutable` on purpose: Abseil hands out const
        // elements because changing a KEY would corrupt the table. These two fields
        // are not part of the hash (the stored `hash` is), so the "+1 patch" and the
        // first-wins takeover may rewrite them in place.
        struct Slot {
            xhash_t hash = 0;
            mutable uint32_t line = kNone;
            mutable uint32_t section_line = kNone;
        };

        // what a lookup/insert asks for
        struct Query {
            Source src;
            xhash_t hash;
            LineKind kind;
            std::string_view section, key; // key is empty for sections
        };

        // make a query (constructor)
        static Query make_query(const Source& src, LineKind kind,
            std::string_view section, std::string_view key) {
            return Query{ src, Hash::Entry(kind, section, key), kind, section, key };
        }

        // Hook 1: Abseil calls this to place an element. For a stored Slot we just
        // return the hash we saved (also what Abseil calls on every rehash, so
        // growing never touches the text). For a Query the hash is precomputed.
        struct SlotHash {
            using is_transparent = void;
            size_t operator()(const Slot& s) const { return static_cast<size_t>(s.hash); }
            size_t operator()(const Query& q) const { return static_cast<size_t>(q.hash); }
        };

        // Hook 2: Abseil calls this ONLY after a hash/control-byte match (the 7-bit
        // H2 filter already rejected ~99% of non-matches). Returning false is never
        // an error, Abseil just keeps probing. Abseil calls it as (element, key),
        // so both argument orders are provided and mean the same thing.
        // Verify order: the line's kind, then the key text, then the section header's
        // text via section_line, all with the same case fold as the hash.
        template <bool caseInsensitive>
        struct SlotEq {
            using is_transparent = void;

            bool operator()(const Query& q, const Slot& s) const { return (*this)(s, q); }

            static inline bool inPlaceComparison(std::string_view x, std::string_view y) {
                if constexpr (caseInsensitive) {
                    return Utils::InplaceLowercaseComparison(x, y);
                }
                return Utils::InplaceComparison(x, y);
            }

            bool operator()(const Slot& s, const Query& q) const {
                const std::vector<Line>& lines = *q.src.lines;
                assert(s.line < lines.size());
                const Line& ln = lines[s.line];
                if (ln.kind != q.kind) return false; // also rejects tombstones (Blank) bleh

                if (q.kind == LineKind::Section)
                    return inPlaceComparison(q.src.name(ln), q.section);

                if (!inPlaceComparison(q.src.name(ln), q.key)) return false;

                if (s.section_line == kNone) return q.section.empty(); // key before any section
                assert(s.section_line < lines.size());
                const Line& header = lines[s.section_line];
                if (header.kind != LineKind::Section) return false;
                return inPlaceComparison(q.src.name(header), q.section);
            }

            // Slot vs Slot: exact structural equality. Used by the debug check
            // ("is this exact slot in the live table?") and keeps Abseil's own
            // key_type paths well-formed.
            bool operator()(const Slot& a, const Slot& b) const {
                return a.hash == b.hash && a.line == b.line && a.section_line == b.section_line;
            }
        };

        class LineTable {
        public:
            using Set = absl::flat_hash_set<Slot, SlotHash, SlotEq<true>>;

            size_t size() const { return set_.size(); }
            size_t capacity() const { return set_.capacity(); }
            const Set& slots() const { return set_; }

            void reset(size_t entries) {
                set_.clear();
                set_.reserve(entries);
            }

            // returns the line index, or kNone.
            uint32_t find(const Query& q) const {
                auto it = set_.find(q);
                return it == set_.end() ? kNone : it->line;
            }

            // first wins, in ONE probe: lazy_emplace looks the Query up and only
            // builds the Slot if nothing verified. A verified match is a duplicate.
            // Exception: if the existing winner sits AFTER the new line (we are
            // inserting in the middle), the new line is earlier in the file, so it
            // takes over the slot.
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

            // The "+1 patch": a line is about to be inserted at `index`, so every
            // reference at or after it moves down by one. One pass, no hashing.
            void shift_from(uint32_t index) {
                for (const Slot& s : set_) {
                    if (s.line >= index) ++s.line;
                    if (s.section_line != kNone && s.section_line >= index) ++s.section_line;
                }
            }

            // Debug helper: is exactly this slot (hash + both indices) in the table?
            bool contains(const Slot& want) const { return set_.find(want) != set_.end(); }


            // the single place that decides what goes in the table. needed by rebuild()
            // and also used by the debug check to verify
            void fill(Source& source, LineTable& t) const {
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

            void rebuild(Source& source) {
                const std::vector<Line>& lines = *source.lines;
                lines.erase(std::remove_if(lines.begin(), lines.end(),
                    [](const Line& l) { return l.kind == LineKind::Blank; }),
                    lines.end());
                set_.reset(lines.size());
                fill();
                //dirty_ = false;
            }

        private:
            Set set_;
        };

    } // namespace detail

}