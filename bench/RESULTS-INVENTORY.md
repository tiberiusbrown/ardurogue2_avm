# Inventory optimization results (2026-10-05)

All **30/30 benchmarks pass the 100 ms limit**. The worst inventory response
drops from **613.089 ms** to **50.509 ms**. The full-pack downward
scroll improves from **613.089 ms** to **35.770 ms**.

The slowest case in the full suite remains `wait_dense` at **75.340 ms**.
Text formatting and partial redraw remain deferred, as proposed in the
[profiling analysis](PROPOSALS-INVENTORY.md).

## Separately measured changes

Each row is a cumulative build. The full 30-case suite ran after each
individual optimization, before applying the next one. Timings include the
submitted input, computation, rendering, display transfer, and return to the
input boundary. They are deterministic emulated AVR time, not host time.

| Applied change | Full pack DOWN ms | Wands OPEN ms | Worst inventory ms | Cases <=100 ms |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 613.089 | 127.265 | 613.089 | 23/30 |
| One-pass count | 515.724 | 87.316 | 515.724 | 24/30 |
| Direct first slot | 517.122 | 73.010 | 517.122 | 24/30 |
| Direct position | 152.498 | 73.010 | 152.498 | 25/30 |
| Bounded row lookup | 51.651 | 41.612 | 63.968 | 30/30 |
| Reuse selected row | 45.984 | 41.620 | 58.179 | 30/30 |
| Cache modal total | 35.770 | 39.010 | 50.509 | 30/30 |

1. `count()` makes one inventory pass, using a group-presence mask for headers.
2. `first_slot()` directly finds the earliest group and preserves slot order.
3. `position()` counts preceding items and headers in one pass, retaining
   support for item slots, header identifiers, filtered views, and invalid slots.
4. `entry_at()` uses nine temporary group counts to locate the group, then
   scans its slots once. It no longer scans all earlier groups for each row.
5. The choice modal retains the selected row. Movement updates it, and
   viewport adjustment reuses it instead of computing the position twice.
6. The modal computes its total once and passes it to navigation and rendering.
   Opening another modal rebuilds the metadata after inventory/filter changes.

The first-slot change mainly helps opening. Its small increases in scrolling
are retained in the reported complete-response totals. Bounded row lookup is
the first step that puts all 30 cases under budget.

The final count-caching build initially inlined `render_inventory()`, so the
runner rejected it for lacking a verifiable renderer boundary. The renderer
now has `noinline`; the existing validation was retained and the full suite
rerun. The rejected partial run is excluded from these results.

## All inventory cases

| Case | Baseline | Count | First | Position | Rows | Selection | Total (final) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| inventory_open_full | 73.234 | 35.215 | 33.035 | 33.035 | 36.239 | 36.247 | 33.641 |
| inventory_down_full | 613.089 | 515.724 | 517.122 | 152.498 | 51.651 | 45.984 | 35.770 |
| inventory_up_full | 362.257 | 287.436 | 287.608 | 115.016 | 47.248 | 41.557 | 33.971 |
| inventory_open_wands | 127.265 | 87.316 | 73.010 | 73.010 | 41.612 | 41.620 | 39.010 |
| inventory_down_wands | 444.697 | 367.243 | 367.461 | 103.351 | 63.968 | 58.179 | 50.509 |
| inventory_up_wands | 330.858 | 257.551 | 257.726 | 100.869 | 62.011 | 56.222 | 48.556 |
| inventory_open_singletons | 61.501 | 35.118 | 32.475 | 32.475 | 27.257 | 27.265 | 25.498 |
| inventory_down_singletons | 355.156 | 292.287 | 293.229 | 102.817 | 37.043 | 33.175 | 26.503 |
| inventory_up_singletons | 232.335 | 173.961 | 174.442 | 88.535 | 39.155 | 35.453 | 28.784 |

All values in the table are milliseconds.

## Correctness and memory

- Every completed stage matches the baseline's captured before/after named
  game state, exploration bytes, and display pixels for all 30 cases.
  Browsing captures both logical frames; selection highlights and scrolling
  overlap checks remain enabled. Bench fixtures, inputs, and launch identities
  other than the rebuilt game image/ELF are unchanged.
- All nine final native CTest entries pass. The new independent inventory
  test covers 256 empty, sparse, and full packs across four filters (1,024
  views), comparing grouped rows to a sorted reference. It checks headers,
  positions, movement boundaries, cached selection rows, and viewport limits.
  UI tests also reopen after inventory and filter changes, including an empty view.
- `InventoryView` retains only its source reference and filter. There is no
  persistent row map or save-layout change. Row lookup uses a nine-byte local
  array; the modal retains two scalar row/count bytes.
- Both normal and benchmark ELFs build in RelWithDebInfo. Linker maximum
  stack bounds remain **238 bytes** and **236 bytes**, respectively, complete
  with zero analysis gaps. The root `ardurogue2.arduboy` is rebuilt.

## Artifacts

Each completed stage retains its ELF copy and `verified.json` under
`build/inventory-optimization/<stage>/`. Its run directory contains the
30 profiles, debugger transcripts, state snapshots, display captures, and summary.

| Stage | Summary | Benchmark ELF SHA-256 |
| --- | --- | --- |
| Baseline | [summary](../build/turn-benchmarks/20261005T145451Z-edhn0zua/summary.md) | `8ee89bc261a98b5fcd0e7bbf3098087e40491eedc1ebf6c7b3dd8c3da77ce5c2` |
| One-pass count | [summary](../build/inventory-optimization/01-count/20261005T152034Z-1pbgd8jp/summary.md) | `a7aa24614080644275bb58890b94a6b218148c921132cb139c986634c51d7237` |
| Direct first slot | [summary](../build/inventory-optimization/02-first-slot/20261005T152555Z-fx8jd8i3/summary.md) | `3beebcf6015f446e82b5442aa789328e9eb24a8b53873c27aef0e7cebe213e78` |
| Direct position | [summary](../build/inventory-optimization/03-position/20261005T153057Z-2rp3c8ef/summary.md) | `91411d56d80cf6a60b76f2a9630197ed07efa5594246f990a0bff0d9fcbfa65e` |
| Bounded row lookup | [summary](../build/inventory-optimization/04-row-lookup/20261005T153406Z-bwj8doca/summary.md) | `dc612ff28520ca87674644e80e71805e8758b1ac9948fcf51808f5db325a3524` |
| Reuse selected row | [summary](../build/inventory-optimization/05-selection-row/20261005T153704Z-pff52p37/summary.md) | `eb8aa9cac37bbcbda53347c9c8778aa3c83e915192a558d5037c40eb0f6220b2` |
| Cache modal total | [summary](../build/inventory-optimization/06-modal-count/20261005T154302Z-8v201wnb/summary.md) | `9a0fb5734220eb38b757b386cf55edab545fa3b4db5e65ab7b92d838097a194b` |

Scenario SHA-256: `0a9fd67454d43deb83863133ca93139a8a1a376a71403311642abad143829f41`.

Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`.

## Reproduce the final build

```text
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
cmake --build build/native --config RelWithDebInfo --parallel
ctest --test-dir build/native -C RelWithDebInfo --output-on-failure
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check --output build/turn-benchmarks
```
