# Native balancing simulator, milestone 1

`ardurogue2_sim` runs the production rules directly, from `start_new(seed)` to
`session.ended`. It has no AVM, input handling, renderer, framebuffer, timers,
sleep or animation dependencies. All generated data and save state remain in
the production `Game`; telemetry lives exclusively in host simulator objects.

## Build and run

A C++17 host compiler and CMake 3.20 or newer suffice. No AVM SDK is needed.
Configure from the game repository root, rather than the enclosing SDK tree:

```sh
cmake -S . -B build/sim-native -DCMAKE_BUILD_TYPE=Release
cmake --build build/sim-native --config Release --parallel
ctest --test-dir build/sim-native -C Release --output-on-failure
```

The executable is under `build/sim-native/sim/` (and the configuration
subdirectory for a multi-configuration generator). On Windows this project
also works with Clang supplied by Visual Studio, from a developer command prompt:

```sh
cmake -S . -B build/sim-clang -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build build/sim-clang --parallel
ctest --test-dir build/sim-clang --output-on-failure
```

Examples below use the single-configuration executable path; append `.exe`
on Windows:

```sh
build/sim-native/sim/ardurogue2_sim --seed 4
build/sim-native/sim/ardurogue2_sim --seed 0x4 --trace --output build/escape-4
build/sim-native/sim/ardurogue2_sim --seeds 1:10000 --output build/balance
build/sim-native/sim/ardurogue2_sim --count 10000 --start-seed 1 --output build/balance
build/sim-native/sim/ardurogue2_sim --seed 4 --max-actions 5 --trace
```

Ranges are inclusive. Seeds must fit the production 16-bit API; batches never
wrap. Production maps seed zero to `0xace1`, so CSV records both the requested
`seed` and `effective_seed`. Without `--output`, stdout contains `runs.csv`.
With `--output`, the directory receives all four CSV files. Trace is restricted
to one seed and goes to stderr, keeping CSV parseable. `--no-telemetry` disables
aggregate collection for isolation diagnostics; run/floor action and turn
counts, safety checks and tracing continue to work.

Batch execution is serial and resets both the agent and cross-run best-score
history. A failed simulation yields its own `SIM_STUCK` or `SIM_ERROR` row and
the batch continues. These results are never counted as deaths. Invalid CLI
arguments and output failures return a nonzero process exit code; completed
batches return zero even when an individual seed hits a simulator limit.

## Architecture and files

| File | Responsibility |
| --- | --- |
| `sim/CMakeLists.txt` | Separate host library/executable and simulator tests; defines only `ARDUROGUE2_SIM` on simulator targets |
| `sim/main.cpp` | Seed selection, output files, compact batch summary |
| `sim/agent.hpp` | `Agent`, read-only decision context, mechanical `Action`, typed diagnostics, BFS interface |
| `sim/omniscient_agent.cpp` | Deterministic policy and cardinal BFS |
| `sim/simulator.hpp`, `sim/simulator.cpp` | Production API dispatch, lifecycle, safety and action digest |
| `sim/frontend.cpp` | Native `Game` definition; no-op status and animation frontend |
| `sim/metrics.hpp`, `sim/metrics.cpp` | Host collectors, death attribution and four CSV writers |
| `sim/trace.hpp`, `sim/trace.cpp` | Human-readable action and gameplay event trace |
| `sim/tests.cpp` | Determinism, RNG isolation, mechanics dispatch, policy, telemetry, safety, competence |
| `sim/test_cli.py` | Executable/CSV/seed-range integration tests |
| `sim/check_zero_cost.py` | Compare ordinary optimized AVM IR with the pre-hook revision |
| `src/sim_hooks.hpp` | Entirely gated event declarations, attribution scopes, zero-cost macro |

Existing files changed: root `README.md` links these instructions; root
`CMakeLists.txt` adds the host-only subdirectory
and the production hook-header build dependency; `src/state.cpp`,
`src/combat.cpp`, `src/items.cpp`, `src/world_gen.cpp` gain gated hooks.
`src/model.hpp`, persistence code, save version and saved fields are unchanged.

The simulator library compiles and links `state.cpp`, `combat.cpp`,
`combat_math.cpp`, `items.cpp`, `world.cpp`, `world_gen.cpp` and
`world_gen_population.cpp`. It does not link `main.cpp`, `ui.cpp`, `render.cpp`,
`status.cpp` or persistence. Normal SDK configurations never enter `sim/`.

The runner owns execution, counters and output. Agents return choices and never
receive a metrics or trace object. `Action` supports cardinal movement, waiting,
pickup with an optional full-pack replacement, explicit ground swap, stairs,
inventory use with an optional target slot, potion throwing, directional or
immediate wand use, and dropping with the production discard-confirmation flag.
Dispatch validates cardinal moves and current-tile pickups, then calls only
the production APIs. Waiting calls `end_turn()` directly; `action()` is a UI
repeat-equipment operation and would be inappropriate for waiting.

Future policies can implement `Agent` without modifying the runner or CSV
collectors. The current context exposes the full read-only world for the
omniscient policy; a future restricted observation layer can sit at that
interface. No normal-information agent or experimental management is included.

## Omniscient policy

The policy uses fixed integer preferences and deterministic array/direction
tie breaking. It never reads future RNG outcomes, calls `roll()` or advances
`next_random()`. The runner checks the complete game state before and after
every decision and reports `SIM_ERROR` on any mutation, including RNG changes.

Priorities are paralysis recovery; healing/strength recovery and food;
equipment upgrades; experience potions; ranged control/damage; adjacent melee;
permanent stat potions and targeted enchantment; useful loot; nearby early
combat experience; descent or the Lord; Yendor pickup; ascent and escape.

Equipment preferences use production weapon/armor definitions plus separate
instance enchantment. Cursed equipment is avoided. Rings replace the weaker
removable ring through two ordinary `use_inventory()` actions, always choosing
the strongest available candidate. Speed amulets and protection rings are
preferred enchantment targets, followed by armor and weapons. Food retention
has its own score so the acquisition cap cannot cause repeated swaps.

Healing is used at lower HP thresholds, experience can restore HP through
production leveling, and food is eaten before hunger becomes dangerous.
Paralysis, weakness, confusion and slowing potions control dangerous monsters;
striking/ice/fire wands and harming potions deal ranged damage. Fire is avoided
against dragons and when the target burst would catch the player without
immunity. Useful visible mass scrolls and emergency fear/teleport are supported.
The agent deliberately collects supplies rather than racing downstairs.

BFS plans over the full map with closed doors treated as traversable estimates;
production movement opens them and consumes the real turn. BFS also supports
monster-blocking routes with monsters as reachable endpoints. The oracle's
normal routes permit occupied cells and resolve blockers through real combat;
this prevents moving monsters from repeatedly changing corridor choices.
Paths are recomputed each decision. Mimics always count as monsters.

Pickup first calls `take_item()`. Only `PICKUP_NEEDS_SWAP` triggers the specified
lowest-value removable slot through `swap_ground_item()`. Inventory arrays and
equipment slots are never written by the agent. Once Yendor is acquired, loot
seeking stops and navigation targets upward stairs on each floor.

No gameplay code reads the renderer's exploration bits to decide these effects.
Scroll visibility already uses production `can_see()` and monster visibility,
which work without frames. The oracle therefore leaves `game.explored` to
production mechanics such as mapping/digging; it does not fabricate exploration.

## Telemetry and instrumentation

All CSV rows include `seed` and `agent`. Floors additionally include a sequential
visit number and descent/ascent direction. A normal successful run has 31
visits: floors 0..15 down, then 14..0 up. Floor 15's Yendor acquisition and
initial return to its up stairs occur within the original final-floor visit.

* `runs.csv`: result, 64-bit actions/actual turns, score, deepest/final floor,
  level, final/max HP, Yendor, entered/exited floors, safety reason, immediate
  death cause and a stable FNV-1a digest of mechanical actions.
* `floors.csv`: entry/exit HP and level, actions/turns, kills, actual damage
  taken/dealt, pickups, consumable activations and whether the visit ended by
  stairs/escape. The final death/stuck visit is retained with `exited=0`.
* `items.csv`: every item type, generated/reached/picked-up units, successful
  uses, actual consumed units, equip operations, dropped/discarded units, final
  carried units, wand charges, potions drunk/thrown, scrolls read and equipped
  turns. Stacks count physical units; equipment and wands count individual
  objects. Potion conservation can make uses exceed consumed units. Wand
  consumption records the object crumbling, separately from charges spent.
* `monsters.csv`: every monster type, generation, encountered/engaged instances,
  kills, player attacks, actual damage to monsters, enemy attacks/hits, actual
  player damage, direct deaths, poison/confusion/paralysis and fire applications.

An item is reached when the player stands on its tile. A monster is encountered
when within production geometric sight, even if naturally invisible to a
normal player; attacks also establish encounters/engagement. Entity flags reset
on fresh floor generation and on ground-slot reuse/polymorph. Pickups/drops
record transactions; an object legitimately dropped and picked up again can
appear more than once. Generated supply counts include each fresh floor's
population and the Lord's Yendor drop, never player drops. Ascent generates
fresh monsters through production generation and no ordinary supplies.

Damage is capped to the recipient's HP before each effect; overkill is excluded.
Player attacks include melee and targeted potion/scroll/wand effects, including
each monster affected by a fire burst. Monster kills include environmental
kills and never manufacture XP credit. Polymorph creates an encounter with the
new form but does not count as generation of a new physical monster.

The production hook sites are deliberately mechanical:

| Source | Hook sites |
| --- | --- |
| `state.cpp` | Run finish and floor exit before generation changes the current floor |
| `world_gen.cpp` | Floor entry after population; host collector scans that population once |
| `combat.cpp` | Actual turn start; player damage; melee/other monster damage and defeat; Lord drop; player/enemy attacks and hits; enemy poison/confusion/paralysis/fire; starvation; scoped monster/fire/cursed-vampire attribution |
| `items.cpp` | Successful pickup/swap/drop/discard; item use/equip; actual potion/food/scroll consumption; thrown potion; wand charge/crumbling; scroll/ray monster attacks, torment damage, polymorph; self-inflicted item damage attribution |

`SIM_EVENT(...)` expands to `((void)0)` without `ARDUROGUE2_SIM`. Its arguments
are not evaluated. Extra local variables, attribution scopes and conditional
hook-only work are themselves enclosed in `#if defined(ARDUROGUE2_SIM)`.
There is no runtime observer or simulator branch in ordinary gameplay output.
The only active collector and attribution state live in `sim/metrics.cpp`.

## Deterministic replay and safety

Tracing records action/turn numbers, floor, HP, level, hunger, position, goal,
target, item/monster identities, choices, significant effects, transitions and
the final outcome. It does not influence policy or RNG. A replay is the same
executable, agent version and seed, with `--trace` enabled. Save the executable
or its hash with results when comparing different revisions; the action digest
is a compact check, not an independently executable saved-action recording.

Default limits: 20,000 actions, 32 consecutive rejected actions, 32 consecutive
identical game states, 32 consecutive path failures, and eight repeated reverse
inventory swaps. Policies throwing a standard exception or mutating production
state yield `SIM_ERROR`. Starvation and combat finish through the production
death path and remain `death`. Limit termination never calls `finish(DEATH)`.

## Verification

See [RESULTS.md](RESULTS.md) for the fixed batch, reproducible traces, compiler
proof and commands/results from this implementation. CTest verifies:

* identical complete metrics, full trace, final state and action digest on repeat;
* trace on/off and telemetry on/off preserving all gameplay and RNG;
* seed 4 defeating the Lord, picking up Yendor and escaping all 31 floor visits;
* fixed seeds 1..32 leaving floor 0, reaching deep floors, killing monsters and
  using healing, food and equipment;
* doors/occupancy, current-tile pickup, full-pack swap and cursed removability;
* ring and food swap loop regressions, overkill accounting and death causes;
* action/path/rejection/inventory safety and RNG-mutation/agent-error detection;
* CLI validation, both batch syntaxes and byte-identical four-stream CSV output.

Existing correctness, generation snapshot/stream-isolation, visibility, UI,
rendering, item formatting and combat distribution tests remain enabled.
