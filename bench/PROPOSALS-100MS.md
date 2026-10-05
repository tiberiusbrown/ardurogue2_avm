# Profiling proposals: 100 ms target (2026-10-04)

Follow-up: proposals 1 and 2 were approved and implemented separately. See
[their measured results](RESULTS-100MS.md). Proposal 3 remains a suggestion.
The analysis below records the pre-implementation measurements against the
then-temporary 100 ms target, now the documented limit and runner default.

**19 of 21 cases met 100 ms (1,600,000 emulated AVR cycles) before implementation.**
Dense movement needed 13.408 ms of savings; dense waiting needed 18.145 ms.
No gameplay, renderer, firmware, benchmark workload, or packaged-game changes
were made during that profiling round.

| Failing case | Total ms | Before final-render breakpoint ms | Remaining final render ms | Over 100 ms |
| --- | ---: | ---: | ---: | ---: |
| move_dense | 113.408 | 29.179 | 84.229 | 13.408 |
| wait_dense | 118.145 | 29.094 | 89.052 | 18.145 |

The split profiles use a named breakpoint at `render()` and together reconcile
exactly to the uninterrupted complete-turn cycle count. Final state, exploration,
and controller frames also match. Both failures spend about three quarters of
their time rendering. Native profiles show 99.82%/99.36% active interpreter time,
so input waits provide little opportunity to reach this target.

Source hotspots (times include inline work attributed to the enclosing function):

| Rendering work | move_dense ms | wait_dense ms |
| --- | ---: | ---: |
| Floor visibility, including ray tests | 37.208 | 40.213 |
| Ray tests alone, a subset of floor visibility | 19.914 | 22.595 |
| Wall visibility and exploration | 7.937 | 8.131 |
| Terrain preparation | 10.973 | 10.306 |
| Terrain drawing | 15.417 | 15.453 |

## 1. Share visibility ray prefixes and use fixed row/bit tests

Current rendering tests 113 lit targets separately. Each target interprets a
flash-encoded Bresenham path, even when many paths start with the same tiles.
The profiles show 216 blocker checks and 276 path-byte reads for movement;
waiting makes 245 checks and 310 reads. Reconstructing the existing paths and
reading the actual post-turn walls/doors finds only **61/66 distinct visited
prefixes**, respectively. The full radius-six path set has 96 distinct prefixes.
Those are repeated ordered paths, not merely repeated coordinates.

Compile the same paths into shared prefix branches with constant row indices
and bit masks, then produce the visible row masks. A blocked prefix can reject
all its descendants once. Fixed tests can also remove flash path-byte decoding,
variable column shifts, and repeated address construction. Preserve the existing
Bresenham tie rules, exclusion of the target from its own blocker test, room
shortcut, circular light mask, viewport clipping, and closed-door blockers.

The measured ray-test portion is 19.914/22.595 ms; that is the cost available to
reduce, **not a predicted saving**. Prefix sharing alone reduces the distinct
blocker evaluations by about 72-73%, but code layout, branch overhead, and mask
output still need measurement. Prefer generated/inlined fixed tests over a new
runtime table interpreter. Avoid runtime recursion; check flash growth and the
247/256-byte stack bound before accepting the result.

## 2. Apply visibility and exploration as row masks

`reveal_view_walls()` already calculates `nearby_walls` as a packed row, then
loops over every column to test each bit, OR it into `sight`, and call `explore`.
In these cases that 13-by-13 scan accounts for much of the 7.937/8.131 ms stage.
Its loop, per-column bit construction, and bit-test lines alone account for
4.530/4.526 ms. Per-cell exploration across the visibility phases adds
1.570/1.816 ms.

Clip the wall mask to valid viewport columns and OR it into `sight[sy]` once.
After floor and wall visibility are complete, merge each sight row into the
packed exploration map using the same clipped byte boundaries. This removes
per-cell coordinate/index work and repeated writes without caching across turns.
Keep the two visibility phases and their closed-door rules intact.

The figures identify affected work; new packed merge operations still cost time.
Verify all exploration bytes and frame hashes, including initially unexplored
maps and clipped corners. This complements proposal 1 and is the next priority.

## 3. Reuse adjacent terrain halo rows

`terrain_row()` extracts the previous, current, and next wall row for each of
13 screen rows: **39 wall-row extractions**, although there are only **15 distinct
halo rows**. Maintain a sliding three-row window and reuse the two overlapping
rows on each iteration. Continue reading exploration after visibility is complete.
This needs only a six-byte row window and preserves existing exposure and join
rules, including solid padding beyond map edges.

Wall-row extraction accounts for about 7.107/6.444 ms. Removing 24 of 39 repeated
extractions represents roughly **4.4/4.0 ms of gross work** at the observed average
cost; this is an estimate before window-maintenance overhead and code generation.
It is a smaller, simpler opportunity. Stack/global limits must still pass.

Recommended order: proposals 1 and 2 first, then 3 for further margin. No proposal
is yet proven to make both cases pass 100 ms. Approval should be followed by the
existing correctness suite, exact state/exploration/controller-frame comparisons,
linker memory checks, and the full 21-case suite with `--goal-ms 100 --check`.

## Complete run

| Benchmark | Cycles | Elapsed ms | Temporary 100 ms target |
| --- | ---: | ---: | --- |
| move_room | 1,056,758 | 66.047 | PASS |
| move_corridor | 1,190,753 | 74.422 | PASS |
| move_map_edge | 527,130 | 32.946 | PASS |
| move_dense | 1,814,522 | 113.408 | OVER |
| wait | 1,059,541 | 66.221 | PASS |
| wait_dense | 1,890,325 | 118.145 | OVER |
| attack_hit | 1,149,736 | 71.859 | PASS |
| attack_miss | 1,237,306 | 77.332 | PASS |
| attack_kill | 1,136,149 | 71.009 | PASS |
| open_door | 1,314,480 | 82.155 | PASS |
| eat_food | 1,106,893 | 69.181 | PASS |
| drink_healing | 1,252,247 | 78.265 | PASS |
| equip_weapon | 1,141,238 | 71.327 | PASS |
| equip_armor | 1,126,225 | 70.389 | PASS |
| equip_ring | 1,185,264 | 74.079 | PASS |
| equip_cursed_amulet | 1,357,248 | 84.828 | PASS |
| scroll_mapping | 1,426,929 | 89.183 | PASS |
| scroll_teleport | 1,408,933 | 88.058 | PASS |
| drop_food | 1,161,566 | 72.598 | PASS |
| wand_digging | 1,468,131 | 91.758 | PASS |
| pickup_food | 1,196,463 | 74.779 | PASS |

The full rerun has the exact same 21 cycle counts, initial state hashes,
named before/after snapshots, complete exploration arrays, controller frames,
ELF identity, and firmware identity as the preceding optimized run. The two
native reruns also match their complete-turn cycles and captured outputs.
All windows are complete and contain final rendering; setup, generation,
animation, and pagination acknowledgement remain outside these workloads.

Reproduce from the project directory:

```text
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --goal-ms 100 --check --html --output build/turn-benchmarks
```

The runner returns 2 for the two over-target cases; these are valid measurements,
not execution/correctness failures. The full run and both native reruns retain
source/instruction profiles and HTML reports. For native profiling, use an
interpreter ELF matching the installed firmware as described in [README.md](README.md).

Benchmark ELF SHA-256: `45f3e639b6c9bbafa5fb714b126dfc8cbd754ec65910fd3b2f268ea0f9eb1f7e`.

Scenario source SHA-256: `c9e2062e36cc915cb49d289986974b6b52eb049b7507ba0ffdaa1e4030fe0ec3`.

Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`.


Ignored local artifacts:
[full run](../build/turn-benchmarks/20261005T013715Z-dhgy46ym/summary.md),
[native failures](../build/turn-benchmarks/20261005T013821Z-akbxoccp/summary.md),
[split timings](../build/turn-benchmarks/20261005T013715Z-dhgy46ym/stage-summary.json),
[hotspot/prefix analysis](../build/turn-benchmarks/20261005T013715Z-dhgy46ym/hotspot-analysis.json),
and [comparison verification](../build/turn-benchmarks/20261005T013715Z-dhgy46ym/verification.json).
