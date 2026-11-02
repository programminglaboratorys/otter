# Otter Prototype Evaluation: What to Keep vs. What to Discard

This audit reviews the prototype in `junk/otter` to identify patterns, data structures, and edge-case logic worth keeping when rewriting a simpler, cleaner, pointer-free implementation.

---

## 1. What to Keep (High Value Core Logic)

### A. The In-Place Patch Mechanics (`src/patcher.cpp`)
Keep the replacement rules; these directly match ground-truth Windows profile behavior:
1. **Value replacement slice**: Overwriting from `equals_pos + 1` up to the last non-blank character, leaving any trailing whitespace and the existing line terminator intact.
2. **Whitespace-only value preservation**: When replacing `key= `, preserving the space so it becomes `key=NEW `.
3. **Key casing invariance**: Updating an existing key with a differently cased name (`KEY` on `Key=old`) retains the original line's key spelling (`Key=NEW`).
4. **Indentation-preserving delete (Case D07)**: Deleting an indented line by trimming from the first non-whitespace character to line end, letting leading whitespace join the subsequent line.
5. **Section deletion boundary**: Deleting from the section header through the last key or valueless line, leaving post-section comments and blanks intact. Empties the entire buffer if it was the only section.
6. **Trailing line sanitation**: Stripping trailing unterminated whitespace-like bytes (`' '`, `\0`, `\x1a`) and ensuring the file ends with a clean `\r\n` / `\n`.

### B. Heuristic Line Classification (`src/line_table.cpp`)
1. **Header recognition**: Name extracted up to the first `]` (ignoring any subsequent text on the line, e.g., `[A]xyz` -> section `A`). Unclosed headers (`[A`) still form valid sections.
2. **Comment definition**: Strictly lines whose first non-whitespace character is `;`. `#` and `//` are treated as valueless lines or ordinary keys.
3. **Valueless lines**: Kept in the index so they appear in section enumerations, but excluded from key lookups.

### C. Redo Journal Ordering & Checkpoint Invariant (`src/journal.cpp`)
The binary layout and recovery protocol are solid:
1. **Record verification**: A prefix hash of bytes preceding the patch offset (`prefix_hash`) paired with a post-mutation hash (`after_hash`).
2. **Commit barrier**: Journal flush MUST precede the in-place file write. If a crash occurs before the journal flushes, the file is untouched; if after, the journal safely replays the patch.

### D. Windows-Specific Read Quirks (`src/ini.cpp`)
1. **Quote stripping**: Only values whose first and last character are matching `"` or `'` (length >= 2) are unquoted on key lookup. Defaults passed to `get()` must never be unquoted.
2. **`GetInt` behavior**: Decimal parsing with fallback support for lowercase `0x` hexadecimal; parsing halts at the first invalid digit without throwing; empty/whitespace inputs yield the default value.

---

## 2. What to Discard & Simplify

### A. Eliminate Heap Indirections and Smart Pointers
- **Current**: `IniFile` -> `std::unique_ptr<Impl>` -> `std::unique_ptr<Storage>` -> `std::thread`, `std::mutex`, `std::condition_variable`.
- **Target**: Flat value semantics. A single struct or class holding:
  - `std::vector<uint8_t> buffer;`
  - `std::vector<LineSpan> lines;`
  - Simple file descriptor / handle.
  No heap-allocated PIMPL, no `shared_ptr`, no dynamic trait wrappers.

### B. Replace Line Re-parsing with Offset Shifts
- **Current**: `replace_byte_range()` calls `table.parse()` on every write, doing a full buffer re-scan.
- **Target**: In a hand-written version, a length-changing edit only shifts line offsets after the edit point by `(new_len - old_len)`. Re-parsing is unnecessary.

### C. Consolidate String Conversions
- **Current**: Separate CP1252-to-UTF8 and UTF8-to-CP1252 tables with branching loops.
- **Target**: A single 256-entry static lookup table (`uint8_t` / `char32_t`) for branchless conversion and whitespace checking.

### D. Flatten Storage and Journaling
- **Current**: Full multi-threaded background queue with condition variables and asynchronous flushing.
- **Target**: A synchronous, single-threaded model by default: write to memory, append journal record, update file when requested or on flush. Keep threading strictly optional or external.

---

## 3. Recommended Minimal Architecture

```
[Otter Engine]
   │
   ├── Buffer: std::vector<uint8_t> (raw file image)
   ├── Index:  Flat array of line spans (offset, length, type)
   ├── Patch:  Direct in-place memmove + delta offset adjustment
   └── File:   Direct OS handle with sharing flags (read/write/delete)
```
