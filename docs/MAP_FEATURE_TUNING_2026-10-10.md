# Map-feature tuning — 2026-10-10 pickup-first revision

This revision retains all twenty feature types, twelve three-byte feature slots,
save version 26, and the existing artifact roster, including Ring of Invisibility.
Historical initial measurements remain in [MAP_FEATURES.md](MAP_FEATURES.md).
Earlier exploratory runs in `build/feature-tuning/` used the same tuning with
entry effects preceding pickups; they are retained, but are not final results.

## Implementation

Teleport placement prefers open/junction tiles and proves that the candidate's
passable cardinal neighbors reconnect after removing it inside a 5×5 window.
Scalar 32-bit masks provide a conservative, bounded proof. Closed doors are
traversable topology. Geometry/spacing fallbacks never relax connectivity or
ordinary exclusions. An unplaceable teleport is omitted. Deterministic topology
fixtures and full-floor floods across 4,096 generated floors verify the proof.
Safe long-range alternatives may be rejected by this deliberately local test.

The maintained omniscient-v2 policy blocks observed teleport traps during normal
routing, even when avoiding them requires more than sixty steps. A mandatory
unreachable goal allows one bounded relaxed path calculation. Eight observed
positions and a progress budget abandon looping optional detours. This state
exists only in the host agent. Hidden feature ID and position are erased from
observations; tests exercise changed hidden traps with nonempty history and
verify eventual progress after real activation/displacement.

Item/feature overlaps offer pickups before entry effects. Full-pack swaps and
declines use the same order. Accepted pickup turns and the movement turn occur
after feature resolution; fatal entry effects terminate further turn processing.
New gifts created by a feature still receive their usual pickup offer.
Yendor stays where the Lord drops it and can be collected before teleportation.
Both UI and simulator share the production deferred-entry APIs. The agent's
pickup decisions use item observations, not hidden features. Ordinary item
tiles without features retain their previous behavior.

| Depth | Feature budget | Trap probability among non-holy slots | Roster/effect changes |
| --- | --- | --- | --- |
| 0–1 | 2–4 | 30% | Spike 2–4, dart 1–2 with normal weakness, confusion 3–5; these three traps only |
| 2–3 | 3–5 | 40% | Add flame 3–5, frost 2–4 with normal slowing/MR, snare 1–2, teleport; spike/dart/confusion use full effects |
| 4–15 | 4–8 | 55/95 | Complete ten-trap roster; original full-strength effects |

The named constants live in `world_gen.hpp` and `features.hpp`. Beneficial
objects remain uniform among their nine types; no guaranteed healing or
unrelated combat/supply compensation was introduced.

Holy selection occurs once per generated floor with probability
`min(30, 4 + 2 × generated undead/demonic count)` percent. A selected symbol
uses one feature-budget slot and existing geometry/fallback rules. Repeatable
pulses, damage, fear immunity for the Lord, and legitimate kill rewards remain
unchanged.

The independent generation audit covers seeds **1–10,000**, sixteen depths,
and two Holy rules: **320,000 generated floors**, with original early settings
held fixed to isolate Holy selection. `holy-generation.csv` retains floor,
archetype, unholy count and eligible/preferred geometry bins of 128 tiles.
For **zero unholy monsters**, each rule has **62,892 floors**: old generation
placed **16,664 symbols (26.496%)**, new generation **2,528 (4.020%)**.
For **twelve unholy monsters**, each rule has **20,099 floors**: old generation
placed **18,921 (94.139%)**, new generation **5,652 (28.121%)**. These independent
generation denominators avoid the survivor selection of visited-floor telemetry.

## Memory, stack and performance

| Production measurement | Before | Final | Change |
| --- | ---: | ---: | ---: |
| Saved `.saved` bytes | 809 | 809 | 0 |
| Unsaved `.data` bytes | 106 | 108 | +2 |
| Flash `.text` bytes | 79,329 | 79,863 | +534 |
| Flash `.rodata` bytes | 9,546 | 9,546 | 0 |
| Complete maximum stack bound | 256 | 236 | −20 |
| Worst existing turn benchmark | 96.076 ms | 83.466 ms | −12.610 ms |

The two unsaved bytes defer pickup turns; no saved/per-feature state or
map-sized generation buffer was added. Pickup, full-pack UI, potion throwing,
wand effect phases and retaliation release temporaries before nested turn/status
processing. Terrain rendering rolls three scalar halo rows, uses a fast in-map
row extraction, and shares vertical masks across a row. Existing framebuffer
comparisons pass. The final SDK stack bound is complete with **zero gaps**.
The benchmark image's complete bound also falls from **254 to 236 bytes**.
All **58/58 existing AVM workloads pass 90 ms**, including the unchanged benchmark
coverage. The final package is rebuilt as `ardurogue2.arduboy`.

All **16/16 native suites pass**. Additional pickup-order UI/policy regressions
also pass. Coverage includes all feature effects and protections, hidden
observations, save layout, all four archetypes, artifact scheduling, renderer
snapshots, inventory/swap flows, simulator conservation, parallel CSV merging,
and deterministic interventions. Raw SDK/profile/test logs remain under
`build/feature-tuning/`.

## Experiment definitions and reporting

All final comparisons use the frozen `pickup-first-simulator.exe`, maintained
policy hash `619b6529d0048f5764ad982b6b08e75163aaa1bd0664ea82f1c767211f6acf4c`,
and identical effective seeds. Specifications record executable SHA-256; manifests
record rules and policy. Removal happens after generation at the generated
locations, with no replacement and no RNG resynchronization. Original-setting
variants change only the named generation/effect dimensions and share the policy.

| Label | Controlled comparison |
| --- | --- |
| A | Final vs removal of all teleport traps |
| B | Final vs removal of damaging traps on floors 0–3; status-only traps retained |
| C | No features vs interactive objects only; traps and Holy removed |
| D | No features vs traps only; interactive objects and Holy removed |
| E | Original settings vs conservative teleport placement; early/Holy rules held original |
| F | Safe placement/new Holy with original early rules vs final early rules |
| G | Safe placement/original early rules with original Holy vs new once-per-floor Holy |
| F components | Independently revert early density/category split, damage/status duration, or eligible roster |

Generated features count placed instances on an entered visit. Discovered traps
count legitimate revelations. Activations count entry effects, including repeated
trap/Holy entries; accepted single-use interactions are counted separately from
declines. Effective damage counts HP actually removed after defenses. Direct
trap deaths use production damage attribution. Teleport displacement counts an
actual position change. Holy damage counts eligible monster HP removed.
Secondary-death counts mean a terminal visit with preceding trap exposure and a
different direct death cause; they do **not** establish trap causation.

`feature_experiments.py report` exports typed generation/discovery/activation/use,
damage/death totals, starvation, moves/actions/turns, each floor/direction's entry
and survival counts, successful HP/hunger/level transition sums, teleport rates
per generated/active visit, all >10-entry outliers, and raw CSV SHA-256 seals.
Transition means divide sums by exited visits and remain survivor-selected.
Routine paired comparisons include approximate paired 95% intervals and
BH-adjusted McNemar q-values across the report's comparisons. The full finite
seed census reports exact differences without sampling intervals or p-values.

## Routine sample — seeds 1–10,000

The final treatment records **3,341 / 10,000 escapes (33.410%)** and **zero
simulator failures**. Teleports activate **6,547** times; the same-trap maximum
per floor visit is **2**, with **zero visits above ten entries**. In the preserved
pre-pickup-order exploratory revision it was **187**, with two >10-entry outliers.
Final traces for seeds **2465** and **5270** show Yendor picked up before teleport
activation and both runs escaping. Seed 2465 previously died of starvation after
187 activations on its final-floor teleport.

The final no-feature control matches **all 10,000 run outcomes and action hashes**
and **all common columns in all ten CSV streams**, including 192,308 entry-state
records, of the preserved pre-pickup-order reference in
`build/feature-tuning/no-features/`. Its policy hash changes with the new pickup
interface; that interface is unused when no features exist. The compatibility
audit is retained as `pickup-first-10k/compatibility.json`.

Discordant trace review covers both gained and lost escapes for A, E, F and G.
E seeds 42/57 first diverge after teleport displacement on floors 8/2: seed 42
changes from Troll death to escape; seed 57 from escape to Dragon death. F seeds
16/13 first differ on opening-floor optional goals and later end in Orc death
versus escape, and escape versus floor-10 Troll death. G and A likewise show
both directions of benefit/loss. This is expected when movement changes later
combat and RNG naturally. It does not justify attributing every later death to
the first differing activation. Complete actions/events and first-difference
notes are retained in `pickup-first-10k/audit/`.
<!-- final tables -->

| Variant | Escapes / 10,000 | Escape % | Floor-0 exits / 10,000 | Direct trap deaths | Starvation deaths | Teleport activations |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| original | 2,482 | 24.820 | 8,092 | 796 | 173 | 71,154 |
| placement | 2,723 | 27.230 | 8,167 | 695 | 92 | 6,448 |
| holy | 2,595 | 25.950 | 8,081 | 748 | 110 | 6,879 |
| final | 3,341 | 33.410 | 8,878 | 207 | 117 | 6,547 |
| no-teleport | 3,313 | 33.130 | 8,878 | 190 | 117 | 0 |
| no-early-damage | 3,555 | 35.550 | 8,968 | 150 | 106 | 6,822 |
| interactive | 5,224 | 52.240 | 8,971 | 0 | 0 | 0 |
| traps | 2,441 | 24.410 | 8,971 | 212 | 95 | 5,785 |
| no-features | 4,200 | 42.000 | 9,058 | 0 | 0 | 0 |
| original-density | 2,863 | 28.630 | 8,474 | 359 | 137 | 6,543 |
| original-damage | 3,310 | 33.100 | 8,817 | 257 | 108 | 6,480 |
| original-roster | 3,312 | 33.120 | 8,866 | 222 | 103 | 6,522 |

All twelve variants have **zero SIM_STUCK/SIM_ERROR**. Each difference below is treatment minus control, on the same 10,000 seeds.

| Comparison | Difference pp | Approx. paired 95% interval pp | Gained / lost escapes | BH q |
| --- | ---: | --- | ---: | ---: |
| A no teleport | -0.280 | [-0.731, +0.171] | 251 / 279 | 0.274 |
| B no early damage | +2.140 | [+1.443, +2.837] | 741 / 527 | 2.92e-09 |
| C interactive only vs no features | +10.240 | [+9.361, +11.119] | 1570 / 546 | 4.87e-109 |
| D traps only vs no features | -17.590 | [-18.398, -16.782] | 124 / 1883 | <1e-300 |
| E safe placement | +2.410 | [+1.724, +3.096] | 736 / 495 | 1.19e-11 |
| F early tuning | +7.460 | [+6.594, +8.326] | 1378 / 632 | 9.91e-62 |
| G floor holy probability | -1.280 | [-2.169, -0.391] | 965 / 1093 | 0.00657 |
| F density component | +4.780 | [+3.937, +5.623] | 1176 / 698 | 5.28e-28 |
| F damage component | +0.310 | [-0.247, +0.867] | 419 / 388 | 0.303 |
| F roster component | +0.290 | [-0.468, +1.048] | 762 / 733 | 0.453 |
| complete vs no features | -8.590 | [-9.455, -7.725] | 582 / 1441 | 9.56e-81 |

The full early bundle is retained: placement/routing alone leaves opening survival substantially below the tuned result. Density/category changes provide the clearest independent improvement. Damage/status-duration and roster components have much smaller escape-rate effects; their necessity is not established by these individual comparisons. Their restricted opening effects are retained as the measured depth-specific package, with full strength restored on deeper floors.

### Final treatment feature counts — seeds 1–10,000

| Type | Generated instances | Discovered traps | Activations / accepted uses | Declines | Effective player damage HP | Direct deaths | Holy monster damage HP |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Spike | 53,738 | 24,940 | 34,945 | 0 | 50,965 | 30 | 0 |
| Poison dart | 54,374 | 25,724 | 36,177 | 0 | 36,822 | 28 | 0 |
| Flame | 47,101 | 22,566 | 32,580 | 0 | 114,097 | 82 | 0 |
| Frost | 46,941 | 21,814 | 30,423 | 0 | 101,346 | 51 | 0 |
| Snare | 46,433 | 9,310 | 12,341 | 0 | 0 | 0 | 0 |
| Teleport | 47,000 | 6,539 | 6,547 | 0 | 0 | 0 | 0 |
| Confusion | 54,095 | 25,217 | 35,181 | 0 | 0 | 0 | 0 |
| Magic drain | 41,478 | 19,017 | 26,657 | 0 | 12,982 | 16 | 0 |
| Summoning | 41,736 | 6,645 | 7,414 | 0 | 0 | 0 | 0 |
| Rust | 41,117 | 18,840 | 26,518 | 0 | 0 | 0 | 0 |
| Fountain | 45,566 | 0 | 23,666 | 735 | 0 | 0 | 0 |
| Healing spring | 45,820 | 0 | 20,107 | 2,227 | 0 | 0 | 0 |
| Altar | 44,929 | 0 | 24,008 | 741 | 0 | 0 | 0 |
| Sacrificial altar | 46,057 | 0 | 18,584 | 3,526 | 0 | 0 | 0 |
| Enchanted forge | 45,496 | 0 | 23,687 | 958 | 0 | 0 | 0 |
| Statue | 45,461 | 0 | 24,263 | 788 | 0 | 0 | 0 |
| Sarcophagus | 44,704 | 0 | 23,869 | 806 | 0 | 0 | 0 |
| Runestone | 45,622 | 0 | 23,845 | 1,195 | 20,944 | 0 | 0 |
| Alchemical pool | 45,381 | 0 | 24,163 | 820 | 0 | 0 | 0 |
| Holy symbol | 17,821 | 0 | 4,817 | 0 | 0 | 0 | 3,861 |

Total effective trap damage: **316,212 HP**. Actual teleport displacements: **6,547**. Terminal visits with earlier trap exposure but another direct death cause: **2,920 runs**; this is descriptive, not causal. Generated-teleport visits: **40,664**; activated-teleport visits: **6,386**. Rates are **0.1610** activations per generated visit and **1.0252** per active visit.

### Descent survival and successful transitions — seeds 1–10,000

| Floor | No-feature entries / exits | Final entries / exits | Final survival % |
| --- | ---: | ---: | ---: |
| 0 | 10,000 / 9,058 | 10,000 / 8,878 | 88.780 |
| 1 | 9,058 / 8,877 | 8,878 / 8,667 | 97.623 |
| 2 | 8,877 / 8,565 | 8,667 / 8,346 | 96.296 |
| 3 | 8,565 / 8,337 | 8,346 / 8,094 | 96.981 |
| 4 | 8,337 / 8,084 | 8,094 / 7,798 | 96.343 |
| 5 | 8,084 / 8,024 | 7,798 / 7,699 | 98.730 |
| 6 | 8,024 / 7,885 | 7,699 / 7,501 | 97.428 |
| 7 | 7,885 / 7,800 | 7,501 / 7,340 | 97.854 |
| 8 | 7,800 / 7,666 | 7,340 / 7,137 | 97.234 |
| 9 | 7,666 / 7,614 | 7,137 / 7,036 | 98.585 |
| 10 | 7,614 / 7,472 | 7,036 / 6,790 | 96.504 |
| 11 | 7,472 / 7,231 | 6,790 / 6,445 | 94.919 |
| 12 | 7,231 / 6,948 | 6,445 / 6,107 | 94.756 |
| 13 | 6,948 / 6,526 | 6,107 / 5,537 | 90.666 |
| 14 | 6,526 / 5,931 | 5,537 / 4,882 | 88.170 |
| 15 | 5,931 / 5,292 | 4,882 / 4,244 | 86.932 |

| Floor | No-feature successful entry→exit HP | Final successful entry→exit HP | No-feature successful entry→exit hunger | Final successful entry→exit hunger |
| --- | ---: | ---: | ---: | ---: |
| 0 | 18.00→20.98 | 18.00→21.07 | 220.00→133.86 | 220.00→136.59 |
| 3 | 34.92→40.75 | 35.20→41.07 | 152.58→153.66 | 150.21→152.68 |
| 12 | 76.43→78.84 | 75.49→77.02 | 150.77→151.09 | 151.97→150.96 |
| 15 | 84.72→100.46 | 84.09→98.97 | 151.58→153.19 | 151.63→153.44 |

These transition means condition on successful exits; changes in who survives prevent causal interpretation. Complete descent/ascent counts and transition sums are in the raw paired summary. Moves mean submitted Move actions, including melee and door-opening attempts.

| Variant | Actions | Turns | Move actions |
| --- | ---: | ---: | ---: |
| no-features | 40,454,097 | 40,267,589 | 37,390,319 |
| original | 33,415,556 | 33,285,451 | 31,038,560 |
| placement | 33,038,189 | 32,902,097 | 30,589,540 |
| holy | 32,508,381 | 32,376,173 | 30,108,123 |
| final | 39,688,663 | 39,524,358 | 36,797,633 |
| no-teleport | 39,654,651 | 39,490,510 | 36,758,096 |
| no-early-damage | 41,117,424 | 40,945,798 | 38,128,544 |

## Full census — seeds 1–65,535

| Variant | Escapes / 65,535 | Escape % | Direct trap deaths | Starvation deaths | Teleport activations | Same-trap maximum / visit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| final | 21,698 | 33.109 | 1,224 | 674 | 42,751 | 3 |
| no-features | 27,585 | 42.092 | 0 | 0 | 0 | 0 |

The exact paired full-population difference is **-8.983 percentage points**, with **3,644 gained** and **9,531 lost escapes**. Both variants have **zero simulator failures**. The final census has **0 visits above ten same-trap entries**, **2,067,127 HP** effective trap damage and **24,818 HP** Holy damage. Sampling intervals/p-values are omitted for this complete deterministic seed population.

## Remaining balance concerns and artifacts

The complete feature system still lowers maintained-policy escapes relative to no features. Interactive-only benefits and trap-only costs are large; removal of early damaging traps yields a further improvement. These results do not establish a human win rate or justify eliminating all early risk. Twelve descent floors remain outside the existing broad mortality bands in the final scorecard; opening floors 0–1 are in band. Late-floor effects remain full strength. Normalizing Holy availability has a measurable difficulty cost in G, retained because the old rule made unholy-heavy floors nearly universal symbol floors. No compensating reward was added.

Evidence is retained under `build/feature-tuning/`: `pickup-first-10k/` (twelve 10k variants, scorecard, paired JSON/Markdown, compatibility and trace audits), `pickup-first-census/` (two full-census variants and paired report), `holy-generation.csv` (independent conditioned generation), `bench-final-gift-offer/20261010T222105Z-we69gbxn/` (all 58 final SDK profiles), and `final-provenance.json` (source/artifact hashes and verification logs). Historical reports and raw runs are preserved.

Changed files cover generation (`world_gen.hpp`, `world_gen_population.cpp`), entry/effect/stack phases (`features.*`, `combat.cpp`, `items.cpp`, `model.hpp`, `main.cpp`, `ui.cpp`), terrain rendering (`render.cpp`), host policy/interventions/telemetry and CLI (`sim/*`, `sim_hooks.hpp`), new experiment/audit tools, native/UI/policy regressions, documentation and the rebuilt package. No unrelated artifact or ordinary-generation constants changed.

## Subsequent monster interaction

The original tuned baseline above remains the control for
[monster trap interactions](MONSTER_TRAPS_2026-10-10.md). That implementation
preserves generation, player tuning, save version 26 and artifact availability,
and adds a same-executable `no-monster-traps` control with schema-5 telemetry.
