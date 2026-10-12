# Five-spell and Lightning wand expansion

Validated on 2026-10-12. The retained mechanics use the requested initial
damage, costs and study durations. Lightning can strike and reveal disguised
mimics, as requested during implementation. Invisible monsters, blindness,
out-of-sight positions and obstructed jump paths still prevent secondary arcs.

## Gameplay and metadata

| ID | Spell | MP / HP | Study | Retained effect |
| ---: | --- | --- | ---: | --- |
| 11 | Fireburst | 6 MP | 80 | Range 5; radius-1 square burst, 3-6 fire damage per affected monster/player. Existing fire immunity, vulnerability and MR apply; dragons are immune. |
| 12 | Phase Step | 5 MP | 60 | Prefer a cardinal landing exactly 3 tiles away, then 2; never cross walls, closed doors or edges. Cross monsters/intermediate traps; occupied landings fail. Existing forced-entry handling activates landing traps without automatic pickup/stair use. |
| 13 | Chain Lightning | 6 MP | 80 | Range 5; primary 3-5, one secondary 3-4 within Chebyshev 2. Nearest eligible target, then lowest monster slot. |
| 14 | Slumber | 5 MP | 60 | Range 4; refresh existing stun to max(old, rolled 3-4). The immediate enemy response consumes the first stun action. |
| 15 | Blood Bolt | 3 MP + 3 HP | 60 | Range 5; primary 6-9. Pay raw HP before firing, even on a miss; sacrifice ignores mitigation. Final death prevents bolt/enemy response. Phoenix Heart retains ordinary resurrection behavior. |

All spells use ordinary action/MP rules and the existing paced, interruptible
study lifecycle. Original IDs 0-10, mana capacity/regeneration and study properties
are unchanged. The scrolling Cast Spell menu supports all 16 bits; Blood Bolt's
description and direction prompt disclose its HP cost. Fire uses existing burst
animation; electrical rays/arcs use short jagged lines with no saved animation state.

`SPELL_DEFINITIONS` is the single explicit roster for IDs, flash names/descriptions,
mana/health costs, study duration, targeting and generation category/weight.
Compile-time checks cover all definitions and the existing 16-bit mask.

## Wand and shared lightning

| Modifier | Range / target progression |
| --- | --- |
| Normal | Range 6; 6-10 primary, 4-7 secondary. |
| Cursed | Player-only backfire, 8-12 magical damage through ordinary MR, power 8; no monster ray. |
| Unreliable | Random cardinal direction; normal progression. |
| Spreading | Normal progression from North, East, South, West. |
| Powerful | Range 6; 10-14 primary, 7-10 secondary, 4-7 tertiary. |
| Overpowered | Four stable cardinal rays with Powerful progression. |

Spell and wand effects share `lightning_ray` and flash profiles. A bounded loop
tracks at most three hits per ray and shares a 16-bit hit mask across the whole
activation. Every jump originates at the previous target's captured position,
including after death. A supercover geometric check blocks walls, doors and
diagonal corner cutting. Ordinary monster occupancy does not obstruct arcs.
The caller owns resource payment and scoped damage attribution.

The eighth contiguous wand adds the distinct unknown descriptor `forked` to the
appearance roster. Each run permutes all eight unique descriptors; Lightning is
not permanently tied to one descriptor. Identification, charges, depletion,
crumbling, enchant recharge and curse removal reuse the existing lifecycle.
There are 63 identifiable types in the existing eight-byte bitmap; Item stays
two bytes. Inserting the wand shifts SPELLBOOK's raw ID, so save version 31
rejects older saves without migration.

## Generation and policies

Books retain the isolated 6% replacement stream, locations and exclusions:
food, arrows, light, artifacts/Yendor, ascent and the all-phantom floor. Four basic
spells keep weight 3. In the deeper pool, original six book spells have weight 3,
Phase Step/Slumber/Blood Bolt weight 2 and Fireburst/Chain Lightning weight 1.
Intermediate/advanced metadata currently share that pool; depth bias stays 3.
The initial Magic Dart remains obtainable at run start rather than as generated loot.

Lightning replaces 12% of ordinary wand draws using an isolated derived draw;
location, charges, modifier and the ordinary generation stream remain unchanged.
Same-executable controls restore the original book pool and seven wand types.
Weighted selection can also reshuffle original book identities at fixed slots,
so the spell comparison measures the whole expanded pool, including study and
route effects, rather than isolated new-spell damage.
Tests compare 1,024 seed/floor combinations and retain geometry, features,
monsters, RNG, supplies/artifacts, item locations and wand instance encoding.

Both maintained policies learn/use all five spells and Lightning. They select
safe clusters, safer Phase Step landings, efficient chains and emergency Slumber.
Blood Bolt requires a large HP reserve plus healing/escape supplies; explorer
also uses a 32-turn sacrifice cooldown, while the oracle seeks efficient kills
using actual enemy HP. Known cursed/unreliable wands are avoided; unidentified
individual Lightning modifiers require a backfire reserve in explorer. Both
prepare study food/light and avoid learned/carried duplicates.

Observation protocol 7 withholds remote generic spellbook identity until standing
pickup inspection, then remembers the public name. Inventory names stay public.
Phase Step uses known terrain/traps and observed monsters; public chain prediction
uses observed monsters and known obstruction. Hidden traps, enemies, items and
timers do not enter policy observations. Boundary tests and the separately compiled
policy retain these restrictions. Mimic revelation is an actual effect, not advance
disclosure to explorer's targeting policy.

## Correctness, memory and performance

The final source passes **29/29 native CTest targets**, including 962 explorer
checks, all five spell learning/casting paths, costs/misses/cancellation, fire,
Phase landing/fatal traps, all lightning modifiers, target geometry/ties/death,
invisibility/blindness/mimic revelation, exact stun aging, raw HP sacrifice and
Phoenix resurrection, identification/formatting, UI scrolling and telemetry.
The simulator checks also pass after adding an explicit frequency guard around
the 12% Lightning replacement rate. This test-only addition leaves the simulator
executable hash unchanged.

| Firmware section | Original bytes | Final bytes | Change |
| --- | ---: | ---: | ---: |
| Saved Game | 844 | 844 | 0 |
| Transient static data | 122 | 125 | +3 |
| Code | 100,730 | 104,524 | +3,794 |
| Read-only flash | 12,015 | 12,368 | +353 |

Code plus read-only flash increases by **4,147 bytes**. Three transient bytes hold
sequential lightning scratch; no new Game fields, timers or saved animation state
exist. Native host policy memory is 744,608 bytes and is outside firmware RAM.
Production and benchmark linker analyses both prove **247 bytes maximum stack**,
within 256, with a complete bound and zero analysis gaps.

The complete final-ELF suite passes **123/123 cases at <=100 ms**. The worst case
is `monster_trap_busy` at **94.710 ms**. New cases, in milliseconds:

| Fireburst | Phase landing trap | Chain | Slumber | Blood | Normal wand | Powerful wand | Dense Overpowered |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 47.793 | 50.471 | 54.027 | 44.917 | 49.577 | 63.516 | 62.307 | 69.344 |

Benchmarks include the actual menu/targeting input, gameplay response and final
rendering. Generation and presentation animation remain excluded by convention.
Wand fixtures validate charge expenditure and actual primary/secondary/tertiary
damage rather than treating a charged wand as a fully consumed item.


## Paired balance

Eight full-telemetry runs use effective seeds 1-10,000, the same final executable,
unchanged policies, a 60,000-action cap and entry-state telemetry. Controls are
`expanded-spells=0/1` and `lightning-wand-percent=0/12`; the original 6% book
replacement rate is held fixed. Spells/wand conditions change their identity
pools at existing item locations rather than adding loot. No RNG resynchronization
occurs after trajectories diverge. Every planned arm has 10,000 completed runs
and zero SIM_STUCK/SIM_ERROR results.

Historical 1.466% explorer and 50.764% oracle results used older policies and
are references, not causal controls. The current original-content oracle replay
matches 900 of 1,000 reference seeds in all 34 shared run fields. The other 100
reflect the carried-duplicate-book acquisition fix: sampled seeds 25, 141 and 76
first diverge immediately after acquiring a book, when the old policy seeks another
copy and the updated policy chooses other loot or stairs. This sample gains eight
escapes and loses sixteen. Explorer's remote-book observation correction also
prevents strict historical policy parity. Fresh controls establish both references.

The discarded 20,000-action development batch had six explorer truncations.
Traces showed continuing progress: seed 5056 escaped at 20,430 actions and seed
1252 died normally at 21,579. Both final arms use the same 60,000 budget. Invalid
development batches contribute no balance estimates below.

| Agent | Condition | Escapes / 10,000 | Escape % | Delta pp | Paired 95% CI pp | Gains / losses | Exact p | BH q |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | existing | 132 | 1.320 | - | - | - | - | - |
| explorer-v1 | spells | 139 | 1.390 | +0.070 | -0.210 to +0.340 | 103 / 96 | 0.6707 | 0.6707 |
| explorer-v1 | wand | 128 | 1.280 | -0.040 | -0.140 to +0.060 | 10 / 14 | 0.5413 | 0.6495 |
| explorer-v1 | combined | 142 | 1.420 | +0.100 | -0.180 to +0.380 | 110 / 100 | 0.5347 | 0.6495 |
| omniscient-v2 | existing | 5088 | 50.880 | - | - | - | - | - |
| omniscient-v2 | spells | 4960 | 49.600 | -1.280 | -2.180 to -0.360 | 996 / 1124 | 0.005798 | 0.03479 |
| omniscient-v2 | wand | 5066 | 50.660 | -0.220 | -0.880 to +0.460 | 566 / 588 | 0.5365 | 0.6495 |
| omniscient-v2 | combined | 4978 | 49.780 | -1.100 | -2.040 to -0.160 | 1013 / 1123 | 0.01833 | 0.05499 |

Tests use paired exact McNemar/binomial inference and 2,000 deterministic paired
bootstrap replicates (seed 20261005). BH q-values adjust the six primary content
comparisons together. Detailed floor survival, paired outcome intervals, death
causes, resource shifts and discordant seed lists are retained with each comparison.


| Agent | Existing escapes / 65,535 | Combined escapes / 65,535 | Existing % | Combined % | Delta pp | Gains | Losses |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | 916 | 995 | 1.3977 | 1.5183 | +0.1205 | 744 | 665 |
| omniscient-v2 | 33412 | 32434 | 50.9834 | 49.4911 | -1.4923 | 6445 | 7423 |

The final census covers every nonzero 16-bit seed in existing/combined conditions
for both policies. Its terminal dataset combines the frozen routine outcomes for
seeds 1-10,000, completed rows retained from the interrupted census, and continuation
chunks covering every remaining seed. All sources use the same executable,
policies and 60,000-action budget. Routine sources include full telemetry;
continuations omit detailed telemetry. All 9,231 overlapping retained rows match
in every one of the 34 terminal fields. Source manifests, hashes, exact coverage
and assembly provenance are retained. All four complete arms have zero simulator
failures. These are exact effects on the deterministic seed population;
sampling confidence intervals and significance tests are unnecessary. Spell-only
and wand-only attribution remains the four-condition 10,000-seed evaluation.

Across the full population, the expansion gains 79 explorer escapes (+0.1205
percentage points) and loses 978 oracle escapes (-1.4923 points). The explorer's
1.5183% still falls below the longer-term 2-5% goal. The oracle penalty is a real
tradeoff for this frozen policy/seed population, not sampling noise; broader book
selection, study and routing are part of that content effect. The routine
four-condition screen and discordant traces support retaining the requested
initial mechanics and moderate generation weights rather than tuning damage from
survival-selected cast counts. Original-book dilution remains the main balance
risk for follow-up evaluation.


## Diagnostics and tradeoffs

| Agent | Spell | Generated | Collected | Learned | Casts | Damage | Initial lightning | Chained | Kills | Disabled | Rolled stun actions | HP paid | Sacrifice deaths |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | Fireburst | 1443 | 864 | 633 | 76 | 680 | 0 | 0 | 9 | 0 | 0 | 0 | 0 |
| explorer-v1 | Phase Step | 2702 | 1601 | 1240 | 5204 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| explorer-v1 | Chain Lightning | 1541 | 917 | 686 | 618 | 4522 | 2378 | 2144 | 106 | 0 | 0 | 0 | 0 |
| explorer-v1 | Slumber | 2769 | 1601 | 1259 | 8586 | 0 | 0 | 0 | 0 | 8586 | 30053 | 0 | 0 |
| explorer-v1 | Blood Bolt | 2853 | 1582 | 1205 | 134 | 997 | 0 | 0 | 3 | 0 | 0 | 402 | 0 |
| omniscient-v2 | Fireburst | 2588 | 2107 | 1814 | 811 | 7172 | 0 | 0 | 122 | 0 | 0 | 0 | 0 |
| omniscient-v2 | Phase Step | 4948 | 3568 | 3295 | 4297 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| omniscient-v2 | Chain Lightning | 2901 | 2333 | 1981 | 1797 | 13214 | 6989 | 6225 | 260 | 0 | 0 | 0 | 0 |
| omniscient-v2 | Slumber | 5306 | 3741 | 3425 | 9247 | 0 | 0 | 0 | 0 | 9247 | 32443 | 0 | 0 |
| omniscient-v2 | Blood Bolt | 5251 | 3741 | 3288 | 624 | 3488 | 0 | 0 | 624 | 0 | 0 | 1872 | 0 |

| Agent | Condition | Depth-15 runs | Descent learned mean bounds | Median bounds | Studies | Learned | Interrupted | Study turns |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | existing | 881 | 4.928 to 4.928 | [5.0, 5.0] | 36616 | 29362 | 7254 | 1622041 |
| explorer-v1 | combined | 859 | 5.449 to 5.449 | [5.0, 5.0] | 38808 | 31005 | 7803 | 1740616 |
| omniscient-v2 | existing | 6695 | 6.254 to 6.254 | [6.0, 6.0] | 63674 | 57106 | 6568 | 3227132 |
| omniscient-v2 | combined | 6604 | 7.034 to 7.034 | [7.0, 7.0] | 70995 | 63579 | 7416 | 3687080 |

| Agent | Phase casts | Successful relocations | Total tiles | Runs using Phase | Escapes among users |
| --- | --- | --- | --- | --- | --- |
| explorer-v1 | 5204 | 5196 | 15185 | 1071 | 24 |
| omniscient-v2 | 4297 | 4297 | 12533 | 1706 | 717 |

| Agent | Lightning generated | Picked up | Unique activated | Charges used | Attributed damage | Initial electrical | Chained electrical | Attributed kills |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | 9,392 | 6,670 | 1,196 | 6,162 | 39,582 | 36,267 | 3,315 | 3,989 |
| omniscient-v2 | 14,148 | 13,171 | 10,031 | 58,556 | 417,708 | 376,733 | 40,926 | 37,776 |

| Agent | Condition | Potion uses | Scroll reads | Wand charges | Arrows fired | Bow-equipped turns | Weapon-equipped turns |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | existing | 173572 | 98964 | 29156 | 11515 | 201883 | 50120879 |
| explorer-v1 | combined | 174001 | 99284 | 31529 | 12042 | 202300 | 50172749 |
| omniscient-v2 | existing | 361578 | 80849 | 194248 | 141757 | 299030 | 50546235 |
| omniscient-v2 | combined | 357906 | 80003 | 227182 | 139121 | 292645 | 50942941 |

| Agent | Condition | Reach floor 4 | Reach floor 8 | Reach floor 12 | Reach floor 15 | Reach ascent floor 0 |
| --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | existing | 7931 | 6106 | 3414 | 881 | 144 |
| explorer-v1 | combined | 8024 | 6123 | 3370 | 859 | 161 |
| omniscient-v2 | existing | 9561 | 9387 | 8672 | 6695 | 5089 |
| omniscient-v2 | combined | 9553 | 9402 | 8705 | 6604 | 4979 |

Counts above are totals across the combined 10,000-seed arm, not equal-exposure
item comparisons. Longer survival changes loot exposure and resource use. Study,
learning and Phase-user cohorts are descriptive and selected by survival; they do
not establish per-spell causal benefits. Descent learning bounds account for books
destroyed/sacrificed versus successfully learned, and exclude starting Magic Dart.
Initial/chained damage columns specifically split electrical effects; other spell
damage remains in the total column. Item totals follow existing action-scope
attribution, including indirect effects during the wand's ordinary response;
the electrical counters isolate actual ray/arc hits. Oracle Lightning's attributed
417,708 damage includes 49 beyond its 417,659 electrical damage. Neither this
small residual nor total action-scope kills should be read as additional bolt
damage. Slumber counts affected targets and rolled
stun actions before aging, not additive duration. Phase tiles are landing distance,
not path traversal or counterfactual survival. Existing physical-use diagnostics
are arrows fired and weapon/bow-equipped turns; total monster attack counters
include spells and do not isolate melee-only action counts.

Among runs reaching floor 15, explorer's mean 5.45 additional descent spells and
median five remain within the requested approximate 4-6 target. The oracle's
mean 7.03 and median seven exceed it; its deliberate knowledge and acquisition
policy are a separate cohort rather than a human forecast. Both use the same
unchanged book frequency. Compared with the existing-content arm, combined study
occupies 11.86 more ordinary turns per explorer run and 45.99 more per oracle run
on average. These exposure-selected opportunity costs accompany a broader roster;
they cannot individually explain the paired escape difference.

Combined Blood Bolt sacrifices total 402 HP in 134 explorer casts and 1,872 HP
in 624 oracle casts, with zero sacrifice deaths for either policy. Oracle's 624
direct Blood Bolt kills reflect its intentional low-enemy-HP kill selection;
explorer's three direct kills reflect conservative use without exact enemy HP.
This is evidence of distinct policy behavior, not proof that every explorer
sacrifice was strategically optimal. Fireburst remains infrequently cast and
the spell-only and wand-only telemetry is retained separately for inspection.


explorer-v1 largest death-cause shifts:

| Cause | Existing | Combined | Difference |
| --- | --- | --- | --- |
| TROLL | 1509 | 1585 | 76 |
| fire:DRAGON | 1334 | 1285 | -49 |
| starvation | 676 | 628 | -48 |
| MIMIC | 543 | 591 | 48 |
| ORC | 589 | 629 | 40 |
| TARANTULA | 216 | 182 | -34 |
| PHANTOM | 585 | 615 | 30 |
| ZOMBIE | 268 | 243 | -25 |

omniscient-v2 largest death-cause shifts:

| Cause | Existing | Combined | Difference |
| --- | --- | --- | --- |
| fire:DRAGON | 1497 | 1535 | 38 |
| DRAGON | 790 | 827 | 37 |
| HOBGOBLIN | 110 | 139 | 29 |
| TROLL | 959 | 933 | -26 |
| ANGEL | 101 | 123 | 22 |
| INCUBUS | 259 | 279 | 20 |
| LORD | 376 | 395 | 19 |
| ORC | 77 | 67 | -10 |

## Reproduction and limits

Raw specifications, manifests, hashes, 10,000-seed telemetry, paired reports,
census runs, discordant traces, native logs and AVM profiles are retained under
ignored `build/magic-expansion/`. The principal directories are
`balance-retained-10k`, `census-complete`, `compatibility` and
`benchmarks-retained/20261012T213430Z-a326f58p`.

Routine runs use `--seeds 1:10000 --jobs 8 --max-actions 60000 --entry-state`;
the original census uses `--all-seeds --no-telemetry --max-actions 60000`. The
continuation runner `resume_census.py` executes uncovered contiguous ranges with
`--seeds FIRST:LAST --no-telemetry --max-actions 60000`, then validates and seals
the exact 1-65,535 assembly. `finalize_census.py` verifies complete seed populations,
every routine prefix, policy/executable hashes and provenance before the final
report is generated. Pass each content
control using `--intervention balance:NAME=VALUE`, as listed in sim/README.md.
`sim/balance.py compare` validates policy/seed/budget/schema identity and produces
paired reports. `sim/spell_report.py` supplies all study, electrical damage,
disable, sacrifice, Phase-user and resource diagnostics. Core telemetry is 10;
observation protocol is 7, exploration telemetry 6 and survival telemetry 2.

No damage, cost, study duration, monster/phantom behavior or book-frequency tuning
was retained. Public policies remain heuristics rather than optimal play. Rare
spells and Blood Bolt can have sparse practical-use exposure. The oracle's health
knowledge permits more precise efficient sacrifices; explorer intentionally stays
conservative. Book-pool expansion trades original escape/control-book probability
for new utility at the same generation rate. Long study can still be interrupted
by ordinary gameplay messages. Wand backfire risk remains part of testing an
unidentified individual. The approximately 2-5% explorer escape goal is longer-term;
the measured outcome below that band is not corrected with unrelated monster nerfs.
Sampling uncertainty in the four-condition screen and survival selection in
per-spell cohorts constrain balance interpretation even after the complete census.

explorer-v1 policy SHA-256: `b179bfd175128f35f32a65f39caac097c60cd102fbe86dcdd5ac8510c6360cfc`.

omniscient-v2 policy SHA-256: `f8a632f5461aa6d19b014c11b15ca34b3c5ba3fcb5ce002dc5ccd489a8e92dfa`.

Simulator SHA-256: `6f150a2334e7214c4e94ce2059d8add693ddaf4d537f85124038b4226aebd357`.


Representative discordant trace review:

| Agent | Condition | Seed | Existing outcome | Candidate outcome | First differing action | New spell ID:casts | Lightning activations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | spells | 122 | LORD on floor 15 | escaped | 5089 | 14:51 | 0 |
| explorer-v1 | spells | 433 | escaped | fire:DRAGON on floor 14 | 3583 | none | 0 |
| omniscient-v2 | spells | 3 | fire:DRAGON on floor 14 | escaped | 55 | 13:1, 14:3 | 0 |
| omniscient-v2 | spells | 16 | escaped | DRAGON on floor 13 | 1798 | 13:1, 14:1, 15:1 | 0 |
| explorer-v1 | combined | 56 | INCUBUS on floor 12 | escaped | 177 | none | 5 |
| explorer-v1 | combined | 26 | escaped | fire:DRAGON on floor 14 | 1657 | none | 0 |
| omniscient-v2 | combined | 3 | fire:DRAGON on floor 14 | escaped | 55 | 13:1 | 5 |
| omniscient-v2 | combined | 16 | escaped | TROLL on floor 12 | 1798 | 13:1, 14:2 | 0 |

Explorer spell-only seed 122 first differs on floor 8 at action 5,089: Confound
versus Slumber. Its 51 Slumber casts provide direct evidence of the new emergency
control tactic in a gained escape. Seed 433 loses an escape without casting any
new spell; its earlier acquisition/study/mana decisions change the route. Combined
seed 56 gains an escape with five Lightning activations and no new spell casts;
seed 26 loses on floor 14 without using either new attack family. These are
whole-pool trajectory effects, not proof that an unused spell caused a death.

Oracle seeds 3 and 16 represent a gain and loss in both conditions. First divergence
is an early book-seeking decision versus a healing item. Later chains and disables
do not identify a unique causal action after routing diverges. Raw traces include
all health/resource/action context and were reviewed before retaining constants.
Longer winning traces naturally contain more melee actions: explorer combined
seed 56 rises from 335 to 1,051, while losing seed 26 falls from 1,127 to 672.
These descriptive counts cannot establish replacement of physical combat.

The retained expansion intentionally exchanges some original-book availability
and study opportunity for new tactical options. The routine oracle spell-only
penalty is small but remains a balance risk; neither rare Fireburst/Blood Bolt
usage nor survival-selected learned cohorts justify adjusting their effects yet.
The four-arm screen does not show a broad power increase. No unrelated difficulty
changes were used to force the longer-term explorer target.
