# Compiled turn baseline (2026-10-04)

Measured one submitted input through player and monster actions, status updates,
intermediate/final rendering, and return to the main input wait. Generation
and animated actions are excluded. Scenarios are compiled in `bench.cpp`;
no save files or saved-layout fixtures are created.

Historical goal: **150 ms / 2,400,000 cycles**. **9 of 21 cases meet that goal; 12
exceed it.** Each case runs once (21 profiles total). Times below are exact
emulated input-to-ready latencies.

The current [documented limit](README.md) is 100 ms; the pass/over labels below
retain the threshold used for this historical run.

| Benchmark | Cycles | Elapsed ms | Historical 150 ms goal |
| --- | ---: | ---: | --- |
| move_room | 1,627,623 | 101.726 | PASS |
| move_corridor | 2,182,164 | 136.385 | PASS |
| move_map_edge | 586,920 | 36.682 | PASS |
| move_dense | 2,687,031 | 167.939 | OVER |
| wait | 1,471,637 | 91.977 | PASS |
| wait_dense | 2,757,739 | 172.359 | OVER |
| attack_hit | 1,565,736 | 97.859 | PASS |
| attack_miss | 1,653,306 | 103.332 | PASS |
| attack_kill | 1,552,163 | 97.010 | PASS |
| open_door | 2,266,958 | 141.685 | PASS |
| eat_food | 2,896,929 | 181.058 | OVER |
| drink_healing | 3,041,682 | 190.105 | OVER |
| equip_weapon | 2,931,274 | 183.205 | OVER |
| equip_armor | 2,916,261 | 182.266 | OVER |
| equip_ring | 2,975,300 | 185.956 | OVER |
| equip_cursed_amulet | 3,147,284 | 196.705 | OVER |
| scroll_mapping | 4,001,921 | 250.120 | OVER |
| scroll_teleport | 3,463,312 | 216.457 | OVER |
| drop_food | 2,971,300 | 185.706 | OVER |
| wand_digging | 4,473,939 | 279.621 | OVER |
| pickup_food | 1,763,430 | 110.214 | PASS |

The largest non-animated cases are digging (279.621 ms), mapping (250.120 ms),
and teleportation (216.457 ms). Discovering and equipping the unknown cursed
amulet takes 196.705 ms, including its warning and final view, without a
pagination acknowledgement. No gameplay optimizations were made for this run.

Build: `-g -std=c++17 -O2 -flto`, installed AVM SDK, Windows host, 16 MHz
emulated AVR clock. The benchmark links the gameplay objects with the normal
input-loop body and a compiled scenario startup; only the benchmark ELF adds
the one-byte case selector. Setup code is outside every measured interval.
The different startup can change LTO/code layout, so this compiled baseline
replaces the previous serialized-fixture baseline for comparisons.

Game/scenario source commit: `724b6c1ee894e46634835517632ef6ed0488a0e4`. The runner/reporting
changes are in this working tree.

Benchmark ELF SHA-256: `619e6bf520d1ccc520cd1a1786a2ab253c4e577db76b1effdfcdcd0df88bcbb0`.

Scenario source SHA-256: `c9e2062e36cc915cb49d289986974b6b52eb049b7507ba0ffdaa1e4030fe0ec3`.

Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`.

Raw results and per-case `.lldb`/`.avmp` files are retained in
[the local run directory](../build/turn-benchmarks/20261005T004609Z-1eijrlfk/summary.md).
This is an ignored local build artifact. See [README.md](README.md) to reproduce
the run or compare profiles.
