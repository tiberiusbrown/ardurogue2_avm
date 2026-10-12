# Explorer-v1

`explorer-v1` is a deterministic native simulation policy with player-limited
observations. It runs the production descent, Lord combat, Yendor
pickup and return ascent. `omniscient-v2` remains the default balance oracle.
The policy adds no firmware storage. The current engine awareness change,
competence experiments and preserved historical reference are described in
[the competence and rebalance report](EXPLORER_COMPETENCE_AND_REBALANCE.md).
The new consumables extend this same `explorer-v1` policy in place; item tactics,
observation protocol 5 and current paired validation are described in
[the consumables report](CONSUMABLES.md).

## Observation boundary

The actual dungeon is 64 by 32, with 16 floors. `explorer.hpp` defines a plain
observation/action protocol without production-state declarations. The policy
is a separate CMake object target. It includes only this protocol, its runtime
preferences, standard library headers and generated **pure public rules**.
Those rules are the item/species definitions preceding `GroundItem` in
`model.hpp`; they contain no `Game`, session, globals or gameplay functions.
The policy does not inherit the oracle's `Agent` or accept `DecisionContext`.

`explorer_observation.cpp` is the engine-side adapter. It computes the same
local 13 by 13 ray sight, closed-door occlusion, circular lighting mask and
opaque wall-face halo as the renderer. Blindness and radius zero retain only
the occupied square unless equipment provides effective sight. Renderer
visibility is compared directly against this adapter in 1,792 generated cases.
It never scans full-map line of sight to choose a destination.

The protocol supplies player position, health, base stats, known status
flags, effective radius, equipment slots and displayed inventory details.
Undisclosed effect timers are excluded.
Haste, Levitation, Fire Resistance and See Invisible are active flags only.
Restore Vitality receives a public drained-vitality flag. Detect Objects supplies
a temporary set of ground-item positions, without item categories, identities,
terrain, traps or mimics; the agent may remember positions after dismissal.
Visible terrain and stairs are supplied; offscreen terrain remains unknown.
Mapping/digging provide explicit terrain snapshots. Terrain known from a map
is tracked separately from cells actually surveyed for entities. A mapped door's subsequent
offscreen state does not update until observed again. Hidden feature IDs and
positions are removed, including from the feature-choice path.

Dungeon item sprites disclose category, not shuffled subtype, enchantment,
curse, quantity or object index. Detailed descriptions become available in the
pack or on the occupied square. Pickup handles exist only there. Shuffled
potion, scroll, wand, ring and amulet identities require production type
identification; appearance mappings contain only identified types. Instance
curse/enchantment and wand charges/modifier require individual identification.
Jewelry magnitude is not printed by the real interface and is never exposed.
Unrevealed mimics are indistinguishable from the corresponding item category.
Empty inventory slots disclose no residual item bits. Visible monster sprites
cover underlying ground sprites, just as they do in the renderer; invisible
monsters do not conceal a displayed item. The occupied-square pickup interface
remains available independently of the player sprite.

Only visible monsters appear, including production invisibility/see-invisible
rules. They supply position and public species definitions, without HP, live
effects, stun, AI targets or aggression flags. Native status adapters also
supply information actually printed to the player: an incoming attacker name,
or confirmation that the agent's own attempted melee square made contact.
An incoming attack supplies **no bearing**. Protocol version 3 introduced the correction removing
the historical native status leak: a creature printed as “something” supplies
no species identity. Its own attempted melee square can still establish contact. Confused movement cannot establish
an inferred contact square. Damage events and telemetry never feed this path.

## Memory and decisions

The host agent retains terrain, doors, discovered features, stairs, visits,
failed attempts and item descriptions per floor. Returning to a remembered
square does not erase it. Missing sprites are invalidated when their square
is observed empty. Monster-covered items retain their last legitimate description
until the square can be inspected again. Identified appearance knowledge survives
consumption and floor changes. Monster memory stores only last-seen positions and times,
expires after a configurable interval and is invalidated by current sight.
It adds cautious route cost; it never supplies a live combat target.

Production regenerates different layouts on ascent. Historical descent
geometry remains recorded separately, but ascent routes require fresh observed
terrain. The agent is about 662 KiB of host memory, with no firmware footprint.

Dijkstra routes traverse known passable cells only, with configurable costs
for traps and remembered threats. Frontiers score nearby unknown area,
distance, repeated visits and commitment to the current destination. A heap
allocation is retained across decisions. Radius-zero movement is explicitly
labelled as tactile probing rather than pretending an unknown route is known.
Unsuccessful probes are retained until fresh terrain observations supersede
them. Optional frontier/loot cycles have bounded attempts based on knowledge
growth, and 16-bit visit counts preserve long-run history. Complete dark-floor
return journeys can exceed 20,000 actions; current paired explorer validation
uses the same 100,000-action guard in both arms.
Loot seeking uses remembered item sprites and displayed pack value; inventory
replacement protects equipped gear, the best melee weapon/bow and ammunition
dependencies. Discovered descent stairs compete with observable frontier gain;
ascent prioritizes known up stairs with short food detours when supplies run out.
Mapping reveals terrain rather than entities, so mapped, unsurveyed cells remain
exploration targets. On an exhausted final floor the agent patrols known cells
by its own visit history until it observes the Lord or Yendor. No-path and
rejection limits produce simulator failures, never synthetic deaths.

Tactics include known healing, experience/stat potions, hunger management,
equipment upgrades, observed straight-line ranged attacks, fire safety,
bounded retreat, committed adjacent melee and emergency teleport/invisibility.
Invisible contact has explicit suspected, confirmed, uncertain and disengaging
states. Printed melee hits and roll misses confirm the attempted square at
attack time and refresh commitment. Favorable fights have no 12-turn stationary
cutoff. Missing feedback, movement, confusion and observable encounter changes
reduce or invalidate contact. SurvivalRisk, displayed HP loss, hunger, resources
and observable replenishment can justify disengagement independently of contact
confidence. A four-direction tested mask bounds probes at each player position;
known traversable squares precede unknown tactile probes. Route avoidance
survives a completed retreat independently of contact certainty. See
[the invisible-contact report](EXPLORER_INVISIBLE_CONTACT.md) for inference rules,
loop safeguards, telemetry definitions and measured comparisons.
Unknown jewelry can be tried even when a mediocre identified item occupies its
slot, with an upper protection threshold for valuable known gear. Planned ring
replacement retains its intended new slot across the removal action, preventing
the former ring from being equipped again first. Unknown jewelry and consumables
are tried under bounded, configurable quiet
and health conditions. Individually unexamined wands, including ones whose type
was learned from another instance, are tested against observed distant enemies.
Successful production use reveals their modifier and remaining charges.
Known offensive types can be tested at close range under small light radii;
unknown types and unexamined fire wands retain a three-tile safety distance.
Digging is not tested as an attack, and known escape/transformation wands
require observable emergency pressure.
Close contact restores a melee weapon before further bow shots.
Known harmful potions are not drunk as experiments. The agent can
probe for an attacker whose name was reported, then retain confirmed contact
briefly, without tracking its real position. Feature choices use the same
sanitized observations and normal production movement completion.

Ordinary supplies do not replenish on regenerated ascent floors. The food
stock target therefore applies during descent, and food pickup/retention values
agree to prevent reverse swaps. Permanent illumination receives a configurable
bonus when other permanent sources leave the player in darkness; its value is
computed with that source removed, so wearing it cannot trigger an immediate
switch back. Late fire immunity, final-floor clarity, ascent sustenance and
quiet ascent lighting have separate preferences. None reveals unseen entities.

## Runtime preferences and evolution

`explorer_parameters.hpp` defines integer defaults, names and bounds for 102
runtime preferences. They cover frontier costs, loot costs, threat memory,
trap avoidance, healing/emergency thresholds, food, safe experimentation,
equipment ratings, unknown-item values, early experience and retreats.
Preferences change decisions only; they never change game mechanics.

```sh
build/sim-native/sim/ardurogue2_sim --write-agent-defaults build/defaults.json
build/sim-native/sim/ardurogue2_sim --agent explorer-v1 --agent-params build/preferences.json --seeds 1:1000 --jobs 8 --output build/explorer-run
```

A JSON file may override a subset of fields; remaining values use the shipped
defaults. Unknown/duplicate keys, fractional values, out-of-range integers and
malformed JSON fail before simulation. Canonical serialization writes every
field in a fixed order. Omit `--agent-params` to use defaults. The option is
rejected for the oracle. Worker processes receive the complete configuration.

The separate `ardurogue2_explorer_tune` executable uses seeded mutation and
crossover with four retained elites. Every candidate in a stage uses the same
seed range. Fitness is ordered by **escapes, Yendor acquisitions, Lord kills,
weighted progression milestones, fewer actions, then production score**.
Mutations cycle through navigation, tactics, equipment and resource groups.
Binary ablation controls and offhand strategy modes are excluded from mutation. A batch with any `SIM_STUCK`/`SIM_ERROR` is
invalid and cannot become an elite. Equal fitness retains the earlier candidate.
The host search RNG is separate from production gameplay RNG.

```sh
# Coarse search; inspect improvement by generation in history.csv.
build/sim-native/sim/ardurogue2_explorer_tune --seeds 1:100 --population 12 --generations 20 --jobs 8 --random-seed 71 --output build/tune-100
# Expand the range after progress plateaus; re-evaluate the winner before evolving.
build/sim-native/sim/ardurogue2_explorer_tune --seeds 1:1000 --start-params build/tune-100/best.json --population 12 --generations 20 --jobs 8 --random-seed 91 --output build/tune-1000
# Or automate the ramp after three generations without improvement:
build/sim-native/sim/ardurogue2_explorer_tune --seeds 1:1000 --initial-seeds 100 --plateau-generations 3 --jobs 8 --output build/tune-staged
# Historical validation example; this range is now inspected development data.
build/sim-native/sim/ardurogue2_sim --agent explorer-v1 --agent-params build/tune-1000/best.json --seeds 1001:10000 --jobs 8 --output build/tune-validation
```

Search directories must be fresh. `--minutes N` bounds search time between
complete candidates; `--max-actions N` sets the usual simulator limit.
With `--initial-seeds`, plateaus expand the range tenfold, capped by the
requested range, and re-evaluate every elite. Seed counts are recorded for
every fitness row. Final elites always receive complete-range validation,
even if the search time budget ends in a coarse stage. The final validation
can therefore extend beyond `--minutes` by up to four batches.
`--simulator FILE` selects the matching worker executable, otherwise the sibling
simulator is used. A worker policy/configuration mismatch fails the search.
Outputs retain baseline/best JSON, each candidate configuration and per-seed
fitness CSV, a flushed history with hashes and timing, and the search manifest.
Defaults are updated deliberately after reviewing and rechecking the winning
configuration; the executable does not rewrite source during a search.

## Identity, telemetry and validation

The oracle hashing algorithm remains unchanged; the light-reserve repair changes
its policy identity. Explorer code and its observation adapter,
public definitions and preferences have a separate code hash. An explorer
manifest records the complete runtime parameters and their canonical SHA-256.
`agent_policy_hash = SHA256(code_hash + ":" + parameter_hash)`.
Partial and full files resolving to the same configuration have the same
identity. Changing any preference changes policy identity. Same-policy gameplay
interventions require matching hashes and parameters. Comparisons of distinct
policies are descriptive, using identical seed sets with differing policy hashes.

Core telemetry is version 9, including new consumable activity and expiration.
The additional `exploration.csv` has its own schema version (6) and records per-visit terrain/floor
coverage, actually surveyed floor cells, exploration turns, travel, repeated visits, frontier changes, failed
routes, stale items, unseen damage, observed items, discovery-distance bins,
stair discovery/reach times and displacement. Version 5 appends invisible-contact
probes, hit/miss confirmations, loss and abandonment proxies, episode lengths,
damage, healing, kills, disengagements and summoning classifications. Their
observation/reporting boundary is documented in
[EXPLORER_INVISIBLE_CONTACT.md](EXPLORER_INVISIBLE_CONTACT.md).
Reachable area and geometric
stair-route length are ground-truth **reporting denominators only**, never
inputs to the agent. Darkness can reduce what is learned without removing
retained knowledge. The oracle's sidecar describes its visible surroundings;
its own navigation still uses oracle information.

`items_observed` is a ground-cell encounter proxy: it counts a placement when
its cell enters terrain sight, including a placement whose sprite is covered
by a monster. Dropping an item and later encountering it can count another
placement. This reporting counter does not disclose that item to the policy;
the observation adapter still suppresses covered sprites. Actual pickup/use
totals are reported separately. Interpret this proxy as potentially discoverable
loot rather than exact identification or acquisition.

```sh
python sim/balance.py ab --agent explorer-v1 --agent-params build/preferences.json --control-exe build/sim-native/sim/ardurogue2_sim --treatment-exe build/sim-native/sim/ardurogue2_sim --seeds 1:10000 --jobs 8 --control-intervention full-light --output build/explorer-light
```

Noninterference checks vary hidden corridors, stairs, loot, monsters, traps,
RNG, unidentified subtype/seed, unknown curses/enchantment and monster HP/AI
effects while holding observations equal. Their actions and feature choices
must match. The compilation-boundary test also rejects forbidden state/function
references and production-symbol imports. The runner checks that observation
and decisions leave the entire `Game`, including RNG, unchanged. Replay,
telemetry-disabled execution and serial/process-worker execution agree.
Constructed coverage includes mapping, blindness, traps, swaps, light exhaustion,
stairs, regenerated ascent and a complete production Lord/Yendor/escape run.

## Historical measurement and limitations

The retained reference seeds 1–1,000 produced 41 oracle escapes. All original
nine CSVs from the instrumented oracle matched the frozen reference byte for
byte, including action hashes, and the oracle policy hash was preserved.
Initial explorer policies produced no escapes; low competence is addressed
through observations, heuristics and parameter evolution, not extra knowledge.

The final three 20-minute searches evaluated 79 configurations: 48 on 100
seeds and 31 on 1,000 seeds. The staged search expanded tenfold after two
generations without improvement and re-evaluated its four elites. Two batches
had simulator failures and were excluded. The other two searches started from
earlier evolved weights, rather than discarding useful previous search results.
Four configurations escaped and were re-evaluated after the final observation
and memory audit. The retained winner has **1 escape, 683,304 total score and
zero failures** on seeds 1–1,000, versus **0 escapes and 308,045 score** with the
pre-tuning default preferences under the same final decision rules. The 88
evolved values were promoted to `sim/explorer_parameters.hpp`; the winner
remained the carried-forward elite after the final search rounds. Adding the
stationary-contact retry budget (128 turns by default) brings the current
runtime configuration to 89 preferences. Re-evaluating all four finalists with
that guard leaves their 1,000-seed fitness unchanged.

Seed 363 completes the production Lord fight, Yendor pickup and all ascent
exits. No constructed equipment or world substitutions are used for this
competence result. The agent stocks food during descent because ordinary
supplies are not generated during ascent. Earlier traces exposed starvation,
ring replacement loops, reverse inventory swaps, incomplete mapping surveys,
and continuing weak bow attacks at adjacent range; their fixes preserve the
observation boundary and are covered by regression checks.

The final native Release build passed **all 22 CTests** in 77.32 seconds with
four test processes, including **273 explorer checks** and the **1,792-case
renderer comparison**. The policy occupies **538,384 bytes** on this host.
Runtime parameter, telemetry, trace and worker tests pass. A final default replay
matches all eleven CSVs (the original nine, exploration and entry state) from
the explicitly configured winning policy. Removing undisclosed player-effect
timers also leaves all eleven files identical; only known status flags reach
the policy. The oracle's original nine files and policy hash still match its
frozen 1,000-seed reference.

Final explorer policy hash:
`6027f13c465c1ee91a0c40503681f223576b70e1b3ac76084616ab0e2837aeed`.
Canonical parameter hash:
`5e3635402bba1bca967e854f19139058f33e589c70197c11ef303609bb30afab`.
Frozen executable SHA-256:
`ee1ad0c78871f423fa5b718ea4677538939ca042767d8fe2c7c0b0553955b50f`.

Raw training configurations, fitness, executable snapshots, selection ledger,
replays and test log are retained under ignored `build/explorer/`.
`guard-default-selection.json` records candidate fitness and search provenance;
`selected-guard-defaults.json` is the complete winning configuration;
`final-guard-0-1000/` retains the final replay. The larger explorer experiment
uses the frozen `final-v12-bin/` executable, seeds 1–10,000 for each of the three
explorer conditions, and fresh seeds 20,001–30,000 for separate validation.
All larger conditions have the same 50,000-action limit and entry telemetry;
training used 20,000 actions. These larger seeds are not used to select
parameters. The development probe had previously inspected seeds 1–10,000,
so that range is explicitly not presented as untouched validation.

The first larger full-light explorer batch contained one invalid run: seed
9145 repeatedly struck respawning invisible phantoms while healing offset
starvation. It reached the 50,000-action limit on floor 4. That batch is kept
under `build/explorer/final-10000/` and is excluded from final comparisons.
The observation-based contact detour now takes that seed to floor 13, where it
dies normally to a dragon after 5,565 actions. The fix neither reads the trap's
unseen origin nor invents a monster position. The complete explorer comparison
is repeated with the guard, and its validation range is moved beyond previously
inspected seeds.

The oracle pair is reused from that first experiment: both oracle conditions
use the same frozen executable, policy hash, seeds and limits, with zero
failures. The explorer-only guard has no effect on the oracle; all original
nine streams still reproduce its reference. Each controlled pair uses one
binary and one policy. Cross-agent rows remain descriptive. The final data,
per-condition binary provenance, tables and paired reports are retained under
`build/explorer/final-v12-10000/`.

## Historical larger measurements

Tuning has stopped. The promoted defaults and all search history are retained
for a later rebalance. Each main condition below uses seeds 1–10,000; the
separate equipment-light validation uses untouched seeds 20,001–30,000.
All six batches reconcile successfully, with **zero simulator failures across
60,000 runs**.

| Policy and condition | Escapes | Deaths | Escape rate | Total score |
| --- | ---: | ---: | ---: | ---: |
| Oracle, equipment light | 386 | 9,614 | 3.86% | 17,593,003 |
| Oracle, permanent full light | 2,023 | 7,977 | 20.23% | 40,498,237 |
| Explorer, equipment light | 1 | 9,999 | 0.01% | 6,594,199 |
| Explorer, permanent full light | 1 | 9,999 | 0.01% | 7,600,204 |
| Explorer, shields removed | 0 | 10,000 | 0.00% | 3,958,773 |
| Explorer, separate validation | 1 | 9,999 | 0.01% | 6,558,303 |

The oracle's paired full-light escape increase is **16.37 percentage points**,
with a paired bootstrap 95% interval of **[15.62, 17.13]**. It gains 1,720
escaping seeds and loses 83. Its exact McNemar p and BH-adjusted q are below
floating-point representable precision; reported numeric zeros mean underflow.
The explorer's full-light comparison gains seed 6216 and loses seed 363,
for a zero-point change and interval **[-0.03, 0.03]**. Shield removal loses
seed 363, for **-0.01 points**, interval **[-0.03, 0.00]**. Both explorer
comparisons have exact p = 1 and BH q = 1. The BH family comprises these
three prespecified escape comparisons. Rare escapes and these finite seed
ranges do not establish absence of prospective lighting or shield effects.

Full lighting improves the explorer's information acquisition and typical
progress even though escapes remain rare. Its mean score rises from 659.42
to 760.02, with a paired mean difference of 100.60 and bootstrap interval
[84.52, 118.97]. This is a secondary measurement. Coverage and per-floor
means below are conditioned on survival, so they should not be interpreted
as independent causal effects.

| Explorer condition | Known/surveyed reachable coverage per visit | Exploration turns/visit | Mean stair discovery turn | Movement stretch | Monsters seen before adjacent contact |
| --- | ---: | ---: | ---: | ---: | ---: |
| Equipment light | 69.6% / 65.3% | 319.7 | 443.5 | 5.47 | 29.5% |
| Permanent full light | 76.2% / 74.0% | 156.6 | 287.8 | 3.64 | 88.5% |
| Shields removed | 69.7% / 66.1% | 214.6 | 348.1 | 4.32 | 77.7% |

Movement stretch divides actual floor travel by the reporting collector's
shortest objective route; that hidden denominator never reaches the policy.
Equipment-light explorer turns spend 73.7% at radius one, 18.7% at radius
three and 4.8% at radius four. Removing shields shifts those shares to 6.7%,
61.3% and 22.0%, respectively. Full lighting remains radius six on 99.6%
of explorer turns; blindness accounts for radius zero. The most frequent
equipment-light deaths are PHANTOM (2,133), SNAKE (1,868) and ORC (1,253).
Without shields they are PHANTOM (2,710), SNAKE (2,349) and GOBLIN (1,253).
These counts expose the defense/exploration tradeoff without proving that
one intervention globally improves escape probability.

The reviewed seed-363 traces show a complete normal equipment-light escape,
a full-light death to the Lord on floor 15, and a shield-removal death to a
dragon on floor 13. They are retained in `build/explorer/final-traces/`.
The selected policy completes the normal descent, objective fight, pickup
and regenerated ascent; the low aggregate escape rate leaves substantial
room for better resource planning and tactics.

Raw CSVs, source hashes, intervention definitions and binary provenance are
in `build/explorer/final-v12-10000/`. Its `report/summary.md` and
`report/summary.json` contain the five-condition tables, floor survival,
death causes, full radius distribution, fuel use, item observations/pickups,
stair timing and movement measures. `report/validation.json` retains the
fresh validation separately. `paired/*/comparison.json` contains the
reconciled paired bootstrap results.

The four-worker explorer batches measured 9.11 runs/second with equipment
light, 10.00 with full light and 14.32 without shields; fresh validation
measured 9.27. The three-worker oracle batches measured 7.80 and 4.43,
respectively. These are observed batch throughputs under concurrent load,
not isolated CPU benchmarks. At the measured full-light oracle throughput,
65,535 seeds would take about 4.1 hours for that condition alone. The full
census is deferred following the request to wrap up; the 10,000-seed
measurements and complete artifacts support resuming the rebalance later.

## Differences from human players

This is not an exact human-player simulation. It has perfect retention of
legitimate observations, deterministic preferences, limited risk estimates,
simplified experimentation, short-lived monster/contact memory and no deep
multi-turn tactical search. It can waste controls whose effects are not
currently visible, cannot infer every item effect from prose, and can be
surprised by unseen attackers or unavoidable hidden traps. Parameter search
can overfit its seed range; separate validation and stable paired identities
remain necessary. Coverage, item totals and floor turns are conditioned on
survival; cross-agent comparisons are descriptive, not policy-controlled causal
estimates. A low escape rate limits the precision of lighting escape effects.

The next useful improvements are planning food for the entire regenerated
ascent, choosing permanent light and defense jointly, better observation-based
Lord tactics, and testing learned item effects with remembered outcomes. A
broader untouched validation range matters more than repeatedly optimizing
the same successful seed. These improvements must retain the observation
boundary and use production actions.

## Files and shared infrastructure

New native modules are `sim/explorer.hpp`, `explorer_agent.cpp`,
`explorer_observation.{hpp,cpp}`, `explorer_events.{hpp,cpp}`,
`explorer_parameters.{hpp,cpp}`, `policy_hash.{hpp,cpp}`, `tuning.hpp`,
`explorer_tune.cpp` and `explorer_report.py`. Their checks are
`explorer_checks.cpp`, `explorer_render_checks.cpp`,
`check_explorer_boundary.py`, `test_explorer_cli.py` and `test_explorer_tune.py`.

`sim/simulator.{hpp,cpp}` retains one production action dispatcher and run loop
for both agents, adapting only each policy's input and action protocol.
`frontend.cpp` adds native status capture; `main.cpp` and
`parallel.{hpp,cpp}` add selection, complete parameter forwarding and worker
identity checks. `manifest.cpp`, `build_metadata.hpp.in` and `CMakeLists.txt`
record/build the independent policy. `metrics.{hpp,cpp}` add the separate
exploration stream while preserving the existing streams. `balance.py`,
`feature_experiments.py`, `test_balance_cli.py` and `test_check_balance.py`
support the new agent and optional stream. Documentation changes are this
file, the project `README.md`, `sim/README.md` and `sim/BALANCE.md`.
These statements describe the historical explorer implementation. The current
pass changes initial monster awareness and repairs an oracle light-reserve swap
cycle; save layout remains unchanged. See the current competence report.

The contact-aware policy supersedes the historical stationary budget and
invisible-response measurements above. `contact_aware=0` retains the frozen
decision logic on the archived contact release; combining it with `invisible_response=0` reproduces the existing
reassessment ablation, which still probes and repeats contact.
`stationary_contact_turns` is a legacy-only preference. All other defaults are
held fixed in the [new policy report](EXPLORER_INVISIBLE_CONTACT.md). Protocol 3
adds public melee outcomes, attack counts, attack origin and conservative
encounter-change/summoning messages, without hidden species or live positions.

## Current competence pass

The current defaults use separate safe (41%), approaching (55%), and melee
(65%) healing thresholds, bounded one/two-action incoming-risk estimates,
control before inadequate healing, earlier invisible-contact reassessment, and
contextual offhand evaluation with a 30-point switching margin and six-turn
cooldown. Contextual light coefficient 4 was selected after the stronger setting
hurt equipment-light progression. Old emergency percentage and combat behavior
remain available only as explicit ablations.

Frontier gain uses rays through remembered terrain, with known walls and closed
doors blocking sight; unknown terrain is optimistic and mapped unsurveyed cells
have lower weight. Short remembered healing-feature detours, distant duplicate
loot limits, critical-supply retention and regenerated-ascent food planning use
only observed information. Public Lord traits guide clarity, fire resistance,
speed and control choices; no Lord path or live HP is supplied.

`sim/competence_experiments.py` freezes executables/configurations, seals raw
CSVs, rejects failures and provides paired escape/progression statistics.
The seed ledger and current results are in the competence report. The historical
results above remain archival evidence, not estimates for these defaults.

## Known summoning hazards

The default `hazard_routing=1` first computes routes excluding every legitimately
remembered summoning trap, including unvisited discovered traps. Safe routes can
reuse visited terrain. A second pass is reserved for stairs or exhausted survey
progression; optional loot never uses it. Healing, escape, recent activation
damage, and a bounded reassessment interval govern costly repeat entries. Route
costs are 32-bit, and hazard count precedes ordinary cost in the second pass.

Objectives receive modest hysteresis. Three failed or fruitless survey attempts
defer a target; recent ABAB movement retires its survey request. Danger and urgent
resource decisions still run before navigation. Ascent invalidates remembered
features. `hazard_routing=0` reproduces the contact-aware routing baseline; use it
alongside contact/invisible ablation switches when reproducing older behavior
with the current executable.

See [the hazard, survival diagnostics, and AVM report](EXPLORER_HAZARD_ROUTING_AND_AVM_OPTIMIZATION.md)
for constructed tests, paired results, exact limitations, and reproducibility.
