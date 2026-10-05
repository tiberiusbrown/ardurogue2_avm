# ArduRogue 2 comprehensive balance audit — 2026-10-05

## Executive summary

1. **The curve has two cliffs separated by a very safe middle. [D/J]** Floor 0 kills 22.260% of starting runs. Every descent floor 5–11 survives at least 99.612% of its entered visits; floor 13 survives only 92.290%. This irregularity merits design review, without choosing an arbitrary target win rate.
2. **Permanent invisibility is the largest tested combat accessory lever. [C/J]** Removing its ring changes escape by **-17.769 percentage points**. It suppresses pursuit and dragon fire for monsters without see-invisible; it does not neutralize every late monster.
3. **The dragon transition is the clearest late-game content spike. [D/A/C/J]** Dragons cause 7,320 immediate deaths, including 6,392 from fire. Same-position Dragon→Griffin replacement changes escape by **+13.433 pp**; this is a diagnostic comparator, not a proposed wholesale replacement.
4. **Speed is another strong defense with an enchantment feedback path. [C/J]** Removing speed amulets changes escape by **-11.702 pp**. Combined speed/invisibility removal changes it by **-38.015 pp**. The frozen agent prioritizes speed enchantments until the enemy-turn budget reaches its floor.
5. **Control resources collectively matter more than healing drops alone. [C/J]** Removing the selected ten-type control/emergency pool changes escape by **-20.800 pp**, compared with **-6.470 pp** for healing and **-5.652 pp** for offensive wands. These are overlapping strategies, not additive effect estimates.
6. **Experience potions are strong, but ordinary leveling is the broader scaling engine. [D/C/J]** Experience removal changes escape by **-5.478 pp**. Every level grants +3 base max HP and fully restores effective HP. Median descent entry level is already 14 on floor 5 and 30 on floor 15; escaping runs finish at median level 38 / max HP 129.
7. **Food is abundant in ordinary play but close to necessary. [D/C/J]** Baseline starvation deaths are zero and only 490 turns have hunger below 32. Food generation is 3,381,471 units versus 677,572 consumed. Removing all generated food leaves **3.444%** escape and causes **41,213** immediate starvation deaths. Abundance does not establish irrelevance; pickup/routing and a retained stack still matter.
8. **The equipment ladder works, but reaches high tiers early. [D/C/J]** Dagger/leather have real early tenure; two-handed sword/plate dominate later. Median armor rating reaches 6 by floor 5. Adjacent substitutions change escape by +0.624 pp (Dagger→Spear), -1.793 pp (Plate→Splint), and -3.244 pp (Two-handed sword→Mace). Enchantment does not erase intrinsic tiers.
9. **The ascent is mostly safe after the first few return floors. [D/J]** Escape is **53.861%**, Lord kill is **56.660%**, and Yendor acquisition is **56.657%**. Survival after acquisition is **95.066%**. Most return risk is on floors 14–12; there are only 3 ascent deaths on floor 2 and none on floors 0–1.
10. **There is no strong geometry tuning case, and the oracle leaves important human systems unmeasured. [A/J]** Geometry effects are tiny; the largest adjusted count effect is major features, +0.092 pp per SD. Mapping, identify, remove-curse and digging have zero pickup/use by policy design. Mimic disguise and hidden monster information are also discounted. These results do not justify removing or buffing that content for humans.

## Scope, evidence and reproducibility

This audits local commit **846ec7e9dce4bbd8684b2d6956467c46b72df238** of `tiberiusbrown/ardurogue2_avm`, built with Clang 22.1.3 in **RelWithDebInfo**. Production balance, production sources and the frozen `omniscient-v2` agent were left unchanged. No commits or pushes were made.

The baseline is the complete population of **65,535 unique effective seeds, 1..65535**, run with eight isolated simulator workers. It contains **35,298 escapes, 30,237 deaths and zero SIM_STUCK/SIM_ERROR**. All 13 native/framework tests passed. Standard summaries, factor reports, paired comparisons, intervention ledgers, manifests, executable/source snapshots and traces are retained under ignored `build/balance-audit-20261005/`.

Evidence labels throughout: **D** = descriptive observation; **A** = adjusted statistical association; **C** = controlled intervention effect under the frozen policy; **J** = design judgment. A classification is an approximate J informed by D/A/C, not proof of causal strength or a requested production change.

Full-population paired effects are exact for these seeds, this executable and this policy. They do not need sampling intervals or McNemar p-values. The standard exploratory factor model retains clustered Wald intervals and BH q-values: 74/79 factors are estimable and 46 have q≤0.05. Many statistically notable effects are well below one percentage point. The factor family and the separate descriptive selection family must not be conflated.

The standard factor is **generated presence on an entered visit**, adjusted for floor/direction, entry HP/level/stats/speed/hunger/armor/food/healing and geometry. It estimates incremental availability on that visit, not the total benefit of carrying an item into later visits. Full removal measures a different intervention, including routing, inventory choices, later entry conditions and subsequent RNG divergence. No RNG was resynchronized.

A supplemental **read-only host instrument**, built entirely under ignored `build/`, adds state snapshots, equipment instance bins and wand modifier counters. All seven ordinary CSV streams match the original executable **byte-for-byte across all 65,535 seeds**; frozen-agent source hashes match. Final HP/max HP/level and instance counters reconcile with standard telemetry. This instrument adds observations without changing gameplay.

Internal floor indices are **0–15**, as in the simulator documentation. Floor 15's Lord fight, Yendor pickup and movement back to the up stairs share its original descent visit; they are not a separate ascent-floor-15 observation.

Primary reports: [baseline summary](../build/balance-audit-20261005/population/summary/summary.json), [adjusted factors](../build/balance-audit-20261005/population/factors/factors.md), [selection associations](../build/balance-audit-20261005/population/factors/selection_associations.csv), [instrument validation](../build/balance-audit-20261005/instrument-validation.json), [counter validation](../build/balance-audit-20261005/supplemental-counter-validation.json).

## 1. Overall progression and difficulty curve

**[D]** The table gives unconditional reach among every starting seed and conditional survival among visits that actually entered that floor. Ascent populations have already survived the Lord and acquired Yendor; their low death rates cannot be compared as if they were the original descent population.

| floor | down entered | down reach % | down survival % | down deaths | up entered | up reach % | up survival % | up deaths |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 65,535 | 100 | 77.74 | 14,588 | 35,298 | 53.861 | 100 | 0 |
| 1 | 50,947 | 77.74 | 98.245 | 894 | 35,298 | 53.861 | 100 | 0 |
| 2 | 50,053 | 76.376 | 96.899 | 1,552 | 35,301 | 53.866 | 99.992 | 3 |
| 3 | 48,501 | 74.008 | 98.402 | 775 | 35,301 | 53.866 | 100 | 0 |
| 4 | 47,726 | 72.825 | 98.889 | 530 | 35,302 | 53.867 | 99.997 | 1 |
| 5 | 47,196 | 72.016 | 99.862 | 65 | 35,302 | 53.867 | 100 | 0 |
| 6 | 47,131 | 71.917 | 99.822 | 84 | 35,304 | 53.87 | 99.994 | 2 |
| 7 | 47,047 | 71.789 | 99.934 | 31 | 35,304 | 53.87 | 100 | 0 |
| 8 | 47,016 | 71.742 | 99.915 | 40 | 35,316 | 53.889 | 99.966 | 12 |
| 9 | 46,976 | 71.681 | 99.991 | 4 | 35,318 | 53.892 | 99.994 | 2 |
| 10 | 46,972 | 71.675 | 99.808 | 90 | 35,380 | 53.986 | 99.825 | 62 |
| 11 | 46,882 | 71.537 | 99.612 | 182 | 35,522 | 54.203 | 99.6 | 142 |
| 12 | 46,700 | 71.26 | 97.929 | 967 | 35,854 | 54.71 | 99.074 | 332 |
| 13 | 45,733 | 69.784 | 92.29 | 3,526 | 36,625 | 55.886 | 97.895 | 771 |
| 14 | 42,207 | 64.404 | 93.994 | 2,535 | 37,121 | 56.643 | 98.664 | 496 |
| 15 | 39,672 | 60.536 | 93.57 | 2,551 | — | — | — | — |

![Difficulty curve](../build/balance-audit-20261005/figures/difficulty_curve.png)

**[D/J]** Floor 0 is disproportionately dangerous before any starter weapon, armor, food or healing. Its 14,588 deaths account for 48.245% of all baseline deaths; 14,246 are caused immediately by snakes. Floor 2 is the next early bump (3.101% visit mortality), then difficulty drops despite stronger encounter lists. Floors 5–11 contain many encounters and consumable activations, but rarely terminate a run. Floor 13 mortality (7.710%) is roughly 3.7 times floor 12 mortality (2.071%). Floors 14 and 15 remain substantial but do not exceed the floor-13 spike.

**[D]** The Lord is killed in 37,132 runs, Yendor acquired in 37,130, and 1,832 acquisitions end in death. Two runs die before acquisition after the kill (11042: dragon fire; 14821: Angel). Nine die after acquisition on floor 15, and the remaining 1,823 during ascent. This separates boss victory, pickup safety and return survival.

**[D/J]** A very early bad run is seed 26694: six actions, zero XP/score, two failed melee attempts, bounded retreat, then snake death. Its first routing goal is an ironblood amulet, whose paralysis immunity cannot help against that snake. Seed 11 similarly dies before the first level-up after pursuing stat loot. These show danger under the frozen routing policy; neither proves the seed is unwinnable for a human or another policy. Seed 60551 is an exceptionally high-scoring escape (11,191 score, 6,320 turns), illustrating the other tail.

Full deepest-floor and final-state distributions are in [deepest_floor_distribution.csv](../build/balance-audit-20261005/tables/deepest_floor_distribution.csv), [final_level_distribution.csv](../build/balance-audit-20261005/tables/final_level_distribution.csv), [final_max_hp_distribution.csv](../build/balance-audit-20261005/tables/final_max_hp_distribution.csv), and [final_player_states.csv](../build/balance-audit-20261005/tables/final_player_states.csv). Deaths finish at median level 5 / max HP 30; escapes finish at median level 38 / max HP 129. Effective and raw strength/dexterity, gear types/enchantment, accessory magnitudes and carried reserves are included in the final-state export.

## 2. Monster balance

The complete monster catalog includes all 16 types. Direct damage uses actual HP loss, excluding overkill; immediate death attribution is not a complete causal history. Poison/weakness can enable a later death attributed to another monster. Encounters and engagements are policy-selected. Generated floors and encountered floors differ because polymorph can create types outside their native encounter lists.

| monster | classification | generated | encountered | engaged | killed | damage_to_player | deaths_caused | damage_per_engagement | deaths_per_1000_engagements |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BAT | appropriate | 403,465 | 278,362 | 160,968 | 159,555 | 38,821 | 342 | 0.24117 | 2.1246 |
| SNAKE | high impact | 1,496,265 | 1,123,052 | 957,153 | 936,351 | 1,375,175 | 14,671 | 1.4367 | 15.328 |
| RATTLESNAKE | appropriate | 345,206 | 267,180 | 225,221 | 223,420 | 282,038 | 469 | 1.2523 | 2.0824 |
| ZOMBIE | appropriate | 713,861 | 553,054 | 463,252 | 458,640 | 636,419 | 759 | 1.3738 | 1.6384 |
| GOBLIN | appropriate | 1,337,215 | 1,003,336 | 787,312 | 772,092 | 1,440,185 | 443 | 1.8292 | 0.56267 |
| PHANTOM | high impact | 1,367,536 | 1,037,247 | 898,189 | 887,045 | 2,881,547 | 1,363 | 3.2082 | 1.5175 |
| ORC | appropriate | 958,394 | 699,566 | 530,133 | 502,632 | 1,453,579 | 377 | 2.7419 | 0.71114 |
| TARANTULA | low impact | 1,183,219 | 840,979 | 613,182 | 590,441 | 546,690 | 21 | 0.89156 | 0.034248 |
| HOBGOBLIN | appropriate | 1,552,312 | 1,119,703 | 814,777 | 761,888 | 2,544,925 | 152 | 3.1235 | 0.18655 |
| MIMIC | low impact | 1,683,483 | 1,162,471 | 690,673 | 641,482 | 1,065,324 | 426 | 1.5424 | 0.61679 |
| INCUBUS | appropriate | 1,292,101 | 977,976 | 847,930 | 764,601 | 3,739,959 | 440 | 4.4107 | 0.51891 |
| TROLL | high impact | 820,874 | 592,333 | 433,490 | 330,131 | 2,830,467 | 830 | 6.5295 | 1.9147 |
| GRIFFIN | low impact | 658,533 | 462,223 | 320,553 | 278,423 | 284,613 | 156 | 0.88788 | 0.48666 |
| DRAGON | extreme outlier | 1,137,464 | 787,090 | 542,721 | 335,737 | 6,834,915 | 7,320 | 12.594 | 13.488 |
| ANGEL | appropriate | 608,480 | 470,426 | 397,080 | 364,776 | 2,154,831 | 222 | 5.4267 | 0.55908 |
| LORD | high impact | 39,672 | 39,522 | 39,518 | 37,132 | 3,372,415 | 2,246 | 85.339 | 56.835 |

| monster | monster_attacks | monster_hits | hit_pct | poison | confusion | paralysis | fire | generated_floors | status | adjusted_difference_pp | q_value |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BAT | 35,369 | 26,604 | 75.218 | 0 | 0 | 0 | 0 | 0 | ok | 13.276 | 1.0521e-08 |
| SNAKE | 1,169,830 | 716,262 | 61.228 | 0 | 0 | 0 | 0 | 0,1 | insufficient_data | — | — |
| RATTLESNAKE | 267,893 | 159,489 | 59.535 | 28,898 | 0 | 0 | 0 | 1 | unstable_model | — | — |
| ZOMBIE | 715,011 | 346,968 | 48.526 | 0 | 0 | 0 | 0 | 2,3 | ok | 0.68672 | 0.0084857 |
| GOBLIN | 1,077,357 | 652,981 | 60.61 | 0 | 0 | 0 | 0 | 2,3,5 | ok | 0.73355 | 0.10862 |
| PHANTOM | 1,758,258 | 1,027,307 | 58.428 | 0 | 0 | 0 | 0 | 2,3,4 | ok | -0.57076 | 2.0982e-06 |
| ORC | 811,863 | 458,146 | 56.431 | 0 | 0 | 0 | 0 | 3,5,6,7 | unstable_model | — | — |
| TARANTULA | 636,578 | 340,867 | 53.547 | 0 | 0 | 50,295 | 0 | 6,7,8,9,10 | ok | 0.058659 | 0.0015294 |
| HOBGOBLIN | 1,282,498 | 680,961 | 53.096 | 0 | 0 | 0 | 0 | 5,6,7,8,9,10,11 | ok | 0.029733 | 0.13336 |
| MIMIC | 732,835 | 379,977 | 51.85 | 0 | 0 | 0 | 0 | 6,7,8,9,10,11,12 | ok | 0.24216 | 1.9691e-14 |
| INCUBUS | 1,901,817 | 921,531 | 48.455 | 0 | 180,157 | 0 | 0 | 8,10,11,12,13,14,15 | ok | 0.40087 | 9.2584e-10 |
| TROLL | 1,394,228 | 596,503 | 42.784 | 0 | 0 | 0 | 0 | 10,11,12,13 | ok | 0.12658 | 0.094123 |
| GRIFFIN | 205,998 | 117,730 | 57.151 | 0 | 0 | 0 | 0 | 11,12,13 | ok | 0.8606 | 6.749e-16 |
| DRAGON | 2,142,889 | 1,634,145 | 76.259 | 0 | 0 | 0 | 1,178,218 | 12,13,14 | ok | -2.6623 | 3.055e-19 |
| ANGEL | 751,321 | 421,981 | 56.165 | 0 | 82,320 | 61,344 | 0 | 14,15 | ok | 1.8416 | 0.0055006 |
| LORD | 590,551 | 328,279 | 55.589 | 63,863 | 66,652 | 51,958 | 0 | 15 | insufficient_data | — | — |

**BAT — appropriate. [D/J]** Low burden is appropriate for a passive introductory monster; early XP hunting still has risk.

**SNAKE — high impact. [D/J]** Dominates immediate deaths while starter armor/weapon/resources are absent; do not infer that its raw stats should simply be nerfed.

**RATTLESNAKE — appropriate. [D/J]** Poison/weakness matters, but floor 1 survivor population and acquired equipment buffer its burden.

**ZOMBIE — appropriate. [D/J]** Early floor-2 transition and faster speed create measurable but smaller pressure.

**GOBLIN — appropriate. [D/J]** Early opener/combat pressure; later exposure is buffered by scaling.

**PHANTOM — high impact. [D/J]** Early strength/see-invisible opener burden; floor 4 is entirely Phantoms, preventing a within-floor presence contrast there.

**ORC — appropriate. [D/J]** Midgame armor/opener pressure; weak whole-floor death counts reflect player progression, not an automatically broken type.

**TARANTULA — low impact. [D/J]** Paralysis applications are real, but extremely few immediate deaths under ranged control/immunity; human threat is unmeasured.

**HOBGOBLIN — appropriate. [D/J]** Midgame combat burden without a large lethality cliff.

**MIMIC — low impact. [D/J]** Stationary and relatively cheap to avoid/control; its disguise is neutralized by the oracle.

**INCUBUS — appropriate. [D/J]** See-invisible and confusion produce persistent late-game pressure; no individual causal replacement.

**TROLL — high impact. [D/J]** High speed/armor/regen combat burden; controlled Griffin comparator shares 18 XP, avoiding an XP reward difference.

**GRIFFIN — low impact. [D/J]** Very low damage burden for 18 XP and its progression tier; same-position Troll replacement tests the cheap late threat.

**DRAGON — extreme outlier. [D/J]** Fire dominates direct deaths; generation weight jumps at floor 13. Immunity and invisibility create major phase-dependent defenses.

**ANGEL — appropriate. [D/J]** Multi-status, see-invisible final-tier monster; surprisingly modest direct deaths under this policy. Polymorph downgrades it to a more lethal Dragon.

**LORD — high impact. [D/J]** 85+ damage per engagement and concentrated boss deaths are consistent with a boss. XP full healing after defeat does not remove surrounding-monster acquisition hazards.

**[A]** Dragon generated presence associates with −2.662 pp visit survival (95% interval −2.961 to −2.364; q≈3.06×10⁻¹⁹). Phantom presence associates with −0.571 pp. Positive BAT (+13.276 pp), ANGEL (+1.842) and GRIFFIN (+0.861) associations must not be described as monsters helping the player: fixed population slots substitute them for other threats, and floor-conditioned exposure still leaves composition confounding. The BAT contrast mainly compares floor-0 populations containing a Bat against the rare all-Snake population.

**[A/insufficient evidence]** Snake and Lord generated-presence effects lack supported variation; Orc and Rattlesnake models are unstable. These are statistical model limitations, not simulator errors. They do not erase abundant direct burden data, but no adjusted point estimate should be manufactured for those rows.

**[D/C/J]** Dragon descent damage per engagement is 24.903 on its first native floor (12), then 14.856 on floor 13 and 10.018 on floor 14; ascent rates are 7.27, 7.13 and 9.57. First exposure before accumulated immunity/gear is especially costly. Generation weight jumps from one of six encounter-list entries on floor 12 to three of six on floors 13–14. Dragon→Griffin replacement improves escape by +13.433 pp, reaching floor 15 by +10.474 pp and Lord kill/Yendor by +10.997 pp. It also lowers post-Yendor deaths by 2.435 pp among all starting runs. The replacement changes health/armor/speed/XP as well as fire; it does not isolate fire damage alone.

**[D/C/J]** Griffin burden is 0.888 damage/engagement versus Troll 6.529, despite both granting 18 XP. Griffin→Troll replacement costs only -1.833 pp escape overall. Griffin is a low-impact late type and cheap XP source, but breathing room can be intentional. Tarantula has 50,295 paralysis applications yet only 21 immediate deaths; low direct lethality under ranged control and immunity is not proof that paralysis is harmless to humans.

Per-floor/direction generated counts, engagement damage and deaths are in [monster_progression_burden.csv](../build/balance-audit-20261005/tables/monster_progression_burden.csv); the full rates, adjusted intervals, floors and classifications are in [monster_catalog.csv](../build/balance-audit-20261005/tables/monster_catalog.csv).

## 3. Item balance and interpretation

Every one of the 55 item types is covered in the catalog appendix and CSV. Ordinary generation rates are measured over actually reached descent floors; ascent does not replenish supplies. All ordinary types can generate on every descent floor. High-tier equipment is rarer but not depth-gated. Yendor is a deterministic objective drop, excluded from availability inference.

**[D]** Pickup/drop telemetry records transactions. A legitimately swapped or dropped item can be reached and picked up again; consequently pickup/generated can exceed 100%. That ratio is not a unique-object acquisition probability. There are **571,179 dropped units and zero discarded units**: the agent normally replaces via ground swaps rather than destruction. Frequency of losing a slot must therefore use drops/replacements, not the discarded field. Wands have separate distinct-object acquisition/activation counters.

**[A/C]** The largest positive adjusted item signals are invisibility rings (+1.043 pp), experience (+0.724), striking wands (+0.690), ice wands (+0.647) and plate (+0.597). Strong negative signals include ironblood (−0.599), clarity (−0.404), teleport scrolls (−0.297), polymorph wands (−0.296) and sustenance rings (−0.262). These are availability contrasts in a fixed-slot generation system, not causal benefits/harms. Even food is negative (−0.773) although complete removal loses 50.417 pp escape. Fire-immunity availability is near zero (+0.077, q=0.456) despite a −5.432 pp removal effect: early acquisition affects later floors, saturation matters, and conditioning on entry state removes earlier accumulated advantages.

**[D/J]** Experience/healing/enchantment and many tactical consumables are almost always collected when useful. Digging, mapping, identify and remove-curse are never collected. Top equipment is sought as an upgrade; low pickup frequency for common gear largely reflects replacement by already-held better gear. Emergency wands often occupy inventory without ever being activated, but that can be legitimate insurance rather than redundancy. Selection associations are reported separately and do not establish causal value.

## 4. Weapons and armor

**[D/J]** The generation ladder is reflected in observed frequency: weapon weights are Dagger 25%, Spear 20%, Long Sword 30%, Mace 15%, Two-handed Sword 10%; armor is Leather 25%, Ring 20%, Scale 20%, Chain 15%, Splint 12%, Plate 8%. Intrinsic weapon ranges/accuracy are respectively 1–4/+2, 2–5/+1, 2–6/0, 3–7/−1 and 4–8/−2. Armor ratings are 1–6. Enchantment biases rolls within each type's fixed range; it does not replace or pack the intrinsic tier.

| item | generated_per_run | acquired runs % | equipped_run_per_picked_run_pct | turns_per_equipping_run | replacements / 100 equipping runs | median first equip floor | adjusted_difference_pp | classification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LONG_SWORD | 6.2936 | 41.105 | 99.978 | 593.44 | 109.9 | 0 | 0.11664 | appropriate |
| DAGGER | 5.1887 | 20.224 | 99.985 | 217.9 | 96.634 | 0 | -0.057621 | appropriate |
| SPEAR | 4.1317 | 24.718 | 99.994 | 420.89 | 102.65 | 0 | 0.026209 | appropriate |
| MACE | 3.0888 | 45.389 | 99.993 | 1506 | 99.536 | 1 | 0.12594 | strong |
| TWO_HANDED_SWORD | 2.033 | 66.6 | 99.995 | 3712 | 26.641 | 3 | 0.093869 | strong |
| CHAIN_MAIL | 3.1489 | 32.105 | 99.962 | 792.09 | 106.04 | 1 | 0.49983 | appropriate |
| LEATHER_ARMOR | 5.1268 | 17.748 | 99.991 | 217.27 | 92.39 | 0 | -0.015874 | appropriate |
| RING_MAIL | 4.1733 | 19.353 | 99.984 | 304.95 | 98.707 | 0 | 0.13612 | appropriate |
| SCALE_MAIL | 4.206 | 27.169 | 99.978 | 455.87 | 102.02 | 0 | 0.32261 | appropriate |
| SPLINT_MAIL | 2.4439 | 43.038 | 99.993 | 1745.3 | 90.076 | 1 | 0.35617 | strong |
| PLATE_MAIL | 1.665 | 62.002 | 99.993 | 3751.3 | 17.46 | 4 | 0.59741 | strong |

Replacement frequency counts departures from an equipped slot per 100 runs that equipped the type, including repeated departures. Exact counts and denominators are in [equipment_ladder.csv](../build/balance-audit-20261005/tables/equipment_ladder.csv).

**[D/J]** Dagger and leather each average about 218/217 turns equipped among equipping runs, so neither is immediately obsolete. Mace has 1,506 turns/run versus two-handed sword 3,712; Splint 1,745 versus Plate 3,751. Most weak-tier replacements are normal progress. By entry to floor 15, 35,475/39,672 visits (89.42%) hold a two-handed sword and 33,781/39,672 (85.15%) hold Plate. Median armor rating reaches 6 by floor 5, well before the Dragon transition; this early saturation contributes to the quiet middle.

**[C]** Dagger→Spear: 54.485% escape (+0.624 pp; 1,406 gains / 997 losses). Plate→Splint: 52.068% escape (-1.793 pp; 1,327 gains / 2,502 losses). Two-handed Sword→Mace: 50.617% escape (-3.244 pp; 1,447 gains / 3,573 losses). Each preserves position, enchantment and curse state. The resulting effects include availability substitution and frozen equipment/routing decisions; they are not mathematical DPS comparisons for an optimally played character.

**[D/J]** Natural enchantment frequencies are approximately 5%/10%/70%/10%/5% at −2/−1/0/+1/+2, with independent 1/8 equipment curses. The oracle rejects cursed equipment. Enchantment affects which instances are accepted and how long they remain useful; higher intrinsic tiers remain distinct. Post-generation +3..+5 bins arise from enchantment scrolls, not rare natural generation. There is no evidence-based reason to flatten tiers merely because enchantment exists.

Exact enchantment/curse generation, pickup, equip, turn and replacement bins are in [equipment_and_wand_instance_bins.csv](../build/balance-audit-20261005/tables/equipment_and_wand_instance_bins.csv); progression-conditioned gear/enchantment occupancy is in [weapon_enchantment_progression.csv](../build/balance-audit-20261005/tables/weapon_enchantment_progression.csv) and [armor_enchantment_progression.csv](../build/balance-audit-20261005/tables/armor_enchantment_progression.csv).

## 5. Rings and amulets

There are two ring slots and one amulet slot. Preferences and enchantment priorities materially affect measured opportunity cost. Accessory equip counts can include re-equipping/removal transactions; per-run equip prevalence and accumulated turns are safer summaries than treating every use as a new object.

| item | classification | generated_per_run | acquired runs % | equipped_run_per_picked_run_pct | turns_per_equipping_run | replacements / 100 equipping runs | adjusted_difference_pp | q_value |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AMULET_CLARITY | situational | 0.59355 | 13.674 | 100 | 1410.5 | 73.284 | -0.40395 | 1.8581e-05 |
| AMULET_CONSERVATION | weak | 0.62744 | 9.2561 | 99.984 | 698.26 | 92.399 | -0.0011006 | 0.98995 |
| AMULET_IRONBLOOD | strong | 0.60711 | 26.561 | 99.989 | 2235.7 | 35.84 | -0.59888 | 8.8472e-11 |
| AMULET_REGENERATION | weak | 0.65949 | 10.309 | 100 | 763.2 | 91.963 | -0.13714 | 0.14987 |
| AMULET_SPEED | very strong | 0.70346 | 40.813 | 99.993 | 3621.7 | 0 | 0.44706 | 2.9315e-07 |
| AMULET_VAMPIRE | strong | 0.64247 | 21.917 | 100 | 1924.1 | 59.249 | -0.032161 | 0.7849 |
| AMULET_VITALITY | appropriate | 0.66006 | 15.908 | 99.99 | 1458.5 | 75.201 | 0.025984 | 0.82951 |
| AMULET_WISDOM | appropriate | 0.66934 | 12.767 | 100 | 953.66 | 89.853 | -0.021289 | 0.83548 |
| RING_ATTACK | appropriate | 0.70201 | 22.73 | 99.993 | 1459.8 | 84.485 | -0.24105 | 0.0076459 |
| RING_DEXTERITY | appropriate | 0.64794 | 31.109 | 99.985 | 2500.9 | 56.853 | -0.10855 | 0.25689 |
| RING_FIRE_IMMUNITY | strong | 0.62316 | 31.008 | 99.975 | 2786 | 51.447 | 0.077261 | 0.45556 |
| RING_INVISIBILITY | very strong | 0.67295 | 39.524 | 99.981 | 4770.8 | 0 | 1.0425 | 1.3901e-32 |
| RING_MAIL | appropriate | 4.1733 | 19.353 | 99.984 | 304.95 | 98.707 | 0.13612 | 0.0030882 |
| RING_PROTECTION | appropriate | 0.65452 | 35.044 | 99.991 | 3408.1 | 25.662 | -0.022226 | 0.83548 |
| RING_SEE_INVISIBLE | weak | 0.60281 | 15.598 | 100 | 1070.6 | 92.79 | -0.019703 | 0.84887 |
| RING_STRENGTH | appropriate | 0.68759 | 25.863 | 99.988 | 1749.1 | 74.751 | -0.1778 | 0.055249 |
| RING_SUSTENANCE | situational | 0.65167 | 18.808 | 100 | 1194.6 | 88.528 | -0.26237 | 0.0054313 |

Replacement frequency uses the same departures-per-100-equipping-runs denominator as the equipment ladder; ring removals can repeat. Exact counts are in [accessory_competition.csv](../build/balance-audit-20261005/tables/accessory_competition.csv).

**[D/C/J]** Invisibility is near-auto-upgrade under this policy: 34,715 pickup transactions, 25,897 equipping runs, roughly 4,771 turns per equipping run, and a -17.769 pp removal effect. Speed has 26,748 pickup transactions, 26,745 equipping runs and about 3,622 turns/equipping run; removal changes escape by -11.702 pp. Normal speed budget is 4; magnitudes up to 3 reduce it toward 1, providing more player actions per enemy opportunity. The oracle enchants speed first, making the modifier and enchantment system nonlinear.

**[C/J]** Fire immunity removal costs 5.432 pp with dragons, but gains 0.336 pp after Dragon→Griffin replacement. Its net value is phase-dependent rather than universally superior. Protection has high policy preference and enchantment priority, but the 10k removal screen is only -0.230 pp (95% paired interval -0.620 to +0.160). The armor/protection observational interaction is strongly negative, consistent with diminishing conditional value, but also adjusted for armor rating that already includes protection. That is not a direct mechanical marginal-armor estimate.

**[D/C/J]** Vampire has higher actual tenure than regeneration/conservation; its 10k removal effect is -1.080 pp (95% paired interval -1.400 to -0.740). Regeneration is -0.090 pp (95% paired interval -0.310 to +0.130), conservation -0.180 pp (95% paired interval -0.430 to +0.060). These screens establish neither a final full-population verdict nor that forcing these effects into a free slot would be weak. Speed displaces them, full level-up healing reduces marginal recovery demand, and conservation only preserves drunk potions, not the many thrown control potions. A fair forced-slot or same-position Speed comparator is still needed before buffs.

**[C/J]** Dexterity Ring→Strength Ring, preserving magnitude and curse, changes escape by +0.778 pp. This is a small opportunity-cost tuning signal. The full dexterity-ring generated-presence association itself is not notable (−0.109 pp, q=0.257). Ironblood and clarity remain situational status defenses even with negative availability coefficients; see-invisible has reduced information value for the oracle, and sustenance competes against stronger ring effects in an abundant-food baseline.

All accessory pair/amulet entry combinations are retained in [accessory_combinations.csv](../build/balance-audit-20261005/tables/accessory_combinations.csv). Regeneration and vampire cannot be equipped together because they compete for the same single amulet slot; an additive equipped combination is not a production interaction.

## 6. Potions, scrolls and wands

**[D/C/J]** Healing is used tactically and clears weakness, but ordinary XP/HP restoration and defenses provide alternatives. Experience is a 50-XP grant, making it especially powerful relative to early thresholds (4+3×current level). Strength/dexterity potions permanently scale toward raw-stat cap 12; the oracle stops collecting them at the cap. These caps prevent unbounded offensive scaling. Most confusion/paralysis/slowing/poison potions are thrown, whereas experience/stat/healing potions are drunk; availability and use must be separated.

Fear, teleport and invisibility have lower activation than pickup because they are emergency reserves. Torment/mass-confuse/mass-poison are frequently used mass effects. Enchantment is frequently collected but mostly allocated first to speed/protection. A negative use or availability association for a tactical consumable does not show that using it caused a loss.

| item | generated | picked_up | used | consumed | potions_drunk | potions_thrown | scrolls_read | carried | classification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CONFUSION | 340,408 | 336,178 | 323,070 | 323,070 | 0 | 323,070 | 0 | 86 | strong |
| DEXTERITY | 325,548 | 293,557 | 295,482 | 293,393 | 295,482 | 0 | 0 | 101 | strong |
| EXPERIENCE | 338,567 | 331,198 | 333,595 | 331,179 | 333,595 | 0 | 0 | 19 | very strong |
| HARMING | 345,496 | 317,712 | 300,291 | 300,291 | 0 | 300,291 | 0 | 200 | appropriate |
| HEALING | 341,732 | 334,078 | 274,711 | 273,284 | 274,711 | 0 | 0 | 60,794 | strong |
| INVISIBILITY | 342,520 | 334,685 | 105,851 | 105,489 | 105,851 | 0 | 0 | 226,189 | strong |
| PARALYSIS | 341,032 | 332,544 | 332,372 | 332,372 | 0 | 332,372 | 0 | 172 | strong |
| POISON | 339,443 | 342,748 | 310,168 | 310,168 | 0 | 310,168 | 0 | 354 | appropriate |
| SCROLL_ENCHANT | 147,690 | 144,476 | 143,859 | 143,859 | 0 | 0 | 143,859 | 617 | strong |
| SCROLL_FEAR | 147,043 | 161,653 | 70,833 | 70,833 | 0 | 0 | 70,833 | 57,357 | situational |
| SCROLL_IDENTIFY | 156,465 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | nearly irrelevant |
| SCROLL_MAPPING | 141,362 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | nearly irrelevant |
| SCROLL_MASS_CONFUSE | 162,785 | 158,739 | 158,406 | 158,406 | 0 | 0 | 158,406 | 333 | strong |
| SCROLL_MASS_POISON | 151,437 | 148,382 | 144,335 | 144,335 | 0 | 0 | 144,335 | 2,514 | appropriate |
| SCROLL_REMOVE_CURSE | 158,673 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | nearly irrelevant |
| SCROLL_TELEPORT | 147,435 | 174,235 | 47,102 | 47,102 | 0 | 0 | 47,102 | 49,491 | situational |
| SCROLL_TORMENT | 153,119 | 149,237 | 145,762 | 145,762 | 0 | 0 | 145,762 | 3,474 | strong |
| SLOWING | 341,520 | 344,902 | 311,946 | 311,946 | 0 | 311,946 | 0 | 415 | appropriate |
| STRENGTH | 341,485 | 290,492 | 292,453 | 290,311 | 292,453 | 0 | 0 | 89 | strong |

| item | generated | wands_picked_up | distinct pickup / generated % | wands_activated | wand_activated_per_picked_pct | charges_used | carried | classification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WAND_DIGGING | 94,739 | 0 | 0 | 0 | — | 0 | 0 | nearly irrelevant |
| WAND_FIRE | 106,235 | 80,046 | 75.348 | 79,272 | 99.033 | 500,927 | 1,299 | strong |
| WAND_FORCE | 92,163 | 72,897 | 79.096 | 18,934 | 25.974 | 105,016 | 51,665 | situational |
| WAND_ICE | 93,681 | 72,972 | 77.894 | 72,912 | 99.918 | 467,886 | 157 | strong |
| WAND_POLYMORPH | 88,531 | 59,374 | 67.066 | 4,921 | 8.2881 | 22,598 | 35,379 | situational |
| WAND_STRIKING | 109,672 | 82,894 | 75.584 | 82,842 | 99.937 | 546,284 | 223 | strong |
| WAND_TELEPORT | 89,171 | 68,526 | 76.848 | 25,558 | 37.297 | 141,055 | 48,871 | situational |

**[D/J]** Almost every acquired striking/ice/fire wand is activated. Force (25.97%), teleport (37.30%) and polymorph (8.29%) have much lower distinct activation, while their final carry counts are high. These are measurable inventory insurance costs; they do not by themselves make the emergency types useless. Digging has zero acquisition/activation by explicit policy design.

| modifier | generated | picked_up | generated_charges | charges_used | generated % |
| --- | --- | --- | --- | --- | --- |
| cursed | 64,437 | 0 | 429,325 | 0 | 9.5577 |
| normal | 397,347 | 370,115 | 2,569,603 | 1,331,107 | 58.937 |
| overpowered | 12,371 | 12,088 | 82,931 | 31,229 | 1.8349 |
| powerful | 73,835 | 70,804 | 481,978 | 247,823 | 10.952 |
| spreading | 51,858 | 48,567 | 342,247 | 173,607 | 7.6919 |
| unreliable | 74,344 | 0 | 473,930 | 0 | 11.027 |

**[C/J] Modifier comparisons:** all wand slots retain their original type/location and receive exactly five charges. Thus normal→powerful/overpowered comparisons change the modifier while holding charge count fixed. The full-population normal policy escape rate is 53.492%; powerful is 56.100% (**+2.608 pp**), and overpowered is 56.489% (**+2.997 pp** versus normal). These are global modifier substitutions across the existing wand mix, not per-type modifier causal estimates.

The 10k normal→spreading screen is +0.760 pp (95% paired interval +0.200 to +1.320). Normal→cursed and normal→unreliable are both −10.140 pp; the resulting action/outcome hashes match because the policy rejects both variants rather than using their different mechanics. This is strong evidence of zero tactical support under `omniscient-v2`, not a human mechanical comparison of cursed versus unreliable.

**[D/J]** Intended modifier weights are 60/10/10/8/10/2 percent for normal/cursed/unreliable/spreading/powerful/overpowered. Powerful variants are measurably stronger in the fixed-charge tests, and overpowered is rarer and has spreading/aiming/self-fire constraints. Rarity broadly follows power and downsides, but no precise rarity change follows from escape rate alone. Afflicted wand generation consumes around one fifth of wand objects that the oracle will not use. Per-type human utility and rare modifier interactions remain insufficiently measured.

Normal-vs-modifier paired reports are in [compare.md](../build/balance-audit-20261005/modifier-comparisons/full/powerful/compare.md) and [compare.md](../build/balance-audit-20261005/modifier-comparisons/full/overpowered/compare.md); all six modifier descriptions, per-type counts and charge totals are in the instance-bin CSV. Sample screens are separately labeled under `modifier-comparisons/` and `experiments/sample/`.

## 7. Food and hunger

**[D]** Baseline food reserve is median 4 entering floor 1, then 8 on every descent floor 2–15. It declines during ascent, from median 8 on floor 14 to median 4 entering floor 0. Escapes end at median 3 food units. There are zero starvation deaths and only 490 actual turns below hunger 32 out of roughly 240 million turns. The ordinary oracle does not face meaningful food scarcity, although it routes to food, eats before hunger is dangerous and retains a stack in the 16-slot inventory.

**[D/C]** Generated 3,381,471; picked up 926,332 transactions; consumed 677,572; final carry 248,760 units. Generation is about five times consumption over reached floors. Removing all generated food changes escape from 53.861% to 3.444% and creates 41,213 immediate starvation deaths. Deaths concentrate around descent floors 2–5; food-free escapes still occur through other recovery/XP paths. This flags food as close to necessary in practice, not logically mandatory for every seed.

**[J]** Hunger's actual strategic pressure is replenishment/routing and maintaining a slot, with a generous scarcity buffer. That can be a valid pacing constraint. The abundance deserves a marginal-availability test (for example cap removals per visit or same-position substitution of part of the food pool), rather than deleting hunger or interpreting zero starvation as a useless mechanic. Complete removal is an extreme diagnostic and does not estimate the effect of a small supply reduction.

## 8. Player scaling

**[D]** Entry-state progression below is survivor-conditioned. Higher late-floor means do not independently prove that arbitrary starting seeds can recover: weak runs have already disappeared. Means, medians, 10th/90th percentiles, raw/effective stats, XP residue, accessory magnitudes and equipment quality are preserved in the CSVs.

| floor | hp_median | level_median | max_hp_median | raw_strength_median | raw_dexterity_median | armor_rating_median | speed_median | food_median | healing_median | useful_wand_charges_median |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 18 | 1 | 18 | 5 | 4 | 0 | 4 | 0 | 0 | 0 |
| 1 | 24 | 3 | 24 | 5 | 4 | 3 | 4 | 4 | 0 | 0 |
| 2 | 33 | 7 | 36 | 6 | 5 | 5 | 4 | 8 | 0 | 5 |
| 3 | 39 | 9 | 42 | 6 | 5 | 5 | 4 | 8 | 0 | 8 |
| 4 | 47 | 12 | 51 | 6 | 6 | 5 | 4 | 8 | 0 | 9 |
| 5 | 52 | 14 | 57 | 7 | 6 | 6 | 4 | 8 | 0 | 11 |
| 6 | 60 | 16 | 63 | 7 | 6 | 6 | 4 | 8 | 1 | 11 |
| 7 | 63 | 18 | 69 | 8 | 7 | 6 | 4 | 8 | 1 | 10 |
| 8 | 69 | 19 | 72 | 8 | 7 | 6 | 4 | 8 | 1 | 11 |
| 9 | 75 | 21 | 78 | 9 | 8 | 6 | 4 | 8 | 2 | 12 |
| 10 | 78 | 22 | 81 | 9 | 8 | 6 | 4 | 8 | 2 | 14 |
| 11 | 82 | 24 | 87 | 10 | 9 | 6 | 4 | 8 | 2 | 15 |
| 12 | 87 | 25 | 92 | 10 | 9 | 6 | 4 | 8 | 2 | 17 |
| 13 | 90 | 27 | 96 | 11 | 9 | 6 | 3 | 8 | 2 | 18 |
| 14 | 93 | 28 | 99 | 11 | 10 | 6 | 3 | 8 | 2 | 20 |
| 15 | 101 | 30 | 105 | 11 | 10 | 6 | 3 | 8 | 3 | 21 |

![Player scaling](../build/balance-audit-20261005/figures/player_scaling.png)

**[D/J]** Level/HP growth, stat potions and rapid equipment acquisition outpace ordinary midgame monster pressure. Every level restores all effective HP, creating a positive loop: low-burden kills grant XP, leveling replenishes health and increases the next health reserve, which funds more exploration and kills. Equipment absorbs low-tier damage while full healing replenishes it. The data identify this plausible scaling mechanism; they do not isolate the causal contribution of level-up healing from +3 HP growth and normal XP gain.

**[C/J]** Experience-pot removal costs 5.478 pp, including −2.646 pp floor-0 exit and −5.695 pp reaching floor 12. Its effect therefore starts early and persists beyond the first visit. A decisive next experiment would separate XP amount, max-HP growth and level-up restoration using simulator-only hooks, keeping generation and the frozen policy constant. Nerfing experience potions alone would not address the whole kill/level/heal engine.

**[D/J]** Escaping players finish at median raw strength 12 / raw dexterity 11, effective strength/dexterity 11/11, armor rating 6, weapon enchantment 0 and armor enchantment 1. Effective reductions can reflect weakness or vampire drain; they are not evidence of broken raw stat storage. There are 87 capped level-50 baseline runs; most wins are below the cap. Successful runs become much stronger, but this audit cannot label failed early seeds permanently unrecoverable without a controlled recovery intervention.

## 9. Resource economy

**[D/J]** The baseline accumulates food, control supplies and wand charges through the middle. Median healing stock is zero entering floors 1–5, rises to 1–3 on later descent, then is median zero on ascent after the Lord fight. Useful wand charges rise from median 5 entering floor 2 to 21 entering floor 15, and remain around 21 throughout ascent. Offensive single-use units are typically depleted by late descent while emergency/control and wand stocks persist. These pools are not interchangeable: polymorph/force charges counted as useful by policy can sit unused.

Median inventory occupancy reaches 16 on many middle floors, and 41,185,026 turns occur at a full pack. Item replacements and food retention therefore represent real inventory pressure even when supply generation is generous. No destructive discards occur; outgoing content is usually left on the ground.

**[C/J]** Complete category removal proves that primary control, offensive wands, healing and XP have distinct useful paths. All still permit substantial escape when individually removed. There is no supported claim that a single healing drop or wand type is mandatory. Food is the clear near-mandatory category, as expected for an active metabolic constraint. Per-visit entry/exit differences are descriptive because variant entry populations differ; unconditional paired reach/exit and all-run consumption outcomes support intervention effects more cleanly.

| floor | direction | phase | food_median | healing_median | experience_median | offensive_units_median | control_units_median | useful_wand_charges_median | inventory_slots_median |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | ascent | entry | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 0 | ascent | exit | 3 | 0 | 0 | 0 | 9 | 21 | 13 |
| 1 | ascent | entry | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 1 | ascent | exit | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 2 | ascent | entry | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 2 | ascent | exit | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 3 | ascent | entry | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 3 | ascent | exit | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 4 | ascent | entry | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 4 | ascent | exit | 4 | 0 | 0 | 0 | 9 | 21 | 13 |
| 5 | ascent | entry | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 5 | ascent | exit | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 6 | ascent | entry | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 6 | ascent | exit | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 7 | ascent | entry | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 7 | ascent | exit | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 8 | ascent | entry | 6 | 0 | 0 | 0 | 9 | 21 | 13 |
| 8 | ascent | exit | 5 | 0 | 0 | 0 | 9 | 21 | 13 |
| 9 | ascent | entry | 6 | 0 | 0 | 0 | 9 | 21 | 13 |
| 9 | ascent | exit | 6 | 0 | 0 | 0 | 9 | 21 | 13 |
| 10 | ascent | entry | 6 | 0 | 0 | 0 | 9 | 21 | 13 |
| 10 | ascent | exit | 6 | 0 | 0 | 0 | 9 | 21 | 13 |
| 11 | ascent | entry | 7 | 0 | 0 | 0 | 9 | 21 | 13 |
| 11 | ascent | exit | 6 | 0 | 0 | 0 | 9 | 21 | 13 |
| 12 | ascent | entry | 7 | 0 | 0 | 0 | 9 | 21 | 13 |
| 12 | ascent | exit | 7 | 0 | 0 | 0 | 9 | 21 | 13 |
| 13 | ascent | entry | 7 | 0 | 0 | 0 | 9 | 21 | 13 |
| 13 | ascent | exit | 7 | 0 | 0 | 0 | 9 | 21 | 13 |
| 14 | ascent | entry | 8 | 0 | 0 | 0 | 9 | 21 | 13 |
| 14 | ascent | exit | 7 | 0 | 0 | 0 | 9 | 21 | 13 |
| 0 | descent | entry | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 0 | descent | exit | 4 | 0 | 0 | 0 | 1 | 0 | 7 |
| 1 | descent | entry | 4 | 0 | 0 | 0 | 2 | 0 | 7 |
| 1 | descent | exit | 8 | 0 | 0 | 1 | 4 | 5 | 11 |
| 2 | descent | entry | 8 | 0 | 0 | 1 | 4 | 5 | 11 |
| 2 | descent | exit | 8 | 0 | 0 | 1 | 5 | 8 | 14 |
| 3 | descent | entry | 8 | 0 | 0 | 1 | 5 | 8 | 14 |
| 3 | descent | exit | 8 | 0 | 0 | 2 | 6 | 9 | 15 |
| 4 | descent | entry | 8 | 0 | 0 | 2 | 6 | 9 | 15 |
| 4 | descent | exit | 8 | 0 | 0 | 2 | 7 | 10 | 16 |
| 5 | descent | entry | 8 | 0 | 0 | 2 | 7 | 11 | 16 |
| 5 | descent | exit | 8 | 1 | 0 | 2 | 8 | 10 | 16 |
| 6 | descent | entry | 8 | 1 | 0 | 2 | 8 | 11 | 16 |
| 6 | descent | exit | 8 | 1 | 0 | 2 | 9 | 10 | 15 |
| 7 | descent | entry | 8 | 1 | 0 | 2 | 9 | 10 | 15 |
| 7 | descent | exit | 8 | 1 | 0 | 3 | 10 | 11 | 16 |
| 8 | descent | entry | 8 | 1 | 0 | 3 | 10 | 11 | 16 |
| 8 | descent | exit | 8 | 2 | 0 | 2 | 9 | 12 | 15 |
| 9 | descent | entry | 8 | 2 | 0 | 2 | 9 | 12 | 15 |
| 9 | descent | exit | 8 | 2 | 0 | 2 | 10 | 14 | 16 |
| 10 | descent | entry | 8 | 2 | 0 | 2 | 10 | 14 | 16 |
| 10 | descent | exit | 8 | 2 | 0 | 0 | 8 | 15 | 13 |
| 11 | descent | entry | 8 | 2 | 0 | 0 | 8 | 15 | 13 |
| 11 | descent | exit | 8 | 2 | 0 | 0 | 8 | 17 | 13 |
| 12 | descent | entry | 8 | 2 | 0 | 0 | 8 | 17 | 13 |
| 12 | descent | exit | 8 | 2 | 0 | 0 | 8 | 18 | 13 |
| 13 | descent | entry | 8 | 2 | 0 | 0 | 8 | 18 | 13 |
| 13 | descent | exit | 8 | 2 | 0 | 0 | 8 | 19 | 13 |
| 14 | descent | entry | 8 | 2 | 0 | 0 | 8 | 20 | 13 |
| 14 | descent | exit | 8 | 2 | 0 | 0 | 9 | 21 | 14 |
| 15 | descent | entry | 8 | 3 | 0 | 0 | 9 | 21 | 14 |
| 15 | descent | exit | 8 | 0 | 0 | 0 | 9 | 20 | 13 |

Exact entry/exit reserves, means/medians/quantiles and successful-versus-failed visit changes are in [entry_exit_resources_and_scaling.csv](../build/balance-audit-20261005/tables/entry_exit_resources_and_scaling.csv) and [resource_visit_deltas.csv](../build/balance-audit-20261005/tables/resource_visit_deltas.csv). `offensive_units` counts Harming/Torment/Mass-poison; `control_units` counts Confusion/Paralysis/Slowing/Strength/Invisibility/Fear/Teleport/Mass-confuse. Wand charges are reported separately; strength also has permanent-stat use. These are transparent operational categories, not exhaustive measures of every possible tactical use.

## 10. Dungeon generation and geometry

**[D]** Existing generation diagnostics cover archetype, area, major features, corridors, loops, open connections and feature families. An additional native sweep runs all 65,536 raw generation seed identifiers, all 16 floors and both directions (2,097,152 generated floors; the nonzero balance population is contained in this sweep). Every existing generation property check passes, including connectivity, placement, topology and deterministic regeneration checks. The zero generation identity is an extra diagnostic case, not an additional balance seed.

| factor | modeled_visits | adjusted_difference_pp | difference_low_pp | difference_high_pp | q_value |
| --- | --- | --- | --- | --- | --- |
| archetype:WARREN vs CHAMBERS | 561,909 | 0.12598 | -0.057613 | 0.30958 | 0.22023 |
| major_features | 1,123,337 | 0.091603 | 0.059811 | 0.12339 | 1.1273e-07 |
| floor_tiles | 1,123,337 | 0.055834 | -0.011203 | 0.12287 | 0.14197 |
| archetype:FORTRESS vs CHAMBERS | 561,857 | 0.053741 | -0.087991 | 0.19547 | 0.52886 |
| corridors | 1,123,337 | 0.026463 | -0.0066716 | 0.059598 | 0.15445 |
| loops | 1,123,337 | 0.019679 | -0.024854 | 0.064213 | 0.45556 |
| archetype:RUINS vs CHAMBERS | 561,627 | -0.014212 | -0.1242 | 0.095777 | 0.83548 |
| open_connections | 1,123,337 | 0.0016291 | -0.03844 | 0.041698 | 0.94933 |

**[A/J]** Only major-feature count has a notable adjusted effect, and it is small: +0.092 pp per SD. Archetypes, area, corridors, loops and open connections provide no substantial adjusted survival signal. Dragon×corridors and Troll×corridors interaction tests are not notable (q=0.621); this does not exclude local line-of-fire or crowding hazards that a floor-wide count cannot resolve.

**[D/J]** Population placement protects an initial Euclidean radius of 6 tiles on floors 0–2 and 4 later, with the final Lord deliberately on the objective stairs. Snake speed and player routing can close that separation rapidly, as seed 26694 demonstrates. There is no identified stair-connectivity defect. Exact local ranged alignment, simultaneous hostile counts and accessibility would need spatial event telemetry before a geometry redesign.

Generation output: [generation-full.log](../build/balance-audit-20261005/generation-full.log). Plausible interaction models and their separate BH family: [adjusted_interactions.csv](../build/balance-audit-20261005/tables/adjusted_interactions.csv). No geometry change is recommended from these weak/global associations.

## 11. Major interactions and seed dependence

**[C] Dragon × fire immunity.** Full 2×2 intervention arms are baseline, no fire immunity, Dragon→Griffin, and Dragon→Griffin plus no fire immunity. Removing immunity changes escape by −5.432 pp with dragons and +0.336 pp without them: a **5.768 pp change in its removal effect**. That establishes a strong dragon-dependent availability pathway under this policy. It also includes fire-wand safety, slot competition, routing and different RNG consumption after actions diverge.

**[C] Invisibility × speed.** Individual removals are -17.769 and -11.702 pp; combined is -38.015 pp. Combined loss minus the sum of individual losses is **-8.544 pp**. This is an exact population interaction for the removal package, not a per-fight synergy coefficient. The interventions alter exposure, leveling, accessory alternatives and routing throughout the run.

**[A/J] Armor × protection and speed × weapon.** Observational interaction coefficients are negative, with separate-family q<0.05. That is compatible with diminishing conditional benefits and overlaps, but the controls include effective armor/speed and the populations are selected survivors. No causal nonlinear armor or weapon nerf follows. The paired adjacent-gear comparisons are more relevant for design.

**[D/J] Conservation × consumables.** The 25% preservation chance affects drunk potions. Most control potion use is throwing, so conservation does not preserve that main resource path. Experience/stat/healing use slightly exceeds consumption in the baseline. Amulet-slot competition and level-up restoration limit actual conservation/regeneration tenure; forced-slot tests are needed to evaluate their standalone effects.

**[D/J] Invisibility × perception.** Phantom, Incubus, Angel and Lord see invisible players; dragons do not. The ring blocks pursuit and dragon fire rather than universally removing combat. The oracle still resolves route blockers through real combat, so zero pursuit does not imply zero engagement.

**[D/J] Polymorph × non-monotone monster ordering.** Angel deterministically changes down to Dragon, which has more HP/armor, fire breath and fire immunity despite being a lower enum value. Polymorph also heals the new form and clears control statuses. Floor 15 generates no dragons, yet has 897 dragon encounters and 67 immediate dragon deaths through transformations. This is a concrete interaction to review, not proof those deaths would have been avoided without polymorph: an uncontrolled Angel could also kill the player. The frozen agent uses polymorph only as a severe-emergency last resort.

**[D/C/J] Seed dependence.** The outcomes are strongly bimodal: early deaths stay weak while escapes become highly developed. Item availability is a verified source of variance (especially permanent invisibility and speed); encounter transitions and dragon defenses also produce large controlled effects. Different control/removal variants can rescue some baseline losses while destroying more wins, because loot/routing and later RNG consumption change. The direction-specific discordance counts are retained for every experiment.

One combat realization per effective seed does **not** identify a variance decomposition into generation versus combat RNG. This audit cannot call a seed almost unwinnable solely from one oracle loss. A future design should hold generation identities constant and vary an independently defined initial combat stream, never resynchronizing after divergence. Human routing, incomplete information and alternative strategies remain outside this policy.

## 12. Controlled experiments performed

All major tests below use the same original executable, frozen agent, full effective population, preserved positions, unchanged production constants and deterministic simulator-only rules. Item replacements preserve encoding group state where stated; monster substitutions use the production target type's clean health/status state. The baseline census is reused as the zero-intervention control rather than rerunning identical control datasets. Manifests and seven-stream hashes validate every comparison.

| experiment | escape % | delta pp | gains | losses | failures |
| --- | --- | --- | --- | --- | --- |
| [no-experience](../build/balance-audit-20261005/experiments/full/no-experience/comparison/compare.md) | 48.383 | -5.478 | 2,347 | 5,937 | 0 |
| [no-healing](../build/balance-audit-20261005/experiments/full/no-healing/comparison/compare.md) | 47.391 | -6.4698 | 2,137 | 6,377 | 0 |
| [no-invisibility-ring](../build/balance-audit-20261005/experiments/full/no-invisibility-ring/comparison/compare.md) | 36.092 | -17.769 | 152 | 11,797 | 0 |
| [no-speed](../build/balance-audit-20261005/experiments/full/no-speed/comparison/compare.md) | 42.159 | -11.702 | 280 | 7,949 | 0 |
| [dragon-to-griffin](../build/balance-audit-20261005/experiments/full/dragon-to-griffin/comparison/compare.md) | 67.294 | 13.433 | 9,365 | 562 | 0 |
| [no-fire-immunity](../build/balance-audit-20261005/experiments/full/no-fire-immunity/comparison/compare.md) | 48.429 | -5.4322 | 577 | 4,137 | 0 |
| [dragon-to-griffin-no-fire-immunity](../build/balance-audit-20261005/experiments/full/dragon-to-griffin-no-fire-immunity/comparison/compare.md) | 67.63 | 13.768 | 9,860 | 837 | 0 |
| [griffin-to-troll](../build/balance-audit-20261005/experiments/full/griffin-to-troll/comparison/compare.md) | 52.029 | -1.8326 | 1,286 | 2,487 | 0 |
| [no-food](../build/balance-audit-20261005/experiments/full/no-food/comparison/compare.md) | 3.444 | -50.417 | 169 | 33,210 | 0 |
| [dexterity-ring-to-strength](../build/balance-audit-20261005/experiments/full/dexterity-ring-to-strength/comparison/compare.md) | 54.64 | 0.77821 | 1,471 | 961 | 0 |
| [no-invisibility-ring-no-speed](../build/balance-audit-20261005/experiments/full/no-invisibility-ring-no-speed/comparison/compare.md) | 15.846 | -38.015 | 226 | 25,139 | 0 |
| [plate-to-splint](../build/balance-audit-20261005/experiments/full/plate-to-splint/comparison/compare.md) | 52.068 | -1.7929 | 1,327 | 2,502 | 0 |
| [dagger-to-spear](../build/balance-audit-20261005/experiments/full/dagger-to-spear/comparison/compare.md) | 54.485 | 0.62409 | 1,406 | 997 | 0 |
| [no-control](../build/balance-audit-20261005/experiments/full/no-control/comparison/compare.md) | 33.062 | -20.8 | 1,521 | 15,152 | 0 |
| [no-offensive-wands](../build/balance-audit-20261005/experiments/full/no-offensive-wands/comparison/compare.md) | 48.209 | -5.6519 | 1,661 | 5,365 | 0 |
| [wand-normal](../build/balance-audit-20261005/experiments/full/wand-normal/comparison/compare.md) | 53.492 | -0.36927 | 3,028 | 3,270 | 0 |
| [wand-powerful](../build/balance-audit-20261005/experiments/full/wand-powerful/comparison/compare.md) | 56.1 | 2.2385 | 3,843 | 2,376 | 0 |
| [wand-overpowered](../build/balance-audit-20261005/experiments/full/wand-overpowered/comparison/compare.md) | 56.489 | 2.6276 | 4,242 | 2,520 | 0 |
| [two-handed-sword-to-mace](../build/balance-audit-20261005/experiments/full/two-handed-sword-to-mace/comparison/compare.md) | 50.617 | -3.2441 | 1,447 | 3,573 | 0 |

Each experiment name links to its paired outcome report. Exact ordered rules are retained in [controlled_experiments.csv](../build/balance-audit-20261005/tables/controlled_experiments.csv), each experiment's `experiment.json`, and its intervention ledger. Wand variants set all seven types to five charges and the named modifier; gear and ring substitutions preserve enchantment/magnitude and curse encoding.

The ten-type control removal is Confusion, Paralysis, Slowing, Invisibility potion, Fear/Teleport/Mass-confuse scrolls and Force/Teleport/Polymorph wands. Poison, Strength, offensive effects and remaining defenses stay available. It is a selected primary control/emergency pool, not every possible form of control. Offensive-wand removal covers Fire/Striking/Ice only.

Full paired reports include Lord/Yendor/post-Yendor outcomes, reach to floors 12/15, all-run turns/actions/damage/consumables, per-floor reach and conditional survival, immediate death cause shifts, per-item/per-monster shifts and discordant seeds. Exact unconditional floor effects and survivor-conditioned visit rates are labeled separately.

Secondary **10k screens** cover conservation, vampire, regeneration, protection and all six fixed-charge wand modifiers. Sampled paired intervals/McNemar tests are retained. Powerful/overpowered modifier signals were promoted to the full population; spreading/afflicted and minor accessory screens remain lower-priority evidence, with no final human-balance verdict claimed from them.

For every full and sampled intervention, up to two discordant seeds in each win/loss direction were replayed. All such trace files and first differing event/action context are saved. The review shows expected loot/equipment/monster/routing divergence, real combat outcomes and ordinary post-divergence RNG differences; no agent mutation, error or fabricated death was found. Examples: experience seed 21 loses the early recovery route and dies to a Snake; no-control seed 32 instead escapes because the deleted loot changes its early route; no-fire-immunity seed 58 changes from escape to dragon-fire death; Dragon→Griffin seed 65 still changes an escape into a Lord death despite the net large benefit. These justify keeping both discordance directions in the audit.

Saved evidence: [trace review](../build/balance-audit-20261005/trace-review.json), [very early death trace](../build/balance-audit-20261005/baseline-traces/26694.txt), [early resource/routing death](../build/balance-audit-20261005/baseline-traces/11.txt), [high-scoring escape](../build/balance-audit-20261005/baseline-traces/60551.txt). The preserved simulator is [reference-simulator.exe](../build/balance-audit-20261005/reference-simulator.exe); snapshots are under `source-snapshot/` and the audit-only instrument source/build under `instrument-src/` / `instrument-build/`.

## 13. Ranked recommended follow-up changes

These are **review priorities and experimental directions**, not approved balance edits. No exact new constants or target win rate are recommended.

### Critical balance issues

**1. Progression shape / midgame scaling**

- Mechanic/content: Progression shape / midgame scaling.
- Measured evidence: Floor 0 mortality 22.260%; floors 5–11 each ≤0.388%; entry level 14 by floor 5; level-up fully restores HP.
- Effect size: 19+ pp gap in floor mortality; XP-pot removal −5.478 pp escape.
- Confidence/evidence quality: High D/C for curve and potion package; causal share of full restoration unresolved.
- Why it may be undesirable: A dangerous opening followed by low mortality for many floors can make progression feel uneven and successful starts snowball.
- Recommended controlled experiment: Separate level-up HP restoration from max-HP growth/XP; test same-seed early safety/resource substitutions.
- Possible tuning direction: Redistribute pressure/power across phases; preserve meaningful upgrades rather than flatten all equipment.

**2. Permanent invisibility availability**

- Mechanic/content: Permanent invisibility availability.
- Measured evidence: Ring removal -17.769 pp; high slot priority/tenure; suppresses pursuit/fire for most types.
- Effect size: 17.769 pp absolute escape loss.
- Confidence/evidence quality: High full-population C under frozen policy; human applicability limited.
- Why it may be undesirable: A rare permanent defense can create a large find/miss branch and suppress broad encounter behavior.
- Recommended controlled experiment: Already tested removal and speed factorial; next use same-position temporary-invisibility or comparable-ring substitution, split by phase.
- Possible tuning direction: Review permanence/perception/availability tradeoffs; retain its distinct stealth identity.

**3. Dragon encounter-list transition / immunity branch**

- Mechanic/content: Dragon encounter-list transition / immunity branch.
- Measured evidence: Dragon deaths 7,320; floor-13 mortality 7.710%; same-position Dragon→Griffin +13.433 pp; immunity removal effect changes by 5.768 pp without dragons.
- Effect size: 13.433 pp diagnostic replacement; 5.432 pp fire-immunity removal.
- Confidence/evidence quality: High full-population C for package, A for generated presence; fire-only causal share not isolated.
- Why it may be undesirable: The abrupt composition transition and large defense-dependent burden can outweigh the preceding smooth scaling.
- Recommended controlled experiment: Replace only capped Dragon slots on floor 13 with a mechanically closer target; isolate fire/perception before any stat tuning.
- Possible tuning direction: Smooth encounter introduction or broaden useful counters; do not automatically nerf every Dragon stat.

### Likely balance issues

**4. Speed enchantment / defensive overlap**

- Mechanic/content: Speed enchantment / defensive overlap.
- Measured evidence: Speed removal -11.702 pp; combined removal -38.015 pp; first enchantment target to magnitude 3.
- Effect size: Full removal effect -11.702 pp.
- Confidence/evidence quality: High C for availability package; exact encounter-rate versus slot/scroll pathways unresolved.
- Why it may be undesirable: An effect reducing enemy opportunities broadly can displace many other accessories and amplify strong gear.
- Recommended controlled experiment: Magnitude-preserving same-position Speed→Ironblood/Vampire comparator; separate enchantment allocation from base availability.
- Possible tuning direction: Review accessory competition and enchantment distribution, without assuming a universal nerf.

**5. Early equipment saturation**

- Mechanic/content: Early equipment saturation.
- Measured evidence: Median armor rating 6 by floor 5; by floor 15, 89.42% hold Two-handed sword and 85.15% Plate.
- Effect size: Adjacent gear replacement effects: -1.793 pp Plate→Splint; -3.244 pp Two-handed sword→Mace.
- Confidence/evidence quality: High D/C; saturation versus monster scaling needs a scoped test.
- Why it may be undesirable: Early access to top tiers can reduce later upgrade decisions and create a long plateau.
- Recommended controlled experiment: Depth-scoped/capped high-tier same-position substitutions, with enchants preserved.
- Possible tuning direction: Consider pacing high-tier availability while keeping intrinsic tier differences and common early usefulness.

**6. Weak competing amulets under this policy**

- Mechanic/content: Weak competing amulets under this policy.
- Measured evidence: Conservation/regeneration low pickup/tenure; 10k removals −0.180/−0.090 pp; speed/level healing crowd recovery.
- Effect size: Small sampled total effects with intervals including zero.
- Confidence/evidence quality: Medium D, limited sampled C; standalone effects insufficiently isolated.
- Why it may be undesirable: Slots may be decided by a few dominant effects, reducing meaningful accessory variety.
- Recommended controlled experiment: Forced-slot or same-position comparisons against speed, plus drink/throw-specific conservation telemetry.
- Possible tuning direction: Review niches or slot competition after causal comparators; do not buff purely from use correlations.

### Minor tuning opportunities

**7. Griffin burden / reward**

- Mechanic/content: Griffin burden / reward.
- Measured evidence: 0.888 damage/engagement, 18 XP; Troll 6.529 damage/engagement, same XP; replacement −1.833 pp.
- Effect size: 1.833 pp total effect; large burden difference but modest overall lethality.
- Confidence/evidence quality: High D/C; desirability is J.
- Why it may be undesirable: Cheap late XP can feed level healing, although harmless encounters can be intended relief.
- Recommended controlled experiment: Cap equal-XP replacement by phase or hold combat stats fixed while testing reward.
- Possible tuning direction: Preserve deliberate breathing room; review reward/threat placement before direct buffs.

**8. Food surplus / sustenance niche**

- Mechanic/content: Food surplus / sustenance niche.
- Measured evidence: Median descent stock 8, fivefold generated/consumed ratio, only 490 low-hunger turns; no-food −50.417 pp.
- Effect size: Huge extreme-removal loss, but negligible baseline scarcity.
- Confidence/evidence quality: High D/C for necessity, insufficient C for marginal surplus.
- Why it may be undesirable: Replenishment may be routine rather than a scarcity decision, weakening sustenance competition.
- Recommended controlled experiment: Deterministically remove/substitute only a capped fraction of food slots per visit; compare sustenance paths.
- Possible tuning direction: Tune replenishment pressure only if routing/inventory decisions improve; do not remove hunger from zero-starvation data.

**9. Dexterity/strength ring opportunity cost**

- Mechanic/content: Dexterity/strength ring opportunity cost.
- Measured evidence: Dexterity→Strength substitution +0.778 pp; dexterity availability itself −0.109 pp, q=0.257.
- Effect size: 0.778 pp exact replacement effect.
- Confidence/evidence quality: High C for substitution, small design magnitude.
- Why it may be undesirable: A neighboring accessory may often offer less useful benefit under current stat/weapon scaling.
- Recommended controlled experiment: Phase-scoped/magnitude-preserving comparisons, including attack-ring/weapon-accuracy combinations.
- Possible tuning direction: Review niche differentiation or progression timing rather than broad stat buffs.

**10. Polymorph ordering and emergency inventory**

- Mechanic/content: Polymorph ordering and emergency inventory.
- Measured evidence: Angel→Dragon heals/clears statuses and adds fire; 67 final-floor Dragon deaths despite zero native Dragon generation; only 8.29% of distinct polymorph pickups activate.
- Effect size: Rare descriptive hazard; causal net effect unisolated.
- Confidence/evidence quality: High D/code evidence; no isolated C.
- Why it may be undesirable: A nominal downward transformation can increase danger, and low-use insurance occupies scarce inventory.
- Recommended controlled experiment: Same-position Polymorph→Teleport/Force comparator and trace dangerous transformations with/without fire immunity.
- Possible tuning direction: Review transformation ordering/niches only after the comparator; do not infer causality from final death attribution.

### Apparently healthy systems

- **Equipment tiers and enchantment remain distinct. [D/C/J]** Common low tiers have real early use; stronger tiers are rarer, retained longer and mechanically different. Adjacent replacement effects are measured. No tier-flattening recommendation is warranted.
- **Multiple combat resource paths remain viable. [C/J]** Individual removal of healing, XP potions, offensive wands or primary control still permits substantial escape. There is no identified mandatory individual combat drop.
- **Food meaningfully constrains continuation. [C/J]** Its baseline buffer is generous, but the removal test causes starvation and preserves a small alternative-recovery tail. Necessity and abundance are separate.
- **The Lord remains a distinct boss. [D/J]** Its highest per-engagement damage and concentrated final-floor deaths are consistent with intended boss scale; total Lord deaths are not themselves a nerf argument.
- **Generation/connectivity is robust. [D/A/J]** The full generation sweep passes and adjusted geometry effects are small. Spatial complexity should not be reduced from weak associations.
- **Raw stat caps and non-universal invisibility preserve limits. [D/J]** Stat potion collection stops at caps, and several late types retain see-invisible pressure.

### Insufficient evidence

- Human information/exploration value of Identify, Mapping, Remove-curse, Digging, See-invisible and Mimic disguise: deliberately neutralized or unsupported by the oracle.
- A final mechanical comparison of cursed versus unreliable wands: both are rejected, so the same outcomes reflect policy behavior rather than equal mechanics.
- Per-type causal modifier effects, fine rarity changes, forced-slot amulet value, isolated status immunity value and most item pairs: generation/quality/position/slot confounding remains.
- Adjusted generated-presence estimates for Lord/Snake, unstable Orc/Rattlesnake models and outcome-selected Yendor availability.
- Local line-of-fire, crowd geometry, dangerous stair adjacency beyond generator guarantees, and an independent generation/combat RNG variance decomposition.
- A human win-rate target, exact new balance constants or a claim that any observed losing seed is unwinnable.

## Appendix A. Complete item catalog

All totals below cover the full baseline census. "Pickup" is transaction units; "carry" is final units. Every item also has generation/pickup/use/equip/discard/carry **run prevalence**, drop counts, charge/drink/throw/read counters, equipment tenure, generated floors, first-activity distributions, adjusted intervals/OR/q and selection correlations in the machine-readable catalog. The appendix makes classification and principal evidence readable without loading raw telemetry.

### FOOD — appropriate

**[D]** Generated 3,381,471 (51.598/run); reached 1,303,372; pickup 926,332; used 677,572; equipped 0; dropped 0; discarded 0; final carry 248,760 in 57,533 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 0.01/0; first used floor mean/median 0.91/1.

**[A]** Adjusted generated presence: -0.773 pp (95% -1.174..-0.372; OR 0.744; q=0.00153; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.647, used ρ=0.857. These correlations are not causal.

**[J]** Ample baseline stock; hunger still consumes routing and one stack slot. Removal tests necessity, not marginal scarcity.

### HEALING — strong

**[D]** Generated 341,732 (5.214/run); reached 334,288; pickup 334,078; used 274,711; equipped 0; dropped 0; discarded 0; final carry 60,794 in 15,421 runs. Charges used 0, drunk/thrown 274,711/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.60/1; first used floor mean/median 3.46/2.

**[A]** Adjusted generated presence: +0.530 pp (95% +0.452..+0.609; OR 1.207; q=2.51e-37; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.584, used ρ=0.436. These correlations are not causal.

**[J]** Useful but not mandatory: XP restoration and other defenses support many wins without it.

### CONFUSION — strong

**[D]** Generated 340,408 (5.194/run); reached 339,985; pickup 336,178; used 323,070; equipped 0; dropped 13,022; discarded 0; final carry 86 in 85 runs. Charges used 0, drunk/thrown 0/323,070, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.69/1; first used floor mean/median 4.74/4.

**[A]** Adjusted generated presence: -0.080 pp (95% -0.160..-0.000; OR 0.973; q=0.0762; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.566, used ρ=0.552. These correlations are not causal.

**[J]** Frequently thrown control; pool experiment establishes collective value, not this potion alone.

### POISON — appropriate

**[D]** Generated 339,443 (5.180/run); reached 353,247; pickup 342,748; used 310,168; equipped 0; dropped 32,226; discarded 0; final carry 354 in 312 runs. Charges used 0, drunk/thrown 0/310,168, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.80/1; first used floor mean/median 4.36/3.

**[A]** Adjusted generated presence: -0.212 pp (95% -0.292..-0.131; OR 0.929; q=7.42e-07; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.577, used ρ=0.547. These correlations are not causal.

**[J]** Thrown weakening/control; pickup transactions exceed generation because of swaps/repicks.

### HARMING — appropriate

**[D]** Generated 345,496 (5.272/run); reached 328,386; pickup 317,712; used 300,291; equipped 0; dropped 17,221; discarded 0; final carry 200 in 179 runs. Charges used 0, drunk/thrown 0/300,291, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.75/1; first used floor mean/median 2.94/2.

**[A]** Adjusted generated presence: -0.195 pp (95% -0.275..-0.115; OR 0.935; q=4.89e-06; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.545, used ρ=0.537. These correlations are not causal.

**[J]** Ranged damage; losing-run use association is not evidence of harmful tactical value.

### STRENGTH — strong

**[D]** Generated 341,485 (5.211/run); reached 298,223; pickup 290,492; used 292,453; equipped 0; dropped 92; discarded 0; final carry 89 in 89 runs. Charges used 0, drunk/thrown 292,453/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.58/1; first used floor mean/median 1.58/1.

**[A]** Adjusted generated presence: -0.188 pp (95% -0.268..-0.109; OR 0.937; q=9.34e-06; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.614, used ρ=0.614. These correlations are not causal.

**[J]** Permanent offensive scaling until raw stat cap, and weakness recovery; effect not isolated experimentally.

### DEXTERITY — strong

**[D]** Generated 325,548 (4.968/run); reached 298,014; pickup 293,557; used 295,482; equipped 0; dropped 63; discarded 0; final carry 101 in 101 runs. Charges used 0, drunk/thrown 295,482/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.68/1; first used floor mean/median 1.68/1.

**[A]** Adjusted generated presence: -0.109 pp (95% -0.189..-0.028; OR 0.963; q=0.0141; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.594, used ρ=0.594. These correlations are not causal.

**[J]** Permanent accuracy scaling until raw stat cap; effect not isolated experimentally.

### PARALYSIS — strong

**[D]** Generated 341,032 (5.204/run); reached 332,723; pickup 332,544; used 332,372; equipped 0; dropped 0; discarded 0; final carry 172 in 164 runs. Charges used 0, drunk/thrown 0/332,372, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.63/1; first used floor mean/median 3.83/3.

**[A]** Adjusted generated presence: +0.090 pp (95% +0.011..+0.170; OR 1.032; q=0.0418; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.573, used ρ=0.574. These correlations are not causal.

**[J]** Frequently used hard control; collective pool effect is strong.

### SLOWING — appropriate

**[D]** Generated 341,520 (5.211/run); reached 355,537; pickup 344,902; used 311,946; equipped 0; dropped 32,541; discarded 0; final carry 415 in 334 runs. Charges used 0, drunk/thrown 0/311,946, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.72/1; first used floor mean/median 4.37/3.

**[A]** Adjusted generated presence: -0.107 pp (95% -0.187..-0.027; OR 0.963; q=0.0142; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.577, used ρ=0.547. These correlations are not causal.

**[J]** Useful tactical control; individual causal contribution remains unisolated.

### EXPERIENCE — very strong

**[D]** Generated 338,567 (5.166/run); reached 331,429; pickup 331,198; used 333,595; equipped 0; dropped 0; discarded 0; final carry 19 in 19 runs. Charges used 0, drunk/thrown 333,595/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.60/1; first used floor mean/median 1.60/1.

**[A]** Adjusted generated presence: +0.724 pp (95% +0.646..+0.802; OR 1.296; q=4.82e-69; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.576, used ρ=0.575. These correlations are not causal.

**[J]** 50 XP, level gains, +3 max HP per level and full HP restoration; measured removal effect bundles all pathways and routing.

### INVISIBILITY — strong

**[D]** Generated 342,520 (5.227/run); reached 335,622; pickup 334,685; used 105,851; equipped 0; dropped 3,007; discarded 0; final carry 226,189 in 37,194 runs. Charges used 0, drunk/thrown 105,851/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 1.65/1; first used floor mean/median 6.70/6.

**[A]** Adjusted generated presence: +0.188 pp (95% +0.109..+0.267; OR 1.068; q=1.01e-05; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.587, used ρ=-0.113. These correlations are not causal.

**[J]** Temporary emergency escape/control; distinct from permanent ring invisibility.

### LONG_SWORD — appropriate

**[D]** Generated 412,449 (6.294/run); reached 126,285; pickup 31,668; used 31,660; equipped 31,660; dropped 27,198; discarded 0; final carry 4,470 in 4,138 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 15,982,458. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 0.62/0; first picked_up floor mean/median 0.62/0; first used floor mean/median 0.62/0.

**[A]** Adjusted generated presence: +0.117 pp (95% +0.039..+0.194; OR 1.042; q=0.00642; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.215, used ρ=0.215, equipped ρ=0.215. These correlations are not causal.

**[J]** Intrinsic weapon range/accuracy retained independently of enchantment. Lower tiers have early tenure; strong tiers dominate later. Frozen preferences are not an optimized weapon comparison.

### DAGGER — appropriate

**[D]** Generated 340,041 (5.189/run); reached 82,636; pickup 14,018; used 14,016; equipped 14,016; dropped 11,605; discarded 0; final carry 2,413 in 2,336 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 2,887,603. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 0.21/0; first picked_up floor mean/median 0.21/0; first used floor mean/median 0.21/0.

**[A]** Adjusted generated presence: -0.058 pp (95% -0.137..+0.022; OR 0.980; q=0.195; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.088, used ρ=0.088, equipped ρ=0.088. These correlations are not causal.

**[J]** Intrinsic weapon range/accuracy retained independently of enchantment. Lower tiers have early tenure; strong tiers dominate later. Frozen preferences are not an optimized weapon comparison.

### SPEAR — appropriate

**[D]** Generated 270,774 (4.132/run); reached 77,887; pickup 17,774; used 17,773; equipped 17,773; dropped 15,221; discarded 0; final carry 2,553 in 2,439 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 6,817,625. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 0.58/0; first picked_up floor mean/median 0.58/0; first used floor mean/median 0.58/0.

**[A]** Adjusted generated presence: +0.026 pp (95% -0.057..+0.110; OR 1.009; q=0.614; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.138, used ρ=0.138, equipped ρ=0.138. These correlations are not causal.

**[J]** Intrinsic weapon range/accuracy retained independently of enchantment. Lower tiers have early tenure; strong tiers dominate later. Frozen preferences are not an optimized weapon comparison.

### MACE — strong

**[D]** Generated 202,422 (3.089/run); reached 90,334; pickup 35,702; used 35,700; equipped 35,700; dropped 24,436; discarded 0; final carry 11,266 in 10,483 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 44,794,493. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 1.96/1; first picked_up floor mean/median 1.96/1; first used floor mean/median 1.96/1.

**[A]** Adjusted generated presence: +0.126 pp (95% +0.036..+0.216; OR 1.045; q=0.0117; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.294, used ρ=0.294, equipped ρ=0.294. These correlations are not causal.

**[J]** Intrinsic weapon range/accuracy retained independently of enchantment. Lower tiers have early tenure; strong tiers dominate later. Frozen preferences are not an optimized weapon comparison.

### TWO_HANDED_SWORD — strong

**[D]** Generated 133,230 (2.033/run); reached 77,023; pickup 55,077; used 55,075; equipped 55,075; dropped 8,410; discarded 0; final carry 46,667 in 43,527 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 162,005,814. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 4.25/3; first picked_up floor mean/median 4.25/3; first used floor mean/median 4.25/3.

**[A]** Adjusted generated presence: +0.094 pp (95% -0.010..+0.197; OR 1.034; q=0.109; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.531, used ρ=0.531, equipped ρ=0.531. These correlations are not causal.

**[J]** Intrinsic weapon range/accuracy retained independently of enchantment. Lower tiers have early tenure; strong tiers dominate later. Frozen preferences are not an optimized weapon comparison.

### CHAIN_MAIL — appropriate

**[D]** Generated 206,361 (3.149/run); reached 76,409; pickup 23,431; used 23,423; equipped 23,423; dropped 20,256; discarded 0; final carry 3,175 in 3,015 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 16,659,169. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 1.16/1; first picked_up floor mean/median 1.16/1; first used floor mean/median 1.16/1.

**[A]** Adjusted generated presence: +0.500 pp (95% +0.412..+0.587; OR 1.197; q=2.26e-26; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.232, used ρ=0.232, equipped ρ=0.232. These correlations are not causal.

**[J]** Intrinsic armor tier remains meaningful; enchantment changes absorption within its fixed range. Common tiers provide early upgrades; later replacement is intended progression.

### LEATHER_ARMOR — appropriate

**[D]** Generated 335,988 (5.127/run); reached 80,187; pickup 12,243; used 12,242; equipped 12,242; dropped 9,731; discarded 0; final carry 2,512 in 2,406 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 2,526,810. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 0.20/0; first picked_up floor mean/median 0.20/0; first used floor mean/median 0.20/0.

**[A]** Adjusted generated presence: -0.016 pp (95% -0.096..+0.064; OR 0.994; q=0.781; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.057, used ρ=0.057, equipped ρ=0.057. These correlations are not causal.

**[J]** Intrinsic armor tier remains meaningful; enchantment changes absorption within its fixed range. Common tiers provide early upgrades; later replacement is intended progression.

### RING_MAIL — appropriate

**[D]** Generated 273,494 (4.173/run); reached 70,700; pickup 13,431; used 13,429; equipped 13,429; dropped 11,592; discarded 0; final carry 1,839 in 1,765 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 3,867,122. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 0.34/0; first picked_up floor mean/median 0.35/0; first used floor mean/median 0.34/0.

**[A]** Adjusted generated presence: +0.136 pp (95% +0.053..+0.219; OR 1.049; q=0.00309; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.115, used ρ=0.115, equipped ρ=0.115. These correlations are not causal.

**[J]** Intrinsic armor tier remains meaningful; enchantment changes absorption within its fixed range. Common tiers provide early upgrades; later replacement is intended progression.

### SCALE_MAIL — appropriate

**[D]** Generated 275,642 (4.206/run); reached 82,613; pickup 19,353; used 19,349; equipped 19,349; dropped 17,005; discarded 0; final carry 2,348 in 2,209 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 8,115,012. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 0.59/0; first picked_up floor mean/median 0.59/0; first used floor mean/median 0.59/0.

**[A]** Adjusted generated presence: +0.323 pp (95% +0.240..+0.405; OR 1.121; q=2.64e-13; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.170, used ρ=0.170, equipped ρ=0.170. These correlations are not causal.

**[J]** Intrinsic armor tier remains meaningful; enchantment changes absorption within its fixed range. Common tiers provide early upgrades; later replacement is intended progression.

### SPLINT_MAIL — strong

**[D]** Generated 160,164 (2.444/run); reached 76,697; pickup 32,493; used 32,491; equipped 32,491; dropped 20,740; discarded 0; final carry 11,753 in 11,225 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 49,224,075. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 2.37/1; first picked_up floor mean/median 2.37/1; first used floor mean/median 2.37/1.

**[A]** Adjusted generated presence: +0.356 pp (95% +0.261..+0.451; OR 1.136; q=5.35e-12; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.322, used ρ=0.322, equipped ρ=0.322. These correlations are not causal.

**[J]** Intrinsic armor tier remains meaningful; enchantment changes absorption within its fixed range. Common tiers provide early upgrades; later replacement is intended progression.

### PLATE_MAIL — strong

**[D]** Generated 109,116 (1.665/run); reached 63,570; pickup 47,727; used 47,724; equipped 47,724; dropped 5,245; discarded 0; final carry 42,482 in 40,633 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 152,414,618. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 4.64/4; first picked_up floor mean/median 4.64/4; first used floor mean/median 4.64/4.

**[A]** Adjusted generated presence: +0.597 pp (95% +0.490..+0.705; OR 1.245; q=6.49e-24; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.524, used ρ=0.524, equipped ρ=0.524. These correlations are not causal.

**[J]** Intrinsic armor tier remains meaningful; enchantment changes absorption within its fixed range. Common tiers provide early upgrades; later replacement is intended progression.

### YENDOR_AMULET — insufficient evidence

**[D]** Generated 37,132 (0.567/run); reached 37,130; pickup 37,130; used 0; equipped 0; dropped 0; discarded 0; final carry 37,130 in 37,130 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 15. first picked_up floor mean/median 15.00/15.

**[A]** Adjusted generated presence: insufficient_data. **Selection-biased D associations:** picked_up ρ=0.945. These correlations are not causal.

**[J]** Objective, not random power loot. Lord-drop availability is outcome-selected and excluded from inference.

### RING_SEE_INVISIBLE — weak

**[D]** Generated 39,505 (0.603/run); reached 23,612; pickup 10,863; used 20,348; equipped 10,863; dropped 7,728; discarded 0; final carry 3,135 in 2,983 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 10,943,391. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 2.90/2; first picked_up floor mean/median 2.90/2; first used floor mean/median 2.90/2.

**[A]** Adjusted generated presence: -0.020 pp (95% -0.196..+0.156; OR 0.993; q=0.849; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.109, used ρ=0.116, equipped ρ=0.109. These correlations are not causal.

**[J]** Oracle knows natural-invisible monster locations already; low slot priority. Human information value is unmeasured.

### RING_STRENGTH — appropriate

**[D]** Generated 45,061 (0.688/run); reached 32,393; pickup 18,593; used 31,259; equipped 18,591; dropped 9,255; discarded 0; final carry 9,338 in 8,582 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 29,641,372. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 4.20/3; first picked_up floor mean/median 4.20/3; first used floor mean/median 4.20/3.

**[A]** Adjusted generated presence: -0.178 pp (95% -0.346..-0.010; OR 0.941; q=0.0552; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.171, used ρ=0.191, equipped ρ=0.171. These correlations are not causal.

**[J]** Offensive damage accessory competes with invisibility, immunity and protection; no isolated removal test.

### RING_DEXTERITY — appropriate

**[D]** Generated 42,463 (0.648/run); reached 33,985; pickup 23,294; used 36,261; equipped 24,672; dropped 7,339; discarded 0; final carry 15,955 in 14,196 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 50,978,423. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 4.94/4; first picked_up floor mean/median 4.94/4; first used floor mean/median 4.94/4.

**[A]** Adjusted generated presence: -0.109 pp (95% -0.281..+0.064; OR 0.963; q=0.257; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.200, used ρ=0.231, equipped ρ=0.207. These correlations are not causal.

**[J]** Negative availability association, if present, does not establish harmful dexterity. Same-position strength substitution tests the opportunity cost.

### RING_PROTECTION — appropriate

**[D]** Generated 42,894 (0.655/run); reached 33,792; pickup 27,567; used 34,323; equipped 28,430; dropped 3,645; discarded 0; final carry 23,922 in 20,484 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 78,263,182. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 5.58/5; first picked_up floor mean/median 5.58/5; first used floor mean/median 5.58/5.

**[A]** Adjusted generated presence: -0.022 pp (95% -0.191..+0.147; OR 0.992; q=0.835; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.269, used ρ=0.287, equipped ρ=0.273. These correlations are not causal.

**[J]** High policy priority; armor interaction shows diminishing conditional association. Sample removal is small; no causal nerf case.

### RING_FIRE_IMMUNITY — strong

**[D]** Generated 40,839 (0.623/run); reached 32,599; pickup 23,526; used 34,992; equipped 24,540; dropped 6,137; discarded 0; final carry 17,389 in 15,428 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 56,599,723. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 5.63/5; first picked_up floor mean/median 5.63/5; first used floor mean/median 5.63/5.

**[A]** Adjusted generated presence: +0.077 pp (95% -0.095..+0.250; OR 1.028; q=0.456; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.334, used ρ=0.344, equipped ρ=0.334. These correlations are not causal.

**[J]** Strong protection concentrated around dragons, plus safer fire-wand use; factorial isolates the total dragon-dependent component.

### RING_ATTACK — appropriate

**[D]** Generated 46,006 (0.702/run); reached 31,208; pickup 16,254; used 28,837; equipped 16,253; dropped 9,651; discarded 0; final carry 6,603 in 6,116 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 21,743,415. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 3.71/3; first picked_up floor mean/median 3.71/3; first used floor mean/median 3.71/3.

**[A]** Adjusted generated presence: -0.241 pp (95% -0.409..-0.073; OR 0.921; q=0.00765; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.149, used ρ=0.166, equipped ρ=0.150. These correlations are not causal.

**[J]** Accuracy accessory competes with stronger defensive rings; useful lower priority, no individual removal test.

### RING_SUSTENANCE — situational

**[D]** Generated 42,707 (0.652/run); reached 26,928; pickup 13,190; used 24,102; equipped 13,190; dropped 8,655; discarded 0; final carry 4,535 in 4,282 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 14,724,842. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 3.30/2; first picked_up floor mean/median 3.30/2; first used floor mean/median 3.30/2.

**[A]** Adjusted generated presence: -0.262 pp (95% -0.437..-0.088; OR 0.914; q=0.00543; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.119, used ρ=0.130, equipped ρ=0.119. These correlations are not causal.

**[J]** Halves hunger tick frequency but baseline food is abundant; competes for a ring slot.

### RING_INVISIBILITY — very strong

**[D]** Generated 44,102 (0.673/run); reached 36,166; pickup 34,715; used 34,710; equipped 34,710; dropped 0; discarded 0; final carry 34,715 in 25,902 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 123,550,393. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 5.96/5; first picked_up floor mean/median 5.96/5; first used floor mean/median 5.96/5.

**[A]** Adjusted generated presence: +1.043 pp (95% +0.894..+1.191; OR 1.504; q=1.39e-32; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.656, used ρ=0.656, equipped ρ=0.656. These correlations are not causal.

**[J]** Highest ring score; prevents pursuit/fire by monsters lacking see-invisible. Late perception flags limit universal immunity.

### AMULET_SPEED — very strong

**[D]** Generated 46,101 (0.703/run); reached 30,002; pickup 26,747; used 26,745; equipped 26,745; dropped 0; discarded 0; final carry 26,747 in 26,747 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 96,862,382. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 5.98/5; first picked_up floor mean/median 5.98/5; first used floor mean/median 5.98/5.

**[A]** Adjusted generated presence: +0.447 pp (95% +0.291..+0.603; OR 1.177; q=2.93e-07; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.556, used ρ=0.556, equipped ρ=0.556. These correlations are not causal.

**[J]** Highest amulet score and first enchantment target, to magnitude 3. Reduces enemy-turn budget from 4 toward 1.

### AMULET_CLARITY — situational

**[D]** Generated 38,898 (0.594/run); reached 19,382; pickup 8,961; used 8,961; equipped 8,961; dropped 5,134; discarded 0; final carry 3,827 in 3,827 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 12,639,290. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 3.07/2; first picked_up floor mean/median 3.07/2; first used floor mean/median 3.07/2.

**[A]** Adjusted generated presence: -0.404 pp (95% -0.586..-0.222; OR 0.873; q=1.86e-05; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.088, used ρ=0.088, equipped ρ=0.088. These correlations are not causal.

**[J]** Confusion immunity helps Incubi, Angels and Lord, but loses slot to preferred accessories.

### AMULET_CONSERVATION — weak

**[D]** Generated 41,119 (0.627/run); reached 17,460; pickup 6,066; used 6,065; equipped 6,065; dropped 4,957; discarded 0; final carry 1,109 in 1,109 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 4,234,950. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 1.88/1; first picked_up floor mean/median 1.88/1; first used floor mean/median 1.88/1.

**[A]** Adjusted generated presence: -0.001 pp (95% -0.172..+0.170; OR 1.000; q=0.99; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.085, used ρ=0.085, equipped ρ=0.085. These correlations are not causal.

**[J]** 25% potion preservation competes with speed; frozen policy gives low priority and short tenure. Human/forced-slot power unresolved.

### AMULET_REGENERATION — weak

**[D]** Generated 43,220 (0.659/run); reached 18,556; pickup 6,756; used 6,756; equipped 6,756; dropped 5,363; discarded 0; final carry 1,393 in 1,393 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 5,156,197. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 2.16/1; first picked_up floor mean/median 2.16/1; first used floor mean/median 2.16/1.

**[A]** Adjusted generated presence: -0.137 pp (95% -0.309..+0.035; OR 0.954; q=0.15; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.094, used ρ=0.094, equipped ρ=0.094. These correlations are not causal.

**[J]** 1 HP on a 1/20 turn roll while injured; largely displaced by speed and level-up full healing.

### AMULET_VAMPIRE — strong

**[D]** Generated 42,104 (0.642/run); reached 25,458; pickup 14,363; used 14,363; equipped 14,363; dropped 6,224; discarded 0; final carry 8,139 in 8,139 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 27,636,132. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 4.37/3; first picked_up floor mean/median 4.37/3; first used floor mean/median 4.37/3.

**[A]** Adjusted generated presence: -0.032 pp (95% -0.203..+0.138; OR 0.989; q=0.785; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.167, used ρ=0.167, equipped ρ=0.167. These correlations are not causal.

**[J]** Heals 1 HP on successful melee; useful but contingent on combat and amulet-slot competition.

### AMULET_IRONBLOOD — strong

**[D]** Generated 39,787 (0.607/run); reached 25,848; pickup 17,407; used 17,405; equipped 17,405; dropped 4,375; discarded 0; final carry 13,032 in 13,032 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 38,911,679. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 5.16/4; first picked_up floor mean/median 5.16/4; first used floor mean/median 5.16/4.

**[A]** Adjusted generated presence: -0.599 pp (95% -0.782..-0.416; OR 0.820; q=8.85e-11; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.125, used ρ=0.125, equipped ρ=0.125. These correlations are not causal.

**[J]** Paralysis immunity; frequently retained and mechanically relevant against Angels/Lord and Tarantulas.

### AMULET_VITALITY — appropriate

**[D]** Generated 43,257 (0.660/run); reached 22,333; pickup 10,425; used 10,424; equipped 10,424; dropped 6,065; discarded 0; final carry 4,360 in 4,360 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 15,202,942. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 3.02/2; first picked_up floor mean/median 3.02/2; first used floor mean/median 3.02/2.

**[A]** Adjusted generated presence: +0.026 pp (95% -0.142..+0.194; OR 1.009; q=0.83; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.103, used ρ=0.103, equipped ρ=0.103. These correlations are not causal.

**[J]** HP reserve accessory; moderate priority and no isolated causal intervention.

### AMULET_WISDOM — appropriate

**[D]** Generated 43,865 (0.669/run); reached 21,344; pickup 8,367; used 8,367; equipped 8,367; dropped 6,326; discarded 0; final carry 2,041 in 2,041 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 7,979,281. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first equipped floor mean/median 2.55/2; first picked_up floor mean/median 2.55/2; first used floor mean/median 2.55/2.

**[A]** Adjusted generated presence: -0.021 pp (95% -0.188..+0.145; OR 0.993; q=0.835; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.112, used ρ=0.112, equipped ρ=0.112. These correlations are not causal.

**[J]** 50% more XP feeds level/HP restoration, but this audit does not isolate it from experience potion scaling.

### SCROLL_IDENTIFY — nearly irrelevant

**[D]** Generated 156,465 (2.388/run); reached 28,591; pickup 0; used 0; equipped 0; dropped 0; discarded 0; final carry 0 in 0 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. no pickup/use/equip.

**[A]** Adjusted generated presence: -0.250 pp (95% -0.349..-0.150; OR 0.918; q=2.1e-06; modeled visits 766,294). **Selection-biased D associations:** constant-zero activity; not estimable. These correlations are not causal.

**[J]** Zero pickup/use because oracle knows types and curse state. Human value cannot be judged with this policy.

### SCROLL_ENCHANT — strong

**[D]** Generated 147,690 (2.254/run); reached 144,566; pickup 144,476; used 143,859; equipped 0; dropped 0; discarded 0; final carry 617 in 583 runs. Charges used 0, drunk/thrown 0/0, scroll reads 143,859, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 3.68/3; first used floor mean/median 3.72/3.

**[A]** Adjusted generated presence: -0.093 pp (95% -0.194..+0.009; OR 0.968; q=0.103; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.573, used ρ=0.577. These correlations are not causal.

**[J]** Nearly all generated units picked; policy primarily enchants speed, protection, armor, then weapons.

### SCROLL_REMOVE_CURSE — nearly irrelevant

**[D]** Generated 158,673 (2.421/run); reached 28,278; pickup 0; used 0; equipped 0; dropped 0; discarded 0; final carry 0 in 0 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. no pickup/use/equip.

**[A]** Adjusted generated presence: -0.222 pp (95% -0.322..-0.122; OR 0.926; q=2.55e-05; modeled visits 766,294). **Selection-biased D associations:** constant-zero activity; not estimable. These correlations are not causal.

**[J]** Zero pickup/use because oracle avoids afflicted gear and wands. Human curse recovery value is unmeasured.

### SCROLL_TELEPORT — situational

**[D]** Generated 147,435 (2.250/run); reached 208,568; pickup 174,235; used 47,102; equipped 0; dropped 77,642; discarded 0; final carry 49,491 in 21,403 runs. Charges used 0, drunk/thrown 0/0, scroll reads 47,102, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 4.03/3; first used floor mean/median 9.11/12.

**[A]** Adjusted generated presence: -0.297 pp (95% -0.400..-0.194; OR 0.903; q=3.57e-08; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.508, used ρ=-0.115. These correlations are not causal.

**[J]** Emergency-only, often retained. Transaction pickup counts can exceed generated units; selection is not causal.

### SCROLL_MAPPING — nearly irrelevant

**[D]** Generated 141,362 (2.157/run); reached 26,430; pickup 0; used 0; equipped 0; dropped 0; discarded 0; final carry 0 in 0 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. no pickup/use/equip.

**[A]** Adjusted generated presence: -0.178 pp (95% -0.282..-0.074; OR 0.940; q=0.00159; modeled visits 766,294). **Selection-biased D associations:** constant-zero activity; not estimable. These correlations are not causal.

**[J]** Zero pickup/use because oracle knows the map. Human value cannot be judged with this policy.

### SCROLL_FEAR — situational

**[D]** Generated 147,043 (2.244/run); reached 171,728; pickup 161,653; used 70,833; equipped 0; dropped 33,463; discarded 0; final carry 57,357 in 21,893 runs. Charges used 0, drunk/thrown 0/0, scroll reads 70,833, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 3.89/3; first used floor mean/median 9.46/11.

**[A]** Adjusted generated presence: +0.131 pp (95% +0.032..+0.231; OR 1.047; q=0.0177; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.550, used ρ=0.060. These correlations are not causal.

**[J]** Emergency AoE control; high stock and lower actual activation are consistent with insurance.

### SCROLL_TORMENT — strong

**[D]** Generated 153,119 (2.336/run); reached 149,313; pickup 149,237; used 145,762; equipped 0; dropped 1; discarded 0; final carry 3,474 in 2,705 runs. Charges used 0, drunk/thrown 0/0, scroll reads 145,762, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 3.67/3; first used floor mean/median 8.78/8.

**[A]** Adjusted generated presence: -0.165 pp (95% -0.266..-0.064; OR 0.945; q=0.00281; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.530, used ρ=0.556. These correlations are not causal.

**[J]** Frequently used mass damage; no individual same-position comparator in this audit.

### SCROLL_MASS_CONFUSE — strong

**[D]** Generated 162,785 (2.484/run); reached 158,822; pickup 158,739; used 158,406; equipped 0; dropped 0; discarded 0; final carry 333 in 283 runs. Charges used 0, drunk/thrown 0/0, scroll reads 158,406, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 3.44/2; first used floor mean/median 6.65/7.

**[A]** Adjusted generated presence: +0.146 pp (95% +0.049..+0.242; OR 1.053; q=0.00642; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.532, used ρ=0.535. These correlations are not causal.

**[J]** Frequently used mass control; removal is bundled into the primary control-pool experiment.

### SCROLL_MASS_POISON — appropriate

**[D]** Generated 151,437 (2.311/run); reached 148,765; pickup 148,382; used 144,335; equipped 0; dropped 1,533; discarded 0; final carry 2,514 in 2,060 runs. Charges used 0, drunk/thrown 0/0, scroll reads 144,335, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 3.67/3; first used floor mean/median 7.69/8.

**[A]** Adjusted generated presence: -0.094 pp (95% -0.194..+0.006; OR 0.968; q=0.0952; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.524, used ρ=0.541. These correlations are not causal.

**[J]** Frequently used mass effect; individual causal value remains unisolated.

### WAND_FORCE — situational

**[D]** Generated 92,163 (1.406/run); reached 104,558; pickup 93,916; used 105,016; equipped 0; dropped 28,943; discarded 0; final carry 51,665 in 29,311 runs. Charges used 105,016, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 5.19/4; first used floor mean/median 9.95/13.

**[A]** Adjusted generated presence: +0.342 pp (95% +0.224..+0.460; OR 1.131; q=1.59e-07; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.499, used ρ=-0.043. These correlations are not causal.

**[J]** Only a minority of distinct pickups activated; retention is policy insurance, not proof of redundancy.

### WAND_TELEPORT — situational

**[D]** Generated 89,171 (1.361/run); reached 73,328; pickup 69,618; used 141,055; equipped 0; dropped 1,252; discarded 0; final carry 48,871 in 26,547 runs. Charges used 141,055, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 5.31/4; first used floor mean/median 9.99/13.

**[A]** Adjusted generated presence: +0.148 pp (95% +0.027..+0.268; OR 1.054; q=0.0298; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.438, used ρ=-0.079. These correlations are not causal.

**[J]** Emergency tool with substantial minority activation; ordinary use counts are charges, not objects.

### WAND_DIGGING — nearly irrelevant

**[D]** Generated 94,739 (1.446/run); reached 16,771; pickup 0; used 0; equipped 0; dropped 0; discarded 0; final carry 0 in 0 runs. Charges used 0, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. no pickup/use/equip.

**[A]** Adjusted generated presence: -0.092 pp (95% -0.212..+0.028; OR 0.969; q=0.168; modeled visits 766,294). **Selection-biased D associations:** constant-zero activity; not estimable. These correlations are not causal.

**[J]** Zero pickup/use by explicit frozen-policy design; human routing/terrain utility is insufficiently measured.

### WAND_FIRE — strong

**[D]** Generated 106,235 (1.621/run); reached 94,184; pickup 87,143; used 500,927; equipped 0; dropped 10,254; discarded 0; final carry 1,299 in 1,172 runs. Charges used 500,927, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 5.01/4; first used floor mean/median 5.80/5.

**[A]** Adjusted generated presence: +0.186 pp (95% +0.073..+0.298; OR 1.068; q=0.00309; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.478, used ρ=0.483. These correlations are not causal.

**[J]** Almost every acquired distinct wand activated; avoids dragons and unsafe self-bursts; interacts with fire immunity.

### WAND_STRIKING — strong

**[D]** Generated 109,672 (1.673/run); reached 87,968; pickup 83,427; used 546,284; equipped 0; dropped 664; discarded 0; final carry 223 in 218 runs. Charges used 546,284, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 4.97/4; first used floor mean/median 5.18/4.

**[A]** Adjusted generated presence: +0.690 pp (95% +0.585..+0.796; OR 1.291; q=2.72e-32; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.469, used ρ=0.467. These correlations are not causal.

**[J]** Nearly every acquired distinct wand activated; strong sustained ranged throughput.

### WAND_ICE — strong

**[D]** Generated 93,681 (1.429/run); reached 77,516; pickup 73,772; used 467,886; equipped 0; dropped 1,075; discarded 0; final carry 157 in 147 runs. Charges used 467,886, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 5.20/4; first used floor mean/median 5.58/5.

**[A]** Adjusted generated presence: +0.647 pp (95% +0.532..+0.761; OR 1.270; q=1.96e-24; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.455, used ρ=0.451. These correlations are not causal.

**[J]** Nearly every acquired distinct wand activated; strong ranged damage/control alternative.

### WAND_POLYMORPH — situational

**[D]** Generated 88,531 (1.351/run); reached 120,009; pickup 93,698; used 22,598; equipped 0; dropped 55,887; discarded 0; final carry 35,379 in 24,699 runs. Charges used 22,598, drunk/thrown 0/0, scroll reads 0, equipped turns 0. Generated floors: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15. first picked_up floor mean/median 5.58/5; first used floor mean/median 13.32/13.

**[A]** Adjusted generated presence: -0.296 pp (95% -0.422..-0.169; OR 0.904; q=8.51e-06; modeled visits 766,294). **Selection-biased D associations:** picked_up ρ=0.402, used ρ=-0.175. These correlations are not causal.

**[J]** Rare last-resort activation. Can turn Angel into fire-immune Dragon, restore HP and clear statuses; enum rank is not a monotone threat ladder.

## Appendix B. Artifact index and validation limits

- [complete item metrics and classifications](../build/balance-audit-20261005/tables/item_catalog.csv) and [complete monster metrics and classifications](../build/balance-audit-20261005/tables/monster_catalog.csv).
- [entry progression means, medians and quantiles](../build/balance-audit-20261005/tables/progression.csv), [final raw/effective stats and equipment](../build/balance-audit-20261005/tables/final_player_states.csv), [monster burden by floor/direction](../build/balance-audit-20261005/tables/monster_progression_burden.csv).
- [entry/exit resources](../build/balance-audit-20261005/tables/entry_exit_resources_and_scaling.csv), [resource changes by visit outcome](../build/balance-audit-20261005/tables/resource_visit_deltas.csv), [first activity timing](../build/balance-audit-20261005/tables/item_first_acquisition_use_equip.csv).
- [enchantment/curse/modifier bins](../build/balance-audit-20261005/tables/equipment_and_wand_instance_bins.csv), [accessory combinations](../build/balance-audit-20261005/tables/accessory_combinations.csv), [six plausible interaction models](../build/balance-audit-20261005/tables/adjusted_interactions.csv).
- Baseline `population/` contains every standard report and all seven raw streams. `experiments/full/` contains 19 full-population interventions; `experiments/sample/` contains 12 labeled screens. `modifier-comparisons/` retains direct five-charge comparisons.
- [final audit validation](../build/balance-audit-20261005/audit-validation.json), [discordant trace review](../build/balance-audit-20261005/trace-review.json), [full generation sweep](../build/balance-audit-20261005/generation-full.log), [byte equality](../build/balance-audit-20261005/instrument-validation.json) and [counter reconciliation](../build/balance-audit-20261005/supplemental-counter-validation.json).

Residual confounding, entry-survivor selection, generated fixed-slot composition, adaptive routing and oracle knowledge remain. A perfect deterministic census removes seed-sampling uncertainty for this build; it does not remove these interpretation limits. Supplemental equipment replacement counts record leaving an equipped slot (including deliberate ring removal), not unique destroyed objects. Instance-bin wand pickup is transactional; distinct-object wand rates use standard telemetry. No production balance changes were made during this audit.
