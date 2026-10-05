# Frozen omniscient-v2 reference

Validated October 5, 2026 on Windows x64, Visual Studio Clang 22.1.3, Release.
`omniscient-v2` is ready to freeze and is the frozen balance-reference policy.
Future game-content comparisons must use exactly this agent version on both
baseline and candidate. Any material policy change requires a new version and
a new fixed-seed baseline. The name/comment in `sim/agent.hpp` is part of
experimental reproducibility. This pass adds no statistical/A-B framework.

The historical baseline is revision
`8a4080749d3b501b63a8e1b2ec0a7f0849425ea1`, documented in
[RESULTS.md](RESULTS.md). Production balance, saved fields, persistence and
save version 23 are unchanged.

## Behavioral changes

* Fire safety scans the actual production ray endpoints and every spreading
  burst before use. Normal/spreading radius is one; powerful/overpowered radius
  is two. Any burst covering the player is rejected unless a positive equipped
  fire-immunity ring prevents damage. Cursed/unreliable fire is rejected. There
  is no deliberate nonimmune self-fire exception.
* A small current-state danger heuristic precedes ordinary combat. It sums
  nearby uncontrolled hostile pressure using production strength/speed, player
  armor/speed, weakness, fire/status attacks and relevant immunities. Low HP,
  multiple adjacent enemies and impaired player statuses trigger emergencies.
  Healing remains first; emergency experience recovery, hard potion control,
  mass confusion/fear, useful invisibility, teleport scrolls, teleport/force/
  polymorph wands, then a bounded retreat precede ordinary melee.
* Teleport wands target dangerous current ray/area groups within two tiles.
  Normal, powerful, spreading and overpowered forms use the actual modifier
  predicates and endpoint coverage. Afflicted forms and healthy trivial fights
  are declined. The production effect selects destinations and applies confusion.
* Force checks the production push ray for useful separation or collision stun.
  It handles the longer powerful push and all spreading directions. Actual
  displacement, collisions and stuns are exclusively production effects.
* Polymorph is a last-resort severe-emergency option for late monster types.
  The Lord, trivial forms and targets below half their normal maximum HP are
  excluded, including unsuitable members of an affected area group. Production
  polymorph shifts one type down/up, restores HP and clears statuses. Choices
  never inspect the resulting roll; repeated emergency use can still be risky.
* Retreat scores empty, passable, open cardinal cells by current danger reduction,
  connectivity and progress toward the strategic stairs. It rejects dead ends,
  occupied cells, closed-door turns, confusion and immediate retreat reversal.
  At most three steps are allowed for a tracked hostile group. That budget
  survives distance/visibility changes and temporary safety, and resets only
  when tracked threats are gone/no longer hostile or the floor changes. Actual
  movement and following monster turns remain production operations.
* Healthy reasonable encounters still use ordinary ranged damage and melee.
  The Lord remains a combat objective, and no useful escape means fighting.
* Force value changes from `30 + 3*charges` to `45 + 4*charges`; teleport from
  `35 + 3*charges` to `65 + 5*charges`; polymorph from `20 + 3*charges` to
  `30 + 3*charges`. Retention uses these same values. Remove-curse changes from
  20 to zero because this oracle avoids afflicted gear/wands. Equipment scoring,
  food retention and other item preferences remain as in v1.

`omniscient-v2 intentionally assigns WAND_DIGGING no tactical value`.
Production digging carves/explores six cells per ray, opens doors and uses three
lanes when powerful. It does not move the player or control enemies. The oracle
deliberately avoids spending danger turns on speculative shortcuts; digging's
acquisition/retention value and usage remain zero.

## Files and production geometry

| Files | Change |
| --- | --- |
| `sim/agent.hpp`, `sim/omniscient_agent.cpp` | Version, fire safety, emergency wand/control choices, bounded retreat memory, small value fixes |
| `sim/policy_checks.cpp`, `sim/tests.cpp` | Focused deterministic policy and physical-wand telemetry tests |
| `sim/metrics.hpp`, `sim/metrics.cpp`, `sim/simulator.cpp` | Host-only wand identities and two additive CSV columns; prepare pickup/drop bookkeeping before dispatch |
| `sim/main.cpp`, `sim/parallel.hpp`, `sim/parallel.cpp` | User-requested `--jobs N` process parallelism with ordered CSV merging |
| `sim/CMakeLists.txt`, `sim/test_cli.py` | Build new sources; serial/parallel CLI integration checks |
| `src/world.hpp`, `src/combat.cpp`, `src/items.cpp` | Shared pure square coverage and fire-radius helpers |
| `sim/README.md`, `sim/RESULTS.md`, `sim/V2_RESULTS.md` | Current policy, historical baseline link, frozen results |
| `ardurogue2.arduboy` | Regenerated ordinary AVM package; size remains 150,004 bytes |

`square_contains()` factors the existing square burst/area coverage;
`wand_fire_radius()` shares the modifier-to-radius rule. Existing production
`scan_ray()`, modifier predicates, monster definitions and combat equipment
helpers supply planning facts. The refactor changes no effect ordering, damage,
target eligibility, RNG call or gameplay API.

The extraction changes optimized IR in `combat.cpp` and `items.cpp`; the old
strict identical-IR check therefore is not claimed to pass for this refactor.
Ordinary `state.cpp` and `world_gen.cpp` IR remain identical to v1, no simulator
symbols occur in the ordinary inspected IR, and saved model/persistence files
are unchanged. Exhaustive coordinate tests verify both axes for every byte
coordinate difference at radii one and two. See the local
[production review](../build/sim-v2-production-review/review.json) and diffs.

## Fixed seeds 1..10000

```sh
build/sim-clang/sim/ardurogue2_sim.exe --seeds 1:10000 --jobs 8 --output build/sim-v2-acceptance
build/sim-clang/sim/ardurogue2_sim.exe --seeds 1:10000 --output build/sim-v2-serial-check
```

| Measurement | omniscient-v1 | Frozen omniscient-v2 |
| --- | ---: | ---: |
| Escaped | 4,079 (40.79%) | 5,293 (52.93%) |
| Reached floor 12+ | 6,878 | 7,078 |
| Reached floor 15 | 5,019 | 5,989 |
| Lord kills | 4,234 | 5,582 |
| Yendor acquired | 4,233 | 5,582 |
| Post-Yendor deaths | 154 | 289 |
| Production deaths | 5,921 | 4,707 |
| Simulator stuck/error | 0 | 0 |
| Total actions | 35,102,391 | 36,462,736 |
| Maximum actions | 6,688 | 7,248 |
| Self-fire deaths | 122 | 0 |
| Starvation deaths | 0 | 0 |
| Serial wall time, all four CSVs | 213.477 s | 230.994 s |
| Eight-worker wall time, all four CSVs | Not measured | 40.837 s |

Escape increases by 12.14 percentage points. Matched outcomes comprise 1,986
v1-death/v2-escape seeds, 772 v1-escape/v2-death seeds, 3,307 shared escapes and
3,935 shared deaths. These are agent-quality observations, not a production
balance change or an estimate of normal-player difficulty. Different actions
naturally change later production RNG consumption and generated floors.

All four final serial/parallel CSVs are byte-identical over all 10,000 seeds,
including action digests and detailed telemetry. Eight workers are about 5.66
times faster here; startup, temporary output and final merge are included.
See [serial timing](../build/sim-v2-serial-check/timing.json) and the
[CSV equivalence check](../build/sim-v2-acceptance/parallel-equivalence.txt).
The executable defaults to one worker; `--jobs` accepts 1..64, capped by seed
count. Processes isolate global `Game`/`session`, and contiguous chunks merge
in seed order. Tracing one seed stays serial. Windows is the validated platform;
the POSIX launcher implementation was not exercised on this machine.

Every seed's floor visits, exits, action/turn totals and physical wand accounting
reconcile. Artifacts: [runs.csv](../build/sim-v2-acceptance/runs.csv),
[floors.csv](../build/sim-v2-acceptance/floors.csv),
[items.csv](../build/sim-v2-acceptance/items.csv),
[monsters.csv](../build/sim-v2-acceptance/monsters.csv),
[summary.json](../build/sim-v2-acceptance/summary.json),
[timing.json](../build/sim-v2-acceptance/timing.json), and
[ledger check](../build/sim-v2-acceptance/ledger-check.txt).
Bulky generated artifacts remain in ignored `build/`.

## Tactical wand use

Percentages count distinct physical wands, including all generated modifiers.
Host identities survive floor transitions, swaps, drops/repickups and slot reuse.
`wands_picked_up` and `wands_activated` are additive item CSV columns; ordinary
`picked_up` counts transactions and `used`/`charges_used` count activations.
An activation is a real successful production wand call, not guaranteed survival.

| Wand | Generated | Distinct picked up | Picked/generated | Distinct activated | Activated/generated | Activated/picked | Uses / charges |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Force | 14,108 | 11,152 | 79.05% | 2,865 | 20.31% | 25.69% | 16,099 |
| Teleport | 13,301 | 10,218 | 76.82% | 3,859 | 29.01% | 37.77% | 21,571 |
| Polymorph | 13,561 | 9,106 | 67.15% | 795 | 5.86% | 8.73% | 3,620 |
| Digging | 14,505 | 0 | 0.00% | 0 | 0.00% | N/A | 0 |

V1 spent zero charges on all four wand types. Its old pickup transaction totals
are force 15,355/14,110 generated, teleport 13,474/13,266, polymorph
13,368/13,571 and digging 0/14,441. Repeated swaps/repickups can exceed generated
supply; those ratios are not percentages of distinct objects acquired. The old
CSV lacks unique pickup identities, so distinct v1 acquisition percentages
cannot be recovered from it. V1 activation percentages are exactly zero.

Unused reserves remain common; these are emergency tools rather than attacks
to spend during every healthy fight. Tests explicitly reject unnecessary use.
Polymorph remains the least frequently activated because of its variance,
HP restoration, Lord exclusion and lower priority.

## Death causes

These are immediate production death causes; dragon fire is distinct from
player wand fire. There are no remaining intentional self-fire seeds to explain.

| Cause | v1 | v2 |
| --- | ---: | ---: |
| Snake | 1,700 | 2,273 |
| Dragon fire | 1,157 | 969 |
| Lord | 585 | 355 |
| Troll | 450 | 136 |
| Dragon melee | 358 | 156 |
| Phantom | 285 | 210 |
| Incubus | 248 | 75 |
| Orc | 179 | 57 |
| Goblin | 177 | 65 |
| Angel | 175 | 40 |
| Self-fire | 122 | 0 |
| Hobgoblin | 111 | 22 |
| Zombie | 109 | 131 |
| Rattlesnake | 88 | 70 |
| Griffin | 77 | 29 |
| Mimic | 50 | 60 |
| Bat | 30 | 55 |
| Tarantula | 20 | 4 |
| Starvation / other | 0 | 0 |

## Representative trace audit

The archived v1 executable's SHA-256 is
`26d1d68a2b3f2c8733a31ede77bfcc51869b8bd79c4ebd24d7c9387c7203076a`.
Paired traces use that exact binary and the final v2 executable on the same seed.
Because policies diverge, later worlds are not a controlled counterfactual of
one isolated tactic. Constructed-state tests supply that narrower evidence.

* **Seed 3, angel death to escape:** [v1 trace](../build/sim-v1-3.trace) dies on
  floor 15 at 4 HP attacking an angel, after 4,410 actions. The
  [v2 trace](../build/sim-v2-final-3.trace) uses fear, five player teleport
  scrolls and four force activations during the final-floor fight. At A4267,
  54/114 HP, it forces the Lord away instead of committing immediately; later
  melee continues while the Lord's HP falls. It earns Yendor and escapes after
  5,769 actions, with action digest `5273658724989440245`.
* **Seed 228, self-fire death to escape:** [v1 trace](../build/sim-v1-228.trace)
  uses spreading fire at A494 on floor 1 from 27 HP and takes two self bursts
  of 14 and 13, dying after 495 actions. The
  [v2 trace](../build/sim-v2-final-228.trace) escapes after 5,549 actions,
  digest `12310968695508755796`. On ascent at A4519 it uses a powerful teleport
  area to remove an adjacent griffin at 62/126 HP; the next actions resume
  upward movement without HP loss. This demonstrates actual tactical use as
  well as the conservative fire change.
* **Seed 173, incubus death to escape:** [v1 trace](../build/sim-v1-173.trace)
  dies to an incubus after 3,891 actions. The
  [v2 trace](../build/sim-v2-final-173.trace) uses polymorph at A3663 with
  36/108 HP against an 18-HP incubus. The real roll makes a fully healed troll,
  illustrating the risk rather than a predicted favorable outcome. It then
  resumes melee, later polymorphs an angel, and escapes after 5,213 actions,
  digest `11443290159619648829`. Seed 239 was also audited and shows repeated
  emergency polymorph with actual intervening damage.
* **Seed 26, escape to snake death:** [v1 trace](../build/sim-v1-26.trace)
  escapes after 5,338 actions. The first different decision in the
  [v2 trace](../build/sim-v2-final-26.trace) is A4: retreat at 10/18 HP from
  a snake rather than another attack. Three retreats precede renewed combat,
  changing subsequent production rolls. Later, at 3 HP, a scored retreat is
  followed by a lethal snake turn. V2 dies after 105 actions. Current-position
  distance/connectivity is not a prediction of the following monster turn.

The audit also examined formerly looping retreat cases, including seeds 56,
3347, 1584, 5420, 5709 and 6476, and long successful runs. The final identity
budget removes repeated retreat/navigation loops around corners or danger
thresholds; zero starvation deaths and the bounded action maximum corroborate
the focused tests. Choices use only current map/monster/item state. No agent
code accesses `random_state`, `roll()` or `next_random()`, and the runner checks
full `Game` immutability on every decision. No inspected gain requires future
information, altered mechanics or fabricated inventory/monster outcomes.

## Tests and AVM checks

**11/11 CTest tests pass** on the final build: the existing game correctness,
bulk generation, inventory view, formatting, UI, rendering, combat distribution,
generated visibility and benchmark-helper tests, plus simulator checks and CLI
integration. Log: [sim-v2-tests.log](../build/sim-v2-tests.log).

Focused tests cover safe/unsafe normal fire, radius-two fire, blocked and
monster-ended non-target spreading rays, overpowered fire, immunity, afflicted
declines and exhaustive square geometry. Tactical wand cases exercise all four
usable modifiers through real dispatch/charge use, healthy trivial declines,
afflicted declines, powerful teleport group coverage, Lord/near-dead polymorph
exclusion, differing RNG states with identical choices, and healing priority.
Retreat tests cover closed doors, real movement, sensible melee, three-step
limits across temporary safety/lost sight, and reset after defeated threats.
Inventory tests cover tactical wand retention and no immediate reverse swap.
Physical wand tests cover drop/repick, swap and fresh slot/floor reuse.

Existing repeat-run tests compare every metric, full trace, final state/RNG and
action digest; trace off and telemetry off preserve mechanics. Seed 4 still
kills the Lord, acquires Yendor and escapes all 31 visits. The fixed competence
set 1..32 leaves floor 0 in 26 runs, reaches floor 12+ in 26 and escapes in 18.
CLI checks compare serial/parallel stdout and all four CSVs, uneven partitions,
seed zero, single-seed tracing with excess requested workers, invalid job counts,
output paths containing spaces, and telemetry-disabled simulator-limit results.

Ordinary `ardurogue2` and `ardurogue2_bench` AVM builds pass. The complete final
ELF turn benchmark suite passes **30/30**, worst case **75.340 ms**, below the
100 ms requirement. Native `Game` remains 774 bytes; AVM layout remains 773
bytes; save version remains 23. See
[AVM build log](../build/sim-v2-avm-build.log),
[benchmark log](../build/sim-v2-turn-benchmarks.log), and
[benchmark summary](../build/sim-v2-turn-benchmarks/20261005T181141Z-b5kor6js/summary.md).

## Remaining limitations and freeze

Early snake deaths increase from 1,700 to 2,273. The heuristic can retreat from
a low-HP early fight that v1 would win, and does not simulate the enemy's next
movement/attack. It still routes through blockers using ordinary combat, can
spend several escape resources on a persistent threat, and leaves many reserved
wands unused. Dragon breath at longer range, target selection, loot detours,
item preferences and the coarse danger estimate remain imperfect.

Post-Yendor deaths rise from 154/4,233 acquisitions (3.64%) to 289/5,582 (5.18%).
More fragile runs now reach and defeat the Lord, but ascent remains vulnerable.
These regressions are retained and disclosed rather than retuned to maximize
the score. Aggregate gains and the traced decisions are credible within the
accepted current-state omniscience advantage; they do not establish optimal play.

The final native executable SHA-256 is
`7da0431b251c5fc48ff3350bd46068bdb207a553d0943e04004193fb7b97bbed`.
A local frozen copy and policy sources are kept in
`build/omniscient-v2-frozen/` for reproducible replay.
Final serial/parallel CSV SHA-256:

```text
runs.csv     a784d0663a4bc34bd3610a1185da6e25b61f2dd3b16b21d42cffaca0dfef8c0e
floors.csv   a566bdb1674d64d6fb9ebd7a28765336748c83628ce9921d6bc98b2699b93d1f
items.csv    eda933f838cb16053a59afa7957659a4cc977a0e126f0308d9ba7821cb9ca97d
monsters.csv 7b17833f70ba50212380bd2735942c7de317231fae51f485d7eb72f4e0588c04
```

**Freeze decision: use exactly `omniscient-v2` for subsequent balance,
statistical and game-content A/B work. Future policy changes must increment
the version and establish a new baseline.**
