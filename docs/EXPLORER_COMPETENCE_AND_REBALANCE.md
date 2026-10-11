# Explorer competence and rebalance

This pass improves tactical reasoning and measures its limits. The explorer is
still a weak completion policy: the provisional 0.5–1% policy-only and 2–5%
long-term escape targets are **not achieved**. Improvements in recognition,
early survival and progression do not establish human win probability. The
selected gameplay change is initial monster awareness; monster statistics,
ordinary loot weights and ascent replenishment remain unchanged.

Frozen final development escapes (equipment/full): explorer 0/1 and oracle 477/2144 per 10,000 seeds. Reserved explorer validation has 0/0 escapes. Complete-population escapes are explorer 1/2 and oracle 2937/14150 per 65,535 seeds, with zero failures. See the [measured results](#measured-results-of-the-frozen-implementation), [compact frozen reference](EXPLORER_COMPETENCE_RESULTS.json) and [seed ledger](EXPLORER_SEED_LEDGER.json).

## Reference, observation correction and seed discipline

The starting subrepository was clean at
`f31a2664ba4a9dfb88b14103d8e888d1ab6c6989` (`explorer-v1`). Reference executable,
DLLs, complete parameters, manifests and action hashes were preserved under
`build/competence/reference/`. All six retained 10,000-seed reference datasets
passed their CSV seals; a 32-seed frozen-executable replay matched historical
action hashes. Equipment/full-light oracle escapes were 386/2,023; historical
explorer escapes were 1/1, shield removal 0, and old validation 1.

The historical explorer combined policy hash was
`6027f13c465c1ee91a0c40503681f223576b70e1b3ac76084616ab0e2837aeed`, parameter hash
`5e3635402bba1bca967e854f19139058f33e589c70197c11ef303609bb30afab`, and executable
SHA-256 `ee1ad0c78871f423fa5b718ea4677538939ca042767d8fe2c7c0b0553955b50f`.
Historical oracle policy hash was
`56f6825c65443400482357ea1a3b964c808837f5fdb972c86fdf8c2dff6a485d`.

An audit found a historical status-adapter leak: `status(Monster)` supplied real
species even when firmware printed “something.” Protocol 2 fixes this. Hidden
incoming attacks disclose neither species nor bearing. Public definitions are
supplied only for a legitimately printed species. Confirmation of the agent's
own non-confused attempted melee square remains legitimate. Displayed HP loss
and incoming feedback are separate; authoritative damage events never enter
the policy. Corrected old decision code was rebuilt behind this boundary and
still escaped once in each 10,000-seed lighting condition. Its mean deepest
floors were 4.0379 and 4.5127. Historical boundary claims must be read with this
correction, rather than treating the old reference as perfectly private.

The [seed ledger](EXPLORER_SEED_LEDGER.json) records training/sensitivity 1:1000,
development 1:10000, intervention 30001:32000, and reserved explorer validation
40001:50000. Earlier explorer work had used 1:30000. Earlier oracle censuses had
already used every nonzero seed: **no globally untouched range exists**. The
reserved range is untouched explorer validation at selection time; oracle rows
are reference checks. Final census results were not used for parameter selection.

## Tactical, equipment and navigation changes

The host policy estimates incoming danger over one and two player actions using
observed species, public strength/speed/traits, distances, displayed HP, known
armor/enchantment, conservative jewelry protection, statuses and uncertainty
around recently observed enemies. It distinguishes quiet, caution, serious and
lethal pressure. Weak snakes need not panic a healthy player; a dangerous lone
enemy, multiple attackers, dragon fire or paralysis can trigger action at
substantial HP. Armor and hit estimates are bounded approximations, not combat
roll predictions. No unseen position, live monster HP/effect or future RNG roll
is available. A modeled retreat is not a guarantee against unseen enemies.

Healing now has distinct safe/approach/melee thresholds (41/55/65%). Serious
incoming risk can override percentages; healing generally requires meaningful
missing HP or a useful cure. Known teleport, fear or a disabling attack can take
precedence over inadequate healing. Experience potions can supply recovery and
early progression, accounting for known cursed wisdom. Invisibility is declined
against observed see-invisible threats. Identified but unexamined wands may be
used without pretending their charges/modifier are known. Mass effects, fire
safety and protective accessories retain existing mechanics. Directional control
and ranged attacks require a currently observed unobstructed cardinal target.

Retreat begins earlier, traverses known passable cells, rejects known traps and
visible occupancy, considers enemy speed/connectivity/separation, penalizes
reversal and stops after a bounded sequence. It does not treat unknown cells as
safe routes. Bow attacks stop when adjacent melee makes switching preferable.
Invisible pressure is reassessed after repeated feedback, damage, hunger and
stationary contact, well before the former 128-turn fallback. Confirmed contact
has a 12-turn default budget; unsuitable contact squares are temporarily avoided.
Protection/control, bounded probing and observed repositioning never receive an
incoming attack direction. Mimic revelations invalidate remembered disguise loot.

Offhand evaluation scores the resulting whole equipment configuration. Jewelry
illumination can preserve shield defense; light offers earlier recognition and
information gain, with diminishing returns and fuel value. Physical encounters
raise shield value, while blindness removes the player's immediate sight benefit.
A 30-point switching margin and six-turn cooldown limit wasted switching turns.
The initial contextual coefficient 11 overvalued light and hurt progression;
coefficient 4 was selected from development sensitivity evidence. This is a
measured compromise, not a permanent-light rule. Strong tower shields can remain
useful during exploration; lanterns replace weaker defense when worthwhile.

Frontier information gain casts optimistic rays through remembered terrain,
blocked by known walls/closed doors and clipped to the circular radius. Unknown
cells count more than mapped but unsurveyed cells. Gain is balanced against
travel, visits and commitment. Short healing-spring/fountain recovery detours,
limits on distant optional loot after discovering stairs, and reduced interest
in likely duplicate weapons/armor reduce waste. Critical food/healing/escape
supplies and weapon/ammunition dependencies resist marginal replacements.
Descent food planning accounts for remaining descent and return floors; depleted
players leave known floors earlier. Yendor makes stairs primary while allowing
short useful supply detours. Ascent geometry is freshly observed because floors
regenerate. Public Lord traits favor final-floor clarity, fire resistance, speed
and emergency reserves; no hardcoded Lord route or live HP is used.

These improvements are uneven. A lower stair-discovery time can coexist with
higher movement stretch, which uses a reporting-only objective-route denominator.
Constructed tactical wins do not establish overall completion competence.

## Optimizer and diagnostic infrastructure

There are 101 bounded integer preferences. Valid candidates rank by escapes,
Yendor, Lord kills, weighted progression, fewer actions and finally production
score. Milestones cover surviving floor 1, reaching floors 5/10/15, Lord/Yendor
and ascent exits. Repeated fighting cannot increase milestone credit. Any
simulator failure invalidates a candidate. Mutation cycles through navigation,
tactics, equipment and resources; binary ablation switches and offhand strategy
modes are excluded from mutation. Staged seed expansion revalidates elites,
including final full-range checks. Fitness CSV parsing respects quoted fields
and names columns instead of relying on positional offsets.

Older exploratory searches were retained, not blindly promoted. The pre-boundary
search's single Yendor trajectory was especially weak evidence. Default selection
instead used tactical tests, policy ablations, six equipment strategies and a
specific illumination sensitivity check. A separate post-freeze optimizer run
exercises staged revalidation without changing the validated defaults.

Core telemetry version 7 appends `lord_killed`; exploration version 4 adds combat
turns, danger classes, potion identification, wasted food nutrition, immediate
post-healing death, remaining known resources, recent damage, model-permitted
retreat, initial alert distance/light and first hostile contact. Existing streams
retain floor entry HP/level/armor/supplies, item generation/pickup/use/retention,
kills/damage/statuses, light/shield occupancy, fuel, survey and movement measures.
`OBS` trace lines capture the actual sanitized policy input and danger estimate;
other trace lines remain privileged reporting. Neither telemetry nor tracing
changes game state, RNG or selected actions.

Diagnostic flags overlap and are **not causal classifications**. Early short
deaths suggest limited counterplay but do not prove inevitability. Carrying a
teleport/control item does not establish that using it would have saved the
player. Most deaths lack identified healing, so late healing underuse alone
cannot explain the low escape rate. Generated-on-visit exposure reflects survival
and differs from pickup and use. Conditional floor entry tables contain survivor
selection; late Lord/dragon/ascent samples remain sparse.

## Awareness design and selected gameplay change

`MON_MEAN` describes hostility; `MON_AGGRO` now retains initial detection.
Ordinary unalerted hostile monsters require a clear bounded ray within
max(2, physical illumination + 1). Phantom/dragon/Lord minimum ranges are 3/4/6.
Adjacent hostility remains dangerous in darkness. Blindness hides the player's
view but does not extinguish worn light. Alert persists after light switching;
invisibility still suppresses pursuit for enemies without see-invisible traits.
Fear/confusion/stun retain existing movement/action handling. Passive bats and
unrevealed mimics are not automatically alerted. Player attacks, summoned enemies
and polymorph use the existing immediate-alert paths; generation/replacement
clears fresh creature state. No extra monster timer, target coordinate or saved
byte is introduced. Detection scans one short ray only for an unalerted enemy.
Dragon fire still requires pursuit, a clear cardinal ray, range at most five,
existing invisibility semantics and the original fire roll.

The candidate was tested independently under both agents and both lighting
conditions. `legacy-awareness` restores historical detection; `light-awareness`
selects current behavior, also the final default. Each causal pair uses one
frozen executable and identical policy/configuration. Darkness now offers reduced
initial exposure, offset by poorer player recognition; persistent pursuit prevents
extinguishing a torch from resetting combat. Selection rests on this encounter
tradeoff and secondary progression evidence, not a claimed escape target.

An awareness batch exposed oracle seed 1461 repeatedly swapping depleted candles.
The oracle now rejects replacing a carried light by one that improves neither
capability nor reserve fuel. This changes its policy hash and is tested in both
arms of fresh comparisons. The failed batch was invalidated, never converted to
a death. Old-content behavior and action hashes are checked against the preserved
reference wherever comparable. An earlier explorer no-switch experiment similarly
exposed seed 169's acquisition/retention mismatch; it was repaired and rerun.
Superseded prototype batches remain under build and are excluded from final data.

## Additional balance experiments and decision

Intervention seeds 30001:32000 test one opening snake replaced by a passive bat,
phantoms replaced by zombies, one dragon per visit replaced by a griffin, and
generated bucklers replaced by food, healing or teleportation. These are
same-location diagnostic contrasts: sixteen ordinary item slots, artifact
schedules and RNG draws before intervention remain intact. There is no RNG
resynchronization after trajectories diverge. They are not all proposed shipping
changes; removing every phantom is deliberately a strong bottleneck probe.

Phantom replacement materially improves progression, confirming a bottleneck
after the policy response changes, but removing this distinctive enemy is rejected.
Opening snake replacement and food/healing replacements have modest secondary
effects. Sparse escapes do not justify rewriting category weights, and losing
buckler defense is part of each resource comparison. Dragon replacement has
little effect for this explorer because so few seeds reach dragons; that is an
exposure limitation, not evidence that dragons are easy. No extra ascent loot
is accepted: ordinary Yendor/ascent trajectories are too rare to establish its
tradeoff. Armor/HP/damage/XP, starting HP, fuel, lighting bonuses, shields, curse
penalties, artifacts and ordinary loot frequency are not rebalanced to hit a
percentage. Ring of Invisibility remains an artifact.

## Hardware, regression and limitations

Implementation file map (paths relative to this subrepository):

| Area | Changed files |
| --- | --- |
| Policy and private observations | sim/explorer.hpp, explorer_agent.cpp, explorer_checks.cpp, explorer_events.cpp, explorer_observation.cpp, explorer_parameters.hpp, frontend.cpp |
| Optimizer | sim/explorer_tune.cpp, tuning.hpp, test_explorer_tune.py |
| Diagnostics and replay | sim/metrics.cpp/.hpp, trace.cpp/.hpp, simulator.cpp, test_explorer_cli.py |
| Experiment controls and schemas | sim/experiment.cpp/.hpp, main.cpp, balance.py, test_cli.py |
| Oracle repair | sim/omniscient_agent.cpp, policy_checks.cpp |
| Experiment/statistical runner | sim/competence_experiments.py, test_competence_experiments.py |
| Production awareness | src/combat.cpp, game_internal.hpp, sim_hooks.hpp |
| Status optimization and native fixtures | src/status.cpp, status_advances.hpp; tests/format_native.cpp, game_native.cpp, awareness_checks.cpp |
| Build and hardware coverage | CMakeLists.txt, sim/CMakeLists.txt, tests/CMakeLists.txt, bench/bench.cpp, bench/test_profile_turns.py |
| Documentation/reference | docs/EXPLORER_AGENT.md, LIGHTING_OFFHAND_BLINDNESS.md, EXPLORER_COMPETENCE_AND_REBALANCE.md, EXPLORER_COMPETENCE_RESULTS.json, EXPLORER_SEED_LEDGER.json; sim/README.md, BALANCE.md |
| Rebuilt game | ardurogue2.arduboy |

Final native verification passes all 23 CTest suites, including 298 explorer
checks, renderer/observation parity, decision noninterference, seeded replay,
trace/telemetry parity, serial/process workers, awareness controls, inventory
regressions, tuning and statistical-report failure handling. Awareness fixtures
cover radii 1/3/4/6, walls/doors, adjacency, persistent alert, invisibility,
passive/disguised enemies, attack alerts and impairments. Existing summoning,
polymorph, fire, traps, generation and item tests remain active.

All 78 final AVM cases pass 100 ms. Worst sample is 99.299 ms (crowded monster
traps), versus historical 97.907 ms. New unalerted dense cases at radii 1/3/4/6
take 74.234/78.737/79.831/88.208 ms. The complete static stack bound is 233/256
bytes, down from 250, with zero analysis gaps. The desired below-240 stack goal
is achieved; the desired below-95 ms timing headroom is **not**. Several rejected
status-rendering candidates exceeded timing or stack limits and were reverted.

Status width measurement uses a flash-resident 95-byte advance table for the
fixed BR5D gameplay font, checked against the SDK font before AVM benchmark
setup. Bounded 24-character draw scratch is released before paging redraws.
Text/article/punctuation/capitalization and paging correctness tests pass.
Saved Game/Monster/Item remain 812/8/2 bytes, ordinary globals 108 bytes, save
version 27. Final ELF text/rodata are 89,873/10,110 bytes (99,983 combined), versus
88,839/10,015 (98,854). FX data is 101,376 bytes, up 1,024; FX save remains 4,096.
The root `.arduboy` was rebuilt. Removing the new awareness event produces
identical ordinary optimized AVM IR; no host telemetry storage reaches firmware.

The explorer host state is 540,016 bytes. Its danger model is approximate, not
a solver; resource scarcity, invisible contact, uncertain loot and many poor
combat opportunities remain. Secondary gains are modest and not uniform.
Completion targets remain unmet. Future work should improve observed combat
engagement/identification decisions and evaluate narrowly bounded phantom
counterplay before further global balance changes. Preserve this final census
as validation evidence rather than mining it repeatedly for new defaults.

## Reproduction and artifact map

Use fresh output directories and the bundled/native GCC runtime DLLs. On Windows
the tested host configure uses Ninja and `C:/msys64/ucrt64/bin/g++.exe`; prepend
`C:/msys64/ucrt64/bin` to PATH. Set `BALANCE_PYTHON` to an absolute interpreter
path that imports numpy, scipy and statsmodels to enable all analysis tests.
This run used `build/balance-venv/Scripts/python.exe`.

```sh
cmake -S . -B build/competence/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build/competence/native
ctest --test-dir build/competence/native --output-on-failure
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite awareness --seeds 1:10000 --jobs 12 --output build/reproduce-ab
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite ablations --seeds 1:1000 --jobs 8 --output build/reproduce-ablations
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite offhand --seeds 1:1000 --jobs 8 --output build/reproduce-offhand
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite balance --seeds 30001:32000 --jobs 8 --output build/reproduce-c
python sim/competence_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --suite validation --seeds 40001:50000 --jobs 8 --output build/reproduce-validation
build/competence/native/sim/ardurogue2_sim.exe --agent explorer-v1 --all-seeds --jobs 12 --no-telemetry --max-actions 50000 --output build/reproduce-census
```

Repeat the census for both agents and both lighting conditions; full lighting
adds `--intervention full-light`. Do not promote new defaults from these runs.
Device configure/build/profile follows AGENTS.md with the AVM SDK root and a
Ninja build directory. All original benchmark coverage is retained:

```sh
cmake -S . -B build/competence/avm -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=C:/Users/Brown/Documents/GitHub/avm/build/avm-sdk-install
cmake --build build/competence/avm --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root C:/Users/Brown/Documents/GitHub/avm/build/avm-sdk-install --check --output build/reproduce-bench
```

Raw evidence is under ignored `build/competence/`: `reference-audit.json`,
`strict-legacy-equipment/full`, `context-balanced-10000`, `ablations-shipped`,
`offhand-shipped`, `final-ab`, `final-c`, `holdout`, `census`, `optimizer-verified`,
`shipped-benchmarks`, `shipped-avm-build.log` and `hooks-proof`. CSVs are sealed;
manifests retain complete parameters, policy identity, seed counts and options.
Paired reports include exact low-count intervals, discordances, absolute/relative
effects, deterministic paired bootstrap intervals and exploratory BH adjustment.
Cross-policy comparisons are explicitly descriptive. Census percentages are
exact for the finite 65,535-seed population and have no sampling intervals.

## Measured results of the frozen implementation

All reported complete batches have zero simulator failures. Floors below are one-based. Development comparisons use seeds 1:10000 and 50,000 maximum actions. Escapes remain too sparse to establish improved explorer completion.

### Stage A: policy-only comparison

| Lighting | Corrected old escapes | Improved escapes | Old mean floor | Improved mean floor | Paired depth difference, 95% CI |
| --- | --- | --- | --- | --- | --- |
| equipment | 1/10000 | 1/10000 | 4.0379 | 4.1607 | +0.1228 [0.0693, 0.1748] |
| full | 1/10000 | 0/10000 | 4.5127 | 4.8684 | +0.3557 [0.2984, 0.4134] |

Equipment: escape difference +0.00 percentage points (relative +0%), gains [6119], losses [363], exact McNemar p=1. Improved escape rate exact 95% interval [0.0003, 0.0557]%; paired bootstrap difference [-0.03, 0.03] pp; conservative exact paired difference [-0.0637, 0.0637] pp.

Full: escape difference -0.01 percentage points (relative -100%), gains [], losses [3270], exact McNemar p=1. Improved escape rate exact 95% interval [0.0000, 0.0369]%; paired bootstrap difference [-0.03, 0.00] pp; conservative exact paired difference [-0.0638, 0.0437] pp.

These compare different policies behind the corrected observation adapter and retain legacy monster mechanics. They are descriptive policy comparisons. Stair discovery improves, but objective-route stretch worsens; navigation efficiency is not uniformly improved.

Corrected old-policy identity: `77375ec28a82076dadb3672f4cce5dffd4a81f900713e04cd9a9195373f0cb28`; complete old configuration and adapter build are preserved in `strict-legacy-equipment/full` and `legacy-source/legacy-build`.

### Stage A: offhand strategies and policy ablations

| Offhand strategy, n=1000 | Escapes | Mean floor | Stair discovery turns | Stretch | Seen before melee | Shield turns |
| --- | --- | --- | --- | --- | --- | --- |
| legacy | 0 | 3.977 | 438.8 | 5.82 | 29.8% | 77.5% |
| strong-light | 0 | 3.862 | 413.8 | 5.59 | 45.1% | 60.1% |
| contextual | 0 | 4.010 | 426.3 | 5.73 | 37.9% | 69.3% |
| always-light | 0 | 3.381 | 347.6 | 4.90 | 77.5% | 0.0% |
| always-shield | 0 | 4.118 | 436.7 | 5.81 | 29.5% | 78.7% |
| no-switch | 0 | 3.488 | 361.0 | 5.06 | 69.6% | 20.2% |

| Policy, n=1000 | Escapes | Mean floor | Depth difference vs combined, 95% CI | Stair discovery turns |
| --- | --- | --- | --- | --- |
| combined | 0 | 4.010 | reference | 426.3 |
| without-tactical_risk | 0 | 4.026 | +0.016 [-0.108, 0.138] | 426.2 |
| without-context_healing | 0 | 4.016 | +0.006 [-0.054, 0.066] | 429.1 |
| without-obstruction_gain | 0 | 3.997 | -0.013 [-0.146, 0.128] | 432.5 |
| without-invisible_response | 0 | 4.092 | +0.082 [0.023, 0.146] | 426.6 |
| without-resource_planning | 0 | 3.881 | -0.129 [-0.235, -0.026] | 424.9 |

All ablation/strategy batches have zero escapes and zero failures; the exact 95% upper escape-rate bound for each is 0.368%. Escape tests therefore all have p=q=1. Context improves recognition relative to legacy preference; always-light improves discovery while reducing survival, and always-shield has the best mean floor in this small batch. No individual tactical/healing gain is established by these batch results. Removing resource planning reduces progression; removing the invisible response improves the point estimate. That response remains a weakness despite bounded behavior and constructed tests. The current default was selected before holdout/census, not from these rare escape outcomes.

### Stage B: isolated initial awareness

| Policy / lighting | Legacy escapes | Awareness escapes | Escape difference pp [bootstrap 95%] | Gains / losses | Relative change | Exact p / BH q | Depth difference [95%] |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1-equipment | 1 | 0 | -0.01 [-0.03, 0.00] | 0 / 1 | -100.0% | 1 / 1 | +0.2426 [0.1883, 0.2960] |
| explorer-v1-full | 0 | 1 | +0.01 [0.00, 0.03] | 1 / 0 | undefined | 1 / 1 | -0.0302 [-0.0837, 0.0237] |
| omniscient-v2-equipment | 386 | 477 | +0.91 [0.51, 1.32] | 259 / 168 | +23.6% | 1.237e-05 / 4.949e-05 | +0.5461 [0.4518, 0.6411] |
| omniscient-v2-full | 2023 | 2144 | +1.21 [0.45, 2.00] | 854 / 733 | +6.0% | 0.002582 / 0.005164 | +0.1859 [0.0748, 0.2951] |

| Selected policy / lighting | Escapes / 10000 | Rate | Exact 95% interval | Mean floor | Lord kills / Yendor |
| --- | --- | --- | --- | --- | --- |
| explorer-v1-equipment | 0/10000 | 0.00% | [0.0000, 0.0369]% | 4.4033 | 1 / 1 |
| explorer-v1-full | 1/10000 | 0.01% | [0.0003, 0.0557]% | 4.8382 | 1 / 1 |
| omniscient-v2-equipment | 477/10000 | 4.77% | [4.3605, 5.2061]% | 7.7444 | 661 / 661 |
| omniscient-v2-full | 2144/10000 | 21.44% | [20.6390, 22.2576]% | 11.4151 | 2842 / 2842 |

Each awareness pair has identical policy identity and effective seeds. Oracle progression and escapes improve; explorer equipment-light progression improves, while full-light progression is essentially unchanged. This supports encounter design and modest secondary gains, not the explorer completion target. Low-count individual binomial intervals remain essential when paired bootstrap intervals are degenerate.

| Awareness comparison | Conservative exact 95% escape difference pp |
| --- | --- |
| explorer-v1-equipment | [-0.0638, 0.0437] |
| explorer-v1-full | [-0.0437, 0.0638] |
| omniscient-v2-equipment | [0.2550, 1.5641] |
| omniscient-v2-full | [-0.0111, 2.4302] |

These conservative paired bounds subtract simultaneous 97.5% Clopper-Pearson gain/loss marginal intervals; the union bound guarantees at least 95% coverage without treating gain and loss as independent. They remain nonzero with no observed discordances. They are deliberately wider than bootstrap intervals and are not inversions of the McNemar test. Depth bootstrap intervals are pointwise secondary intervals; floor-milestone tests receive BH adjustment within each exploratory family.

| Awareness condition | Alert distance, capped 8 | First contact turn | Seen before melee | Combat turns / visit | HP damage / visit | Kills / visit | Terminal hunger | Fuel exhausted |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1-equipment-legacy | 7.57 | 37.6 | 37.1% | 34.8 | 33.9 | 8.9 | 171.4 | 7599 |
| explorer-v1-equipment-light-awareness | 2.90 | 49.5 | 39.2% | 35.1 | 32.9 | 8.4 | 170.2 | 7751 |
| explorer-v1-full-legacy | 7.61 | 32.7 | 87.9% | 60.8 | 34.8 | 9.3 | 189.3 | 4848 |
| explorer-v1-full-light-awareness | 6.14 | 35.3 | 91.6% | 65.1 | 34.5 | 9.1 | 189.0 | 4870 |
| omniscient-v2-equipment-legacy | 7.53 | 24.0 | 82.7% | 37.9 | 38.5 | 8.4 | 155.7 | 4449 |
| omniscient-v2-equipment-light-awareness | 4.45 | 28.3 | 85.4% | 37.0 | 36.5 | 7.8 | 154.7 | 4832 |
| omniscient-v2-full-legacy | 7.46 | 23.5 | 86.4% | 43.5 | 35.8 | 8.3 | 155.0 | 767 |
| omniscient-v2-full-light-awareness | 5.83 | 25.2 | 88.6% | 42.5 | 34.6 | 7.9 | 154.5 | 765 |

Alert distance in legacy mode records first implicit hostile tracking, including distant enemies in the capped 8-or-more bin. Combat-turn and first-contact counters are observation-based proxies. Damage/kills are privileged reporting only; per-visit averages and terminal hunger remain affected by survival selection. Longer exploration and fuel use are real costs, not evidence of uniform efficiency improvement. Full-light also increases physical illumination for initial enemy detection.

### Conditional floor survival and entry condition

| Descent floor | Explorer equipment | Explorer full | Oracle equipment | Oracle full |
| --- | --- | --- | --- | --- |
| 1 | 8732/10000 (87.3%) | 8755/10000 (87.5%) | 8739/10000 (87.4%) | 8871/10000 (88.7%) |
| 2 | 7764/8732 (88.9%) | 8046/8755 (91.9%) | 8309/8739 (95.1%) | 8617/8871 (97.1%) |
| 3 | 5458/7764 (70.3%) | 6215/8046 (77.2%) | 7482/8309 (90.0%) | 8264/8617 (95.9%) |
| 4 | 3884/5458 (71.2%) | 4860/6215 (78.2%) | 6788/7482 (90.7%) | 7956/8264 (96.3%) |
| 5 | 2632/3884 (67.8%) | 3464/4860 (71.3%) | 6103/6788 (89.9%) | 7728/7956 (97.1%) |
| 6 | 1901/2632 (72.2%) | 2585/3464 (74.6%) | 5506/6103 (90.2%) | 7589/7728 (98.2%) |
| 7 | 1172/1901 (61.7%) | 1562/2585 (60.4%) | 4634/5506 (84.2%) | 7322/7589 (96.5%) |
| 8 | 895/1172 (76.4%) | 1148/1562 (73.5%) | 4155/4634 (89.7%) | 7181/7322 (98.1%) |
| 9 | 586/895 (65.5%) | 661/1148 (57.6%) | 3550/4155 (85.4%) | 6966/7181 (97.0%) |
| 10 | 488/586 (83.3%) | 561/661 (84.9%) | 3331/3550 (93.8%) | 6879/6966 (98.8%) |
| 11 | 273/488 (55.9%) | 284/561 (50.6%) | 2717/3331 (81.6%) | 6542/6879 (95.1%) |
| 12 | 151/273 (55.3%) | 138/284 (48.6%) | 2174/2717 (80.0%) | 6148/6542 (94.0%) |
| 13 | 64/151 (42.4%) | 73/138 (52.9%) | 1748/2174 (80.4%) | 5630/6148 (91.6%) |
| 14 | 23/64 (35.9%) | 25/73 (34.2%) | 1300/1748 (74.4%) | 4740/5630 (84.2%) |
| 15 | 10/23 (43.5%) | 5/25 (20.0%) | 908/1300 (69.8%) | 3718/4740 (78.4%) |
| 16 | 1/10 (10.0%) | 1/5 (20.0%) | 658/908 (72.5%) | 2833/3718 (76.2%) |

| Explorer equipment descent entry | Mean HP / max HP | Level | Armor | Food units | Healing units | Mean turns / actions |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 18.0 / 18.0 | 1.00 | 0.00 | 0.00 | 0.00 | 398.9 / 399.8 |
| 2 | 18.4 / 22.4 | 2.43 | 3.36 | 1.35 | 0.05 | 444.5 / 445.3 |
| 3 | 24.9 / 29.0 | 4.59 | 5.04 | 2.24 | 0.19 | 405.9 / 406.6 |
| 4 | 29.9 / 37.7 | 7.49 | 6.25 | 2.95 | 0.27 | 430.5 / 431.2 |
| 5 | 33.7 / 44.7 | 9.82 | 6.89 | 3.57 | 0.31 | 416.0 / 416.7 |
| 6 | 36.7 / 50.8 | 11.84 | 7.37 | 4.17 | 0.33 | 465.6 / 466.4 |
| 7 | 38.9 / 56.2 | 13.64 | 7.66 | 4.80 | 0.23 | 449.8 / 450.4 |
| 8 | 41.1 / 62.0 | 15.55 | 7.89 | 5.31 | 0.19 | 493.5 / 494.3 |
| 9 | 48.9 / 67.3 | 17.34 | 8.05 | 5.90 | 0.19 | 503.4 / 504.1 |
| 10 | 50.2 / 73.4 | 19.36 | 8.25 | 6.48 | 0.19 | 499.1 / 499.9 |
| 11 | 59.6 / 77.6 | 20.77 | 8.35 | 6.99 | 0.24 | 454.4 / 455.0 |
| 12 | 55.1 / 83.7 | 22.74 | 8.63 | 7.34 | 0.23 | 460.5 / 461.0 |
| 13 | 58.0 / 89.5 | 24.60 | 8.83 | 7.83 | 0.23 | 396.7 / 397.1 |
| 14 | 63.4 / 95.4 | 26.47 | 9.11 | 8.81 | 0.11 | 388.1 / 388.5 |
| 15 | 63.1 / 101.6 | 28.78 | 9.35 | 9.26 | 0.17 | 340.7 / 341.2 |
| 16 | 65.7 / 105.5 | 30.00 | 9.00 | 11.40 | 0.00 | 535.5 / 535.6 |

Exited/entered is conditional floor-visit survival, including the floor-16 exit with Yendor, not a causal estimate of that floor alone. Entry means condition on reaching the floor; late rows are tiny selected survivor groups. Complete raw entry state is retained with `--entry-state`. Paired floor milestone tests and BH q-values are in each paired JSON; these are distinct from conditional survival.

| Ascent floor | Explorer equipment | Explorer full | Oracle equipment | Oracle full |
| --- | --- | --- | --- | --- |
| 15 | 1/1 | 1/1 | 580/658 | 2548/2833 |
| 14 | 0/1 | 1/1 | 524/580 | 2353/2548 |
| 13 | no entrants | 1/1 | 495/524 | 2238/2353 |
| 12 | no entrants | 1/1 | 484/495 | 2183/2238 |
| 11 | no entrants | 1/1 | 480/484 | 2156/2183 |
| 10 | no entrants | 1/1 | 479/480 | 2152/2156 |
| 9 | no entrants | 1/1 | 479/479 | 2150/2152 |
| 8 | no entrants | 1/1 | 478/479 | 2148/2150 |
| 7 | no entrants | 1/1 | 478/478 | 2147/2148 |
| 6 | no entrants | 1/1 | 478/478 | 2145/2147 |
| 5 | no entrants | 1/1 | 477/478 | 2145/2145 |
| 4 | no entrants | 1/1 | 477/477 | 2145/2145 |
| 3 | no entrants | 1/1 | 477/477 | 2145/2145 |
| 2 | no entrants | 1/1 | 477/477 | 2145/2145 |
| 1 | no entrants | 1/1 | 477/477 | 2144/2145 |

Ascent exits/entries count regenerated floor visits; no entrant is an absence of evidence rather than zero conditional survival. The final departure from floor 1 is escape. Only one explorer Yendor acquisition per development lighting condition makes explorer ascent comparisons extremely uncertain.

### Encounter, navigation and resource diagnostics

| Metric | Old, legacy | Improved, legacy | Selected equipment | Selected full |
| --- | --- | --- | --- | --- |
| Mean stair discovery | 443.0 | 428.8 | 430.8 | 279.8 |
| Movement stretch | 5.46 | 5.77 | 5.78 | 4.90 |
| Seen before melee | 29.5% | 37.1% | 39.2% | 91.6% |
| Surveyed reachable coverage | 65.2% | 69.2% | 70.6% | 82.8% |
| Shield occupancy | 78.7% | 70.1% | 72.0% | 82.1% |
| Offhand light occupancy | 20.3% | 28.6% | 26.8% | 17.4% |
| Radius 1 occupancy | 73.8% | 65.0% | 66.4% | 0.0% |
| Combat turns / visit | 33.1 | 34.8 | 35.1 | 65.1 |
| Exploration turns / visit | 318.6 | 332.6 | 342.8 | 224.0 |
| Unrecognized damage / visit | 9.0 | 8.8 | 8.3 | 8.0 |
| Fuel exhaustions | 1866 | 7599 | 7751 | 4870 |
| Immediate post-healing deaths | 1 | 0 | 0 | 0 |
| Wasted food nutrition | 0 | 0 | 0 | 0 |
| Identified potions | 18110 | 20849 | 22084 | 25285 |

| Selected explorer death flag (overlapping) | Equipment | Full |
| --- | --- | --- |
| insufficient_known_healing | 9997 | 9986 |
| healing_retained | 3 | 13 |
| escape_retained | 497 | 431 |
| control_retained | 1173 | 918 |
| known_retreat_before_death | 3894 | 4516 |
| late_recognition_candidate | 79 | 118 |
| early_contact_low_counterplay | 60 | 99 |
| invisible_attrition | 2813 | 2956 |
| starvation | 700 | 223 |
| boss_failure | 6 | 2 |
| post_yendor_death | 1 | 0 |

| Death cause | Old equipment | Improved legacy | Selected equipment | Selected full |
| --- | --- | --- | --- | --- |
| PHANTOM | 2196 | 2607 | 2813 | 2956 |
| SNAKE | 1895 | 1504 | 1252 | 1288 |
| ORC | 1210 | 1154 | 1122 | 1179 |
| GOBLIN | 1057 | 1100 | 965 | 813 |
| starvation | 603 | 633 | 700 | 223 |
| HOBGOBLIN | 712 | 712 | 689 | 1004 |
| RATTLESNAKE | 567 | 442 | 408 | 337 |
| ZOMBIE | 526 | 416 | 395 | 314 |
| MIMIC | 193 | 213 | 295 | 370 |
| TROLL | 158 | 174 | 215 | 276 |

| Selected equipment resource | Generated on visits | Picked up | Used | Consumed | Carried at termination |
| --- | --- | --- | --- | --- | --- |
| FOOD | 120689 | 83299 | 50621 | 50621 | 32580 |
| HEALING | 27665 | 16681 | 16087 | 15933 | 586 |
| EXPERIENCE | 7396 | 4244 | 3895 | 3866 | 253 |
| INVISIBILITY | 14498 | 8908 | 5996 | 5946 | 1021 |
| SCROLL_TELEPORT | 6958 | 4510 | 4420 | 4420 | 89 |
| SCROLL_FEAR | 6998 | 4562 | 4276 | 4276 | 272 |

Selected equipment: first hostile contact mean 49.5 turns; initial awareness distance mean (capped at 8) 2.90 across 342341 alerts. Radius occupancy counts 0..6: [292759, 12508587, 244837, 4290811, 1333521, 50683, 113507].

Selected full: first hostile contact mean 35.3 turns; initial awareness distance mean (capped at 8) 6.14 across 430694 alerts. Radius occupancy counts 0..6: [57127, 0, 0, 0, 0, 0, 18023765].

Unrecognized damage is reporting damage when the preceding decision observation lacked the attacker; it can include an enemy entering sight during the turn. The old policy did not populate risk/retreat fields; their zeros are not evidence that no safe route or danger existed. Event-based diagnostics, including post-healing deaths, are collected in the corrected-old rebuild. Food/potion generation, pickup, consumption and retention use separate event accounting. Item counts are not nutrition or independent exposure opportunities. Unknown potions may conceal additional healing.

Retained escape/control flags count identified potion/scroll consumables, not wand charges; wand acquisition/activation/charges are retained separately in items.csv and traces. These diagnostic counts are not a complete counterfactual measure of available escape capability.

### Stage C: isolated diagnostic replacements

| Treatment, n=2000 | Escapes | Mean floor | Depth difference vs control [95%] | Escape difference pp | Gain / loss | Exact p / BH q |
| --- | --- | --- | --- | --- | --- | --- |
| control | 0 | 4.4200 | reference | - | - | - |
| early-snake | 0 | 4.4930 | +0.0730 [-0.0460, 0.1910] | +0.00 | 0 / 0 | 1 / 1 |
| phantom | 0 | 5.0890 | +0.6690 [0.5820, 0.7535] | +0.00 | 0 / 0 | 1 / 1 |
| dragon | 0 | 4.4240 | +0.0040 [0.0005, 0.0080] | +0.00 | 0 / 0 | 1 / 1 |
| food | 0 | 4.5370 | +0.1170 [0.0310, 0.2025] | +0.00 | 0 / 0 | 1 / 1 |
| healing | 1 | 4.6940 | +0.2740 [0.1885, 0.3605] | +0.05 | 1 / 0 | 1 / 1 |
| escape | 0 | 4.3225 | -0.0975 [-0.1780, -0.0145] | +0.00 | 0 / 0 | 1 / 1 |

The single healing-replacement escape is not significant (p=q=1); its exact 95% rate interval and paired discordance are retained in the raw report. Phantom replacement improves depth substantially. No additional replacement ships: weak completion, survivor selection, lost buckler defense, and altered post-divergence RNG trajectories limit broad conclusions. Lord/ascent samples do not support a general ascent-generation change.

Healing replacement: gained seed [30839]; exact escape-rate 95% interval [0.0013, 0.2783]%; conservative exact paired difference [-0.2182, 0.3186] pp. Zero-escape/zero-discordance replacements have the same conservative difference bounds [-0.2189, 0.2189] pp.

### Frozen selection and oracle compatibility

- `agent_policy_hash`: `7a76eaec8126c34d7769af79f5886afd6c011e4a26eb75eae6c4f0aa94eceec7`
- `agent_code_hash`: `67b6c61f7b5ed4511927a4f3cef261bc15b389695d6489e95eec737cd1e031a2`
- `agent_parameter_hash`: `4e28b1403e5e622baddbb7275a2a19fa66f33ec2d4f3c02acdc897ef7b24e178`
- `observation_interface_hash`: `bbdcea19ef89c9a2fb23dc4cf7b8e08d05e48f3aecedf8581a9221bc957dbd82`
- Oracle policy: `bcd233ea46edab435d79309d53da3bb2469c33814616d2a4a5676e8f9a4bd440`
- Final host executable SHA-256: `8b11166c60ffd280f83a268ded99620826df68565f05aa04a36b85e68fc2ea3d`
- Final package SHA-256: `5556c4658b484e98bfdae07726c8a34c5e53b16277ef1ba4f9d0db33ccfa2d0c`

Oracle legacy equipment: 0/10000 action-hash changes, 0 outcome changes, and 0 seeds with any common run-field difference versus the archived reference. Seed lists and original-column comparison are retained in `oracle-compatibility.json`.

Oracle legacy full: 0/10000 action-hash changes, 0 outcome changes, and 0 seeds with any common run-field difference versus the archived reference. Seed lists and original-column comparison are retained in `oracle-compatibility.json`.

The post-freeze optimizer demonstration evaluates 16 candidates, expands 100 to 1000 seeds and revalidates four elites; no failures occur. Best candidate 14 has zero escapes, one Lord kill and one Yendor. It is not promoted and was never evaluated on the reserved holdout.


### Paired trace review

Six same-seed awareness pairs were traced in both directions: explorer
equipment 6119 and full 6176; oracle equipment 69/38 and full 5/3. Every trace
matches its batch action hash and outcome. First differing trace actions are
16/261/4/18/35/63 respectively. These differences concern early encounters;
the whole later RNG/action trajectory diverges, so a final death cause is not
a single-action causal explanation. Oracle gains escape where controls die to
incubus/goblin; losses remain lethal dragon-fire encounters. Awareness does not
neutralize dragon alignment or make every seed easier.

Explorer equipment seed 6119 escapes with legacy awareness but dies on ascent
with awareness. Its trace shows twelve summoning activations overall and repeated
entries onto the same observed feature on ascent floor 14, despite HP falling
from 129 to 18. At the final entry it still has 26 visible food units, no identified
healing, radius 1 and the feature in its legitimate sight. The last decisions
recognize serious/lethal danger (8/22 damage estimate), find no modeled retreat
and continue melee; an incubus kills it. This is concrete evidence of a remaining
known-feature revisitation/combat problem, not proof that extra ascent food would
solve it. Explorer full seed 6176 dies against a visible dragon with legacy
awareness and escapes with awareness. Neither single trajectory establishes
general completion competence. Oracle repair seed 1461 now terminates normally
after 3,717 actions, dying to dragon fire rather than a candle swap failure.

Traces, sanitized OBS lines, manifest/action-hash reconciliation and first
divergences are retained under `build/competence/traces/`. No policy/default was
changed after holdout or census inspection. These remaining weaknesses are
reported rather than using validation seeds for a further selection round.

### Reserved validation and final finite-population census

Selection was frozen before these runs. Reserved explorer seeds 40001:50000
were not used for selection. Oracle history had already evaluated every seed;
its rows below are reference checks. All runs use the same final executable,
policy identities and 50,000-action limit. No simulator failures occur.

| Policy / lighting | Holdout escapes / 10000 | Exact 95% rate interval | Holdout mean floor | Census escapes / 65535 | Exact census rate | Census mean floor | Census Lord / Yendor |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1-equipment | 0/10000 | [0.0000, 0.0369]% | 4.3472 | 1/65535 | 0.001526% | 4.3906 | 10 / 10 |
| explorer-v1-full | 0/10000 | [0.0000, 0.0369]% | 4.8135 | 2/65535 | 0.003052% | 4.7986 | 8 / 8 |
| omniscient-v2-equipment | 456/10000 | [4.1594, 4.9873]% | 7.8386 | 2937/65535 | 4.481575% | 7.7822 | 4097 / 4097 |
| omniscient-v2-full | 2146/10000 | [20.6587, 22.2779]% | 11.4231 | 14150/65535 | 21.591516% | 11.3824 | 18534 / 18533 |

Census percentages describe the complete finite seed population, with no sampling confidence intervals. Census raw telemetry was disabled after trace/telemetry action parity was verified; development and holdout retain complete telemetry. Explorer census escape seeds: explorer-v1-equipment: [38700]; explorer-v1-full: [6176, 51523].

[The compact frozen reference](EXPLORER_COMPETENCE_RESULTS.json) retains final hashes, counts and budgets. Raw CSVs, complete parameters and executable/DLL snapshots remain under ignored build directories. The census includes previously inspected ranges and is final validation evidence, not a new untouched holdout.
