# Bows, arrows and progression rebalance — 2026-10-06

This implements the requested production system and extends maintained
`omniscient-v2`. Measurements began at repository HEAD
`3b5543d27d67fa4e3587ceae41765dfb90ddc780`, verified against the remote. Changes
are uncommitted. Historical reports describe their own revisions and policies;
their results are not the reference for this implementation.

## Production mechanics and final definitions

| Item/action | Damage range | Intrinsic accuracy | Range |
| --- | ---: | ---: | ---: |
| Short Bow, shooting | 4–7 | +1 | 5 |
| Long Bow, shooting | 5–8 | 0 | 6 |
| Either bow, ordinary melee | 1–2 | −2 | adjacent |
| Arrow thrown without a bow | 1–2 | −2 | 3 |

Bows occupy the existing weapon slot. Cardinal shots use production `scan_ray`,
stop at the first monster even on a miss, stop at walls/closed doors, and pass
open doors. Physical range is independent of light; `MAX_BOW_RANGE` equals the
radius-six viewport limit. Each accepted shot consumes one arrow and one turn,
including empty space and blockers. Arrows cannot be recovered.

Accuracy reuses physical hits: effective Dexterity + `(level − 1) / 3` + Ring
of Attack + intrinsic ranged accuracy, through the existing clamp. Enchantment
biases the fixed damage roll, then Strength and existing armor absorption apply,
with minimum one damage. Enchantment never changes accuracy. A targeted monster
becomes aggressive on hit or miss. Damage, defeat, XP and the Lord's Yendor drop
use the existing production paths. Point-blank explicit shooting is allowed;
walking into a monster remains the bow's poor melee attack. Potions still use
their original throwing mechanics through the generalized API.

Monster text now takes `const Monster&`, including the punctuation overload.
It checks the target's position, line of sight, natural/temporary invisibility,
See Invisible rings and cursed-ring flicker, emitting `the <monster name>` or
`something`. Capitalization remains a one-shot formatter modifier. All previous
`status(MonsterType)` callers and that overload have been removed; melee,
arrows, potions, monster effects/fire and force collisions share this rule.
Defeat formats the reference before clearing the monster type.

## Generation, quantities and saves

The measured starting configuration was exactly Food 17 / Arrows 3 and uniform
3–5-arrow bundles. The final configuration uses more frequent, smaller bundles:

| Category | Outcomes out of 72 |
| --- | ---: |
| Food | 15 |
| Arrows | 5 |
| Potions | 20 |
| Scrolls | 8 |
| Weapons | 8 |
| Armor | 8 |
| Wands | 4 |
| Rings | 2 |
| Amulets | 2 |

Final bundles contain **2 or 3 arrows with equal probability**. Food plus ammo
still occupies the same first 20 supply outcomes. Quantity comes from a
slot-local `AMMO_QUANTITY` seed, leaving supply type, equipment, placement and
gameplay streams isolated. Weapon subtype outcomes are Dagger 25%, Spear 14%,
Short Bow 6%, Long Sword 26%, Long Bow 4%; the remaining depth-dependent
Mace/Two-handed Sword region is unchanged. Bows use ordinary equipment enchant,
curse, Enchanting and Remove Curse rules.

Ammo is its own contiguous enum group near Food. Its entire `info` byte stores
quantity 0–255; curse/identification bits have no meaning. Central helpers return
uncursed/known, preserve all eight quantity bits, and define stack capacity 255.
Pickup, overflow, full-pack admission, drop, swap and ground merging use this
capacity with wider capacity totals. Zero after consuming the last arrow clears
the item. Quantity grammar covers singular/plural and values above 63 and 127.

`Item` remains **2 bytes** and `Game` remains **773 bytes on AVM**, **774 bytes
on the native ABI**. No quiver, projectile or telemetry fields were added.
`SAVE_VERSION` is **24** because enum IDs changed. Version-23 saves are rejected;
there is no migration framework. Arrows cannot be cursed, enchanted or require
identification.

## UI, animation and stack structure

The existing menu action is **Throw/Shoot**, with **Throw/Shoot what?** and
**You have nothing to throw or shoot.** Inventory order is Weapons, Ammo,
Armor, Rings, Amulets, Wands, Potions, Scrolls, Food, Quest. The filtered picker
selects Ammo before Potions. Direction selection is cardinal; B cancels without
consuming ammo or a turn. `PROJECTILE_DIRECTION` and deferred main-loop action
dispatch let UI frames unwind before resolution, following the wand pattern.
Arrow locals also unwind before the enemy/end-turn status chain.

`animate_arrow` draws dedicated flash 4×4 sprites for up/right/down/left
(`2f20`, `44e4`, `4f40`, `4e44`), redraws each tile, displays it for 60 ms and
restores the playfield. Six tiles take 360 ms. The bundle icon (`6f69`) is
distinct from the generic weapon icon; bows retain that weapon icon. Native
headless/simulator animation is a no-op. Renderer checks cover all directions,
exact pixels, timer wrap, restoration and state/RNG isolation. Benchmarks exclude
timed animation frames while retaining shot computation and render/display setup.

## Maintained policy, telemetry and compatibility

`ActionKind::Throw` dispatches to production `throw_or_shoot`; traces distinguish
shooting arrows, throwing arrows and potions. Host-only 64-bit counters record
ammo generation/acquisition/firing/carry, both bow types, equipment turns and
switches, shots/hits/damage/kills by bow, distance and target. Schema **3** has
eight streams; optional entry-state schema **1** adds the ninth CSV. Serial and
parallel merges include every stream. Reports reconcile quantities, floor/run
generation, consumption, effective HP damage and typed shot totals. Telemetry
compiles out of the device build.

The policy retains its best melee weapon and a useful bow, rejects cursed bows,
and targets a finite ammo reserve (24 with a bow; modest acquisition before one).
It uses visible hostile targets aligned through the production ray and chooses
shots using expected physical damage/hit chance, target danger and distance,
remaining arrows and switching time versus monster speed. Low ammo is reserved
for dangerous targets or the Lord; tiny expected damage is declined unless a
finisher. It avoids Bat shots, preserves existing tactical consumable priorities,
restores melee near threats or after idle bow turns, and uses cooldown hysteresis.
Decision state is host-only and consumes no gameplay RNG.

The first full census found two rare full-pack bow/ammo reverse-swap loops
(seeds 39396 and 41956). That run is invalid and excluded from balance evidence.
The fix prevents acquiring ammo by discarding its only useful bow, or acquiring
a bow by discarding its last ammo. Focused tests cover both bow types, and both
problem seeds now escape. Guards were not weakened. The policy-only 10k audit
changes 220 run rows, with 10 death→escape and 9 escape→death outcomes, resulting
in **4,430 escapes (44.30%)**, zero failures. This establishes a fresh reference;
it is not a content A/B across mismatched policy hashes.

The final automatic SHA-256 policy identity is
`216ee7bc98e2d3c37c6cfb50079077041651a713ceec9f1747695c6be08d49fb`.
CMake hashes `agent.hpp` and `omniscient_agent.cpp` and tracks their configure
dependencies. Manifests carry the identity; comparisons reject absent/different
hashes. `AGENTS.md` now permits competent new-mechanic extensions in place and
requires a fresh reference after a policy change. A build without Git also
correctly encodes unknown provenance.

With final policy but original monster balance, the no-bow compatibility control
replaces Short Bow→Spear and Long Bow→Long Sword preserving enchant/curse, and
Arrows→one Food deterministically, at the same positions. Its **10,000 run rows
are byte-identical to the pre-bow baseline**, including action hashes and results:
3,525 escapes, 6,475 deaths, zero failures. Both `runs.csv` SHA-256 values are
`01ca47ec44087dd17cc7e44cfef73c03e489aeafaf2e4258b292180e9130d934`.
The compatibility source copy restores only the independently tuned monster
constants, so this tests final policy behavior on the original content.
All shared gameplay fields also match in floors, items, monsters, visit items,
visit monsters and entry-state telemetry after normalizing shifted item IDs.
The added bow/ammo item rows contain only zero counters in this control; new
schema columns and the intervention log are intentional differences.

## Initial measurement and sequential tuning

The specified initial bows were 2–5/+1/range 5 and 3–6/0/range 6, with Food
17/Ammo 3, bundles 3–5. Under the same initial bow-capable binary and policy,
replacement control escaped **35.25%** versus **33.11%** for bows: **−2.14 pp**,
paired 95% bootstrap interval **[−2.82, −1.46]**, 462 gains and 676 losses.
Initial multi-floor generation drought (gap ≥5) affected 15.20% of runs; the
acquisition gap metric affected 9.93%. Initial traces exposed switching near
fast threats and equipment oscillation. Those defects were corrected first.

| Candidate | Change | Escape % | Paired escape Δ pp (95% CI) | Decision / reason |
| --- | --- | ---: | --- | --- |
| initial | Specified starting mechanics | 33.11 | — | Measured before tuning. |
| policy-fixed | Stable switching, approach-time checks | 33.86 | — | Corrects agent defects; new reference, not a content A/B. |
| frequent-ammo | Food 15 / Ammo 5; bundle 2–3 | 33.28 | -0.58 [-1.20, +0.06] | Kept: generation gap ≥5 falls 15.20%→2.15%; escape effect uncertain. |
| damage-plus1 | Short 3–6 / Long 4–7 | 33.70 | +0.42 [-0.09, +0.95] | Kept: reach12 +1.05 pp; escape effect uncertain. |
| damage-plus2 | Short 4–7 / Long 5–8 | 33.81 | +0.11 [-0.40, +0.64] | Kept: reach12 +1.00 pp; escape effect uncertain. |
| fire-minus2 | Dragon fire 6–13 | 35.31 | +1.50 [+1.09, +1.90] | Kept: fire deaths 1,496→1,159; targets excessive late fire burden. |
| fire-third | Eligible Dragon fire chance 1/3 | 36.15 | +0.84 [+0.32, +1.36] | Kept: fire deaths 1,159→807; melee becomes relatively more important. |
| lord112 | Lord HP 112 | 36.97 | +0.82 [+0.48, +1.18] | Kept: floor15 mortality 16.14%→14.78%; shorter costly boss fight. |
| dragon-armor7 | Dragon armor 7 | 39.41 | +2.44 [+1.93, +2.91] | Kept: reduces late attrition without changing breath, HP or speed. |
| lord-armor7 | Lord armor 7 | 40.58 | +1.17 [+0.77, +1.58] | Kept: floor15 mortality falls to 10.42%; boss traits retained. |
| zombie-str3 | Zombie STR 3 | 41.70 | +1.12 [+0.63, +1.62] | Kept: floor2 mortality 4.82%→3.50%, floor3 3.05%→2.62%. |
| troll-str9 | Troll STR 9 | 44.29 | +2.59 [+2.07, +3.11] | Kept: Troll deaths 878→666; final late progression is smoother. |
| inventory-fixed | Preserve complementary bow/ammo slots | 44.30 | — | Corrects census swap loops; final policy reference, 0 failures in 10k. |

Content candidates use identical seeds 1..10000 and identical policy hashes
within each pair. Separate policy corrections establish new references. Damage
steps have uncertain escape effects but improve reach to floor 12 by about one
percentage point each; they were retained to soften approach encounters, not
claimed as decisive escape gains. Higher ammo frequency was retained for drought
reduction despite its uncertain negative escape shift. Accuracy, bow subtype
frequency and the Short/Long range distinction were retained.

Only after bow mechanics and supply were stable were existing progression
constants changed: Dragon eligible fire chance 1/2→1/3 and fire roll 8–15→6–13;
Dragon armor 8→7; Lord HP 128→112 and armor 8→7; Zombie STR 4→3; Troll STR
10→9. All other stats, traits and roster/placement distributions are unchanged.
Dragon/Troll/Lord identities and threatening behavior remain intact.

Representative discordant traces were reviewed in both directions. Dragon armor
seed 60 first changes physical damage at floor 12 and later survives an ascent
death; seed 153 first changes Dragon damage at floor 13 yet later dies to an
Incubus. Lord armor seed 81 shortens the boss fight and escapes; seed 78 later
dies to an Angel on ascent. Zombie seed 49 preserves early HP before a Phantom
encounter; seed 10 changes the emergency-invisibility threshold and later dies.
Troll seed 81 first saves one HP per hit and escapes; seed 74 later dies to an
Incubus. Fire seeds 51/60 and Lord-HP seeds 148/94 were also inspected both ways.
Downstream choices and RNG diverge naturally after changed damage/fight length;
no random streams are resynchronized.

## Final census, controlled experiment and factor analysis

Exactly **65,535 unique effective seeds (1..65535)** under the final policy:

| Variant | Escapes | Deaths | Escape % | SIM_STUCK / SIM_ERROR |
| --- | --- | --- | --- | --- |
| Normal production | 28844 | 36691 | 44.013 | 0 / 0 |
| Bow-replacement control | 29250 | 36285 | 44.633 | 0 / 0 |

The same executable/hash gives **-0.620 pp** net bow-content escape effect: **4,196 gains / 4,602 losses**, 24,648 both escaped, 32,089 both died. This is an exact deterministic population result; sampling intervals/p-values are omitted. It includes displaced food/melee weapons, routing, acquisition, ammo and switching costs; it does not isolate arrow damage alone. The final and pre-swap-fix no-bow census controls are byte-identical, further confirming the correction is unreachable without bows/ammo.

Reach12 is **72.71%**, reach15 **60.36%**, Lord kills **35,573**, Yendor acquisitions **35,567**, post-Yendor survival **81.10%**. The small kill/acquisition difference includes deaths before taking the dropped amulet. Floor0 accounts for **17.05% of deaths**, with 181 deaths within 12 actions and 661 zero-progression deaths.

Descent mortality uses deaths / runs entering that floor; reach uses all starting runs.

| Floor | Entered | Deaths | Mortality % | Reach % | Design band % | Status |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 65535 | 6256 | 9.55 | 100.00 | 8–15 | IN BAND |
| 1 | 59279 | 1181 | 1.99 | 90.45 | 1–3 | IN BAND |
| 2 | 58098 | 2047 | 3.52 | 88.65 | 1–3 | HIGH |
| 3 | 56051 | 1585 | 2.83 | 85.53 | 1–3 | IN BAND |
| 4 | 54466 | 1624 | 2.98 | 83.11 | 1–3 | IN BAND |
| 5 | 52842 | 384 | 0.73 | 80.63 | 1–2.5 | LOW |
| 6 | 52458 | 783 | 1.49 | 80.05 | 1–2.5 | IN BAND |
| 7 | 51675 | 518 | 1.00 | 78.85 | 1–2.5 | IN BAND |
| 8 | 51157 | 832 | 1.63 | 78.06 | 1–2.5 | IN BAND |
| 9 | 50325 | 282 | 0.56 | 76.79 | 1.5–3 | LOW |
| 10 | 50043 | 905 | 1.81 | 76.36 | 1.5–3 | IN BAND |
| 11 | 49138 | 1487 | 3.03 | 74.98 | 1.5–3 | HIGH |
| 12 | 47651 | 1640 | 3.44 | 72.71 | 2–4 | IN BAND |
| 13 | 46011 | 2819 | 6.13 | 70.21 | 3–6 | HIGH |
| 14 | 43192 | 3632 | 8.41 | 65.91 | 4–7 | HIGH |
| 15 | 39560 | 4027 | 10.18 | 60.36 | 5–9 | HIGH |

Ascent mortality, in traversal order (no numerical bands specified):

| Floor | Entered | Deaths | Mortality % |
| --- | --- | --- | --- |
| 14 | 35533 | 1437 | 4.04 |
| 13 | 34096 | 1731 | 5.08 |
| 12 | 32365 | 1223 | 3.78 |
| 11 | 31142 | 1054 | 3.38 |
| 10 | 30088 | 684 | 2.27 |
| 9 | 29404 | 107 | 0.36 |
| 8 | 29297 | 196 | 0.67 |
| 7 | 29101 | 98 | 0.34 |
| 6 | 29003 | 74 | 0.26 |
| 5 | 28929 | 48 | 0.17 |
| 4 | 28881 | 19 | 0.07 |
| 3 | 28862 | 16 | 0.06 |
| 2 | 28846 | 1 | 0.00 |
| 1 | 28845 | 0 | 0.00 |
| 0 | 28845 | 1 | 0.00 |

Same 10k seeds, pre-bow versus final full-game reference: late floor12–15 mortality changes **5.25/8.80/14.54/15.41% → 3.81/5.71/8.14/10.01%**; escape **35.25% → 44.30%**, post-Yendor survival **79.45% → 81.25%**. This overall change includes policy, bows and every measured content adjustment; it is not a bow-only causal estimate.

Representative final control/treatment traces reviewed in both directions:

- Seed 16: a food→ammo replacement changes early routing away from a Snake; treatment later uses a Long Bow and escapes. This gain precedes its first shot.
- Seed 81: supply routing first changes on floor1; treatment later makes a useful Long Bow shot, restores melee for an approaching Zombie, and escapes, versus control dying to Dragon fire on floor14. The outcome is not attributed to that one shot.
- Seed 10: treatment routes past the replaced food opportunity, never fires a bow, uses several control consumables, and dies to a Hobgoblin on floor5. Control escapes.
- Seed 74: treatment uses its Short Bow against Goblins/Orcs while still taking melee damage, later dies to an Incubus on ascent11 with three HP. Control escapes.

Final inventory and last-visit entry reserves were inspected alongside these traces. Seed74 treatment has no carried healing/arrow reserve at death; entry supplies do not imply remaining charges or a guaranteed emergency exit. These cases show finite-stock and routing tradeoffs without swap oscillation or unseen bow targeting.

The full-population factor report contains **82 factors**, **76 estimable** and **50 BH-FDR notable** at q ≤0.05. It uses separate logistic models of entered-visit survival, generated-on-visit exposure, floor/direction and entry-condition/geometry controls, with seed-clustered covariance. These are adjusted observational associations, not causal item power or human survival forecasts. Model-based intervals belong to the exploratory framework; the paired bow census effect remains exact.

| Factor | Modeled visits | Adjusted survival Δ pp | Model 95% interval pp | FDR q | Status |
| --- | ---: | ---: | --- | --- | --- |
| DRAGON | 238,848 | -2.676 | [-2.872, -2.481] | 5.95e-106 | ok |
| PHANTOM | 171,857 | -1.085 | [-1.254, -0.917] | 2.57e-21 | ok |
| FOOD | 827,481 | -0.739 | [-0.952, -0.525] | 2.64e-09 | ok |
| ARROWS | 827,481 | -0.429 | [-0.505, -0.352] | 8.28e-26 | ok |
| SHORT_BOW | 827,481 | -0.342 | [-0.467, -0.217] | 1.13e-07 | ok |
| LONG_BOW | 827,481 | -0.155 | [-0.300, -0.011] | 0.0485 | ok |
| TROLL | 399,259 | -0.045 | [-0.218, +0.129] | 0.635 | ok |
| LORD | 0 | — | — | — | insufficient_data |

Largest absolute estimable associations (all categories):

| Factor | Adjusted survival Δ pp | FDR q |
| --- | ---: | --- |
| ANGEL | +4.321 | 7.68e-06 |
| DRAGON | -2.676 | 5.95e-106 |
| HEALING | +1.408 | 4.01e-284 |
| ZOMBIE | +1.337 | 1.45e-10 |
| RING_INVISIBILITY | +1.109 | 8.71e-08 |
| PHANTOM | -1.085 | 2.57e-21 |
| GRIFFIN | +1.036 | 8.63e-21 |
| WAND_STRIKING | +0.995 | 1.21e-71 |

Unestimable/unstable factors: YENDOR_AMULET (insufficient_data), BAT (insufficient_data), LORD (insufficient_data), RATTLESNAKE (unstable_model), SNAKE (unstable_model), TARANTULA (unstable_model). Yendor availability is excluded because its Lord drop is outcome-selected; the Lord has no suitable within-stratum availability comparator. Unstable models are explicitly flagged and excluded from the estimable FDR family. None are interpreted as zero effects or used to justify additional tuning. See the complete factor report for exposure counts, intervals, model support, geometry and selection-biased acquisition/use correlations.

Positive Angel/Zombie coefficients do not show that adding them helps: fixed
content slots, correlated availability, supported strata and policy choices can
produce composition/selection effects. Food's negative association despite zero
starvation also illustrates why these estimates do not establish intrinsic item
power. The controlled bow substitution measures the specified combined tradeoff;
neither analysis separately identifies every component's contribution.

## Ranged use, supplies and remaining concerns

Per starting run unless a percentage/distance is named:

| Metric | Value |
| --- | --- |
| Arrow bundles / generated units | 13.930 / 34.821 |
| Picked / fired / thrown without bow / carried | 30.882 / 24.358 / 0.000 / 1.661 |
| Bow hit % / HP damage / kills | 88.11% / 119.369 / 3.467 |
| Mean / median firing distance | 3.183 / 3 |
| Bow-equipped turns / switches to / away | 48.956 / 10.114 / 10.103 |
| First generated floor, mean / median / never | 0.427 / 0 / 3.03% |
| First acquired floor, mean / median / never | 0.463 / 0 / 9.08% |
| Generation gap ≥3 / ≥5 / p95 / max | 22.34% / 2.16% / 4 / 10 |
| Bow-owned acquisition gap ≥3 / ≥5 / p95 / max | 14.78% / 1.39% / 3 / 9 |

Gaps count consecutive reached descent floors. Bow-owned acquisition gaps include stocked/cursed bows and therefore do not establish unusable inventory. First-floor means exclude never-found. Effective damage excludes overkill.

| Bow | Generated | Picked | Equipped | Shots | Hit % | HP damage | Kills |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SHORT_BOW | 1.339 | 0.503 | 4.273 | 9.484 | 88.22 | 43.163 | 1.200 |
| LONG_BOW | 0.931 | 0.580 | 5.841 | 14.874 | 88.04 | 76.206 | 2.267 |

Arrow discovery by reached descent floor:

| Floor | Bundles / entered visit | Units / entered visit |
| --- | --- | --- |
| 0 | 1.107 | 2.767 |
| 1 | 1.105 | 2.762 |
| 2 | 1.108 | 2.769 |
| 3 | 1.108 | 2.770 |
| 4 | 1.107 | 2.767 |
| 5 | 1.105 | 2.762 |
| 6 | 1.109 | 2.772 |
| 7 | 1.106 | 2.765 |
| 8 | 1.105 | 2.762 |
| 9 | 1.106 | 2.765 |
| 10 | 1.103 | 2.758 |
| 11 | 1.107 | 2.766 |
| 12 | 1.110 | 2.775 |
| 13 | 1.106 | 2.764 |
| 14 | 1.107 | 2.767 |
| 15 | 1.037 | 2.592 |

Dragon/Troll/Lord direct burden and ranged assistance:

| Target | Shots / run | Arrow HP damage / run | Arrow kills / run | All incoming damage / engagement | Deaths, control / bows |
| --- | --- | --- | --- | --- | --- |
| DRAGON | 1.781 | 6.280 | 0.0264 | 22.56 | 6,348 / 7,071 |
| TROLL | 1.903 | 8.816 | 0.0689 | 8.94 | 4,414 / 4,620 |
| LORD | 0.247 | 0.888 | 0.0017 | 58.92 | 3,173 / 3,007 |

Dragon deaths include fire and melee. Exact immediate fire deaths are **3,674 control / 4,257 bows**. This remains a substantial threat under the replacement experiment; the preceding fire/armor tuning improves the broader game, while bow supply/routing does not independently eliminate Dragon mortality. No Bat arrows are fired.

Bow-firing runs that engage Trolls still take Troll melee damage in **91.01%** of cases (45,479 runs; 125.32 total HP/run across Troll encounters). This is whole-run descriptive burden, not damage conditioned on the currently equipped weapon.

Bow occupancy is **1.21%** of turns versus **48.86%** for Two-handed Sword. Two-handed Sword is equipped at floor15 entry in **89.67%** of entrants. Both bows have material use; the Long Bow gains damage/range at the cost of intrinsic accuracy, while melee remains essential.

End arrows: median **0**, p95 **9**, p99 **12**, maximum **29**; only **23** runs carry ≥24. Ammo dropped is **4.862 units/run**, none explicitly discarded; Short/Long Bow drops are **0.202/0.046/run**. Drop/repick is transactional, so these figures are pressure proxies rather than unique wasted items. Full-pack complementary-slot tests and census traces cover inventory behavior; slot fullness itself is not separately logged.

**Zero starvation deaths**. Floor12/15 mean entry food is **7.97/7.96 units**; floor15 also carries 2.66 healing, 7.08 control and 19.43 usable wand charges (0.56 offensive, 18.87 emergency). These are survivor-selected entry means. Healing drunk is **7.714/run** (control 7.803); control consumables **29.501/run**, wand charges **34.114/run** (replacement control 35.132). The higher total consumable count includes arrows; it is not a comparable measure of scarce magical-resource depletion by itself.

Food use increases from 10.652 to 11.314 units/run in the replacement experiment
while generation falls from 56.142 to 41.791 units/run. The extra walking and
acquisition time has a measurable food cost, but it does not produce starvation
in this census.

**Remaining concerns:** the exact bow replacement effect is mildly negative (−0.62 pp), with 227 extra mean turns/run and reduced reach15 (−1.44 pp). Supply displacement, acquisition/routing and switching remain real costs. Seven descent bands are outside their broad goals: floor2 and floors11/13/14/15 high, floors5/9 low. Floor13–15 mortality is smoother than the original cliff but still modestly above its bands. Rare long generation gaps remain (max 10), and the later ascent remains much safer than its first half. No arbitrary global monster buffs or exact-band optimization were added to hide these results. This policy is an automated reference, not a prediction of human escape rates.

## Validation, AVM size and performance

All **14 native CTest suites pass**, including production game/stack/range/XP
checks, format, inventory grouping, UI, actual renderer, shared sight generation,
combat distributions, generation distributions, benchmark helpers, simulator
policy/CLI, statistical analysis, paired CLI and scorecard validation. New checks
include the full 1/63/64/127/128/254/255 quantity boundaries, exact/overflow/full
pack merges and repick, enchant/curse ineligibility, both bow definitions,
5/6-range boundaries, thrown range 3, wall/door/first-monster/miss stopping,
all consumption cases, physical accuracy/damage equivalence, aggro, XP and Yendor,
generation-purpose isolation, exact subtype weights/heavy regions, deferred UI,
picker/default/text, hidden hit/miss/kill grammar, and reference/index visibility
parity across all slots, rings, invisibility and turn phases. Serial/parallel
streams are byte-identical and agent choices leave production state/RNG intact.

Production and benchmark AVM builds pass with warnings treated as errors.
The production linker reports **244 / 256 bytes**, a complete stack bound with
**zero analysis gaps** (12-byte margin), through wand/teleport/status/render.
Saved state is **773 bytes**; ordinary data is **103 bytes**, totaling
**876 / 1,024 bytes** (148-byte global margin). The one-byte ordinary-data
increase is the existing capitalization modifier now retained by production
monster messages; no telemetry/Game fields are involved. Program sections are
**60,738-byte text + 6,910-byte constants = 67,648 bytes**.

The rebuilt `ardurogue2.arduboy` is **153,076 bytes**, versus 150,772 at HEAD
(+2,304). SHA-256:
`d416efa0d0d1c3a1a44cddc8fef94b3e6c89d60ba536e01c5227e30824f52807`.

All **36 full turn benchmarks pass ≤100 ms**. Arrow hit is 89.681 ms, miss
89.646, kill **95.594** (4.406-ms margin), empty 83.637; equipping a bow is
46.656 and projectile picker 11.654. Dense movement/wait are 73.952/75.024.
These are maxima over the controlled project fixtures, not a claim that every
possible generated combat state was profiled. Timed animation is excluded by
the existing benchmark contract; production shot/render setup remains measured.
The narrow arrow-kill timing and 12-byte stack margins warrant continued checks.

## Reproduction and retained evidence

Raw CSVs, exact executable copies, manifests, source/output hashes, paired
reports, traces and debugger profiles remain in ignored `build/bows/`. Small
reference results are recorded here; large datasets are not committed.

| Evidence | Location |
| --- | --- |
| Original 10k reference | [pre-bow-10k](../build/bows/pre-bow-10k/report/summary.md) |
| Final-policy old-content compatibility | [provenance audit](../build/bows/provenance-audit.json) |
| Initial controlled bow A/B | [initial-ab](../build/bows/initial-ab/compare.md) |
| Sequential 10k candidates | `../build/bows/<candidate>-10k/report/summary.md` and `<candidate>-ab/compare.md` |
| Final policy 10k reference | [inventory-fixed-10k](../build/bows/inventory-fixed-10k/report/summary.md) |
| Final production census | [scorecard](../build/bows/final-fixed-all/report/summary.md), [manifest](../build/bows/final-fixed-all/manifest.json) |
| Final replacement census | [summary](../build/bows/final-fixed-control-all/summary/summary.md), [manifest](../build/bows/final-fixed-control-all/manifest.json) |
| Exact bow controlled A/B | [comparison](../build/bows/final-bow-ab/compare.md) |
| Full population factors | [factor report](../build/bows/final-fixed-all/factors/factors.md) |
| Discordant trace inspection | [final pairs](../build/bows/final-trace-review.md), [content iterations](../build/bows/content-trace-review.md) |
| Ranged/pressure diagnostics | [diagnostics](../build/bows/final-diagnostics.json) |
| Final stack audit | [link report](../build/bows/final-stack-audit.log), [ELF sections](../build/bows/final-sections.txt) |
| Full turn benchmark profiles | [36-case report](../build/bows/final-turn-benchmarks/20261006T185929Z-o0b9xext/summary.md) |
| Measured simulator executable | `../build/bows/final-inventory-fixed-simulator.exe` |
| Measured production/policy source snapshot | [source archive](../build/bows/final-measured-source.zip), [archive SHA-256](../build/bows/final-measured-source-sha256.txt) |

The same final executable runs both census variants. Control interventions are:

```text
--intervention replace-item:SHORT_BOW:SPEAR:info=preserve
--intervention replace-item:LONG_BOW:LONG_SWORD:info=preserve
--intervention replace-item:ARROWS:FOOD:info=1
```

Run `--all-seeds --jobs 8 --entry-state` for each variant. The treatment standard
scorecard uses `make_report(..., expected_runs=65535)`, while `balance.py
summarize`, `compare` and `factors` validate/reconcile the final streams. See
[the balance workflow](../sim/BALANCE.md), [simulator policy](../sim/README.md)
and [game controls](../README.md). The compatibility source preparation and
provenance audit scripts are retained with the local artifacts.

The current native executable additionally corrects its help text to say eight
CSV streams. A 32-seed replay against the measured executable matches all nine
CSV files byte for byte. The retained source archive precedes this help-only
correction and contains per-file SHA-256 hashes. The saved AVM stack audit uses
the production object files and has identical text/constants/data/saved sections
to the packaged game ELF.
