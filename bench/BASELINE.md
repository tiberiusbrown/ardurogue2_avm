# Initial turn baseline (2026-10-04)

Measured the complete response to one submitted input: player action, monster
turns, status updates, intermediate rendering, final rendering/display, and
return to the main input wait. Generation and animated actions are excluded.

Initial goal: **150 ms / 2,400,000 cycles**. **9 of 21 cases meet the goal; 12
exceed it.** Three fresh-emulator samples per case (63 profiles total) produced
identical cycle counts within each case. Times below are both median and worst.

| Benchmark | Cycles | Median/worst ms | 150 ms goal |
| --- | ---: | ---: | --- |
| move_room | 1,629,319 | 101.832 | PASS |
| move_corridor | 2,184,353 | 136.522 | PASS |
| move_map_edge | 590,955 | 36.935 | PASS |
| move_dense | 2,683,759 | 167.735 | OVER |
| wait | 1,471,661 | 91.979 | PASS |
| wait_dense | 2,757,763 | 172.360 | OVER |
| attack_hit | 1,564,594 | 97.787 | PASS |
| attack_miss | 1,652,097 | 103.256 | PASS |
| attack_kill | 1,551,022 | 96.939 | PASS |
| open_door | 2,278,556 | 142.410 | PASS |
| eat_food | 2,896,960 | 181.060 | OVER |
| drink_healing | 3,041,713 | 190.107 | OVER |
| equip_weapon | 2,931,305 | 183.207 | OVER |
| equip_armor | 2,916,292 | 182.268 | OVER |
| equip_ring | 2,975,331 | 185.958 | OVER |
| equip_cursed_amulet | 3,147,315 | 196.707 | OVER |
| scroll_mapping | 4,001,952 | 250.122 | OVER |
| scroll_teleport | 3,463,343 | 216.459 | OVER |
| drop_food | 2,971,331 | 185.708 | OVER |
| wand_digging | 4,473,983 | 279.624 | OVER |
| pickup_food | 1,763,476 | 110.217 | PASS |

The largest non-animated cases are digging (279.624 ms), mapping (250.122 ms),
and teleportation (216.459 ms). Equipment/consumable confirmations take roughly
181-190 ms; these include restoring the view after the item picker, applying the
item, advancing enemies, status work, and the final view. Dense movement/waiting
take 168-172 ms. No gameplay optimizations were made for these measurements.

Equipping an unknown cursed amulet of speed, revealing its type and curse,
and displaying the warning takes 196.707 ms. Both the type knowledge bit and
instance identification bit start clear and become set; the amulet remains
cursed and equipped. This response reaches the final main input wait with
one confirmation input and no pagination acknowledgement.

Build: production `-g -std=c++17 -O2 -flto`, installed AVM SDK, Windows host,
16 MHz emulated AVR clock. Saved `Game` layout is 821 bytes, save version 22.
Game source commit: `e5e6caf93c01c9cdd0b323452b4862671c1ebc04`.

ELF SHA-256: `1ddfb6a2147328c061d58aa372876100464f44f120511d46606940de4f94eee9`.

Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`.

Raw results and per-case `.lldb`/`.avmp` files are retained under
[the local run directory](../../../build/ardurogue2-perf-validation/turn-benchmarks/20261005T000349Z-whc1blms/summary.md).
The three added curse-discovery samples are in
[their local run directory](../../../build/ardurogue2-perf-validation/turn-benchmarks/20261005T001208Z-yj5s4m18/summary.md).
This path is a local build artifact; it is not included in the source patch.
See [README.md](README.md) to reproduce the run or compare profiles.
