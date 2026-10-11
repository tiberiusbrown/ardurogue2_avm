# Native balancing simulator

For manifests, adjusted visit factors, paired A/B experiments and controlled
content interventions, see [BALANCE.md](BALANCE.md). Demonstration results are in
[BALANCE_RESULTS.md](BALANCE_RESULTS.md).

The current explorer invisible-contact state machine, observation protocol 3,
schema-5 diagnostics and descriptive policy comparisons are documented in
[EXPLORER_INVISIBLE_CONTACT.md](../docs/EXPLORER_INVISIBLE_CONTACT.md).
`sim/invisible_contact_experiments.py` snapshots the executable and complete
parameters, runs legacy/disabled/contact variants on identical seeds, seals
raw CSVs, rejects simulator failures and retains paired traces under ignored
`build/` directories. Use a fresh output directory:

```sh
python sim/invisible_contact_experiments.py --simulator build/sim-native/sim/ardurogue2_sim --seeds 1:1000 --jobs 8 --traces --output build/invisible-contact/exploratory
python sim/invisible_contact_experiments.py --simulator build/sim-native/sim/ardurogue2_sim --seeds 10001:20000 --jobs 8 --traces --output build/invisible-contact/validation
```

These comparisons have distinct policy hashes and fixed production mechanics;
they are not same-policy causal gameplay interventions.

`ardurogue2_sim` runs the production rules directly, from `start_new(seed)` to
`session.ended`. It has no AVM, input handling, renderer, framebuffer, timers,
sleep or animation dependencies. All generated data and save state remain in
the production `Game`; telemetry lives exclusively in host simulator objects.

## Build and run

A C++17 host compiler and CMake 3.20 or newer suffice. No AVM SDK is needed.
Configure from the game repository root, rather than the enclosing SDK tree:

```sh
cmake -S . -B build/sim-native -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/sim-native --config RelWithDebInfo --parallel
ctest --test-dir build/sim-native -C RelWithDebInfo --output-on-failure
```

The executable is under `build/sim-native/sim/` (and the configuration
subdirectory for a multi-configuration generator). On Windows this project
also works with Clang supplied by Visual Studio, from a developer command prompt:

```sh
cmake -S . -B build/sim-clang -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/sim-clang --parallel
ctest --test-dir build/sim-clang --output-on-failure
```

Examples below use the single-configuration executable path; append `.exe`
on Windows:

```sh
build/sim-native/sim/ardurogue2_sim --seed 4
build/sim-native/sim/ardurogue2_sim --seed 0x4 --trace --output build/escape-4
build/sim-native/sim/ardurogue2_sim --seeds 1:10000 --output build/balance
build/sim-native/sim/ardurogue2_sim --seeds 1:10000 --jobs 8 --output build/balance
build/sim-native/sim/ardurogue2_sim --count 10000 --start-seed 1 --output build/balance
build/sim-native/sim/ardurogue2_sim --seed 4 --max-actions 5 --trace
```

Ranges are inclusive. Seeds must fit the production 16-bit API; batches never
wrap. Production maps seed zero to `0xace1`, so CSV records both the requested
`seed` and `effective_seed`. Without `--output`, stdout contains `runs.csv`.
With `--output`, the directory receives nine core CSV files, `exploration.csv`
and `manifest.json` (see
`BALANCE.md` for schemas). Trace is restricted
to one seed and goes to stderr, keeping CSV parseable. `--no-telemetry` disables
aggregate collection for isolation diagnostics; run/floor action and turn
counts, safety checks and tracing continue to work.

`--jobs N` runs a batch in 1..64 separate worker processes (default 1), capped
at the number of seeds. Production `Game` and `session` are global, so processes
isolate them and each seed's RNG. Contiguous seed chunks merge in ascending
seed order; all nine CSVs are byte-identical to serial execution. Workers use
temporary directories, which the parent removes after joining them. Only the
parent writes final output. Tracing still requires one seed and runs serially.
Windows workers start without visible console windows; POSIX uses `posix_spawnp`.

Each seed resets both the agent and cross-run best-score history. A failed
simulation yields its own `SIM_STUCK` or `SIM_ERROR` row and
the batch continues. These results are never counted as deaths. Invalid CLI
arguments and output failures return a nonzero process exit code; completed
batches return zero even when an individual seed hits a simulator limit.

## Architecture and files

| File | Responsibility |
| --- | --- |
| `sim/CMakeLists.txt` | Separate host library/executable and simulator tests; defines only `ARDUROGUE2_SIM` on simulator targets |
| `sim/main.cpp` | Seed selection, output files, compact batch summary |
| `sim/parallel.hpp`, `sim/parallel.cpp` | Isolated batch workers and deterministic CSV merging |
| `sim/agent.hpp` | `Agent`, read-only decision context, mechanical `Action`, typed diagnostics, BFS interface |
| `sim/omniscient_agent.cpp` | Deterministic policy and cardinal BFS |
| `sim/simulator.hpp`, `sim/simulator.cpp` | Production API dispatch, lifecycle, safety and action digest |
| `sim/frontend.cpp` | Native `Game` definition; no-op status and animation frontend |
| `sim/metrics.hpp`, `sim/metrics.cpp` | Host collectors, death attribution, typed visits and table-driven CSV writers |
| `sim/trace.hpp`, `sim/trace.cpp` | Human-readable action and gameplay event trace |
| `sim/tests.cpp` | Determinism, RNG isolation, mechanics dispatch, policy, telemetry, safety, competence |
| `sim/policy_checks.cpp` | Constructed fire, tactical wand, retreat and inventory cases |
| `sim/test_cli.py` | Executable/CSV/seed-range integration tests |
| `sim/check_zero_cost.py` | Compare ordinary optimized AVM IR with the pre-hook revision |
| `src/sim_hooks.hpp` | Entirely gated event declarations, attribution scopes, zero-cost macro |

Existing files changed: root `README.md` links these instructions; root
`CMakeLists.txt` adds the host-only subdirectory
and the production hook-header build dependency; `src/state.cpp`,
`src/combat.cpp`, `src/items.cpp`, `src/world_gen.cpp` gain gated hooks.
Instrumentation adds no saved fields. The map-feature gameplay addition appends
36 bytes to `Game` and uses save version 26; see the feature report below.

The simulator library compiles and links `state.cpp`, `combat.cpp`,
`combat_math.cpp`, `items.cpp`, `world.cpp`, `world_gen.cpp` and
`world_gen_population.cpp` and `features.cpp`. It does not link `main.cpp`, `ui.cpp`, `render.cpp`,
`status.cpp` or persistence. Normal SDK configurations never enter `sim/`.

The runner owns execution, counters and output. Agents return choices and never
receive a metrics or trace object. `Action` supports cardinal movement, waiting,
pickup with an optional full-pack replacement, explicit ground swap, stairs,
inventory use with an optional target slot, potion throwing, directional or
immediate wand use, and dropping with the production discard-confirmation flag.
Dispatch validates cardinal moves and current-tile pickups, then calls only
the production APIs. Waiting calls `wait_search()`; `action()` is a UI
repeat-equipment operation and would be inappropriate for waiting.

Future policies can implement `Agent` without modifying the runner or CSV
collectors. The current context exposes the read-only world with hidden map features
erased, including both their IDs and positions. The feature policy uses the
same filtered `observed_features` interface; future policies must retain this
guarantee. The separate `explorer-v1` uses a sanitized observation protocol,
retained memory and frontier routes, rather than this context. See
[EXPLORER_AGENT.md](../docs/EXPLORER_AGENT.md) for the exact boundary, tests and
measurements. It leaves the default oracle and its policy hash unchanged.

## Explorer runtime preferences and automatic tuning

Use `--agent explorer-v1`; omitting `--agent` still selects `omniscient-v2`.
`--agent-params FILE` loads a strict JSON object of runtime explorer weights and
thresholds. Partial files fill omitted values from defaults. Write all defaults
with `--write-agent-defaults FILE`. Workers forward the full resolved values,
and manifests contain those values, their SHA-256 and a combined code/parameter
policy hash. Paired balance runs must hold this configuration constant.

```sh
build/sim-native/sim/ardurogue2_sim --agent explorer-v1 --seeds 1:1000 --jobs 8 --output build/explorer
build/sim-native/sim/ardurogue2_sim --write-agent-defaults build/explorer-defaults.json
build/sim-native/sim/ardurogue2_explorer_tune --seeds 1:1000 --initial-seeds 100 --plateau-generations 3 --population 12 --generations 20 --jobs 8 --output build/explorer-tune
build/sim-native/sim/ardurogue2_sim --agent explorer-v1 --agent-params build/explorer-tune/best.json --seeds 1001:10000 --jobs 8 --output build/explorer-validation
```

The separate tuner evolves preferences with seeded mutation/crossover and
retained elites. Valid batches rank by escapes first, then total score. Each
stage uses an equal seed range for all candidates. Staged searches expand
on plateaus, re-evaluate leaders, and validate finalists on the complete range.
`history.csv`, `best-result.json` and per-candidate configurations/fitness CSVs
make the selection reviewable. `--minutes` limits search between candidates;
final full-range validation can extend that limit by up to four batches.
The tuner does not edit source defaults; promote a reviewed winning configuration
and validate it separately. All search artifacts belong under ignored `build/`.

## Maintained omniscient-v2 policy

`omniscient-v2` is the maintained omniscient balance policy. New production
mechanics may extend it in place. CMake automatically hashes `agent.hpp`, `features.hpp` and
`omniscient_agent.cpp` with SHA-256, combines those hashes and records the
result as `agent_policy_hash` in every manifest. Incremental builds refresh it.
Content comparisons require matching policy hashes; the Python paired framework
rejects mismatches or missing hashes. Establish a new reference after a policy
extension, including a no-new-content compatibility control. Historical
[V2_RESULTS.md](V2_RESULTS.md) describes the earlier implementation.

The policy uses fixed integer preferences and deterministic array/direction
tie breaking. It never reads future RNG outcomes, calls `roll()` or advances
`next_random()`. The runner checks the complete game state before and after
every decision and reports `SIM_ERROR` on any mutation, including RNG changes.

Priorities are paralysis recovery; healing/strength recovery; emergency
experience recovery, hard control, fear, invisibility, player teleport, enemy
teleport/force/polymorph and bounded retreat; food;
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
against dragons and whenever any burst would catch the player without fire
immunity, including non-target rays of spreading/overpowered wands. Planning
uses production `scan_ray`, modifier predicates and shared square/radius helpers.
Afflicted wands are rejected, including unreliable directions.
Useful visible mass scrolls and emergency fear/teleport are supported.
The agent deliberately collects supplies rather than racing downstairs.

Emergency danger is a small integer heuristic using current HP, armor, monster
strength/speed/special attacks, player speed/status/immunities and uncontrolled
hostile pressure within two tiles. Healthy trivial encounters retain ordinary
combat. Teleport scores the actual current ray/area group; force checks the
production push path for useful distance or collision stun. Polymorph is a
last-resort severe-emergency tool: late types only, Lord excluded, and no
nearly defeated or trivial monster in the affected group. Production polymorph
moves one type up/down, heals the new form and clears statuses; the policy
never predicts that random outcome.

Retreat considers empty, passable, open cardinal cells, current threat distance,
connectivity and progress toward the stairs. It avoids dead ends and immediate
reversal, and permits at most three retreats per tracked hostile group. That budget
persists through temporarily safe steps, distance changes and lost sight. It
resets when the tracked hostiles are defeated/no longer hostile or the floor
changes. Monsters still act after production movement, so a scored
step is not a guarantee of safety. With no useful alternative, melee proceeds.

`omniscient-v2 intentionally assigns WAND_DIGGING no tactical value`.
Digging carves/explores six cells per ray, opens doors and uses three lanes when
powerful; it does not move the player or control monsters. This oracle does not
spend emergency turns on speculative shortcuts. Its acquisition value is zero.
Force/teleport/polymorph acquisition and retention values now reflect their
tactical support. Remove-curse scrolls also have zero value because this oracle
avoids acquiring afflicted equipment/wands. Equipment scoring otherwise remains
unchanged.

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
  turns, plus distinct `wands_picked_up` and `wands_activated`. Stacks count physical units; equipment and wands count individual
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
Distinct wand counts use host-only identities preserved across swap/drop/repick
transactions and inventory/floor slot reuse. Thus activation/generated and
activation/picked-up percentages describe physical objects, rather than charges
or repeat pickups. The agent never sees these identities or counts.

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

See [../docs/BOW_BALANCE_2026-10-06.md](../docs/BOW_BALANCE_2026-10-06.md) for the current bow-aware census, and [V2_RESULTS.md](V2_RESULTS.md) for the historical fixed batch and trace audit,
and [RESULTS.md](RESULTS.md) for the historical v1 validation. CTest verifies:

* identical complete metrics, full trace, final state and action digest on repeat;
* trace on/off and telemetry on/off preserving all gameplay and RNG;
* seed 4 defeating the Lord, picking up Yendor and escaping all 31 floor visits;
* fixed seeds 1..32 leaving floor 0, reaching deep floors, killing monsters and
  using healing, food and equipment;
* doors/occupancy, current-tile pickup, full-pack swap and cursed removability;
* ring and food swap loop regressions, overkill accounting and death causes;
* action/path/rejection/inventory safety and RNG-mutation/agent-error detection;
* CLI validation, both batch syntaxes and byte-identical nine-stream CSV output.
* all fire modifiers, immunity and exhaustive byte-coordinate square coverage;
* tactical wand emergencies, modifier groups, afflicted/trivial declines and
  polymorph decisions independent of RNG state;
* safer production retreat, bounded encounters across danger thresholds,
  reasonable melee, tactical inventory retention and distinct wand identities;
* serial/parallel byte equality, seed zero, uneven chunks, excess workers,
  tracing, telemetry-disabled output and simulator-limit propagation.

Existing correctness, generation snapshot/stream-isolation, visibility, UI,
rendering, item formatting and combat distribution tests remain enabled.


## Bow policy and telemetry

The policy retains one best melee weapon, one useful uncursed bow and a capped
arrow supply (target 24 units, lower acquisition value without a bow). Ammo uses
the shared 255-unit capacity. It scores visible hostile cardinal opportunities
through production `scan_ray`, accounting for armor, hit likelihood, threat,
target HP, supply and both equipment turns. It requires enough approach time
before equipping, restores melee for close fights and uses a four-decision
cooldown after restoring melee. No deliberate Bat shots, future RNG inspection,
free switching or additional kiting path is provided. Naturally/temporarily
invisible targets require production See Invisible detection and line of sight.

ActionKind::Throw dispatches production `throw_or_shoot` for both potions and
arrows. Trace labels distinguish throw potion, shoot arrow and throw arrow.
Schema 3 adds `ranged.csv`: uint64 host counters for bundle/unit generation,
pickup/firing/throwing/carried units, bow occupancy/switches, shots/hits/effective
damage/kills by bow, distance and target. Misses are shots minus hits. Distance
zero represents an immediate blocker. Per-floor arrow bundles/units/pickups and
bow ownership provide descent drought analysis. First-found sentinel 255 means
never. Gaps include reached terminal death floors and exclude ascent (no supply
generation). Acquisition gaps while owning any bow may include deliberately
stocked/cursed bows and are descriptive, not proof of exhausted ammunition.

All nine descriptor-managed streams, plus optional entry_state.csv, merge in
seed order. `ranged_report.py` reconciles consumption, inventory conservation,
floor generation/acquisition and typed shot breakdowns. `check_balance.py`
exports ranged usage, first discovery/acquisition, gap distribution summaries,
firing distance and target breakdowns alongside the ordinary scorecard.

## Positioned map features

Schema 5 retains the ninth stream, `features.csv`: sparse visit/type counters for
generation, discovery, activation, decline, damage, direct deaths, spawned
monsters and holy damage. Additive counters track repeated activations, maximum
same-trap entries per visit and actual teleport displacements. Floor records add
exit hunger, move attempts, unholy population and holy placement geometry. Ranged records add
sacrificed arrow units so fired + thrown + sacrificed equals consumption.

Movement gathers explicit feature choices after entering and resolves through
the production resolver before the enemy turn. Item/feature overlaps offer
pickups first, including full-pack replacement and Yendor; accepted pickups
retain their turn costs. The maintained policy blocks known teleports when
an alternate path exists, uses a bounded fallback for mandatory goals, abandons
repeated optional detours, and avoids other known traps with weighted routing. It uses nearby beneficial features within a
food/detour budget, limits holy re-entry, and respects cursed equipment. Hidden
traps never inform route penalties or choices. `no-map-features` disables only
the dedicated generation domain for same-executable, same-policy controls.
See [map-feature results](../docs/MAP_FEATURES.md).

`feature_experiments.py` runs the A–G controlled interventions and independent
early-density/damage/roster variants with one frozen executable, then streams
a paired report with policy/seed checks and raw-file SHA-256 seals. Typed rules
such as `remove-feature:SPIKE_TRAP:floor=0` remove instances after generation
without replacement or RNG resynchronization. `feature_generation_audit` exports
an independent Holy generation census by depth, archetype, unholy count and
geometry, avoiding survivor selection from visited floors. See
[the tuning report](../docs/MAP_FEATURE_TUNING_2026-10-10.md).

Monster entry uses the production trap resolver. `features.csv` appends
`monster_activated`, `monster_damage` (effective HP loss), `monster_killed`,
`monster_teleported` (successful relocations), `monster_status` (status additions
or extensions, including stun), and `monster_spawned` (successful summons).
Existing feature counters retain player-triggered meanings. Typed monster damage
and kills include environmental events once; floor `damage_dealt` remains
player-attributed damage. Environmental kills grant no player XP/score.
`runs.csv` appends final residual `xp` and `final_hunger`; interpret residual XP
alongside final level. No telemetry is stored in Game or production RAM.

`no-monster-traps` disables only monster entry activation, retaining ordinary
feature generation and player effects. Use it as the control for paired
same-policy, same-seed validation. `monster_trap_report.py` streams supplementary
population, encounter, resource and per-trap results from schema-5 paired outputs.
See [the monster-trap report](../docs/MONSTER_TRAPS_2026-10-10.md).

Monster-trap census validation exposed a pre-existing full-pack bow/wand reversal
without ammunition (seed 54808). The maintained agent now values retaining its
best bow at least as highly as reacquiring it. This corrects inventory progress;
hidden observations, trap routing and monster behavior are unchanged. Both
control and treatment validation use this same corrected policy.

## Lighting, offhand equipment and blindness

Telemetry schema 6 adds `fuel_exhaustions`, `blind_turns`, `blind_deaths`,
`shield_turns`, `offhand_light_turns`, and `light_0` through `light_6` to `runs.csv`.
Radius and equipment occupancy are sampled at turn entry, before fuel burn and
effect aging. `blind_deaths` counts deaths while the player timer is active;
it does not establish blindness as the cause. Item activity and generation use
the existing typed streams. Starting torches are not ground-generation exposure.

The maintained oracle equips/replaces lights, keeps fuel reserves, prefers shields
when permanent jewelry supplies sight, values illumination jewelry, and throws
blindness as a confusion-like control potion. It never drinks harmful potions.
Its global terrain/loot knowledge remains omniscient; tactical ranged visibility
uses production sight checks. These results describe the oracle, not human play.

The host-only `full-light` intervention forces radius six unless blinded and lets
the same policy account for that guaranteed sight. It preserves the candidate
content, placement, fuel rules and executable. Same-location shield removal uses
`remove-item:BUCKLER`, `remove-item:ROUND_SHIELD`, and `remove-item:TOWER_SHIELD`.
See [the implementation report](../docs/LIGHTING_OFFHAND_BLINDNESS.md) for paired
results, policy compatibility limits, and reproducible commands.

## Explorer competence and awareness

See [the full report](../docs/EXPLORER_COMPETENCE_AND_REBALANCE.md) for the
current results and [seed ledger](../docs/EXPLORER_SEED_LEDGER.json). Explorer
protocol 3 supplies anonymous incoming feedback without hidden species or
bearing. There are 103 preferences; tuning rewards escapes, Yendor, Lord kills,
progression, fewer actions, then score. Core telemetry is version 7 and
exploration telemetry is version 6 and survival telemetry is version 1. `OBS` trace lines are policy observations;
other trace lines remain privileged reporting and never feed decisions.

Monster awareness is now the engine default. `--intervention legacy-awareness`
restores historical detection; `--intervention light-awareness` selects the
current behavior explicitly. Either combines with `full-light`. These controls
do not alter the observation boundary or consume gameplay RNG themselves.

```sh
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite awareness --seeds 1:10000 --jobs 12 --output build/competence/reproduce-ab
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite offhand --seeds 1:1000 --jobs 8 --output build/competence/reproduce-offhand
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite balance --seeds 30001:32000 --jobs 8 --output build/competence/reproduce-c
```

Fresh output directories are required. Each suite retains an executable and DLLs,
complete parameters, commands embodied in the experiment specification, manifests,
raw seals, summaries and paired reports. The comparison command supports explicit
`--policy-comparison` labeling; gameplay pairs require identical policy hashes.

## Encounter, milestone, and hazard diagnostics

Output now also includes `encounters.csv` and `milestones.csv`. The nine core
CSV schemas remain version 7. Exploration schema 6 appends known/repeated
summoning-trap entries, following damage, safe routing decisions, essential
crossings, abandoned objectives, route failures, reversals and objective changes.
First discovery is separate from knowingly reentering a trap.

Survival schema 1 records combat episodes, mixed participants, first phantom
contact snapshots, resource costs, 25/50/100-turn follow-up and subsequent descent.
Milestones record equipment and supplies alongside cumulative actions/turns.
`competence_experiments.py --compare CONTROL CANDIDATE --policy-comparison
--output REPORT.json` includes these diagnostics, exact binomial intervals,
paired mean intervals and the existing progression bootstrap. The helper
`survival_report.py` is part of this report pipeline, not a policy input.

The new default `hazard_routing=1` uses explicit safe-route feasibility; set it
to zero in a complete parameter JSON for the contact-aware baseline. Binary
feature switches remain excluded from parameter mutation. See the
[implementation and measured results](../docs/EXPLORER_HAZARD_ROUTING_AND_AVM_OPTIMIZATION.md)
for encounter segmentation, counting conventions, seed usage and limitations.
