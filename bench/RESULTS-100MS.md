# Shared visibility and row exploration results (2026-10-04)

The two approved changes were implemented **one at a time**, with correctness
checks and the complete 21-case benchmark suite after each. The target was
100 ms (1,600,000 emulated AVR cycles), now the documented limit and runner default.

| Stage | Cases passing 100 ms | Dense movement ms | Dense waiting ms |
| --- | ---: | ---: | ---: |
| Before this round | 19/21 | 113.408 | 118.145 |
| 1: shared visibility ray prefixes | 21/21 | 95.563 | 97.128 |
| 2: add row visibility/exploration | 21/21 | 74.638 | 76.022 |

The slowest final case is `wait_dense` at 76.022 ms, leaving
23.978 ms to the limit. Times are deterministic emulated
input-to-ready latencies; each case runs once per build.


Change 1 replaces independent interpreted ray paths in the renderer with fixed
branches for their shared ordered prefixes. A generator emits the 96 blocker
branches with constant row/bit tests; the normal game needs no runtime prefix
tree or recursion. The existing individual-ray API remains the comparison
reference in native tests. Room visibility, circular light, clipping, target
exclusion, and closed-door rules retain their previous behavior. The shared ray
stage takes 1.503/1.559 ms in the dense cases, versus 19.914/22.595 ms for the old
individual ray tests. Some lit-room cases initially became about 1-2 ms slower
because rays are computed before the room shortcut is applied; they still pass
100 ms, and the second change more than removes that overhead.

Change 2 applies room, light, and wall-adjacency visibility as row masks, then
merges the complete sight rows into the packed exploration array. Clipped
writes touch at most three map bytes per row and preserve neighboring bits.
Floor visibility, wall visibility, and exploration take 1.792, 1.911, and
0.987 ms respectively in both dense cases. Terrain halo extraction (proposal 3)
was not changed.

## Validation and memory

- Both full benchmark runs pass `--goal-ms 100 --check --html` with exit status 0.
  The compiled scenario source and firmware identity are unchanged.
- A subsequent full run without `--goal-ms` confirms all 21 cases pass the
  100 ms default with `--check`; all six benchmark helper tests also pass.
- All 21 cases in **both stages** exactly match the pre-change named before/after
  state snapshots, complete exploration arrays, and captured controller frames.
- All seven native CTest entries pass after each stage. Shared rays are compared
  across 1,024 blocker patterns, including every single blocker and random
  combinations, and against world visibility on generated maps. Final renderer
  checks compare 552 terrain/room/clipping cases with an independent per-cell
  oracle; the eleven existing golden frame/exploration hashes and pagination
  checks remain unchanged and pass. The generator/header consistency check passes.
- Production stack bounds are 241/256 bytes after change 1 and 238/256 after
  change 2, both complete with zero analysis gaps. The final benchmark bound is
  236/256 bytes. `.saved` stays 821 bytes and `.data` stays 102 bytes, totaling
  923/1,024 bytes; no new saved fields or persistent visibility cache are needed.
- Final production `.text` is 49,743 bytes and `.rodata` is 5,896 bytes. Together
  they grow by 1,265 bytes from the pre-change build, trading flash for fixed
  blocker tests. The root `ardurogue2.arduboy` is rebuilt.

## All 21 cases

| Benchmark | Before ms | Change 1 ms | Changes 1+2 ms | Additional saving from change 2 ms |
| --- | ---: | ---: | ---: | ---: |
| move_room | 66.047 | 67.656 | 40.548 | 27.108 |
| move_corridor | 74.422 | 61.089 | 40.990 | 20.099 |
| move_map_edge | 32.946 | 34.124 | 24.908 | 9.216 |
| move_dense | 113.408 | 95.563 | 74.638 | 20.925 |
| wait | 66.221 | 68.249 | 41.551 | 26.698 |
| wait_dense | 118.145 | 97.128 | 76.022 | 21.106 |
| attack_hit | 71.859 | 73.859 | 46.858 | 27.000 |
| attack_miss | 77.332 | 79.332 | 52.333 | 26.998 |
| attack_kill | 71.009 | 73.009 | 46.009 | 27.000 |
| open_door | 82.155 | 66.024 | 45.522 | 20.502 |
| eat_food | 69.181 | 71.208 | 44.510 | 26.698 |
| drink_healing | 78.265 | 80.293 | 53.595 | 26.698 |
| equip_weapon | 71.327 | 73.355 | 46.657 | 26.698 |
| equip_armor | 70.389 | 72.416 | 45.719 | 26.698 |
| equip_ring | 74.079 | 76.106 | 49.409 | 26.698 |
| equip_cursed_amulet | 84.828 | 86.855 | 60.159 | 26.696 |
| scroll_mapping | 89.183 | 91.210 | 64.513 | 26.698 |
| scroll_teleport | 88.058 | 83.638 | 57.655 | 25.983 |
| drop_food | 72.598 | 74.625 | 47.929 | 26.696 |
| wand_digging | 91.758 | 75.625 | 55.221 | 20.404 |
| pickup_food | 74.779 | 76.415 | 49.609 | 26.806 |

## Reproduction and artifacts

```text
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --goal-ms 100 --check --html --output build/turn-benchmarks
cmake --build build/native --config RelWithDebInfo
ctest --test-dir build/native -C RelWithDebInfo --output-on-failure
```

The shared-ray header is generated with `python tools/generate_ray_sight.py`;
`--check` verifies it without writing. This keeps compilation compatible with the
existing native compiler while producing the same fixed branches for both builds.

- Before benchmark ELF SHA-256: `45f3e639b6c9bbafa5fb714b126dfc8cbd754ec65910fd3b2f268ea0f9eb1f7e`.
- Change 1 benchmark ELF SHA-256: `70a5c5498f5f19ebcf6f3769549e14f0671b11ae725dbde50353a06617b0c32b`.
- Changes 1+2 benchmark ELF SHA-256: `f04ec41bbc324cf71da239047358f4a9e220c19008817ef3558e7fbb004c62c4`.
- Scenario SHA-256: `c9e2062e36cc915cb49d289986974b6b52eb049b7507ba0ffdaa1e4030fe0ec3`.
- Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`.

Raw profiles, HTML reports, scripts, and captured outputs remain in the ignored
build tree: [before](../build/turn-benchmarks/20261005T013715Z-dhgy46ym/summary.md),
[change 1](../build/turn-benchmarks/20261005T024029Z-9kmx4szk/summary.md), and
[changes 1+2](../build/turn-benchmarks/20261005T024413Z-qjbv8oqd/summary.md).
[Comparison verification](../build/turn-benchmarks/20261005T024413Z-qjbv8oqd/stage-comparison.json)
is retained with the final run. Immutable ELF/source snapshots and native-test
logs are in `build/visibility-round-2/step-1/` and `step-2/`.
