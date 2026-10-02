# Streaming status and item text

The status path now measures and draws one complete lexical word with a single `char buf[5]`, drawing at most four characters per call. The cursor ends immediately after the last glyph. A following word accounts for its leading space before wrapping; punctuation is included in the same wrap decision. There is no persistent text scratch buffer.

The comparison uses game revision `c2525fc5c01e8426aa5b6db388ff74134c582e29` and the existing AVM SDK, C++17, `-O2 -flto`, and LTO O2. Changes are left uncommitted. No AVM backend, interpreter, runtime API, or parent build configuration was changed.

Changed files:

- `src/status.cpp`: streaming words, modifier state, direct small integers, and centralized item grammar with final-token suffixes.
- `src/status.hpp`: public word API and its return-pointer contract.
- `tests/format_native.cpp`: lexical layout oracle, modifier, number, item, and pagination regressions.
- `tests/game_native.cpp` and `tests/ui_native.cpp`: status API stubs.
- `ardurogue2.arduboy`: rebuilt game package.
- `docs/status-streaming.md`: this report.

The public API is:

```cpp
void status_suffix(char c);
void status_capitalize();
const char* status_word(const char* word);
const char AVM_PROGMEM* status_word(const char AVM_PROGMEM* word);
void status_words(const char* words);
void status_words(const char AVM_PROGMEM* words);
```

On native test builds the address spaces coincide, so only one overload is declared. On AVM, the template preserves the pointer address space and produces ordinary RAM loads or program-memory loads as appropriate. The linked PROGMEM code uses `ldp8u`; the helper folds into `status_word`, without an extra forwarding call. The RAM overload is compiled but dead-stripped from the current linked game, so its final machine frame is unavailable. `status_words` only loops over `status_word`'s return pointer. Existing string `status` calls use a lightweight compatibility wrapper; suffix-bearing phrase and monster calls select the last lexical word before setting its suffix.

`status_suffix` replaces a pending character. `status_capitalize` sets a pending first-character flag. Empty input, spaces-only input, and leading spaces consume neither modifier. The word renderer snapshots both modifiers, measures the entire word and suffix without clearing them, handles wrapping and the existing [more] input sequence with local cursor coordinates, draws the word and suffix, then commits its cursor and clears the consumed state. Capitalization is applied to the first chunk in both measurement and drawing. No separator is inserted between chunks.

`emit_item(item, style, sink, suffix)` retains all naming, identification, article, quantity, modifier, and bonus rules in one formatter. Each grammar branch explicitly marks its terminal token with `final_word`, `final_number`, or `final_bonus`. A terminal token can contain a phrase, such as `food rations` or `see invisible`; a small pointer lookahead sets the suffix immediately before its final lexical word. Identified sword/armor bonuses remain after the name, preserving existing text such as `the sword +2?`. Inventory wand charges remain after their names. Buffered and inventory-drawing sinks use the same grammar and ignore status punctuation.

`StatusItemText` has no pending token, pending value, kind, or flush method. Ordinary words call `status_word` directly; phrases loop through `status_words`; terminal phrases use the suffix-aware lookahead. Item emission never calls the string `status` parser. Small numeric status output generates decimal digits directly into a separate five-byte buffer and performs measurement, wrapping, drawing and modifier consumption in that function. It does not keep a numeric buffer live across `status_word`. Existing inventory drawing and buffered-label printf calls remain. AVM printf is native AVR code: removing status numeric printf calls reduces AVM argument/caller overhead, not a printf implementation frame on the VM stack.

The former `word[32]`, RAM `part[16]`, punctuation `mark[2]` machinery, conceptual trailing spaces, cursor backtracking, and forced `status_words_P` forwarding wrapper were removed. The explicit general-text scratch reduction is 32 to 5 bytes, or 27 bytes. A very long lexical word is drawn intact in chunks after one wrap decision, as required by the new API; chunks do not cause additional line breaks. Existing game item words and both captured pickup prompts retain their pixels.

Two source-level modifier bytes are reserved, as authorized during the task. Neither is a text buffer. In the current LTO game capitalization has no production caller and its state is eliminated; suffix state occupies one byte. The former RAM space string also disappears. AVM packs zero-initialized static storage into `.data`, rather than a separate `.bss` section.

| Storage / code | Before | After | Delta |
| --- | ---: | ---: | ---: |
| `.saved` | 821 | 821 | 0 |
| `.data`, including zero-initialized storage | 16 | 15 | -1 |
| Separate `.bss` | 0 | 0 | 0 |
| `.text` | 40,360 | 44,740 | +4,380 |
| `.rodata` | 6,529 | 6,529 | 0 |
| Allocated program text + constants | 46,889 | 51,269 | +4,380 |
| Packaged `.arduboy` file | 132,340 | 136,692 | +4,352 |

The code increase comes with the intended two-pass word traversal and more inlined phrase loops. No text-span opcode or runtime change was introduced.

Frames below include callee-saved registers and local/spill allocation, but exclude the caller's three-byte return address. They were inspected in the final linked AVM disassemblies.

| Function | Before | After |
| --- | ---: | ---: |
| Buffered PROGMEM `status_words` | 44 | removed; loop is inlined |
| `status_word_mutable` | 20 | removed |
| Streaming PROGMEM `status_word` | absent | 27 |
| `status(Item, char)` | 29 | 24 |
| Pending `StatusItemText::word` | 8 | removed |
| Terminal `StatusItemText::final_word` | absent | 12 |
| Numeric status helper | 24 | 21 |
| `status_words_P` forwarding layer | 0, tail jump | removed |

The old phrase frame saved eight register bytes and allocated 36 bytes, including its 32-byte array; the new word frame saves eight and allocates 19, including its five-byte array. Its non-array local/spill allowance grows from four to fourteen bytes because it tracks a 24-bit source pointer, measurement/drawing cursors and modifiers. That increase is included in the 27-byte machine frame; the old nested 20-byte mutable renderer and pending-token frame disappear. There are no forced `noinline` annotations in `status.cpp`. Existing `render_yesno_prompt`, `yesno_modal`, `yesno`, `prompt_ground_items`, and other UI modal/renderer boundaries retain their existing annotations.

Validation:

- Built the game through its standalone CMake build in RelWithDebInfo, with the existing SDK and warnings treated as errors.
- Built and passed all four native CTest targets: `game_native`, `format_native`, `ui_native`, and `render_native`.
- Checked words of lengths 1, 4, 5, 8, 9+, multiple words, repeated/leading spaces, empty and spaces-only strings, return-pointer positions, suffix replacement, suffix/capitalization survival, combined modifiers, complete-word wrapping, suffix-forced wrapping, and chunk boundaries.
- Checked [more] pagination, modifier survival through pagination, release/press sequencing, previous-button state, held-direction reset, and repeat suppression.
- Checked food, known/unknown potions and scrolls, swords, armor, rings, amulets, wands, quantities, numeric bonuses, wand modifiers/charges, long/multiword names, prompt question marks and sentence punctuation. Existing inventory label/drawing equivalence checks cover every item type.
- Checked numeric boundaries 0, 1, 9, 10, 99, 100 and 255, including punctuation and modifier consumption.
- Used installed `avm-lldb` to start the old and new game through real button input, install identical deterministic game-state fixtures, move RIGHT onto an item, and stop at `render_yesno_prompt` before display. Food and the identified overpowered teleportation wand produce byte-for-byte identical full-frame captures, including A Yes / B No. The food capture hash is `092cdbc876055085d390db71378944da3028dcf3c10db2be1a39324f15bbb906`; the long-wand hash is `c83aee433d201aa577054db6eb11880eebdcdf0dcf6bd4926a388455e981e929`.
- LLDB source breakpoints and backtraces verify the removed pending-token call chain. At the deepest food status-rendering stops, the old SP is `0x095a` (166 live VM bytes) and the new SP is `0x0983` (125 bytes): 41 fewer bytes on this tested status path. These are VM measurements, excluding the native AVR stack. No exact systemwide stack reduction is claimed.
- `git diff --check` passes. The temporary parent-library emulator test scaffolding was removed; no game tests link against the parent repository.

The tested new prompts show no framebuffer corruption. The baseline revision also remained within the VM stack for these fixtures, so the historical overflow was not reproduced in this checkout. The framebuffer comparisons establish unchanged output for the tested paths, not a guarantee for every possible gameplay state.

The linker's overall maximum provable bound changes from 249 to 252 bytes after LTO, and remains incomplete. Its final longest reported path is a wand/monster animation followed by dungeon sight rendering, ending in `reveal_view_floor` (44-byte frame), rather than status text. Inlining and register allocation change other caller frames, so that report is not a complete runtime bound. Those renderer/animation paths remain outside this refactor. Buffered inventory labels and native numeric inventory drawing also remain outside the status streaming path.

Inspection artifacts and LLDB scripts/captures for this run are under the parent workspace's ignored `build/status-streaming` directory. They are diagnostic files only; the game has no build or link dependency on that directory or the parent emulator libraries.
