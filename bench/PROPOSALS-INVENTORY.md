# Inventory profiling and optimization proposals (2026-10-05)

Repeated grouped-inventory lookup is the dominant cost. `entry_at()` and
`count()` account for **86.9–97.9%** of the seven failing responses, including
their inlined work. Long item names are a secondary cost. No optimization or
gameplay changes were made during this analysis.

The first three recommendations were subsequently implemented, with the full
suite run after each individual change. See the
[implementation results](RESULTS-INVENTORY.md); text and partial redraw remain deferred.

## Measurements

All seven cases were rerun with source and native AVR profiling. They reproduce
the previous baseline's exact cycle counts and launch identities. Each case was
also profiled in two consecutive windows, split at `render_inventory()` entry.
The two windows reconcile exactly to the uninterrupted response, with matching
before/after state, inventory, exploration, and logical display captures.

| Case | Total ms | Before rendering ms | Render + display ms | `entry_at` + `count` share |
| --- | ---: | ---: | ---: | ---: |
| inventory_down_full | 613.089 | 491.440 | 121.649 | 97.9% |
| inventory_up_full | 362.257 | 253.328 | 108.929 | 96.2% |
| inventory_open_wands | 127.265 | 44.756 | 82.509 | 86.9% |
| inventory_down_wands | 444.697 | 348.316 | 96.381 | 95.4% |
| inventory_up_wands | 330.858 | 235.670 | 95.188 | 94.0% |
| inventory_down_singletons | 355.156 | 273.750 | 81.406 | 96.7% |
| inventory_up_singletons | 232.335 | 157.235 | 75.100 | 94.6% |

“Before rendering” includes modal creation for opening and selection/viewport
updates for scrolling. Rendering includes its own inventory lookups, text,
selection highlight, display transfer, and return to the modal input boundary.
The target is 100 ms for the **sum**, not for each window individually.

Native execution is active for **99.41–99.85%** of elapsed cycles. Idle/wait time
is only 0.757 ms for opening and 0.901 ms for scrolling. Changing input waits or
display scheduling cannot recover the hundreds of milliseconds required.

## Why the lookup cost grows

- `position(slot)` enumerates every preceding row by repeatedly calling
  `entry_at(row)`. Each call restarts at group zero and scans inventory slots.
- `move()` finds the old position, then `keep_visible()` finds the new position
  from scratch. Each also invokes row counting. `move()` evaluates `count()`
  again when it skips a group header.
- `count()` scans all 16 slots once for each of the nine groups, even when only
  one group exists. That is 144 logical slot inspections per call.
- Rendering independently retrieves each of seven rows through `entry_at()`.
  Near the bottom, each lookup repeats most of the preceding group scans.
- Wands are the fifth group. Even an all-wand pack repeatedly scans four empty
  groups before finding its first header or item.

Reconstructing these existing loops from captured inventories and the benchmark
input recipes gives the following work for a single measured response. These
are logical loop counts, not claimed hardware load counts.

| Case | `position` calls | `count` calls | `entry_at` calls | Slot inspections |
| --- | ---: | ---: | ---: | ---: |
| inventory_down_full | 2 | 6 | 57 | 5,166 |
| inventory_up_full | 2 | 5 | 41 | 3,017 |
| inventory_open_wands | 0 | 3 | 9 | 1,032 |
| inventory_down_wands | 2 | 5 | 41 | 3,709 |
| inventory_up_wands | 2 | 5 | 29 | 2,771 |
| inventory_down_singletons | 2 | 6 | 43 | 4,417 |
| inventory_up_singletons | 2 | 6 | 31 | 2,845 |

The source profiles support this explanation: group classification alone takes
330.918 ms in `inventory_down_full`, and 233.413 ms in `inventory_down_wands`.
Those figures are subsets of the lookup totals and must not be added to them.

## Recommended changes, in order

### 1. Compute counts, first selection, and positions directly

Replace the group-by-slot row count with a single inventory pass and a small
group-presence mask. Determine the first slot by the smallest group, retaining
slot order within that group. Determine a slot's row by counting items and
nonempty group headers preceding it, rather than reconstructing all earlier
rows. These operations can use a few scalar temporaries and avoid a stored
row map.

This addresses both opening and scrolling. Counting alone represents 44.662 ms
of the wand-opening response, whose excess is 27.265 ms. That identifies work
available to reduce; it is not a prediction of achievable savings.

### 2. Stop restarting grouped lookup for every visible row

Use a cursor to walk consecutive visible rows, or use temporary nine-byte group
counts to locate a row's group and then scan that group's slots once. A bounded
two-pass lookup can replace up to nine full inventory scans with two inventory
passes and a short group-count walk. Reusing metadata across the seven rendered
rows offers a further reduction.

This is necessary alongside navigation improvements: mixed-pack rendering alone
takes 121.649/108.929 ms. Even eliminating all pre-render work would leave these
two cases over budget. Conversely, optimizing rendering alone leaves all six
scrolling navigation phases above 100 ms.

### 3. Reuse the selection's row and the modal's total count

Avoid computing the selected row twice per direction press. Pass the row found
during movement to visibility adjustment, or maintain a row cursor alongside
the selected slot. Cache the total row count for the lifetime of a choice modal;
the inventory does not change inside its browsing loop. Rebuild it when opening
the next modal rather than caching across inventory mutations or turns.

This complements the first two changes. Eliminating only the current
`position()` work is insufficient: subtracting its enclosing-function cost
still leaves every scrolling case above 100 ms (121.534–200.465 ms remaining).

### 4. Revisit text and partial redraw only after lookup improvements

`draw_item_text()` accounts for 3.950–10.505 ms in scrolling and 9.041 ms in wand
opening; arithmetic helpers add up to about 3.2 ms elsewhere. These costs are
much smaller than the repeated scans. An eventual improvement could stop
emitting words and numbers once the row's text is entirely clipped offscreen.
Preserve the visible prefix, highlight colors, equipment markers, and curse
markers. Avoid caching a full formatted name for every inventory slot.

Partial redraw may help later, especially for selection changes that do not
scroll. It cannot solve these failures while navigation still costs
157–491 ms. Prioritize lookup before renderer or interpreter tuning.

## Constraints for a future implementation

Keep group order, slot order within groups, skipped headers, boundary behavior,
filtered potion selection, and all existing stress fixtures. Compare complete
input-to-ready timing and logical frames after each change.

Prefer scalar state, a cursor, or small temporary group counts over a 25-byte
row table. The native inventory test explicitly rejects a stored row mapping
(`tests/game_native.cpp:260`). The current benchmark ELF's proven maximum stack
usage is 236 bytes; rerun linker stack analysis for the affected call paths,
including nested selection and item rendering. Do not infer stack growth by
simply adding a temporary's size to the global maximum.

## Artifacts and identities

Profiles, HTML reports, transcripts, captures, split windows, and analysis data:

`build/inventory-profiling/20261005T150123Z-k0ezj1es/`

Each case contains `turn.avmp`, `profile.html`, and `lldb.log`. Split profiles are
under `split/<case>/navigation.avmp` and `render-display.avmp`; `analysis.json`
contains phase timings and reconstructed work counts. The read-only diagnostic
script is `build/inventory-profiling/analyze_inventory.py`.

- Benchmark ELF SHA-256: `8ee89bc261a98b5fcd0e7bbf3098087e40491eedc1ebf6c7b3dd8c3da77ce5c2`
- Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`

The SDK's bundled interpreter ELF was stale. Native symbolization used the
byte-matching `build/prof-install/bin/avm/interp.elf` through
`AVM_LLDB_INTERP_ELF`; the measured firmware and game ELF were unchanged.
