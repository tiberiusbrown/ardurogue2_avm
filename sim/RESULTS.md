# Milestone 1 validation

This is the historical `omniscient-v1` baseline. The frozen current reference
and v1/v2 comparison are documented in [V2_RESULTS.md](V2_RESULTS.md).

Validated October 5, 2026 on Windows x64 with Visual Studio Clang 22.1.3,
Release, agent `omniscient-v1`. The pre-instrumentation repository revision was
`ed709dabc1a7ace3145aea8236ff177f0853753d`. No gameplay rules or save fields were
changed. See [README.md](README.md) for architecture, policy, hook sites and
metric definitions.

## Fixed batch

```sh
build/sim-clang/sim/ardurogue2_sim.exe --seeds 1:10000 --output build/sim-acceptance-final
```

| Measurement | Result |
| --- | ---: |
| Seeds | 1 through 10,000 inclusive |
| Escaped | 4,079 (40.79%) |
| Died through production mechanics | 5,921 |
| Simulator stuck/error | 0 |
| Reached floor 12 or deeper | 6,878 |
| Reached floor 15 | 5,019 |
| Lord kills / Yendor generated | 4,234 |
| Yendor acquired | 4,233 |
| Died after acquiring Yendor | 154 |
| Total actions | 35,102,391 |
| Maximum actions in a run | 6,688 |
| Native wall time, including four CSV streams | 213.477 seconds |

The final batch wrote [runs.csv](../build/sim-acceptance-final/runs.csv),
[floors.csv](../build/sim-acceptance-final/floors.csv),
[items.csv](../build/sim-acceptance-final/items.csv), and
[monsters.csv](../build/sim-acceptance-final/monsters.csv), with
[summary.json](../build/sim-acceptance-final/summary.json) and
[timing.json](../build/sim-acceptance-final/timing.json). These bulky generated
artifacts stay in the required ignored `build/` directory; commands here
regenerate them. All 10,000 floor action/turn totals reconcile with run totals.

Two complete 10,000-seed executions produced byte-identical run, floor and
item streams. A final additive fire-attack telemetry hook accounts for the
monster-stream difference between those executions. Full monster telemetry,
all other metrics, full traces and final `Game` state are also compared across
repeated complete seed-4 runs in automated tests.

Final CSV SHA-256:

```text
runs.csv     e177aa3c2c3b7f8a70c5a982d31ce5c0a565d1a01c7aa4bd85466bcc8a30db74
floors.csv   5d59d4cd5f5c5d8942526890f8278d272bd62dbd8953f0341724d81f525c43f1
items.csv    cd53067a5a49edfdef836f5ba4f32711d27eb3d3e5c8cc326e01593c5c49ddaa
monsters.csv 7e069c035a2453e0b81521fca9b12256f37f3904e11a09b33c11d60b9107665a
```

Representative telemetry totals: 97,875 food uses, 41,602 healing uses,
89,155 equip operations, 1,156,585 monster kills, 19,467,916 damage to monsters,
and 4,267,586 damage to the player. The one Lord drop not collected belongs
to a run ending before pickup; the simulator never fabricates Yendor possession.

Immediate death causes:

| Cause | Runs |
| --- | ---: |
| Snake | 1,700 |
| Dragon fire | 1,157 |
| Lord | 585 |
| Troll | 450 |
| Dragon melee | 358 |
| Phantom | 285 |
| Incubus | 248 |
| Orc | 179 |
| Goblin | 177 |
| Angel | 175 |
| Self-inflicted fire | 122 |
| Hobgoblin | 111 |
| Zombie | 109 |
| Rattlesnake | 88 |
| Griffin | 77 |
| Mimic | 50 |
| Bat | 30 |
| Tarantula | 20 |
| Starvation | 0 |

These are deterministic observations for this oracle and this build, not
estimates of normal-player difficulty.

## Complete successful replay

```sh
build/sim-clang/sim/ardurogue2_sim.exe --seed 4 --trace --output build/sim-escape-4-final
```

Full trace: [sim-escape-4-final.trace](../build/sim-escape-4-final.trace).
Seed 4 escapes with score 8,198, level 41, HP 138/138, 6,267 actions and 6,236
actual turns. It kills the Lord, picks up Yendor, visits every descent and
ascent depth, then calls production `take_stairs()` to escape from floor 0.
Its mechanical action digest is `3560380640128643736`.

Selected lines from the complete trace:

```text
seed=4 agent=omniscient-v1
A0 T0 F0 HP 18/18 LV 1 HUNGER 220 POS 60,10 goal=seek useful loot target=39,9 loot=DEXTERITY action=MOVE_SOUTH
...
A4719 T4704 F15 HP 99/117 LV 34 HUNGER 90 POS 3,5 goal=melee combat target=3,4 monster=LORD monster_hp=5 action=MOVE_NORTH
  damage LORD=5
  killed LORD
...
A4722 T4707 F15 HP 120/120 LV 35 HUNGER 199 POS 3,4 goal=collect supplies target=3,4 loot=YENDOR_AMULET action=TAKE ground=15 swap_slot=255
  picked up YENDOR_AMULET units=1
...
  exit floor=15
  enter floor=14 ascent=1
...
A6266 T6236 F0 HP 138/138 LV 41 HUNGER 126 POS 5,21 goal=ascend and escape action=STAIRS
  ended result=1
result=escaped actions=6267 turns=6236 score=8198 cause= reason= action_hash=3560380640128643736
```

## Representative death and safety traces

Seed 3 reaches the final floor and dies to an angel while out of immediate
healing resources. Full trace:
[sim-death-3-final.trace](../build/sim-death-3-final.trace).

```text
A4409 T4394 F15 HP 4/117 LV 34 HUNGER 174 POS 48,10 goal=melee combat target=47,10 monster=ANGEL monster_hp=24 action=MOVE_WEST
  damage ANGEL=7
  player damage=4
  ended result=0
result=death actions=4410 turns=4395 score=5145 cause=ANGEL reason= action_hash=12994853623315208504
```

Seed 5 dies in normal dragon melee on floor 13. Full trace:
[sim-death-5-final.trace](../build/sim-death-5-final.trace).

```text
A3352 T3339 F13 HP 4/102 LV 29 HUNGER 199 POS 34,28 goal=melee combat target=35,28 monster=DRAGON monster_hp=44 action=MOVE_EAST
  damage DRAGON=6
  player damage=4
  ended result=0
result=death actions=3353 turns=3340 score=4087 cause=DRAGON reason= action_hash=15422339418498229506
```

A deliberately low limit proves that a simulator failure stays distinct from
death:

```sh
build/sim-clang/sim/ardurogue2_sim.exe --seed 4 --trace --max-actions 5 --output build/sim-stuck-4-final
```

[sim-stuck-4-final.trace](../build/sim-stuck-4-final.trace) ends with:

```text
result=SIM_STUCK actions=5 turns=5 score=0 cause= reason=excessive action count action_hash=15896291321936151134
```

Automated constructed-state cases separately verify starvation as a genuine
death and path, rejection, inventory cycling and agent mutation/error diagnostics.

## Native and AVM validation

```sh
cmake -S . -B build/sim-clang -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build build/sim-clang --parallel 8
ctest --test-dir build/sim-clang --output-on-failure
```

**11/11 passed**: all nine existing correctness/generation/visibility/UI/render/
format/combat/benchmark-helper tests, plus simulator checks and CLI integration.
The fixed competence set 1..32 leaves floor 0 in 27 runs, reaches floor 12 in
24 runs, and escapes in 13 runs. Determinism comparisons include the entire
successful trace, every CSV metric, final state and RNG. Telemetry disabled and
trace disabled each preserve the successful result/action sequence.

Full CTest log: [sim-tests-verified.log](../build/sim-tests-verified.log).
The verified local executable is
`build/sim-clang/sim/ardurogue2_sim.exe` (SHA-256
`26d1d68a2b3f2c8733a31ede77bfcc51869b8bd79c4ebd24d7c9387c7203076a`).

The installed SDK path used here is
`C:/Users/Brown/Documents/GitHub/avm/build/avm-sdk-install`; substitute your SDK:

```sh
cmake -S . -B build/avm-ninja -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=<sdk>
cmake --build build/avm-ninja --target ardurogue2 ardurogue2_bench --parallel 8
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check --output build/sim-turn-benchmarks-verified
```

Both AVM targets build and package successfully. **30/30** complete input-to-render
benchmarks meet the **100 ms** requirement on the final ELF. Linker bounds remain
238 bytes for production and 236 for the benchmark, with zero stack-analysis
gaps. The ordinary ELF symbol table contains no simulator symbols. The packaged
`ardurogue2.arduboy` remains unchanged relative to the baseline.

Logs: [sim-avm-verified.log](../build/sim-avm-verified.log),
[sim-turn-benchmarks-verified.log](../build/sim-turn-benchmarks-verified.log).

## Proof of zero production cost

```sh
python sim/check_zero_cost.py --sdk-root <sdk> \
  --baseline ed709dabc1a7ace3145aea8236ff177f0853753d \
  --output build/sim-zero-cost-final
```

The script compiles each original source and its instrumented counterpart with
the same AVM target and `-O2`, without `ARDUROGUE2_SIM`. It compares all LLVM IR
instructions, globals, constants and attributes; only compilation-unit filename
lines are removed. Every source produces **identical optimized ordinary IR**:

| Source | Normalized IR SHA-256 |
| --- | --- |
| `state.cpp` | `06e26dcd4a6b505b67b87e7deba48349763b546990fb318fa04cd8b5ea2a82cd` |
| `combat.cpp` | `249f2d2a9fe67c8faac2f18bcbdaffbd35ab06550387decf419ad8a97996dc7c` |
| `items.cpp` | `c566378e6ed73ea231f58534a95e4cd82f9773b4aceb266790be6f0b09782c83` |
| `world_gen.cpp` | `c8b4720ac9c27e77c5d2ee1d9b83851f4979f35b22128b9a70a1ae906e22d0af` |

Proof output and both versions of every IR file are in
[sim-zero-cost-final](../build/sim-zero-cost-final/proof.txt).
The script also confirms `src/model.hpp` is unchanged. Existing native layout
checks still require 774 host bytes and AVM compilation still requires 773 saved
bytes with save version 23. There is no telemetry data in `Game` and no simulator
source is required by the production executable.

## Issues before the next milestone

Three policy pathologies found in substantial batches were repaired and are
covered by tests: corridor-route oscillation around moving monsters, re-equipping
a just-removed weaker ring, and repeatedly swapping capped food for a scroll.
The final 10,000 seeds exhibit none of these simulator failures or starvation.

Remaining policy limitations are visible in real deaths. The oracle's fixed
equipment preferences and limited emergency escape use are imperfect; it still
takes some fights with too little HP, and spreading fire can hurt the player
through a different ray than the selected target. The 122 self-fire deaths are
worth addressing in a future policy revision. Force, teleport, polymorph and
digging wands are expressible through dispatch but are not used by this policy.
Some rings/amulets/scrolls are collected more often than their current utility
warrants. No balance changes were made to compensate for these limitations.

Early snake deaths, dragon damage and the Lord account for much of this oracle's
failure rate; improving policy should precede conclusions about tuning those
mechanics. A normal-information observation layer must also preserve production
visibility/identification and exploration semantics, which this omniscient
milestone intentionally does not model. Death attribution describes the immediate
effect rather than a chain of earlier causes, and generated/encountered monster
types can differ after polymorph. None requires changing saved state.
