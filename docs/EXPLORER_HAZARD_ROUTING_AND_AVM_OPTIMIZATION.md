# Known-hazard navigation, survival diagnostics, and AVM optimization

Measured 2026-10-11 from `a1a2497` plus this working-tree change. Safe navigation
now excludes known summoning traps explicitly. The AVM worst response improves
from **99.299 to 92.773 ms**, with **81/81 cases passing** and identical saved
state, RNG and display captures. The population policy results remain mixed:
10,000-seed mean depth changes by **-0.0468 floors**, with a paired 95% interval
spanning zero. This establishes route guarantees, useful diagnostics and hardware
margin; it does not establish an overall survival or escape improvement.

## Baseline preservation and independent experiments

Original source, documentation, executable, production/benchmark ELFs and package
are retained in `build/hazard-routing/reference/`. Existing experiments under
`build/invisible-contact/` remain intact. Original simulator SHA-256:
`b79a07c0155bb8067be3bee327ca72622cd8fc2a51a2a30537aeccca037bb126`.
Historical contact policy hash:
`2940acd0c0155138dcbc9b4692538952db6139abfaf911764d9c6d9811e0073f`.

Before edits, fresh contact runs on 1:1000 reproduced mean depth 4.378, 283 phantom
terminal deaths, zero escapes and zero simulator failures. The original 78-case
AVM suite reproduced `monster_trap_busy` at 99.2985 ms. Stack was complete at
233/256 bytes with zero gaps. Game/Monster/Item were 812/8/2 bytes; save version
was 27. Phantom mechanics, monster statistics, item generation and invisible
contact state transitions were not edited.

Experiment A compares complete configurations differing only in `hazard_routing`
(0 versus 1), in one frozen simulator with identical production gameplay. Other
102 preferences retain contact-release defaults. Seeds 1:1000 are development;
10001:20000 are expanded development, previously used in contact comparisons.
Neither is independent holdout data. No weight search, census escape-seed tuning,
or gameplay intervention was used. Structural policy selection preceded expanded
evaluation. Later reporting fixes changed telemetry only and the final datasets
were replayed. Earlier structural prototypes, including rejected stronger goal
commitment, remain archived.

Experiment B holds explorer/oracle policy fixed and compares original versus
optimized production, using native output/event parity and device saved-state/RNG
captures. The final executable, source hashes, complete parameters, experiment
commands, manifests, raw CSV seals, reports, traces and firmware are under
`build/hazard-routing/delivery/`. Earlier `final/`, `validated/`, `accepted/` and
other directories are retained development snapshots; the tables use `delivery/`.

## Two-pass feasibility and essential crossings

The old visited-trap penalty (`1000 * min(10, visits)`) was finite and omitted
discovered but unvisited traps. Its 30,000 cost ceiling prevented storing oversized
costs but also made expensive geometric routes appear unreachable. Increasing
that penalty would retain both problems.

Pass A computes deterministic Dijkstra routes through remembered traversable
terrain, forbidding every known summoning square. Unknown cells and walls are
unavailable; closed doors retain their opening cost. Visited safe terrain remains
usable. Observed monsters can be combat endpoints but not transit cells. Existing
ordinary-trap, remembered-threat, risk and recent-contact exclusions remain.
Recovery features, items, stairs, frontiers and ascent food detours all use this
safe result. A shorter hazardous route cannot outrank a feasible safe route.

Only after useful safe choices are exhausted does Pass B reuse the same route
buffers and retained host heap. It minimizes summoning crossings first, weighted
cost second, cell index third. Occupied transit cells can be reconsidered as
temporary combat obstructions; movement still uses the production dispatcher.
Hazard count is separate from cost. Positive edge costs produce simple routes,
so a computed route does not revisit one trap. Count 255 is unreachable; there
are at most twelve production feature slots. Costs are 32-bit with ample bounded
map/parameter headroom. Compatibility mode retains old ordering and its ceiling.

Required discovered stairs take priority in Pass B. If stairs remain unknown,
a remaining observed survey can be necessary progression after useful safe
surveys are exhausted. Retired safe surveys and enemy-blocked passages can also
be reconsidered. Optional remembered loot never uses a hazardous route and is
temporarily deferred. This avoids permanently imprisoning the agent behind a
critical trap or exhausted survey.

The existing tactical/resource policy executes before navigation. The fallback
heals before progression when risk is serious or HP is below half and known
healing exists; serious risk with a known teleport scroll escapes first. Remembered
activation count and recent public HP loss increase secondary hazardous cost.
Costly repeat entry at the recently entered trap waits for a bounded 24-turn
reassessment when danger is serious or observed loss reaches half maximum HP.
When resources are exhausted, necessary progression can still proceed. There is
no permanent lifetime ban. Explicit essential action/goal labels distinguish this
from an automatic shortest-path fallback; diagnostics count actual known entries.

Known-trap retreat checks from the contact release remain intact. New routing and
radius-zero probe checks exclude known summoning squares. Invisible-contact
transitions are unchanged. On regenerated ascent, remembered feature knowledge
is cleared and up stairs are the required objective. Hidden traps, hidden monster
slots, authoritative geometry, future RNG and reporting-only BFS never enter
the policy translation unit. Unknown traps remain unknown.

## Goal stability and bounded cycle handling

State consists of an objective, a deferred objective and compact recent-position
history. Established loot receives 20% hysteresis; a frontier gets at least its
old hold bonus or 20% of score magnitude. Three route rejections defer a target
for 16 turns. Three fruitless survey attempts retire that survey until new
information/terrain changes. Recent ABAB navigation retires the current survey
and defers the objective. Observed door changes reset survey exhaustion.
Only tagged navigation updates the cycle history. Danger, visible combat,
invisible contact, pickups and urgent resources override the old objective.

A stronger commitment prototype was rejected after worse development progress
and unwanted overcommitment. The retained policy passes the full constructed
Lord/Yendor/ascent escape. The cycle detector handles short cycles; it is not a
proof against every kind of backtracking. Host ExplorerAgent size grows from
540,120 to 644,704 bytes. None of this state occupies firmware RAM.

## Tests and diagnostic definitions

All 24 native CTest suites pass. There are 662 explorer assertions, including 41
new hazard checks covering a longer safe detour, visited-safe terrain, equal
observations with different hidden traps, essential crossing, optional ammo
abandonment, activation/damage memory, invisible retreat, competing food goals,
door changes, ascent invalidation and replayed no-path recovery. Existing hidden
state noninterference, deterministic replay, contact, telemetry parity, failure
handling and all-stream serial/parallel tests remain. The complete constructed
Lord/Yendor/ascent escape still passes.

New survival tests cover anonymous first contact, temporary disappearance,
reengagement, mixed species, actual movement versus stationary melee, resources,
player/environmental kill credit, known versus first-discovery summoning, known
entry during an episode, subsequent descent, inclusive initial action counting,
25/50/100-turn death horizons and censoring. Thirty-two paired equal-start
production phantom arenas have identical HP, equipment, RNG and preferences
except routing. Both arms produce identical encounter outputs. This isolates
unchanged tactics without a navigation hazard, not phantom balance or all tactics.

Native formatting tests cover multi-chunk words, capitalization, suffixes, item
names, numeric boundaries and pagination. Three appended AVM fixtures cover an
invisible monster, cursed sight and blindness during trap entry. All original
78 cases remain. Benchmark helper tests reject invalid captures and matching
incomplete suite comparisons. Reporting tests reject failures, duplicates,
incomplete batches and changed hashes, and check schema/interval consistency.

Survival schema 1 adds encounters and milestones to the existing competence
report pipeline. Core CSV schema remains 7; exploration becomes 6. Telemetry is
host-only outcome attribution and never informs policy or saved state.

An encounter starts at an actual PlayerAttack/MonsterAttack event, including a
miss or anonymous invisible attack. Proximity alone does not start one. Tracked
monster slots use death/spawn lifecycle events. `species_mask` uses bit `1 <<
public type ID`; category counts can overlap for mixed episodes. `participants`
counts multiple same-species enemies; `mixed` marks different species or ambiguous
polymorph attribution. Closure occurs when all participants die, at floor/run
exit/death, or after six quiet turns with no tracked live enemy within Manhattan
distance two. Temporary disappearance stays together. Teleportation can close
after the quiet interval; later reengagement becomes a new episode.

Only the first actual phantom contact per run has `first_phantom=1`. A phantom
joining an existing mixed episode gets its own HP/max HP, equipment, light,
visibility and supplies snapshot. Whole-episode costs and damage/resources from
phantom contact are separate. Net HP loss is actual contact HP minus exit HP,
possibly negative after healing. Damage is incoming event amount. Healing use
includes healing/experience potions; identified starting healing reserves count
healing potions. Escape units include identified teleport, fear and invisibility
consumables; food is carried stack quantity.

`actions` includes the initiating submitted action through closure. `combat_turns`
counts active-episode production Turn events, including quiet disengagement turns;
event ordering can omit an opening enemy turn or closing player-kill turn.
`movement_turns` requires Move and a changed player position, excluding stationary
melee/door attempts; displacement within a move can count. `phantom_kills`
includes observed environmental deaths; `phantom_player_kills` records player
credit from the existing damage flag/direct defeat effects. Intentional contact
requires explicit visible melee or a ranged/wand action; accidental invisible
probing is not labeled intentional. These are submitted-action semantics.

Known-trap association requires legitimate prior knowledge and entry in the
preceding eight turns or during the episode. Summoning association includes
actual player/monster activation in that window, including unknown first entry.
Following-hazard damage uses an eight-turn window. These are temporal associations,
not proof that a particular summon caused damage. Entry counters reset each
regenerated visit. Feature `repeated` includes second activation after unknown
first discovery, unlike the narrower repeated-already-known entry metric.

Follow-up 25/50/100-turn survival is measured after closure, with in-episode death
as failure. Unfinished horizons/simulator failures are censored (-1); escape is
an absorbing survival outcome. Later descent is tracked separately. Final reports
reject simulator failures, never relabel them as deaths. Conditional contact
cohorts select different HP, equipment, light, floor and enemy mix between policies.

Milestones are unique per run: floor 1 entry/completion, reaching 3/5/8/10/12/15,
Lord contact/defeat, Yendor, ascent start, successive ascent floors and escape.
Snapshots include HP/max HP, level, armor, weapon/enchantment, offhand, food,
identified healing/escape and cumulative actions/turns. Reports show probability
per initial run and intersection-based conditional progress to the next stage.
Equipment remains histograms, not an arbitrary aggregate tuning score.

Reporting-only BFS uses sanitized remembered geometry to count safe-detour
decisions; one long detour may count several. Movement stretch retains the
existing reporting shortest-objective denominator and all observed travel;
it is not optimal-path regret under equal knowledge. Candidate-only no-safe-route,
goal-failure and cycle counters are excluded from paired intervals. Shared actual
entries, reversals, destinations, travel, repeated visits and actions are comparable.

## Actual hotspot and accepted/rejected optimizations

The worst segment is the second crowded trap response, after `[more]` is
acknowledged. Original responses are 38.653/99.299 ms, totaling 137.951 ms of
computation. The existing 100 ms limit applies at each displayed input boundary;
pagination, messages and difficult fixtures were retained.

| Guest function attribution in worst 1,588,776-cycle segment | Cycles | Share |
| --- | ---: | ---: |
| draw_view_terrain | 231,989 | 14.60% |
| Flash draw_status_word | 151,905 | 9.56% |
| move_monster | 113,784 | 7.16% |
| Flash status_word_width | 112,066 | 7.05% |
| advance_monster_action | 104,345 | 6.57% |
| draw_view_objects | 101,655 | 6.40% |
| can_see | 77,275 | 4.86% |
| status_word | 58,392 | 3.68% |
| activate_monster_trap | 53,496 | 3.37% |
| feature_at | 40,096 | 2.52% |

These are instruction-attributed totals, not additive inclusive call costs.
Rendering is substantial; the width/draw/status chain provides a small, testable
optimization opportunity. Feature lookup is not dominant. No monster scheduler,
spatial cache, combat, trap RNG draw or activation-order rewrite was introduced.

Accepted optimization 1 retains word length from width measurement, then uses
SDK address-space-aware native memcpy for bounded 24-byte drawing chunks,
removing a second VM word scan. The 25-byte scratch buffer unwinds before paging.
Text, widths, whitespace, capitalization, suffixes and ordering are unchanged.
This alone measured 95.502 ms.

Accepted optimization 2 formats witnessed monster-trap names using the visibility
and detection proof just obtained by the caller. No movement, effect aging or
equipment mutation intervenes. `status_visible_monster` avoids a redundant
geometry/light/detection query in generic entity formatting. Hidden/blind branches
retain their existing generic/suppressed messages. Native feedback retains the
old monster_status adapter. The combined result is 92.773 ms. No visibility result
is cached across turns or input decisions.

A bitset character-width experiment regressed to 113.095 ms and was rejected.
A bounded-copy width scan regressed the first optimization to 96.562 ms and was
rejected. Their source/logs remain ignored artifacts. No final case is slower;
case medians improve 48.693 to 47.091 ms. Worst savings are 6.526 ms (6.57%),
raising response headroom from 0.702 to 7.227 ms. Three fresh final-ELF repeats
each measured exactly 92.7726875 ms.

## Equivalence, stack, RAM, flash and FX

Original production was separately compiled with the identical final 81-case
manifest. `compare_turns.py` rejects incomplete/differing coverage, failed timing,
different gameplay fields/RNG, full saved Game bytes, endpoints, final display
or any pagination framebuffer. All 81 agree. Captures occur outside timing.
This is strong representative evidence, not exhaustive proof for every state.

Fixed compatibility explorer reproduces all nine core CSVs byte-for-byte on
1:1000 and historical contact 10001:20000, including action hashes/outcomes and
typed gameplay counters. The latter entry-state CSV also agrees. Fixed oracle
reproduces all nine core CSVs on 1:1000 (57 escapes) with unchanged policy hash.
Both fixed policies have identical full privileged event/observation traces on
seed 42. Device Game captures explicitly include RNG.

| Resource | Before | After | Change |
| --- | ---: | ---: | ---: |
| Complete static stack | 233 B | 239 B | +6 B; 17 B spare |
| Analysis gaps | 0 | 0 | 0 |
| Game / saved data | 812 B | 812 B | 0 |
| Writable .data | 108 B | 108 B | 0 |
| Monster / Item | 8 / 2 B | 8 / 2 B | 0 |
| Save version | 27 | 27 | 0 |
| AVM .text | 89,873 B | 89,895 B | +22 B |
| AVM .rodata | 10,110 B | 10,110 B | 0 |
| Native flash bytes in HEX data records | 29,560 B | 29,560 B | identical contents |
| Interpreter HEX member | 83,157 B | 83,157 B | identical contents |
| FX data | 101,376 B | 101,376 B | size unchanged, code updated |
| FX save | 4,096 B | 4,096 B | identical contents |
| Arduboy ZIP | 189,276 B | 189,276 B | size unchanged |

AVM text/rodata reside in FX program storage; HEX member size is its textual
container size, not raw AVR flash consumption. Six additional stack bytes buy
a measured 6.5 ms reduction without persistent RAM. Ordinary and benchmark
linkers have complete bounds. The root `.arduboy` was rebuilt from final production;
save layout/version remain unchanged. Package info and interpreter/save contents
are identical; FX program contents change as intended.

## Reproduction commands

Run from this project directory. Output directories must be fresh. Exact frozen
commands, source hashes and all preferences are also in delivery/experiment.json.

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake -S . -B build/competence/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build/competence/native --parallel 8
ctest --test-dir build/competence/native --output-on-failure --parallel 4

# Frozen original contact baseline:
build/hazard-routing/reference/ardurogue2_sim.exe --agent explorer-v1 --seeds 1:1000 --jobs 8 --max-actions 50000 --output build/hazard-routing/reproduce-original
# Repeat with control.json; repeat both arms on 1:1000 for initial diagnostics:
build/hazard-routing/delivery/bin/ardurogue2_sim.exe --agent explorer-v1 --agent-params build/hazard-routing/delivery/candidate.json --seeds 10001:20000 --jobs 8 --entry-state --max-actions 50000 --output build/hazard-routing/reproduce-candidate
build/balance-venv/Scripts/python.exe sim/competence_experiments.py --compare build/hazard-routing/delivery/control-10000 build/hazard-routing/delivery/candidate-10000 --policy-comparison --output build/hazard-routing/reproduce-comparison.json
build/competence/native/sim/survival_checks.exe build/hazard-routing/reproduce-equal-start.csv

$taskSdk = 'C:/Users/Brown/Documents/GitHub/avm/build/avm-sdk-install'
# Original 78-case suite:
python build/hazard-routing/reference/bench/profile_turns.py --elf build/hazard-routing/reference/ardurogue2-bench.elf --source-dir build/hazard-routing/reference/src --sdk-root $taskSdk --check --output build/hazard-routing/reproduce-original-avm
cmake -S . -B build/avm-ninja -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=$taskSdk
cmake --build build/avm-ninja --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root $taskSdk --check --output build/hazard-routing/reproduce-avm
# Original production, final fixtures:
cmake --build build/hazard-routing/perf-reference/config --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/hazard-routing/perf-reference/build/ardurogue2-bench.elf --source-dir build/hazard-routing/perf-reference/src --sdk-root $taskSdk --check --output build/hazard-routing/reproduce-reference-avm
python bench/compare_turns.py build/hazard-routing/perf-reference-final/20261011T205647Z-hv0nuf6n build/hazard-routing/perf-final/20261011T205648Z-2bk3x9eg --output build/hazard-routing/reproduce-avm-comparison.csv
```

Native builds omit AVM_SDK_ROOT; no SDK is needed. Python reporting uses existing
NumPy/SciPy/statsmodels dependencies in build/balance-venv. Windows native runtime
DLLs require the UCRT bin PATH above. Benchmark source must match its ELF. All
case LLDB scripts, profiles, snapshots and framebuffers remain under avm-before,
perf-reference-final and perf-final. New benchmark capture commands read sizeof
Game through DWARF, without save offsets.

## Remaining limitations

Known entries are rare relative to all play; first phantom contact usually has
no known summoning association. Essential passages may still require repeated
entries. Unknown traps remain unknown. Hysteresis/deferred surveys change encounter
timing, resources and starvation. Constructed guarantees do not establish lower
population reversals, stretch or deaths; unlimited waiting is not success.

Conditional phantom/milestone cohorts select different starting states and cannot
prove tactical improvement or phantom balance. Exact binomial intervals cover
individual rates; common diagnostic paired mean intervals are approximate Student
intervals. Depth uses paired multinomial bootstrap; exploratory floor tests use
BH adjustment. There is no independent holdout or demonstrated escape gain.
Benchmarks are deterministic representative workloads, not an exhaustive maximum.

<!-- MEASURED_RESULTS -->

## Final policy measurements

Floor numbers in these tables are one-based; raw core/encounter floor columns are zero-based. Both arms have zero simulator failures. Each rate uses its stated denominator; the raw paired report contains all floor entry/exit counts, entry supplies, exploratory floor tests and BH-adjusted q-values.

| Seeds | Contact control | Hazard candidate |
| --- | --- | --- |
| 1:1000 mean / median depth | 4.378 / 4 | 4.511 / 4 |
| 1:1000 phantom terminal deaths | 283 | 246 |
| 1:1000 mean actions | 1,866.550 | 1,967.860 |
| 10001:20000 mean / median depth | 4.4901 / 4 | 4.4433 / 4 |
| 10001:20000 phantom terminal deaths | 2707 | 2684 |
| 10001:20000 starvation deaths | 738 | 790 |
| 10001:20000 mean actions | 1,941.943 | 1,949.764 |
| Escapes (each batch) | 0 | 0 |
| Simulator failures (each batch) | 0 | 0 |

Development depth difference is +0.133 with paired 95% interval [-0.024, +0.288]. Expanded difference is -0.0468 [-0.0987, +0.0048]. The sign reversal cautions against treating development gains as generalization. Each 10,000-run escape rate has exact 95% interval 0–0.03688%; zero observed wins does not establish zero true probability or equal policies.

| 10,000-run navigation total | Control | Candidate | Paired difference/run [95%] |
| --- | --- | --- | --- |
| Known summoning entries | 121 | 98 | -0.0023 [-0.0072, +0.0026] |
| Repeated already-known entries | 53 | 54 | +0.0001 [-0.0040, +0.0042] |
| Damage in following 8-turn windows | 841 | 546 | -0.0295 [-0.0670, +0.0080] |
| Safe detour decisions | 1,308 | 1,226 | -0.0082 [-0.0623, +0.0459] |
| Explicit hazard objective deferrals | 0 | 270 | +0.0270 [-0.0047, +0.0587] |
| Actual entries on essential routes | 0 | 83 | +0.0083 [+0.0043, +0.0123] |
| Observed movement reversals | 838,475 | 875,285 | +3.6810 [-0.5939, +7.9559] |
| Observed destination changes | 9,271,584 | 9,158,567 | -11.3017 [-25.9902, +3.3868] |
| Observed cardinal travel | 17,264,584 | 17,350,163 | +8.5579 [-17.5933, +34.7091] |
| Observed repeated cell visits | 5,350,414 | 5,463,595 | +11.3181 [+1.5233, +21.1129] |
| Exploration turns | 15,521,387 | 15,454,272 | -6.7115 [-30.7303, +17.3073] |
| Combat-action turns | 1,610,298 | 1,601,678 | -0.8620 [-4.7318, +3.0078] |

| Other navigation diagnostics | Control | Candidate |
| --- | --- | --- |
| All player summoning activations | 1638 | 1637 |
| Repeated activations, including unknown first entry | 108 | 87 |
| Monsters spawned by player summoning | 2073 | 2067 |
| Monster summoning activations | 8401 | 8216 |
| Mean reporting movement stretch | 5.156 | 5.234 |
| Mean stair discovery / reach turns | 433.874 / 477.608 | 440.903 / 485.071 |
| Candidate no-safe-route fallback decisions | not instrumented | 103,504 |
| Candidate short-cycle detections / goal failures | not instrumented | 256,265 / 264,201 |

Known entries decrease 121→98 (19.0%), following-window damage 841→546 (35.1%), and repeated activations 108→87 (19.4%). Repeated *already-known* entries increase 53→54; total activations are almost unchanged. Their definitions and selected exposure differ. The candidate still progresses, but does not demonstrate better population backtracking, starvation or efficiency. Cycle detections are handled events in a candidate-only instrument, not a comparative proof that the baseline had zero cycles.

| Encounters associated with a known summoning entry | Control | Candidate |
| --- | --- | --- |
| Episodes / survived | 117 / 107 | 93 / 82 |
| Conditional episode survival | 91.45% | 88.17% |
| Survived 25 / 100 turns after closure | 99 / 89 | 77 / 62 |
| Mean episode damage | 13.316 | 9.570 |

These are different, temporally associated cohorts; their conditional rates are not a causal hazard-survival comparison.

### First phantom contact

| 10,000-run first-phantom diagnostic | Control | Candidate |
| --- | --- | --- |
| Runs reaching contact | 7029 | 6998 |
| Survived episode | 6137 | 6082 |
| Died during episode | 892 | 916 |
| Survived episode, died within 25 turns | 88 | 99 |
| Mixed episodes | 1055 | 1017 |
| Observed phantom death | 5976 | 5920 |
| Player killed phantom | 5974 | 5915 |
| Summoning-associated episodes | 3 | 1 |
| Known summoning-entry association | 0 | 0 |
| Later descended another floor | 5156 | 5010 |
| Conditional episode survival [exact 95%] | 87.31% [86.51%, 88.08%] | 86.91% [86.10%, 87.69%] |
| Survived 25 turns after closure | 6049/7029 (86.06%) | 5983/6998 (85.50%) |
| Survived 50 turns after closure | 5921/7029 (84.24%) | 5855/6998 (83.67%) |
| Survived 100 turns after closure | 5703/7029 (81.14%) | 5623/6998 (80.35%) |
| Net HP loss, mean / median | 8.128 / 7.000 | 8.265 / 7.000 |
| Episode actions, mean / median | 10.526 / 8.000 | 10.439 / 8.000 |
| Active episode Turn events, mean / median | 9.046 / 6.000 | 8.956 / 6.000 |
| Actual Move/displacement turns, mean / median | 4.180 / 2.000 | 4.184 / 2.000 |
| Incoming damage from phantom contact (mean / median) | 10.260 / 8.000 | 10.220 / 8.000 |
| Healing/experience units from contact (mean / median) | 0.101 / 0.000 | 0.096 / 0.000 |
| Escape units from contact (mean / median) | 0.045 / 0.000 | 0.043 / 0.000 |

All follow-up horizons are observed in these completed runs (no censored first-phantom rows). Mean/median HP loss uses the actual phantom-contact snapshot. Episode duration includes the whole mixed episode if another enemy attacked first. Contact-origin damage/resources exclude that earlier interval, but still include other enemies afterward. Full reports also retain whole-episode food and resource costs. Phantom encounters overwhelmingly arise without a known summoning entry, so this is not a direct test of the new route rule. Equal-start arenas show identical tactical outcomes, not a survival advantage.

The following strata use each policy's own contact cohort; none is a matched-state comparison. Equipment entries are public type IDs from `src/model.hpp`; armor is the reported rating, offhand 0 means none, weapon 0 means unarmed. Raw snapshots also retain armor enchantment, level, HP/max HP and supplies.

#### Starting HP as percent of maximum

| Starting HP as percent of maximum | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 25_to_49 | 195/383 (50.91%) | 169/363 (46.56%) | 7.799 | 8.240 |
| 50_to_74 | 979/1262 (77.58%) | 967/1230 (78.62%) | 9.082 | 9.138 |
| 75_to_100 | 4938/5217 (94.65%) | 4918/5229 (94.05%) | 8.043 | 8.199 |
| below_25 | 25/167 (14.97%) | 28/176 (15.91%) | 4.329 | 4.153 |

#### Contact floor

| Contact floor | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 3 | 5227/6043 (86.50%) | 5164/6006 (85.98%) | 8.564 | 8.633 |
| 4 | 820/892 (91.93%) | 826/895 (92.29%) | 5.636 | 6.098 |
| 5 | 90/94 (95.74%) | 92/97 (94.85%) | 3.734 | 5.464 |

#### Weapon type

| Weapon type | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 0 | 204/333 (61.26%) | 173/307 (56.35%) | 13.123 | 12.147 |
| 14 | 1576/1774 (88.84%) | 1639/1837 (89.22%) | 8.220 | 8.297 |
| 15 | 592/750 (78.93%) | 585/773 (75.68%) | 10.329 | 10.723 |
| 16 | 751/875 (85.83%) | 743/873 (85.11%) | 8.547 | 8.913 |
| 17 | 2616/2848 (91.85%) | 2551/2768 (92.16%) | 6.937 | 7.120 |
| 18 | 361/376 (96.01%) | 352/369 (95.39%) | 6.290 | 6.100 |
| 19 | 19/36 (52.78%) | 26/44 (59.09%) | 10.556 | 11.795 |
| 20 | 18/37 (48.65%) | 13/27 (48.15%) | 12.189 | 11.741 |

#### Armor rating

| Armor rating | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 0 | 25/52 (48.08%) | 29/57 (50.88%) | 14.481 | 15.246 |
| 1 | 58/107 (54.21%) | 58/101 (57.43%) | 13.953 | 12.861 |
| 2 | 160/243 (65.84%) | 147/233 (63.09%) | 13.317 | 13.219 |
| 3 | 362/537 (67.41%) | 392/570 (68.77%) | 12.637 | 12.900 |
| 4 | 646/811 (79.65%) | 639/788 (81.09%) | 10.901 | 10.839 |
| 5 | 1212/1401 (86.51%) | 1182/1381 (85.59%) | 9.226 | 9.342 |
| 6 | 1160/1246 (93.10%) | 1120/1215 (92.18%) | 7.020 | 7.453 |
| 7 | 1346/1430 (94.13%) | 1367/1469 (93.06%) | 6.209 | 6.366 |
| 8 | 1013/1044 (97.03%) | 996/1029 (96.79%) | 4.602 | 4.532 |
| 9 | 148/151 (98.01%) | 145/148 (97.97%) | 4.272 | 4.831 |
| 10 | 7/7 (100.00%) | 7/7 (100.00%) | 3.000 | 1.000 |

#### Offhand type

| Offhand type | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 0 | 65/81 (80.25%) | 62/80 (77.50%) | 11.667 | 11.312 |
| 32 | 100/125 (80.00%) | 97/122 (79.51%) | 10.936 | 10.943 |
| 33 | 693/915 (75.74%) | 691/896 (77.12%) | 10.233 | 10.554 |
| 34 | 455/573 (79.41%) | 451/566 (79.68%) | 10.520 | 10.458 |
| 35 | 1/1 (100.00%) | 3/3 (100.00%) | 4.000 | 4.667 |
| 36 | 562/659 (85.28%) | 573/664 (86.30%) | 9.282 | 8.846 |
| 37 | 2159/2406 (89.73%) | 2137/2414 (88.53%) | 7.863 | 8.058 |
| 38 | 2102/2269 (92.64%) | 2068/2253 (91.79%) | 6.341 | 6.605 |

#### Light radius

| Light radius | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 0 | 10/10 (100.00%) | 23/24 (95.83%) | 10.300 | 8.667 |
| 1 | 4478/4956 (90.36%) | 4424/4938 (89.59%) | 7.496 | 7.629 |
| 2 | 108/137 (78.83%) | 101/127 (79.53%) | 11.197 | 10.654 |
| 3 | 848/1077 (78.74%) | 850/1064 (79.89%) | 9.617 | 9.898 |
| 4 | 633/774 (81.78%) | 618/761 (81.21%) | 9.459 | 9.464 |
| 5 | 20/25 (80.00%) | 20/29 (68.97%) | 10.160 | 11.414 |
| 6 | 40/50 (80.00%) | 46/55 (83.64%) | 8.200 | 9.782 |

#### Visible at phantom contact

| Visible at phantom contact | Control survived/contact | Candidate survived/contact | Control mean HP loss | Candidate mean HP loss |
| --- | --- | --- | --- | --- |
| 0 | 5678/6541 (86.81%) | 5623/6502 (86.48%) | 8.222 | 8.348 |
| 1 | 459/488 (94.06%) | 459/496 (92.54%) | 6.865 | 7.171 |

### Progression milestones

Every count below is out of 10,000 initial runs. Conditional progress is to the next named row and uses only runs reaching both. Zero-sized entry cohorts have no conditional estimate. Mean equipment/supply/effort details are retained in the paired JSON; weapon/offhand distributions remain histograms rather than numerical quality scores.

| Milestone | Control n (%) | Candidate n (%) | Control next | Candidate next | HP C/T | Level C/T | Armor C/T | Weapon enchant C/T | Food C/T | Healing C/T | Actions C/T | Turns C/T |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| floor_1_reached | 10000 (100.00%) | 10000 (100.00%) | 86.85% | 86.59% | 18.0 / 18.0 | 1.0 / 1.0 | 0.0 / 0.0 | 0.0 / 0.0 | 0.00 / 0.00 | 0.00 / 0.00 | 0 / 0 | 0 / 0 |
| floor_1_complete | 8685 (86.85%) | 8659 (86.59%) | 88.76% | 88.71% | 18.3 / 18.3 | 2.4 / 2.4 | 3.4 / 3.4 | 0.1 / 0.1 | 1.37 / 1.35 | 0.02 / 0.02 | 434 / 445 | 433 / 444 |
| floor_3_reached | 7709 (77.09%) | 7681 (76.81%) | 51.41% | 50.74% | 24.8 / 25.0 | 4.5 / 4.6 | 5.1 / 5.1 | 0.2 / 0.2 | 2.27 / 2.24 | 0.17 / 0.17 | 916 / 935 | 914 / 933 |
| floor_5_reached | 3963 (39.63%) | 3897 (38.97%) | 33.03% | 32.87% | 34.2 / 34.2 | 9.8 / 9.8 | 6.9 / 6.9 | 0.4 / 0.4 | 3.62 / 3.55 | 0.32 / 0.33 | 1,970 / 1,995 | 1,966 / 1,990 |
| floor_8_reached | 1309 (13.09%) | 1281 (12.81%) | 51.95% | 50.43% | 41.3 / 42.2 | 15.6 / 15.7 | 7.9 / 7.9 | 0.4 / 0.4 | 5.39 / 5.37 | 0.25 / 0.21 | 3,612 / 3,655 | 3,605 / 3,647 |
| floor_10_reached | 680 (6.80%) | 646 (6.46%) | 44.71% | 47.37% | 49.8 / 49.9 | 19.3 / 19.4 | 8.3 / 8.3 | 0.3 / 0.4 | 6.65 / 6.72 | 0.25 / 0.25 | 4,761 / 4,819 | 4,752 / 4,810 |
| floor_12_reached | 304 (3.04%) | 306 (3.06%) | 13.49% | 11.11% | 55.3 / 55.0 | 22.5 / 22.6 | 8.6 / 8.6 | 0.4 / 0.4 | 7.98 / 7.76 | 0.32 / 0.20 | 5,878 / 5,958 | 5,867 / 5,947 |
| floor_15_reached | 41 (0.41%) | 34 (0.34%) | 17.07% | 26.47% | 63.3 / 56.7 | 27.9 / 28.0 | 8.9 / 8.8 | 0.6 / 0.4 | 7.39 / 10.29 | 0.37 / 0.24 | 7,749 / 7,665 | 7,735 / 7,651 |
| lord_encountered | 7 (0.07%) | 9 (0.09%) | 28.57% | 33.33% | 91.7 / 93.1 | 33.9 / 33.3 | 8.7 / 9.1 | 0.9 / -0.2 | 9.43 / 6.11 | 0.14 / 0.00 | 8,968 / 9,092 | 8,953 / 9,077 |
| lord_defeated | 2 (0.02%) | 3 (0.03%) | 100.00% | 100.00% | 71.0 / 69.3 | 34.0 / 32.7 | 8.0 / 9.3 | -0.5 / -1.7 | 15.00 / 2.00 | 0.00 / 0.00 | 8,286 / 9,125 | 8,270 / 9,109 |
| yendor | 2 (0.02%) | 3 (0.03%) | 100.00% | 100.00% | 93.0 / 76.3 | 35.0 / 33.0 | 8.0 / 9.3 | -0.5 / -1.7 | 14.50 / 2.00 | 0.00 / 0.00 | 8,288 / 9,127 | 8,272 / 9,111 |
| ascent_started | 2 (0.02%) | 3 (0.03%) | 50.00% | 66.67% | 93.0 / 76.3 | 35.0 / 33.0 | 8.0 / 9.3 | -0.5 / -1.7 | 14.50 / 2.00 | 0.00 / 0.00 | 8,288 / 9,127 | 8,272 / 9,111 |
| ascent_floor_15_complete | 1 (0.01%) | 2 (0.02%) | 100.00% | 100.00% | 111.0 / 83.0 | 36.0 / 36.0 | 8.0 / 9.5 | -3.0 / -3.0 | 11.00 / 1.50 | 0.00 / 0.00 | 9,717 / 9,523 | 9,700 / 9,506 |
| ascent_floor_14_complete | 1 (0.01%) | 2 (0.02%) | 100.00% | 100.00% | 61.0 / 91.5 | 38.0 / 37.5 | 8.0 / 9.5 | -3.0 / -3.0 | 9.00 / 0.50 | 0.00 / 0.00 | 10,250 / 10,119 | 10,232 / 10,101 |
| ascent_floor_13_complete | 1 (0.01%) | 2 (0.02%) | 100.00% | 50.00% | 132.0 / 96.0 | 39.0 / 39.0 | 8.0 / 9.5 | -3.0 / -3.0 | 8.00 / 0.00 | 0.00 / 0.00 | 10,616 / 10,630 | 10,597 / 10,612 |
| ascent_floor_12_complete | 1 (0.01%) | 1 (0.01%) | 100.00% | 100.00% | 64.0 / 92.0 | 39.0 / 40.0 | 8.0 / 10.0 | -4.0 / -2.0 | 7.00 / 0.00 | 0.00 / 0.00 | 10,936 / 11,235 | 10,916 / 11,215 |
| ascent_floor_11_complete | 1 (0.01%) | 1 (0.01%) | 100.00% | 0.00% | 135.0 / 55.0 | 41.0 / 41.0 | 8.0 / 10.0 | -4.0 / -2.0 | 6.00 / 0.00 | 0.00 / 0.00 | 11,450 / 11,547 | 11,429 / 11,526 |
| ascent_floor_10_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 135.0 / — | 41.0 / — | 8.0 / — | -4.0 / — | 5.00 / — | 0.00 / — | 11,742 / — | 11,720 / — |
| ascent_floor_9_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 128.0 / — | 42.0 / — | 8.0 / — | -4.0 / — | 3.00 / — | 0.00 / — | 12,185 / — | 12,162 / — |
| ascent_floor_8_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 144.0 / — | 43.0 / — | 8.0 / — | -4.0 / — | 2.00 / — | 0.00 / — | 12,621 / — | 12,597 / — |
| ascent_floor_7_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 131.0 / — | 43.0 / — | 8.0 / — | -4.0 / — | 1.00 / — | 0.00 / — | 12,814 / — | 12,789 / — |
| ascent_floor_6_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 139.0 / — | 44.0 / — | 8.0 / — | -4.0 / — | 0.00 / — | 0.00 / — | 13,466 / — | 13,440 / — |
| ascent_floor_5_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 139.0 / — | 44.0 / — | 8.0 / — | -4.0 / — | 0.00 / — | 0.00 / — | 13,831 / — | 13,804 / — |
| ascent_floor_4_complete | 1 (0.01%) | 0 (0.00%) | 100.00% | — | 90.0 / — | 45.0 / — | 8.0 / — | -4.0 / — | 0.00 / — | 0.00 / — | 14,148 / — | 14,120 / — |
| ascent_floor_3_complete | 1 (0.01%) | 0 (0.00%) | 0.00% | — | 35.0 / — | 45.0 / — | 8.0 / — | -4.0 / — | 0.00 / — | 0.00 / — | 14,416 / — | 14,387 / — |
| ascent_floor_2_complete | 0 (0.00%) | 0 (0.00%) | — | — | — / — | — / — | — / — | — / — | — / — | — / — | — / — | — / — |
| ascent_floor_1_complete | 0 (0.00%) | 0 (0.00%) | — | — | — / — | — / — | — / — | — / — | — / — | — / — | — / — | — / — |
| escaped | 0 (0.00%) | 0 (0.00%) | — | — | — / — | — / — | — / — | — / — | — / — | — / — | — / — | — / — |

### Representative discordant traces

- Seed 518: depth 8→12, actions 3160→5265. Paired traces and manifests are retained under `delivery/traces/`. These seeds were chosen as the first development gain/loss with observed known-entry exposure, not from historical escape census seeds.
- Seed 600: depth 5→2, actions 1772→570. Paired traces and manifests are retained under `delivery/traces/`. These seeds were chosen as the first development gain/loss with observed known-entry exposure, not from historical escape census seeds.

Seed 518 explicitly continues a progression route after safe surveys are exhausted and later reaches floor 12; its control dies on floor 8. Seed 600 diverges in ordinary exploration/item timing, and the candidate dies on floor 2 after a wand experiment and nearby rattlesnake/snake combat while the control reaches floor 5. A favorable route guarantee does not neutralize the existing resource/unknown-item tactical weaknesses. These traces explain differing trajectories; they do not identify a causal survival effect of one trap.

## Every AVM benchmark

Milliseconds are the worst measured response segment, including final rendering or the displayed `[more]` boundary. Positive differences are improvements. Every row passes 100 ms and saved-state/RNG/display equivalence. Full precision and raw profiles are retained in `delivery/avm-comparison.csv`, `perf-reference-final/20261011T205647Z-hv0nuf6n/` and `perf-final/20261011T205648Z-2bk3x9eg/`.

| Case | Before ms | After ms | Saved ms | Saved % |
| --- | --- | --- | --- | --- |
| awareness_1 | 74.234 | 73.586 | 0.648 | 0.87 |
| awareness_3 | 78.737 | 78.089 | 0.648 | 0.82 |
| awareness_4 | 79.831 | 79.182 | 0.648 | 0.81 |
| awareness_6 | 88.208 | 87.560 | 0.648 | 0.74 |
| light_full_dense | 77.860 | 77.212 | 0.648 | 0.83 |
| light_blind_dense | 73.656 | 73.010 | 0.646 | 0.88 |
| fuel_out | 45.421 | 43.909 | 1.512 | 3.33 |
| blind_recovery | 46.457 | 44.844 | 1.613 | 3.47 |
| equip_light | 40.268 | 39.672 | 0.596 | 1.48 |
| equip_shield | 41.850 | 41.006 | 0.844 | 2.02 |
| drink_blindness | 48.693 | 46.609 | 2.084 | 4.28 |
| throw_blindness | 58.681 | 55.686 | 2.995 | 5.10 |
| move_room | 36.712 | 36.712 | 0.000 | 0.00 |
| move_corridor | 37.154 | 37.154 | 0.000 | 0.00 |
| move_map_edge | 25.340 | 25.340 | 0.000 | 0.00 |
| move_dense | 75.031 | 74.494 | 0.537 | 0.72 |
| wait | 40.762 | 40.113 | 0.648 | 1.59 |
| wait_dense | 77.860 | 77.212 | 0.648 | 0.83 |
| attack_hit | 44.711 | 44.182 | 0.529 | 1.18 |
| attack_miss | 51.813 | 50.195 | 1.618 | 3.12 |
| attack_kill | 43.817 | 43.118 | 0.699 | 1.59 |
| open_door | 40.883 | 40.345 | 0.539 | 1.32 |
| eat_food | 39.334 | 38.848 | 0.486 | 1.24 |
| drink_healing | 49.065 | 47.091 | 1.974 | 4.02 |
| equip_weapon | 41.312 | 40.574 | 0.739 | 1.79 |
| equip_armor | 40.383 | 39.733 | 0.650 | 1.61 |
| equip_ring | 44.516 | 43.380 | 1.136 | 2.55 |
| equip_cursed_amulet | 56.366 | 53.123 | 3.243 | 5.75 |
| scroll_mapping | 60.555 | 57.172 | 3.382 | 5.59 |
| scroll_teleport | 51.128 | 49.028 | 2.100 | 4.11 |
| drop_food | 42.516 | 41.815 | 0.701 | 1.65 |
| wand_digging | 48.615 | 46.655 | 1.961 | 4.03 |
| arrow_hit | 76.425 | 75.563 | 0.862 | 1.13 |
| arrow_miss | 76.107 | 75.138 | 0.969 | 1.27 |
| arrow_kill | 80.890 | 78.938 | 1.952 | 2.41 |
| arrow_empty | 71.910 | 71.315 | 0.595 | 0.83 |
| equip_bow | 40.867 | 40.238 | 0.629 | 1.54 |
| projectile_picker | 12.144 | 12.144 | 0.000 | 0.00 |
| pickup_food | 45.337 | 44.536 | 0.801 | 1.77 |
| inventory_open_full | 38.565 | 38.565 | 0.000 | 0.00 |
| inventory_down_full | 39.227 | 39.227 | 0.000 | 0.00 |
| inventory_up_full | 45.949 | 45.949 | 0.000 | 0.00 |
| inventory_open_wands | 48.199 | 48.199 | 0.000 | 0.00 |
| inventory_down_wands | 62.828 | 62.828 | 0.000 | 0.00 |
| inventory_up_wands | 60.310 | 60.310 | 0.000 | 0.00 |
| inventory_open_singletons | 29.207 | 29.207 | 0.000 | 0.00 |
| inventory_down_singletons | 36.276 | 36.276 | 0.000 | 0.00 |
| inventory_up_singletons | 36.605 | 36.605 | 0.000 | 0.00 |
| storm_splash | 55.712 | 53.197 | 2.514 | 4.51 |
| glass_break | 49.717 | 48.041 | 1.676 | 3.37 |
| hammer_heavy | 53.853 | 52.034 | 1.819 | 3.38 |
| reprisal | 56.004 | 53.435 | 2.568 | 4.59 |
| phoenix | 54.306 | 51.276 | 3.029 | 5.58 |
| titan_speed | 41.428 | 40.782 | 0.646 | 1.56 |
| trap_spike | 44.591 | 43.706 | 0.885 | 1.98 |
| trap_poison | 44.844 | 43.564 | 1.280 | 2.85 |
| trap_flame | 43.139 | 42.534 | 0.605 | 1.40 |
| trap_frost | 43.331 | 42.824 | 0.507 | 1.17 |
| trap_snare | 44.543 | 43.443 | 1.099 | 2.47 |
| trap_teleport | 49.179 | 47.791 | 1.387 | 2.82 |
| trap_confusion | 43.897 | 42.860 | 1.037 | 2.36 |
| trap_drain | 46.813 | 45.215 | 1.597 | 3.41 |
| trap_summoning | 54.523 | 53.761 | 0.762 | 1.40 |
| trap_rust | 47.712 | 46.310 | 1.401 | 2.94 |
| feature_holy | 67.099 | 66.022 | 1.077 | 1.61 |
| feature_spring | 47.501 | 45.485 | 2.016 | 4.24 |
| feature_forge | 40.742 | 39.608 | 1.134 | 2.78 |
| feature_search | 50.683 | 48.589 | 2.094 | 4.13 |
| feature_dense | 77.547 | 77.011 | 0.535 | 0.69 |
| trap_phoenix | 52.856 | 50.520 | 2.336 | 4.42 |
| monster_trap_multi | 64.320 | 59.292 | 5.027 | 7.82 |
| monster_trap_kills | 63.545 | 58.417 | 5.129 | 8.07 |
| monster_trap_teleport | 57.180 | 54.042 | 3.137 | 5.49 |
| monster_trap_summon | 51.892 | 50.062 | 1.830 | 3.53 |
| monster_trap_force | 71.403 | 66.199 | 5.205 | 7.29 |
| monster_trap_hammer | 38.548 | 37.627 | 0.921 | 2.39 |
| monster_trap_busy | 99.299 | 92.773 | 6.526 | 6.57 |
| monster_trap_spread | 64.551 | 61.368 | 3.183 | 4.93 |
| visibility_trap_hidden | 49.255 | 47.677 | 1.578 | 3.20 |
| visibility_trap_cursed | 49.181 | 47.603 | 1.578 | 3.21 |
| visibility_trap_blind | 41.489 | 40.843 | 0.646 | 1.56 |

## Changed files and retained identities

| Area | Files |
| --- | --- |
| Navigation | `sim/explorer_agent.cpp`, `explorer.hpp`, `explorer_parameters.hpp`, `explorer_checks.cpp` |
| Diagnostics | `sim/survival_diagnostics.cpp`, `survival_checks.cpp`, `survival_report.py`, `metrics.cpp`, `metrics.hpp`, `simulator.cpp`, `manifest.cpp`, `main.cpp`, `CMakeLists.txt` |
| Report compatibility/tests | `sim/competence_experiments.py`, `test_competence_experiments.py`, `test_explorer_cli.py`, `invisible_contact_experiments.py` |
| Production performance | `src/status.cpp`, `status.hpp`, `features.cpp` |
| Benchmark/format coverage | `bench/bench.cpp`, `profile_turns.py`, `compare_turns.py`, `test_profile_turns.py`, `tests/bench_checks.cpp`, `format_native.cpp` |
| Documentation | This report, `docs/EXPLORER_AGENT.md`, `EXPLORER_INVISIBLE_CONTACT.md`, `EXPLORER_SEED_LEDGER.json`, `sim/README.md`, `sim/BALANCE.md`, `bench/README.md` |
| Device artifact | Root `ardurogue2.arduboy` |

Final frozen simulator SHA-256: `245bcd65c2fe83f9f3797cc29c81ab632b0c888e2277a165455f4b31dd2c2faf`.

| Identity | SHA-256 |
| --- | --- |
| Compatibility policy | bab586f64906599805b5cf37d55d04bc13802849e21f1502be86fb745a7ecbf0 |
| Hazard policy | cdb0d63811bbc662a9073c9223499eee5ab00aeacf1815a60feefbd4b74d42a1 |
| Policy code | 74008a6790758666d38afe8a78b47514190f9cecf1a9c952cc8665294869b7be |
| Observation implementation (protocol 3) | 2b481695a582e03669af362e47edbbe214f19118a0fe2daabfee25d55024a818 |

The interface hash is conservative and includes changed host declarations; sanitized observation semantics and the contact state machine are unchanged. Complete parameter hashes/values remain in each manifest. `source-sha256.json` seals the frozen binary source, `artifact-sha256.json` seals analysis/firmware, and each dataset has a raw CSV seal. The contact-experiment script now explicitly disables hazard routing when the executable supports it, preserving isolation of that earlier experiment.

