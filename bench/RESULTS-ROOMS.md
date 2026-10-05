# Room metadata removal results (2026-10-04)

Room descriptors now exist only in a 48-byte local array during floor generation.
The renderer clips shared line-of-sight masks without scanning rooms or adding
a room rectangle. Generated rooms are open rectangles with doors outside them,
so this preserves their visibility. The generated terrain and RNG sequence are unchanged.

All **21/21 cases pass the default 100 ms limit**. The slowest case is
`wait_dense` at **75.340 ms**, leaving **24.660 ms** of headroom.

## Memory and saves

- Production `.saved`: **821 -> 773 bytes**; `.data` remains **102 bytes**.
  Total permanent RAM: **923 -> 875 of 1,024 bytes**, leaving 149 bytes free.
- Production maximum stack: **238/256 bytes**, a complete bound with zero gaps.
  The temporary generation array fits within this bound. Benchmark stack remains 236 bytes.
- Production `.text`: **49,743 -> 49,533 bytes**; `.rodata` remains 5,896 bytes.
  The rebuilt root `ardurogue2.arduboy` is 140,788 bytes.
- Save version is **23**. Version 22 and older saves are incompatible; no migration
  is attempted. Layout assertions and native save round-trip/rejection checks pass.

## Correctness and profiles

- All seven native CTest entries pass. New generation snapshots match pre-change
  terrain, exploration, doors, enemies, items, stairs, and player placement for
  four seeds across all 16 floors, descending and ascending (128 generated floors).
  Floor generation still leaves combat randomness untouched.
- All eleven existing rendering goldens and pagination checks pass unchanged.
  The 552 terrain oracle cases use actual open room geometry rather than metadata
  that overrides walls. Mapping now checks every floor tile.
- All 21 benchmark before/after named states, complete exploration arrays, and
  controller frames exactly match the previous build. Only the unused room
  assignments were removed from scenario setup; map geometry and actions are unchanged.
- Full-view floor visibility drops from 1.505-1.506 ms in the sampled room cases
  and 1.792 ms in dense cases to **1.111 ms**. Small complete-turn increases below
  come from the initial `sys idle` interrupt wait, whose phase changes after the
  faster setup render. For example, `move_room` saves 6,319 visibility cycles but
  waits 6,724 extra idle cycles. These are deterministic elapsed-time measurements.

## Complete turns

| Benchmark | Before ms | After ms | Saving ms |
| --- | ---: | ---: | ---: |
| move_room | 40.548 | 40.575 | -0.027 |
| move_corridor | 40.990 | 41.002 | -0.012 |
| move_map_edge | 24.908 | 24.933 | -0.025 |
| move_dense | 74.638 | 73.650 | 0.988 |
| wait | 41.551 | 41.157 | 0.393 |
| wait_dense | 76.022 | 75.340 | 0.681 |
| attack_hit | 46.858 | 46.885 | -0.027 |
| attack_miss | 52.333 | 52.358 | -0.025 |
| attack_kill | 46.009 | 46.036 | -0.027 |
| open_door | 45.522 | 44.536 | 0.986 |
| eat_food | 44.510 | 44.117 | 0.393 |
| drink_healing | 53.595 | 53.202 | 0.393 |
| equip_weapon | 46.657 | 46.264 | 0.393 |
| equip_armor | 45.719 | 45.325 | 0.393 |
| equip_ring | 49.409 | 49.015 | 0.393 |
| equip_cursed_amulet | 60.159 | 59.764 | 0.395 |
| scroll_mapping | 64.513 | 64.119 | 0.393 |
| scroll_teleport | 57.655 | 57.262 | 0.393 |
| drop_food | 47.929 | 47.534 | 0.395 |
| wand_digging | 55.221 | 54.540 | 0.681 |
| pickup_food | 49.609 | 49.216 | 0.393 |

## Reproduction and artifacts

```text
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
cmake --build build/native --config RelWithDebInfo
ctest --test-dir build/native -C RelWithDebInfo --output-on-failure
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check --html --output build/turn-benchmarks
```

[Before](../build/turn-benchmarks/20261005T030121Z-_hrj_42j/summary.md), [after](../build/turn-benchmarks/20261005T040720Z-a87plmst/summary.md),
and [output comparisons](../build/turn-benchmarks/20261005T040720Z-a87plmst/room-comparison.json) remain in the ignored build tree.
Pre-change ELF copies and the SDK stack report are retained in `build/room-removal/`.

Benchmark ELF SHA-256: `1dae2b3f126cd7adf70ded8eeedb7ff96eb4bbb4230edaaafd846670ba90277c`.

Scenario SHA-256: `d559991d52e7c73c150a6b7de2870e0ef541273ecc3fb6ee0c0f3c009e6c583d`.
