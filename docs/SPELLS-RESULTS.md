# Spell implementation and validation

The complete eleven-spell system is implemented in authoritative gameplay,
firmware controls, the native simulator, explorer-v1 and omniscient-v2. Production
uses 6% replacement of eligible descent loot and advanced-book probability
`min(16, floor + 3) / 16`. Spell costs and numerical effects retain the requested
initial values. The 10,000-seed paired evaluation improves explorer escapes from
0.96% to 1.65% and oracle escapes from 43.39% to 50.78%. Explorer remains below the
desired 2–5% range; this is a measured balance limitation.

## Implemented mechanics

| Spell | MP | Study turns | Effect |
| --- | ---: | ---: | --- |
| Magic Dart | 2 | 40 | Range-5 cardinal ray; first monster takes 2–4 magic damage. Known initially. |
| Frostbind | 3 | 40 | Range-5 cardinal ray; slow first monster for 4–6 ticks. |
| Repulsion | 3 | 40 | Range-5 cardinal ray; push first monster 1–2 tiles. |
| Confound | 3 | 40 | Range-5 cardinal ray; confuse first monster for 4–6 ticks. |
| Sense Traps | 3 | 40 | Reveal active hidden traps within circular radius 4, including full-map markings. |
| Blink | 4 | 60 | Random valid destination within circular radius 3; may cross walls and land on traps. |
| Shockwave | 5 | 60 | Push each initially adjacent monster 2–3 tiles directly outward. |
| Piercing Bolt | 5 | 60 | Range-5 cardinal ray; first two monsters each take 3–5 magic damage. |
| Banish | 5 | 60 | Range-5 cardinal ray; ordinary teleport placement for first monster. |
| Exchange | 5 | 80 | Swap with selected, actually visible monster within circular radius 3. |
| Cleanse | 5 | 60 | Remove one condition: blindness, then confusion, then slowing. |

Characters begin with 6 MP. Maximum is `min(16, 6 + level / 3)`; ordinary turns
regenerate one MP every 16 turns unless starving. Costs are charged once and
spells take one ordinary action, including the ordinary enemy response. Empty
rays, blocked Blink and empty Cleanse still spend resources. Invalid selection,
insufficient MP and cancellation spend no turn, MP or RNG. Paralysis prevents
casting. No classes, proficiency, failure rolls or saved player buff timers were
added. Identify and Remove Curse are documented future possibilities only; the
excluded spell roster is absent.

Rays reuse existing collision, animation and magic damage/rewards. Exchange uses
actual lighting, invisibility, blindness and line of sight. Existing monster
condition counters age normally. There is no monster MR attribute in the current
model; spell rays preserve ordinary wand magic semantics and player MR.

Shockwave snapshots adjacent membership and resolves stable monster-slot order.
Every intermediate forced tile activates its trap once. Walls, closed doors,
occupied tiles, edges and blocked diagonal corners stop movement; obstruction
grants at most two existing stun ticks without accumulating them. Trap death or
teleport stops the original push. Summoned monsters are not added mid-wave.
Forced movement can move normally immobile monsters. Blink scans at most 28
candidates with bounded reservoir sampling. Exchange swaps both positions first,
then resolves the monster's origin trap and the player's destination trap,
including fatal and teleporting outcomes. Banish retains ordinary teleport
landing semantics. Direct magic kills receive ordinary rewards; environmental
trap kills and summoned-monster restrictions retain their existing attribution.

## Study, inventory and controls

The two-byte Item representation stores one SPELLBOOK type and its explicit
spell ID in the full info byte. Books are public, nonstackable ordinary items;
study consumes them only on successful learning. Ordinary discard/sacrifice
rules still apply. Learned duplicates offer no benefit.
Starting reserve is `STUDY_HUNGRY (60) + STUDY_FOOD_MARGIN (12) + duration`, or
112/132/152 hunger units. Study adds no special hunger/fuel penalty.

One update advances exactly one ordinary turn, with monster actions, food, fuel,
effects and MP regeneration, followed by a complete display refresh. A 90 ms
presentation cadence permits B cancellation between turns. The right-hand panel
shows spell, numeric progress, bar and a four-phase rune that changes every turn.
Holding direction buttons cannot postpone the study timer.

Every nonempty gameplay status word, including numeric-only output, flags
interruption. The first word clears study graphics; normal paginated gameplay
text remains visible and the interruption notification follows it. Drawing
progress does not enter this stream. Message/displacement/incapacity/hunger/death
and lifecycle interruption precede final-turn completion, preserve the book and
reset progress. Save/exit and resume cancel transient study while preserving MP
and known spells.

The eight-entry action menu scrolls through seven rows to CAST SPELL. Learned
spells show MP costs and insufficient-MP markers. Directional input and visible
Exchange target cycling are cancellable. Resolution happens after UI selection
frames unwind. The HUD includes MP/max MP and retains sight information. Books
have an inventory group, explicit names/descriptions and a ground icon. Mimic
appearance categories retain their original semantics.

## Agent changes, separately from gameplay

Explorer observation protocol 6 adds public MP, maximum MP, learned bits, study
state, explicit book IDs and stable handles for visible Exchange targets. The
policy object builds without production Game declarations. Observation and
boundary regressions check hidden monsters, traps, map geometry and item data.
Only reported adjacent melee contact supplies an unseen ray bearing; anonymous
attack feedback can justify adjacent Shockwave without inventing a direction.

Both policies acquire unknown useful books, reject learned duplicates, prepare
food/light and require sufficient HP, escape routes, lighting, quiet surroundings
and no known local traps before study. Interrupted study is deferred at least
128 turns. They support emergency Blink/Shockwave/Banish/Exchange, trap-directed
Repulsion, Confound/Frostbind control, two-target bolts, conservative Dart,
local trap detection and harmful-condition Cleanse. They reserve mana for learned
escapes and limit redundant control attempts. Omniscient-v2 retains its deliberate
knowledge advantages but obeys actual target validity and ordinary turn costs.

The first explorer census exposed seed 43779's perpetual stationary fight
against replacement visible monsters. That control contained one SIM_STUCK and
was excluded. A separate public-policy guard counts 64 consecutive attacks at
a still-occupied visible square, then permits control tools or an unobstructed
retreat through even a single known exit. It uses issued actions and sightings,
without live monster HP or a hidden spawning source. The recorded seed now ends
in an ordinary Lord death after 9,235 actions. Constructed replacement-opponent
and reset tests cover the guard; both explorer arms were rerun under its new hash.

Oracle disabled magic reproduces all 1,000 old-content reference seeds exactly in
all 33 shared run-CSV fields. Before the combat-loop repair, explorer also matched
every reference seed in thirteen checked fields. The final public guard changes four
of those explorer seeds (299, 419, 522, 742); the other 996 match all 33 shared fields.
These are recorded policy changes, separate from gameplay. Policy extensions were completed before
the paired comparisons; every causal pair uses matching evaluated policy hashes.

The final production defaults replay the retained oracle candidate's first 1,000
seeds exactly in sixteen outcome/action/resource fields, including action hashes.
Explorer comparisons execute the frozen final-default candidate directly.
Oracle comparisons retain frozen executable SHA-256
`10f760d34acefa5098ec91c3a5925a8fd1849934f4ab4c82e275d7b930fb9cc3`.
Fresh explorer comparisons use
`974a86fb5d84d24495ceb2d54513fef6b11340451e06ee267897bbc68708bc17`.
Complete policy hashes used by the balance runs are:

- Explorer: `897fa7d5b21d882965da46827647d3b7ae51318a614b1586633dc8bf73cfa1be`.
- Oracle: `561dccc729652406768fa32c9f7e6ffd85074f63697487ebad093af0460ea83f`.

Final review removed four redundant includes from the simulator bridge. This
changes the exact source-based explorer fingerprint to
`7b8d9111684427c3187c3a79b64a26286c9b85fbc2b5ba0923e42355b253f665`
without changing gameplay or policy logic. The final native executable SHA-256 is
`36892bfd30ef244d67345508bc8387f93017802983003310f7d794d045678922`.
The final-source native suite passes 29/29 targets; additional four-arm,
1,000-seed comparisons verify equivalence with the retained evaluated binaries.
Raw balance manifests preserve their original fingerprints.

## Memory, build and correctness

Saved fields are appended: one mana byte and a two-byte learned mask. Save version
changes 29 to 30, using the existing incompatible-save rejection policy. Study
state is transient; no individual learned-spell records or saved timers exist.
Constant text stays in flash. Small shared ray/movement scratch avoids deep
automatic allocations and recursive UI/gameplay stacks.

| Measured AVM allocation | Before | Final | Change |
| --- | ---: | ---: | ---: |
| Saved bytes | 841 | 844 | +3 |
| Static data bytes | 110 | 122 | +12 |
| Code bytes (.text) | 93,499 | 100,730 | +7,231 |
| Constant bytes (.rodata) | 10,947 | 12,015 | +1,068 |
| Code plus constants | 104,446 | 112,745 | +8,299 |
| Packaged .arduboy bytes | 193,884 | 202,076 | +8,192 |

Both firmware and benchmark builds prove a complete **247-byte maximum stack**,
with zero analysis gaps. The packaged image SHA-256 is
`4f552e2f58e3770473fdde44b962f3e38f8fa17216000fd1e6ce285929cf06e4`.

The final native suite passes **29/29 targets**, including 918
explorer policy/observation checks, real study rendering/control tests and spell
mechanics. The book audit checks 1,024 seeds on all sixteen floors, including
essential loot, ascent, phantom-floor and artifact preservation. Tests cover all
learning durations, clock wrap, starvation, save compatibility, final-turn
interruption, blockers, visibility, multi-tile collision/traps, fatal Blink,
Exchange traps at both ends, rewards and cancellation determinism. The profiler
unit suite passes 12 tests. Optional LLDB argument backtraces were removed after
an SDK crash in optimized DWARF evaluation; saved call profiles and all typed
state, rendering, pagination and timing validation remain active.

The full final-ELF suite passes **115/115 cases at at most 100 ms**. Its worst case
is monster_trap_busy at **94.710 ms**; scroll_objects takes 94.507 ms. The thirteen
added cases cover every spell, an open multi-enemy Shockwave with intermediate
trap entry, and study progression/interruption through real input/rendering.

| New case | Input-to-ready ms |
| --- | ---: |
| Magic Dart | 44.416 |
| Frostbind | 44.097 |
| Confound | 45.264 |
| Repulsion | 56.766 |
| Piercing Bolt | 52.851 |
| Banish | 45.350 |
| Shockwave | 63.738 |
| Blink | 53.118 |
| Exchange | 51.431 |
| Cleanse | 40.924 |
| Sense Traps | 45.250 |
| Study turn | 38.060 |
| Study interruption | 45.666 |

These are elapsed emulated cycles at 16 MHz. Presentation pacing, generation
and deliberate animation are outside the turn-processing contract. Paginated
turns acknowledge every page and validate final state; their worst input-to-page
or ready segment is reported. Profile directory is
`build/magic-complete-turn-benchmarks/20261012T183300Z-fzg9_t5w`.
The measured benchmark ELF SHA-256 is
`466e08b33ba863b260d51c360bb6927c083ba5fbb73df3c7c3e711842c36aedc`.
The production ELF SHA-256 is
`a995d08e1cbc1696938d8edfe04d09e5cac4e0eb9dd35c39a506057e6030be44`.

## Paired balance evaluation

Routine comparisons use effective seeds 1–10,000, the same frozen candidate
executable in both arms, matching policy hashes and 60,000-action budgets. The
magic-disabled control removes initial Dart and restores the original ordinary
loot at every book slot. A separate Dart-only control restores those same slots
while retaining initial Dart. Isolated book draws leave ordinary generation,
loot positions and artifact schedules unchanged; gameplay RNG is allowed to
diverge after actions differ. Food, arrows, lights, artifacts and Yendor are
excluded from replacement. Ascent and zero-based all-phantom floor 4 gain no
fresh books.

| Agent | Magic disabled | Complete magic | Paired change, pp | Paired 95% CI, pp | Gains / losses |
| --- | ---: | ---: | ---: | --- | ---: |
| explorer-v1 | 96/10,000 (0.96%) | 165/10,000 (1.65%) | +0.69 | +0.41 to +0.96 | 139 / 70 |
| omniscient-v2 | 4,339/10,000 (43.39%) | 5,078/10,000 (50.78%) | +7.39 | +6.41 to +8.49 | 1,740 / 1,001 |

All these runs have zero simulator failures. Intervals are deterministic paired
percentile bootstrap intervals with 2,000 repetitions; primary two-sided exact
McNemar/binomial p-values are 2.09e-06 and 1.16e-45. Practical effects and the
exact population census take priority over those sampling statistics.

The same-slot Dart-only comparison separates starting Dart from book availability:

| Agent | Dart only, original loot | Complete magic | Book replacement/study change, pp | Paired 95% CI, pp |
| --- | ---: | ---: | ---: | --- |
| explorer-v1 | 140/10,000 (1.40%) | 165/10,000 (1.65%) | +0.25 | −0.05 to +0.56 |
| omniscient-v2 | 5,612/10,000 (56.12%) | 5,078/10,000 (50.78%) | −5.34 | −6.260 to −4.34 |

The explorer book change has 132 gains and 107 losses (exact p=0.12).
The oracle book penalty is substantial (909 gains, 1,443 losses;
p=2.51e-28). This experiment jointly changes replacement loot and subsequent
acquisition/study/casting, so it cannot attribute the penalty exclusively to
study time or to a particular spell. Starting Dart provides much of the measured
power increase. Book scarcity, study costs and loot tradeoffs remain consequential;
oracle book valuation/study efficiency remains a balance risk despite tactical
coverage of the full roster. These comparator runs also have zero failures.

The final census covers all 65,535 nonzero 16-bit effective seeds in each arm:

| Agent | Magic disabled | Complete magic | Exact change, pp | Gains / losses | Both escaped |
| --- | ---: | ---: | ---: | ---: | ---: |
| explorer-v1 | 704/65,535 (1.074%) | 961/65,535 (1.466%) | +0.392 | 836 / 579 | 125 |
| omniscient-v2 | 28,364/65,535 (43.281%) | 33,268/65,535 (50.764%) | +7.483 | 11,412 / 6,508 | 21,856 |

All four census arms have zero simulator failures, matching policies within
each pair and the same 60,000-action budget. These are exact deterministic
population effects; sampling confidence intervals and p-values are omitted.
The larger population confirms an explorer benefit smaller than the development
sample and below the 2–5% target.

Censuses explicitly disable detailed typed telemetry in both arms. Their sealed
primary-only analysis retains seed, failure, policy, schema, action-limit and
telemetry-scope validation. Detailed floor, spell and resource findings below
come from the fully instrumented 10,000-seed runs. For each agent and each arm,
the census's first 10,000 seeds match those detailed runs exactly in all thirteen
checked outcome/action/resource fields, including every action hash. Enabling
the collector therefore changes none of those measured outcomes.

The earlier 8%/floor-only distribution under the completed tactical policy
produced 1.43% explorer and 49.28% oracle escapes in valid, internally paired
10,000-seed experiments. Lower replacement protects ordinary loot and earlier
advanced access diversifies usable spells. The retained configuration was
validated afresh; this historical tuning observation is separate from its
same-executable causal comparison above. No costs/effect strengths were increased
to force the explorer result into a target band.

### Learning, study and mana (10,000 runs per agent)

| Metric | explorer-v1 | omniscient-v2 |
| --- | ---: | ---: |
| Books generated | 70,238 | 107,646 |
| Books collected | 39,099 | 63,013 |
| Studies started | 38,938 | 63,679 |
| Spells successfully learned | 31,184 | 57,087 |
| Ordinary study turns spent | 1,736,635 | 3,226,781 |
| Interrupted studies | 7,754 | 6,592 |
| Message interruptions | 3,946 | 6,592 |
| Hunger interruptions | 0 | 0 |
| Other cancellation/lifecycle/death | 3,808 | 0 |
| Casts | 488,649 | 1,263,916 |
| MP spent | 1,246,109 | 2,761,274 |
| MP regenerated | 1,208,492 | 2,743,908 |

Conservative food preparation prevented hunger interruptions in these agents;
native tests exercise that interruption directly. Interruption spends real
turns, food and light. Whole-run successful learning is exact; zero explorer and
53 oracle books were otherwise destroyed/sacrificed, producing narrow oracle
bounds when reconstructing descent learning.

| Descent learning cohort | Runs | Mean additional spells | Median | 25th–75th percentile |
| --- | ---: | ---: | ---: | --- |
| Explorer reached floor 10 | 5,296 | 4.261 | 4 | 3–6 |
| Explorer reached floor 15 | 900 | 5.382 | 5 | 4–7 |
| Explorer escaped | 165 | 5.721 | 6 | 5–7 |
| Oracle reached floor 10 | 9,272 | 5.952–5.957 | 6 | 5–7 |
| Oracle reached floor 15 | 6,692 | 6.251–6.258 | 6 | 5–7 |
| Oracle escaped | 5,078 | 6.347–6.355 | 6 | 5–8 |

Starting Dart is excluded; floor IDs are zero-based. These cohorts show substantial
variation around the intended 4–6 learned spells, with oracle survivors tending
slightly above it. Whole-run escaped means, including study of carried books on
ascent, are 5.745 and 6.452.

### Individual casts and direct combat results

| Spell | Explorer casts | Oracle casts | Explorer direct kills | Oracle direct kills |
| --- | ---: | ---: | ---: | ---: |
| Magic Dart | 267,966 | 1,100,299 | 82,599 | 140,061 |
| Frostbind | 22,465 | 23,807 | 0 | 0 |
| Repulsion | 4,847 | 5,271 | 0 | 0 |
| Confound | 49,184 | 44,172 | 0 | 0 |
| Sense Traps | 118,588 | 55,078 | 0 | 0 |
| Blink | 3,070 | 753 | 0 | 0 |
| Shockwave | 889 | 1,824 | 0 | 0 |
| Piercing Bolt | 544 | 3,116 | 106 | 529 |
| Banish | 10,611 | 4,140 | 0 | 0 |
| Exchange | 250 | 753 | 0 | 0 |
| Cleanse | 10,235 | 24,703 | 0 | 0 |

Dart/Bolt deal 696,412 explorer and 3,160,826 oracle direct damage. Their direct
kills account for 8.49% and 7.95% of all monster kills; zero direct kills for
control/displacement spells does not imply zero defensive value or trap kills.
Explorer Shockwave produces 1,639 positive monster displacements, 1,323 of at
least two tiles (80.7%), totaling 3,500 tiles. Oracle has 4,313 positive pushes,
3,692 of at least two tiles (85.6%), totaling 9,522 tiles. These measurements precede
ordinary enemy movement, so they verify actual multi-tile push rather than net
distance after the response.

Potions remain active: 174,425 explorer and 360,855 oracle uses. Scroll reads are
99,513 and 80,650; wand charges spent are 29,075 and 193,317. Physical ranged play
includes 11,778 and 141,853 arrows fired and 210,154 and 294,529 bow-equipped turns.
The broad non-spell kill majority and continued consumable use show that the
system does not replace the rest of the resource model in these policies.

### Floor survival and immediate death causes

Counts below distinguish entering a descending floor from exiting it. These are
unconditional counts among all 10,000 seeds, not conditional survival probabilities.

| Agent / floor | Control entered / exited | Magic entered / exited |
| --- | ---: | ---: |
| Explorer 0 | 10,000 / 8,903 | 10,000 / 9,747 |
| Explorer 4 | 6,973 / 6,687 | 7,949 / 7,617 |
| Explorer 10 | 4,582 / 3,767 | 5,296 / 4,314 |
| Explorer 15 | 787 / 373 | 900 / 576 |
| Oracle 0 | 10,000 / 9,020 | 10,000 / 9,777 |
| Oracle 4 | 8,530 / 8,506 | 9,562 / 9,536 |
| Oracle 10 | 8,216 / 8,038 | 9,272 / 9,035 |
| Oracle 15 | 6,021 / 5,375 | 6,692 / 6,228 |

The largest immediate cause changes include fewer snake deaths (explorer
943→158; oracle 826→115) and fewer Lord deaths (305→189; 580→383). Troll deaths
increase (1,221→1,519; 708→937) as more runs reach later floors. Explorer
starvation increases 531→688; oracle has 42 starvation deaths in treatment.
These counts describe changed run trajectories and resource pressure, rather
than identifying the full causal chain of each death. Detailed paired floor,
ascent and death-cause tables accompany the raw reports.

## Seed review and limits

Matched traces review explorer gain seed 197 and loss seed 575, and oracle gain
seed 21 and loss seed 11. Spell actions and study ticks use authoritative turns.
For oracle seed 11, Dart kills one rattlesnake at 3 HP but another ordinary attack
deals the fatal 3 damage in the following response. Explorer seed 197 escapes
with the retained settings after its disabled-magic counterpart dies to a phantom
on floor 4. These traces explain action/resource tradeoffs; they do not isolate
a single spell's causal survival benefit after RNG trajectories diverge.

Early exploratory runs used a 20,000-action cap and encountered three control
and one treatment SIM_STUCK results. They were excluded from balance inference.
Traces with 60,000 actions showed legitimate long runs (including two ascents
past 20,000 actions and a later starvation death), so both arms were rerun at the
same higher limit. Simulator failures are never counted as deaths.

Spell telemetry records direct damage/kills, studies, interruption categories,
MP and forced push distance before the enemy response. Environmental trap damage
keeps its own attribution. Useful displacement counts are not counterfactual
survival estimates; teleport frequency has cast counts but no per-landing visit
counter. Existing item consumption also includes destruction/sacrifice, so descent
learning is reported with explicit bounds where their direction cannot be
reconstructed; whole-run successful learning is exact. Cohort learning means are
descriptive and subject to survival selection. Resource-use counts establish
continued use, rather than proving each resource indispensable.

Explorer's measured escape rate remains below 2–5%. Oracle benefits substantially
over disabled magic, while complete books perform worse than Dart-only restored
loot; some individual seeds regress. Further balance work should use public agent
behavior and controlled resource/spell experiments; the phantom floor and unrelated
counterplay were not changed to offset magic. The 247-byte stack leaves nine
bytes of headroom, so future call-path changes require renewed analysis.

Raw experiments, manifests, executable/CSV hashes, seed traces and AVM profiles
are retained under ignored build/. [SPELLS.md](SPELLS.md) is the player-facing
mechanics/control reference; sim/spell_report.py creates reproducible detailed
magic reports from validated datasets.

The retained detailed datasets are `build/magic-stalemate-explorer-10k/{control,treatment}`
and `build/magic-distribution-oracle-10k/{control,treatment}`. Their
`comparison/`, `books-comparison/` and `magic-report/` directories contain the
paired outcomes, floor/death tables and spell/resource reports. Dart-only controls
are `build/magic-stalemate-books-explorer-10k` and
`build/magic-distribution-books-oracle-10k`. Full outcome censuses use
`build/magic-stalemate-census-explorer-{control,treatment}` and
`build/magic-census-oracle-{control,treatment}`. Each manifest retains exact
commands, interventions, seed selection, executable identity and output hashes.

To reproduce an arm, use its retained executable and agent, `--seeds 1:10000`
for detailed data or `--all-seeds --no-telemetry` for a terminal-outcome census,
`--max-actions 60000`, and a fresh `--output` directory. Complete magic uses
production defaults; the ordinary-loot control adds `--intervention balance:magic=0`;
the Dart-only comparator adds `--intervention balance:spellbook-percent=0`.
Seal outputs with `balance.seal_manifest`, use ordinary `balance.compare` for
detailed pairs and explicit `balance.pair_runs(..., primary_only=True)` for
terminal-only censuses. Report all 65,535 seeds with `paired_binary(..., census=True)`.
