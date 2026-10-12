# ArduRogue 2: gameplay balance toward 2–5% explorer escapes

This pass ships a temporary Potion of See Invisible, improves preparation and carrying capacity, and reduces selected ordinary monster statistics. The fifth floor still contains twelve naturally invisible PHANTOMs. The selected configuration was frozen before the final census; the maintained explorer uses ordinary equipment-dependent lighting.

<!-- FINAL_RESULTS_START -->
The complete **1:65535 effective-seed census** gives **1,482/65,535 explorer escapes (2.2614%)** for selected gameplay, compared with **0/65,535 (0.0000%)** for original gameplay under the same repaired explorer. Both arms resolved every seed with no simulator failures. The oracle census gives 66.2974% versus 4.4800%. These are exact finite-population measurements for these frozen agents; sampling confidence intervals are unnecessary for a complete census. Human difficulty remains unmeasured.
<!-- FINAL_RESULTS_END -->

## Shipped parameters

| Parameter | Original | Selected |
|---|---:|---:|
| Ground-item slots | 16 | 20 |
| Inventory slots | 16 | 20 |
| Ordinary monsters per floor | 12 | 12 |
| Fifth-floor ordinary monsters | 12 PHANTOMs | 12 PHANTOMs |
| PHANTOM strength / dexterity / health | 6 / 4 / 12 | 4 / 3 / 10 |
| PHANTOM speed / armor / XP | 4 / 1 / 7 | 4 / 1 / 7 |
| Strength and health of types from ORC through LORD | Original | Ceiling of 75% of original |
| Their dexterity, speed, armor, XP, and abilities | Original | Original |
| Additional ascent strength reduction | None | None |
| Sight-potion duration | Absent | 40 enemy-response turns |
| Sight cutoff among ordinary potion selections | 0 | 6/100 normally; 18/100 on indices 2–4 |
| Convert another ordinary potion to healing | 0 | 40/100, excluding sight potions |
| Early equipment redistribution | None | None |
| Save version / saved bytes on AVM | 27 / 812 | 28 / 837 |

The later-monster adjustment is a species threshold, not a floor threshold. It applies wherever these monsters appear, including the ascent. It leaves BAT, SNAKE, RATTLESNAKE, GOBLIN, ZOMBIE and their mechanics alone; PHANTOM has its separate selected values. The final strength/health pairs are ORC 6/12, TARANTULA 4/9, HOBGOBLIN 6/15, MIMIC 6/15, INCUBUS 7/18, TROLL 7/24, GRIFFIN 6/18, DRAGON 9/36, ANGEL 8/18, and LORD 12/84. Poison, draining, mimic disguises, fire, invisibility, movement, accuracy, armor and rewards retain their existing rules.

There is no phantom reveal-on-hit, stagger, knockback, extra combat state, or position disclosure through new hit feedback. PHANTOM remains naturally invisible. Density reductions were screened but were not selected. The return journey still generates ordinary threats without replenishing ordinary floor supplies; it receives no separate difficulty discount.

## Temporary sight and ordinary item integration

`POTION_SEE_INVISIBLE` is an ordinary, randomized-color potion in generation, gifts, identification, inventory, pickup, throwing, drinking, rendering, save state, observations and telemetry. Drinking assigns the saved `uint8_t Game::see_invisible` timer to **40**. Repeated doses **refresh to 40**, rather than add time. The drinking action includes an ordinary enemy response and ages the timer to 39 before the next player decision. The effect is available during that first response; the timer ages after enemy actions. It expires with “Your potion sight fades.” Floor transitions preserve the timer; the existing stair action does not itself advance the enemy-response clock. Save/resume preserves its remaining value.

`player_can_see_monster()` still requires ordinary light, line of sight and an unblinded player. Potion sight reveals both naturally and temporarily invisible monsters only inside that area. It grants neither more light nor sight through walls. A good Ring of See Invisible remains a permanent equipment source. While a dose is active, its reliable sight overrides the cursed ring's existing flicker; when the timer reaches zero, the original aggregate ring bonus and flicker phase resume. Blindness wins over both. There is no change to ring behavior when no potion effect is active.

The light HUD shows an `S` while potion sight is active. It exposes no exact remaining count. Explorer observations expose that boolean and only legitimate visible monsters and known item information. Different hidden timer values produce identical observations. Hidden potion identities and unseen monster positions remain unavailable to explorer-v1. The oracle continues to be an explicitly privileged difficulty reference.

Throwing uses ordinary consumption, collision, messages and identification. The sight case returns before the monster-potion aggression hook and emits “It has no effect.” It changes neither monster statistics nor flags, invisibility, health, AI state or random state. The ordinary enemy response after a thrown action can still make monsters act; that is separate from the potion's effect. Tests compare complete monster state and RNG around the direct effect, then test ordinary throwing through the gameplay dispatcher.

### Exact generation rule

The ordinary item category selector remains `20/87` potion weight. A potion selection advances the old 22-way subtype draw once, even when it selects sight. A copied, domain-separated seed (`seed ^ 0x53e9`) supplies a `floor_roll(..., 100)` draw. A value below **18** chooses sight on floor indices **2, 3 and 4**; below **6** chooses sight elsewhere. Thus the nominal pre-placement ordinary-slot sight weights are **(20/87) × (18/100) = 4.137931%** on those three floors and **(20/87) × (6/100) = 1.379310%** elsewhere. These are exact configured cutoffs and category weights, not a claim that finite correlated 16-bit RNG draws produce those exact empirical probabilities. Placement rejection, artifacts, guaranteed light and final-floor reservation also affect actual generated counts.

For another ordinary potion, a slot-separated draw converts it to HEALING below **40/100**. It never converts a selected sight potion. Existing depth-dependent subtype restrictions remain, so the nominal healing shares of the potion category are 42.7273% on indices 0 and 5–9, 50.4182% on index 1, 43.9818% on indices 2–4, and 45.2909% on indices 10–15. A successful useful-item gift chooses the potion category with weight 1/4 and uses the same depth-dependent sight selector (nominal sight weights 1.5% normally and 4.5% on indices 2–4), without the ordinary-floor healing conversion. Gift activation and placement can instead produce other outcomes. All existing potion types remain obtainable. Equipment distribution, enchantments, curses, ordinary item-category weights and artifact schedules retain their original rules; the rejected early-equipment experiment is zero in production.

Forty turns normally cover a fight and some movement or another contact, rather than a whole fifth floor. The Ring of See Invisible consequently retains value for long exposure. Refreshing gives a simple decision: spend a dose now or conserve it for the next cluster.

## Policy freeze, repairs and experimental controls

The starting reports were `EXPLORER_HAZARD_ROUTING_AND_AVM_OPTIMIZATION.md`, `EXPLORER_INVISIBLE_CONTACT.md`, the competence report, `sim/BALANCE.md`, the existing balance audit and seed ledger. The baseline preserves original gameplay through explicit simulator controls in the same executable as its candidate. Merely running the selected defaults is not a baseline. Every balance arm uses equipment-dependent lighting and the same agent parameters, executable, observation interface, action limit and effective-seed set within its comparison.

The unavoidable first adaptation enlarged dependent explorer/oracle arrays and item bounds, and added handling for an identified sight potion. Explorer values that item at 150 on indices 2–4 and 90 elsewhere, and drinks it for unseen contact, reported phantom danger or fifth-floor threat when sight is inactive, the player is unblinded and no good sight ring is worn. No other weights were tuned. In the original-gameplay 2,000-seed reference, this adaptation matched **every runs.csv field**, including actions and action hashes, against the original reference.

The audited screening policy hash was `9652feb6a7e82da9036d80d4e8532e702c9f266dceaecf637541498ad26c93b2`. An initial screening build used physical maximum ground capacity for some artifact/gift placements instead of the controlled logical capacity. Those exploratory results in `screen/combined` were superseded, the placement paths were corrected, and isolated screens were rerun with the audited build. They are not selection evidence.

Larger batches then exposed navigation/action failures. With the user's explicit authorization to **repair defects and rerun both arms**, two minimal repairs were made:

1. Permit a summoning-trap frontier endpoint in the existing required-progress fallback when it is the remaining route to unexplored terrain. Optional loot routing retains its existing trap avoidance. Seed 2478 had exhausted safe frontier routes and otherwise stopped with the only frontier excluded.
2. Apply the existing ordinary wand-validity checks to emergency control: an unexamined or known empty, cursed or unreliable wand cannot be repeatedly selected there. The bounded experiment path still tests unknown wands. Seed 41822 alternated emergency targets while rejected empty-wand actions advanced no turns.

Constructed regressions and persistent complete-seed regressions cover both defects. Twenty-four recorded failure or interrupted tail seeds were replayed to an ordinary death or escape; interrupted partial rows are not classified as confirmed simulator failures. The repaired 2,000-seed reference changed four original-balance run rows and 21 selected-balance rows, so fresh reference results are reported rather than silently reusing old ones. Neither repair searches weights or changes the gameplay configuration.

The final repaired explorer policy is frozen at **`9dbe6a920da76691ba0fc222d80bf7352192070590da59393583a46f21c45f91`**; code hash `7f7977933f8528c1199f816f649728b3a6503060c8ed855b679fe7e523565556`, parameter hash `9b454f3ab8b100f0b815e494c177f84dc2dfcad455a9a9c264dfc3178c9ac65e`. The retained frozen executable SHA-256 is **`32b47ad5794328d15a4e5dde445fc8826fcf5b95d28a14e05e3e75c02a55265c`**. The oracle policy is unaffected by these explorer-only repairs. The secondary contact-aware comparison changes only `hazard_routing` to zero and uses that same secondary policy in both arms.

Core telemetry is version 8, survival telemetry version 2, exploration version 6 and observation interface version 4. Invalid simulator outcomes invalidate a whole planned batch: they receive no escape estimate and are never relabeled as deaths. All 1:2000 screens are development samples. Historical census and development coverage means **there is no globally untouched holdout**. The ledger records the additional use of these seeds.

## Isolated screening

Each audited isolated arm uses seeds 1:2000. Depth is the one-based deepest floor; floor indices elsewhere remain zero-based. The original mean is 4.4655. The two initial later-strength screens begin at INCUBUS; subsequent species-strength and health screens broaden the threshold to ORC and that is the shipped threshold. Duration screens set sight frequency to 12% of potion selections at every depth. Other isolated sight-frequency screens use 40 turns. All unmentioned controls equal the original baseline.

<!-- ISOLATED_TABLE_START -->
| Isolated arm | Escapes | Escape % | Mean depth | Lord | Yendor | Depth q |
| --- | --- | --- | --- | --- | --- | --- |
| baseline | 0/2000 | 0.000 | 4.465 | 0 | 0 | — |
| phantom-strength-5 | 0/2000 | 0.000 | 4.740 | 0 | 0 | 2.1e-23 |
| phantom-strength-4 | 0/2000 | 0.000 | 4.910 | 0 | 0 | 2.49e-38 |
| phantom-strength-3 | INVALID | — | — | — | — | — |
| phantom-strength-2 | 0/2000 | 0.000 | 5.046 | 0 | 0 | 1.57e-58 |
| phantom-dexterity-2 | 0/2000 | 0.000 | 4.777 | 0 | 0 | 1.38e-16 |
| phantom-dexterity-3 | 0/2000 | 0.000 | 4.538 | 1 | 1 | 0.000548 |
| phantom-health-8 | 0/2000 | 0.000 | 4.771 | 1 | 1 | 3.17e-20 |
| phantom-health-10 | 0/2000 | 0.000 | 4.584 | 0 | 0 | 2.45e-05 |
| phantom-armor-0 | INVALID | — | — | — | — | — |
| phantom-speed-3 | 0/2000 | 0.000 | 3.909 | 0 | 0 | 8.42e-44 |
| phantom-count-8 | 0/2000 | 0.000 | 4.475 | 1 | 1 | 0.109 |
| phantom-count-10 | 0/2000 | 0.000 | 4.455 | 0 | 0 | 0.882 |
| monsters-8 | 0/2000 | 0.000 | 5.006 | 0 | 0 | 4.11e-22 |
| monsters-10 | 0/2000 | 0.000 | 4.647 | 0 | 0 | 3.3e-06 |
| sight-duration-20 | 0/2000 | 0.000 | 4.290 | 0 | 0 | 0.00012 |
| sight-duration-40 | 0/2000 | 0.000 | 4.309 | 0 | 0 | 0.000143 |
| sight-duration-80 | 0/2000 | 0.000 | 4.297 | 0 | 0 | 7.48e-05 |
| sight-percent-6 | 0/2000 | 0.000 | 4.391 | 0 | 0 | 0.00715 |
| sight-percent-20 | 0/2000 | 0.000 | 4.294 | 0 | 0 | 0.0032 |
| sight-percent-30 | 0/2000 | 0.000 | 4.136 | 0 | 0 | 8.36e-10 |
| sight-early-24 | 0/2000 | 0.000 | 4.320 | 0 | 0 | 4.4e-05 |
| capacity-20-16 | 0/2000 | 0.000 | 5.239 | 0 | 0 | 1.7e-31 |
| capacity-16-20 | 0/2000 | 0.000 | 4.726 | 1 | 1 | 5.44e-14 |
| capacity-20-20 | 2/2000 | 0.100 | 5.819 | 6 | 6 | 4.75e-64 |
| capacity-24-20 | INVALID | — | — | — | — | — |
| healing-25 | 0/2000 | 0.000 | 4.842 | 1 | 1 | 1e-18 |
| healing-50 | 0/2000 | 0.000 | 5.215 | 0 | 0 | 9.1e-44 |
| early-equipment-50 | 0/2000 | 0.000 | 4.562 | 0 | 0 | 0.000923 |
| late-strength-85 | 0/2000 | 0.000 | 4.503 | 0 | 0 | 2.08e-07 |
| late-strength-75 | 0/2000 | 0.000 | 4.533 | 3 | 3 | 1.76e-12 |
| ascent-strength-75 | 0/2000 | 0.000 | 4.465 | 0 | 0 | 1 |
<!-- ISOLATED_TABLE_END -->

Thirty-one planned comparisons form separate Benjamini–Hochberg families for exact paired escape McNemar tests and paired depth sign tests. Invalid planned comparisons are conservatively included with p=1 and no effect estimate. Every isolated escape q-value is 1: two capacity-20/20 wins cannot establish an isolated escape benefit statistically. Depth supplies earlier progression evidence, not a substitute for escape. Raw paired effects, 95% deterministic paired percentile bootstrap intervals, discordant seed lists and q-values are retained.

Capacity 20/20 was the smallest tested joint increase with an escape and substantial depth signal. Ground 20 alone improves depth more than inventory 20 alone, and both permit storing the increased supply; 24/20 was not accepted from an invalid batch. On floor zero, baseline and 16/20 generated 34,715 total item units and picked up 25,040; 20/16 and 20/20 generated 43,428 and picked up 30,685, across 2,000 visits each. There were no pack-full attempts on that floor. These totals include ordinary stack units and gifts, so they are not simply slot count times seeds.

Reducing general density to eight improves depth, but twelve threats and twelve fifth-floor phantoms retain the intended character. PHANTOM strength 4, dexterity 3 and health 10 provide a moderate ordinary-stat reduction. Speed 3 is worse in this game's action timing, and was rejected. Armor 0 and strength 3 had invalid navigation outcomes and do not establish benefits.

## Combined selection and rejected alternatives

<!-- COMBINED_TABLE_START -->
| Family | Arm | Runs | Escapes | Mean depth | Median | Lord | Yendor |
| --- | --- | --- | --- | --- | --- | --- | --- |
| later-screen | baseline | 2000 | 0 (0.000%) | 4.465 | 4.0 | 0 | 0 |
| later-screen | early-combined | 2000 | 3 (0.150%) | 7.165 | 7.0 | 14 | 14 |
| later-screen | late-strength-85 | 2000 | 0 (0.000%) | 4.768 | 4.0 | 2 | 2 |
| later-screen | late-strength-75 | 2000 | 2 (0.100%) | 4.963 | 4.0 | 7 | 7 |
| later-screen | combined-strength-85 | INVALID | — | — | — | — | — |
| later-screen | late-strength-65 | 2000 | 2 (0.100%) | 5.136 | 4.0 | 12 | 11 |
| later-screen | combined-strength-75 | INVALID | — | — | — | — | — |
| later-screen | late-strength-50 | 2000 | 6 (0.300%) | 5.455 | 4.0 | 42 | 41 |
| later-screen | combined-strength-65 | INVALID | — | — | — | — | — |
| later-screen | late-health-85 | INVALID | — | — | — | — | — |
| later-screen | combined-strength-50 | INVALID | — | — | — | — | — |
| later-screen | combined-health-85 | INVALID | — | — | — | — | — |
| later-screen | late-health-75 | 2000 | 1 (0.050%) | 4.792 | 4.0 | 4 | 4 |
| later-screen | combined-density-10 | INVALID | — | — | — | — | — |
| later-screen | combined-health-75 | INVALID | — | — | — | — | — |
| later-screen | combined-density-8 | INVALID | — | — | — | — | — |
| later-screen | combined-moderate-all | INVALID | — | — | — | — | — |
| long-screen | combined-strength-75 | 2000 | 7 (0.350%) | 8.299 | 8.0 | 70 | 69 |
| long-screen | combined-strength-65 | 2000 | 24 (1.200%) | 8.755 | 9.0 | 114 | 114 |
| refine-screen | moderate | 2000 | 39 (1.950%) | 9.180 | 11.0 | 165 | 165 |
| refine-screen | healing-40 | 2000 | 42 (2.100%) | 9.738 | 11.0 | 194 | 193 |
| refine-screen | duration-80 | INVALID | — | — | — | — | — |
| refine-screen | duration-20 | 2000 | 40 (2.000%) | 9.153 | 11.0 | 163 | 163 |
| refine-screen | early-12 | 2000 | 35 (1.750%) | 9.188 | 11.0 | 162 | 161 |
| refine-screen | early-24 | 2000 | 45 (2.250%) | 9.130 | 10.0 | 165 | 165 |
| refine-screen | no-sight | 2000 | 39 (1.950%) | 9.537 | 11.0 | 180 | 180 |
| refine-screen | half-late-strength | 2000 | 60 (3.000%) | 9.433 | 11.0 | 239 | 239 |
<!-- COMBINED_TABLE_END -->

The early-only combination (20/20, PHANTOM 4/3/10, sight 6/18 for 40 turns, healing conversion 25%) produced 3/2,000 escapes and mean depth 7.165. Isolated later strength 75% produced 2/2,000 escapes, and isolated later health 75% produced 1/2,000. Combining these effects improves the whole progression curve, rather than just moving phantom deaths one floor deeper. Initial 20,000-action combination runs were invalid; raising the limit to 100,000 allowed seeds 1027 and 1577 to resolve as starvation deaths after 20,046 and 26,163 actions respectively, with no policy change. Action limits are recorded in every manifest and are equal within each accepted pair.

The coherent moderate combination (75% later strength and health, 25% healing conversion) produced 39/2,000 escapes (1.95%), 165 Lord defeats and 165 Yendor acquisitions. Healing conversion 40% produced 42 escapes (2.10%), 194 Lord defeats and 193 Yendor acquisitions before the defect repairs. Relative to healing 25%, the paired escape change was +0.15 percentage points, 95% CI [-0.55, +0.85], p=0.783846 (28 gains, 25 losses, 14 shared escapes): **it did not establish superior escape probability**. Mean depth improved by 0.5575, CI [0.4035, 0.7151]; reaching index 12 and index 15 improved by 5.45 and 2.85 percentage points. Forty percent was selected for a smoother supply/progression curve, not because 42 exceeds 39. Both promising combinations were subsequently run on 10,000 seeds.

The earlier promising 10,000-seed batches were invalid: the moderate arm recorded 166 confirmed escapes and failures on 4621, 4650, 7246, 7356 and 7512; the selected arm recorded 208 confirmed escapes and a failure on 2478. Those numbers are counts of resolved escapes in invalid batches, **not accepted rates**. One moderate failure was an action-limit exhaustion; the others were route failures. The interrupted original-policy explorer census is likewise superseded. The fresh repaired-policy reference and full paired census replace them.

The eight planned refined comparisons against the original 100,000-action reference have their own separate escape/depth FDR families; an invalid 80-turn arm remains in the family with p=1. These tests assess improvement over original gameplay, not superiority of one nearly equal candidate over another. A 50% later-strength reduction reached 3% on the development sample but was rejected as a larger global combat change. General density, early equipment, extra ascent reductions and maximum supply were unnecessary for the selected configuration.

### Duration and rarity decision

Twenty, forty and eighty turns were screened. Twenty covers almost all individual measured phantom episodes; forty covers several contacts and intervening movement, while eighty spends more time active without establishing a better controlled fight outcome. In the pre-repair selected reference, phantom episode duration had mean 4.677 and median 3 enemy-response turns; 97.54% ended within 20, 99.63% within 40 and 99.96% within 80. Episode duration is a telemetry definition, not a guarantee that every unseen enemy has gone away. A fifth-floor descent visit averaged about 529 turns: a 40-turn dose covers roughly 7.6% of that visit, not the entire floor. The repaired-policy and census measurements below supersede development means where shown.

In the refined development sample, 20 turns produced 40 escapes versus 39 for the otherwise moderate 40-turn combination. The 80-turn arm had a route failure and is not evidence of worse gameplay. Equal-start arenas show the same survival and damage for free 20/40/80-turn sight. Forty was chosen as a useful window for several encounters while preserving a reason to renew, not by maximizing noisy escape counts.

Uniform sight cutoffs 6, 12, 20 and 30 and an early cutoff 24 were screened; greater frequency substitutes for other potions and did not reliably improve overall survival. The selected normal/early cutoffs 6/18 concentrate help on the approach to the phantom floor without maximizing sight supply. Early 12 and 24 refined candidates produced 35 and 45 escapes respectively; those adjacent sample counts are not causal proof that the maximum cutoff is best. The sight-free frequency ablation produced 39 escapes and mean depth 9.537, but changing generation frequency can alter subtype selection and subsequent gameplay: the same-location replacement experiment below more directly measures the supply opportunity cost. Sight remains a required shipped feature regardless of its total escape contribution.

## Final progression, combat and resources

<!-- PROGRESSION_START -->
| Comparison | Arm | Runs | Escapes | Escape % | Mean depth | Median | Lord | Yendor | Ascent % | Starved |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Fresh explorer reference | baseline | 2000 | 0 | 0.0000 | 4.4650 | 4.0 | 0 | 0 | — | 150 |
| Fresh explorer reference | selected | 2000 | 42 | 2.1000 | 9.7380 | 11.0 | 196 | 195 | 21.54 | 146 |
| Secondary contact-aware reference | baseline | 2000 | 0 | 0.0000 | 4.4075 | 4.0 | 0 | 0 | — | 150 |
| Secondary contact-aware reference | selected | 2000 | 41 | 2.0500 | 9.7630 | 12.0 | 192 | 192 | 21.35 | 155 |
| Explorer census | baseline | 65535 | 0 | 0.0000 | 4.4280 | 4 | 13 | 13 | 0.00 | 5217 |
| Explorer census | selected | 65535 | 1482 | 2.2614 | 9.7291 | 11 | 6261 | 6251 | 23.71 | 4974 |
| Oracle census | baseline | 65535 | 2936 | 4.4800 | 7.7822 | 7 | 4107 | 4106 | 71.51 | 206 |
| Oracle census | selected | 65535 | 43448 | 66.2974 | 13.9046 | 16 | 48795 | 48794 | 89.04 | 331 |

The fresh 2,000-seed paired explorer escape improvement is +2.10 percentage points, 95% CI [1.50, 2.75], exact McNemar p=4.5475e-13. Its mean one-based depth difference is +5.2730, CI [5.0570, 5.4890]. These are development-reference intervals, not census sampling intervals.

The repaired census's descriptive first 10,000 seeds give 209/10,000 selected escapes (2.09%) versus 0/10,000 original escapes. This is a prefix of the frozen census, not an independent holdout. Both first-2,000 prefixes match every runs.csv field in the fresh reference, including action hashes. The complete paired census improvement is 2.2614 percentage points; mean depth improves by 5.3012.

The following detailed progression/resource tables use **65,535 seeds per arm**.

| Direction | Index | Original entered | Exited | Conditional % | Reach/all % | Exit/all % | Selected entered | Exited | Conditional % | Reach/all % | Exit/all % |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| descent | 0 | 65535 | 56675 | 86.48 | 100.00 | 86.48 | 65535 | 58365 | 89.06 | 100.00 | 89.06 |
| descent | 1 | 56675 | 50230 | 88.63 | 86.48 | 76.65 | 58365 | 55260 | 94.68 | 89.06 | 84.32 |
| descent | 2 | 50230 | 35688 | 71.05 | 76.65 | 54.46 | 55260 | 51455 | 93.11 | 84.32 | 78.52 |
| descent | 3 | 35688 | 25701 | 72.02 | 54.46 | 39.22 | 51455 | 48642 | 94.53 | 78.52 | 74.22 |
| descent | 4 | 25701 | 17898 | 69.64 | 39.22 | 27.31 | 48642 | 47085 | 96.80 | 74.22 | 71.85 |
| descent | 5 | 17898 | 12986 | 72.56 | 27.31 | 19.82 | 47085 | 45068 | 95.72 | 71.85 | 68.77 |
| descent | 6 | 12986 | 8167 | 62.89 | 19.82 | 12.46 | 45068 | 42026 | 93.25 | 68.77 | 64.13 |
| descent | 7 | 8167 | 6235 | 76.34 | 12.46 | 9.51 | 42026 | 40249 | 95.77 | 64.13 | 61.42 |
| descent | 8 | 6235 | 4060 | 65.12 | 9.51 | 6.20 | 40249 | 37888 | 94.13 | 61.42 | 57.81 |
| descent | 9 | 4060 | 3366 | 82.91 | 6.20 | 5.14 | 37888 | 36453 | 96.21 | 57.81 | 55.62 |
| descent | 10 | 3366 | 1864 | 55.38 | 5.14 | 2.84 | 36453 | 32238 | 88.44 | 55.62 | 49.19 |
| descent | 11 | 1864 | 1028 | 55.15 | 2.84 | 1.57 | 32238 | 27658 | 85.79 | 49.19 | 42.20 |
| descent | 12 | 1028 | 523 | 50.88 | 1.57 | 0.80 | 27658 | 22672 | 81.97 | 42.20 | 34.60 |
| descent | 13 | 523 | 173 | 33.08 | 0.80 | 0.26 | 22672 | 16398 | 72.33 | 34.60 | 25.02 |
| descent | 14 | 173 | 57 | 32.95 | 0.26 | 0.09 | 16398 | 10607 | 64.68 | 25.02 | 16.19 |
| descent | 15 | 57 | 13 | 22.81 | 0.09 | 0.02 | 10607 | 6233 | 58.76 | 16.19 | 9.51 |
| ascent | 0 | 1 | 0 | 0.00 | 0.00 | 0.00 | 1640 | 1482 | 90.37 | 2.50 | 2.26 |
| ascent | 1 | 1 | 1 | 100.00 | 0.00 | 0.00 | 1769 | 1640 | 92.71 | 2.70 | 2.50 |
| ascent | 2 | 1 | 1 | 100.00 | 0.00 | 0.00 | 1915 | 1769 | 92.38 | 2.92 | 2.70 |
| ascent | 3 | 1 | 1 | 100.00 | 0.00 | 0.00 | 2067 | 1915 | 92.65 | 3.15 | 2.92 |
| ascent | 4 | 1 | 1 | 100.00 | 0.00 | 0.00 | 2212 | 2067 | 93.44 | 3.38 | 3.15 |
| ascent | 5 | 1 | 1 | 100.00 | 0.00 | 0.00 | 2327 | 2212 | 95.06 | 3.55 | 3.38 |
| ascent | 6 | 1 | 1 | 100.00 | 0.00 | 0.00 | 2410 | 2327 | 96.56 | 3.68 | 3.55 |
| ascent | 7 | 1 | 1 | 100.00 | 0.00 | 0.00 | 2495 | 2410 | 96.59 | 3.81 | 3.68 |
| ascent | 8 | 1 | 1 | 100.00 | 0.00 | 0.00 | 2565 | 2495 | 97.27 | 3.91 | 3.81 |
| ascent | 9 | 2 | 1 | 50.00 | 0.00 | 0.00 | 2604 | 2565 | 98.50 | 3.97 | 3.91 |
| ascent | 10 | 3 | 2 | 66.67 | 0.00 | 0.00 | 2696 | 2604 | 96.59 | 4.11 | 3.97 |
| ascent | 11 | 4 | 3 | 75.00 | 0.01 | 0.00 | 2829 | 2696 | 95.30 | 4.32 | 4.11 |
| ascent | 12 | 5 | 4 | 80.00 | 0.01 | 0.01 | 3139 | 2829 | 90.12 | 4.79 | 4.32 |
| ascent | 13 | 8 | 5 | 62.50 | 0.01 | 0.01 | 3952 | 3139 | 79.43 | 6.03 | 4.79 |
| ascent | 14 | 13 | 8 | 61.54 | 0.02 | 0.01 | 6233 | 3952 | 63.40 | 9.51 | 6.03 |
<!-- PROGRESSION_END -->

Floor survival is conditional on entering a visit. Reaching and surviving counts divided by all seeds are also reported. Descent and ascent are separate: conditioning on deeper survivors changes the cohort. A Lord can die before a player acquires Yendor; Yendor can be acquired before a final-floor visit ends. Ascent survival means escapes divided by Yendor acquisitions. The final floor has no ordinary ascent visit because taking its upward stairs starts the next floor's ascent visit.

<!-- PHANTOM_START -->
| Metric | Original | Selected |
| --- | --- | --- |
| Phantom episodes | 334033 | 712232 |
| Episodes survived | 315388 | 709377 |
| Visible at first contact | 37814 | 126570 |
| Timer active at first contact | 0 | 27637 |
| Episodes with timer-covered attack events | 0 | 82879 |
| Phantom kills in episodes | 337925 | 768708 |
| HP damage after phantom contact in episodes | 2148273 | 2148086 |
| Healing units after phantom contact in episodes | 19507 | 54242 |
| Escape consumables after phantom contact in episodes | 6544 | 3278 |
| Phantom attack events | 3003862 | 5287015 |
| Timer-covered attack events | 0 | 541104 |
| First phantom contacts | 45468 | 53390 |
| First contacts survived | 39528 | 52620 |
| Alive 100 turns after first episode ends | 36627 | 51998 |
| phantom_encounter_turns_mean | 6.488 | 4.566 |
| phantom_encounter_turns_median | 4.000 | 3.000 |
| Fifth floor visits | 25701 | 48642 |
| Fifth floor mean turns | 437.022 | 544.557 |
| Fifth floor mean entry hp | 34.144 | 38.989 |
| Fifth floor mean entry max hp | 44.691 | 44.366 |
| Fifth floor mean entry level | 9.816 | 9.708 |
| Fifth floor mean entry armor rating | 6.942 | 6.984 |
| Fifth floor mean entry healing units | 0.324 | 2.596 |
| Fifth floor mean entry food units | 3.540 | 5.704 |
| Fifth floor mean damage taken | 45.936 | 32.043 |
| Fifth floor mean consumables used | 2.739 | 4.313 |
| PHANTOM species generated | 518799 | 934342 |
| PHANTOM species encountered | 367137 | 779512 |
| PHANTOM species killed | 342435 | 780382 |
| PHANTOM species damage_to_player | 2074358 | 2016182 |
| Episodes ending within 20 turns % | 95.177 | 97.649 |
| Episodes ending within 40 turns % | 99.235 | 99.662 |
| Episodes ending within 80 turns % | 99.965 | 99.979 |

Baseline: timer active at first contact 0.000%; timer present in at least one attack event 0.000%; timer-covered attack events 0.000%.

Selected: timer active at first contact 3.880%; timer present in at least one attack event 11.637%; timer-covered attack events 10.235%.
<!-- PHANTOM_END -->

Encounters are contiguous telemetry episodes, not unique physical monsters. One episode can include several PHANTOMs, and one survivor can encounter another later. “First survived 100” means alive 100 turns after the first episode ends, or an earlier successful terminal escape according to the diagnostic. Episode HP damage and supplies count everything after the first phantom contact, including other participants in a mixed fight; the separate species damage row counts only damage attributed to PHANTOM. Potion-first and potion-any use the saved active timer. Attack-event coverage counts attempts and misses on both sides, not unique turns; overlapping events are not independent trials. An active timer cannot overcome blindness, so this measure alone does not prove every attack occurred with effective vision. Ring sight can contribute to visible-first without an active potion.

<!-- RESOURCES_START -->
| Item | Arm | Generated | Picked up | Used | Identified | Expired | Active turns | Full attempts |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HEALING | baseline | 180358 | 110155 | 106527 | 39931 | 0 | 0 | 11923 |
| HEALING | selected | 1295192 | 779004 | 778557 | 58573 | 0 | 0 | 55240 |
| SEE_INVISIBLE | baseline | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| SEE_INVISIBLE | selected | 254452 | 154686 | 106707 | 46074 | 105452 | 4236957 | 16042 |
| FOOD | baseline | 798743 | 552019 | 339168 | 0 | 0 | 0 | 14068 |
| FOOD | selected | 2190085 | 1641921 | 986028 | 0 | 0 | 0 | 25268 |
| TORCH | baseline | 300703 | 90995 | 37573 | 0 | 0 | 0 | 2054 |
| TORCH | selected | 639845 | 97380 | 37755 | 0 | 0 | 0 | 1195 |
| LANTERN | baseline | 112933 | 36346 | 32127 | 0 | 0 | 0 | 9104 |
| LANTERN | selected | 528352 | 61774 | 67389 | 0 | 0 | 0 | 15389 |
| SCROLL_IDENTIFY | baseline | 46984 | 30914 | 28822 | 22260 | 0 | 0 | 6478 |
| SCROLL_IDENTIFY | selected | 130075 | 82981 | 74519 | 39305 | 0 | 0 | 29967 |

| Arm | Damage/run | Consumables/run | Pickup units/run | Full attempts/run | Fuel exhaustions/run | Blind turns/run | Radius-0 turns/run | Radius-1 turns/run | Final hunger mean |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| baseline | 146.336 | 17.257 | 40.349 | 2.430 | 0.786 | 5.317 | 27.420 | 1293.899 | 167.949 |
| selected | 575.551 | 47.990 | 97.708 | 8.705 | 0.769 | 5.820 | 114.002 | 4070.695 | 173.910 |

| Fifth-floor cohort | Entry HP | Visits | HP mean | Armor mean | Healing reserve | Food reserve | Healing generated/visit | Food generated/visit | Sight generated/visit |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| baseline | at_least_half | 21886 | 37.688 | 7.036 | 0.371 | 3.526 | 0.837 | 2.734 | 0.000 |
| baseline | below_half | 3815 | 13.810 | 6.409 | 0.055 | 3.620 | 0.829 | 2.726 | 0.000 |
| selected | at_least_half | 47271 | 39.712 | 7.017 | 2.669 | 5.703 | 2.036 | 3.435 | 0.801 |
| selected | below_half | 1371 | 14.061 | 5.848 | 0.090 | 5.732 | 2.035 | 3.502 | 0.824 |

The compact JSON retains corresponding floor/direction/entry-HP strata for all visited floors, and observed generation/pickup/use/full-attempt counts within those strata. Per-run normalization divides every raw item table count by the stated seed count; per-visit comparisons use the exposed visit denominator.

Both complete primary censuses pass streamed floor action/turn and typed item/monster counter reconciliation, including the new identification, effect, expiration and pack-full counters.
<!-- RESOURCES_END -->

Generation and pickups count item units, including stacks and gifts; light “use” includes equipment activations. Identification counts learning a type, not re-identifying every instance. Pack-full counts failed pickup attempts, possibly repeated at the same item; it is not a count of unique full inventories. Total damage and consumptions rise with longer survival and repeated ascent visits. Raw counts therefore cannot establish that the selected arm is more resource-efficient. The exposure table stratifies floor, direction and entry HP, with entry armor and reserve means retained. Differences remain observational: equipment, health, opportunity, RNG and survivor selection differ between arms; this is not a fully adjusted causal estimate.

## Visibility benefit versus spending a potion

The equal-start fixtures place one or three ordinary production PHANTOMs in the same 7×5-tile room. The nearest starts one tile east of the player; the additional two start two and three tiles east. Each fixture uses the same 512 seeds, map, starting state, gear and ordinary explorer/dispatcher. No ring or healing is provided. Poor gear is level 4, HP 24, strength 5, dexterity 4, dagger/leather; middle gear is level 8, HP 36, strength 6, dexterity 5, longsword/chain; prepared gear is level 12, HP 48, strength 7, dexterity 6, two-handed sword/plate. Speed is 4 and enchantments are zero.

Arms grant no sight, grant free sight for 20/40/80 turns, or give an identified 40-turn potion for the ordinary policy to recognize danger and drink. The paid arm includes contact recognition and the drinking-action enemy response. It does not include dungeon acquisition cost. All **15,360** fixtures resolved. The CSV rerun with the repaired policy is byte-identical to the earlier selected-policy fixture CSV (SHA-256 `14b8ac190d3fe01e827d0b79ab7603a7588e69a7ad5c075e0e524987072bf12c`).

<!-- ARENAS_START -->
| Gear | Phantoms | Arm | Survived | Mean damage | Mean actions |
| --- | --- | --- | --- | --- | --- |
| poor | 1 | no sight | 458/512 | 14.234 | 14.076 |
| poor | 1 | free 20 | 458/512 | 14.234 | 5.869 |
| poor | 1 | free 40 | 458/512 | 14.234 | 5.869 |
| poor | 1 | free 80 | 458/512 | 14.234 | 5.869 |
| poor | 1 | paid 40 | 394/512 | 16.828 | 12.742 |
| poor | 3 | no sight | 0/512 | 24.000 | 9.502 |
| poor | 3 | free 20 | 13/512 | 23.896 | 10.057 |
| poor | 3 | free 40 | 13/512 | 23.896 | 10.057 |
| poor | 3 | free 80 | 13/512 | 23.896 | 10.057 |
| poor | 3 | paid 40 | 0/512 | 24.000 | 9.953 |
| middle | 1 | no sight | 512/512 | 3.521 | 5.949 |
| middle | 1 | free 20 | 512/512 | 3.521 | 3.949 |
| middle | 1 | free 40 | 512/512 | 3.521 | 3.949 |
| middle | 1 | free 80 | 512/512 | 3.521 | 3.949 |
| middle | 1 | paid 40 | 512/512 | 5.035 | 6.953 |
| middle | 3 | no sight | 497/512 | 19.027 | 18.078 |
| middle | 3 | free 20 | 512/512 | 10.586 | 11.734 |
| middle | 3 | free 40 | 512/512 | 10.586 | 11.734 |
| middle | 3 | free 80 | 512/512 | 10.586 | 11.734 |
| middle | 3 | paid 40 | 494/512 | 21.072 | 15.676 |
| prepared | 1 | no sight | 512/512 | 1.016 | 4.447 |
| prepared | 1 | free 20 | 512/512 | 1.016 | 2.447 |
| prepared | 1 | free 40 | 512/512 | 1.016 | 2.447 |
| prepared | 1 | free 80 | 512/512 | 1.016 | 2.447 |
| prepared | 1 | paid 40 | 512/512 | 1.758 | 5.477 |
| prepared | 3 | no sight | 512/512 | 5.115 | 11.602 |
| prepared | 3 | free 20 | 512/512 | 3.096 | 7.340 |
| prepared | 3 | free 40 | 512/512 | 3.096 | 7.340 |
| prepared | 3 | free 80 | 512/512 | 3.096 | 7.340 |
| prepared | 3 | paid 40 | 512/512 | 6.584 | 10.395 |
<!-- ARENAS_END -->

Free visibility clearly helps three-phantom middle-gear fights, and shortens actions even when one-phantom damage is unchanged. Paying the response cost can erase that benefit or increase damage. Poor gear remains inadequate against a cluster even with free sight. This supports preparation and timing decisions rather than turning the required potion into a universal rescue.

<!-- REPLACEMENT_START -->
| Agent | Sight shipped | Healing in same locations | Healing gain pp | 95% paired CI | McNemar p | Gains | Losses |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | 42/2000 (2.10%) | 61/2000 (3.05%) | 0.95 | [0.15, 1.70] | 0.0248 | 42 | 23 |
| omniscient-v2 | 1309/2000 (65.45%) | 1466/2000 (73.30%) | 7.85 | [6.20, 9.55] | 1.08e-19 | 233 | 76 |

Both 2,000-seed experiments are valid. If these two agent comparisons form one escape family, BH q-values are 0.024812 for explorer and 2.1645e-19 for oracle. Healing in the same locations also raises mean depth by 0.555 (explorer) and 0.305 (oracle). These analyzed-seed comparisons support a supply opportunity cost; the arena comparison establishes that visibility itself can help.
<!-- REPLACEMENT_END -->

The same-location intervention replaces generated SEE_INVISIBLE with HEALING while preserving information state (`replace-item:SEE_INVISIBLE:HEALING:info=preserve`). Both arms use the selected configuration and identical frozen executable/policy. It preserves generated positions and the original stream up to replacement; subsequent actions, combat and RNG naturally diverge. It neither resynchronizes random draws nor supplies hidden identities. Thus it measures visibility plus consumption behavior against a competing healing resource. It is not a reason to omit the mandatory sight item.

## Capacity, persistence and observation audit

Production has 20 `GroundItem` entries (4 bytes each) and 20 `Item` entries (2 bytes each). The changes add **16 + 8 + 1 = 25 saved bytes**, including the effect timer: **837** on AVM, up from 812. Native production has alignment padding and is 838; SIM reserves 24 physical ground entries for controlled capacity experiments and is 854. These native sizes are not firmware RAM measurements. Identification now covers 50 types in the same seven-byte bitset. Potion permutations and enum/count assertions include the additional type.

Every controlled placement path uses logical capacity: ordinary generation, artifact reservation/modulo, gifts, drops and Yendor. The final-floor reserved slot is **logical capacity minus one** (19 in production, 15 in original controls, 23 in the maximum experiment), not the SIM physical array's last element. Inactive slots remain zero. Audit and tests cover late-slot pickup/drop/stacking, saved equipped-slot references, inventory pagination, rendering, mimics and floor transitions. Ground observation capacity is 36 (24 possible ground items plus twelve mimic entries); explorer and oracle iterate the observed logical capacity and updated arrays. The original capacity controls cannot accidentally place artifacts or gifts outside their active slots.

The new timer and item-ID shifts require **save version 28**. Version 27 saves are rejected through the existing compatibility check; no migration is claimed. Save-copy/resume, floor transition, refresh/expiration and high-slot tests cover the new layout. Appearance initialization and artifact selection preserve the original legacy type domain where needed, so a new enum insertion does not silently reweight artifacts. Simulator-only parameter strings and dynamic screening branches compile out of the AVM build; production monster statistics remain flash-table constants.

## Tests and hardware validation

The final RelWithDebInfo native suite passes **26/26** after the persistent defect regressions and streaming-validation checks. It includes gameplay mechanics, duration/refresh/expiration, blindness and ring composition, ordinary throwing without monster effects or aggression, saved timer and floor transitions, every relevant high-slot path, deterministic generation, observer boundaries and deterministic replay. Serial and parallel simulator fixtures match all CSV fields, including maximum-capacity and 100%-sight controls, and new counters reconcile in the balance framework. Structural route and rejected-wand fixtures plus seeds 2478 and 41822 prevent recurrence. Original-gameplay deterministic fixtures use a complete explicit original control configuration rather than selected defaults. The bounded-memory census validator checks seed completeness, floor action/turn totals, typed visit keys and all event counters including sight duration/expiration; tests damage real parallel sidecars to ensure it rejects invalid outcomes, duplicate visits, wrong visit keys and mismatched effect counts.

The prior 812-byte/save-27 layout and package were checked against original source/package snapshots; the retained previous hardware reference is 81 passing cases, 92.773 ms and 239-byte production stack. New measurement uses the existing AVM toolchain and profiling harness, with RelWithDebInfo and complete stack analysis.

| Measurement | Previous | Selected |
|---|---:|---:|
| AVM benchmark cases passing | 81/81 | 85/85 |
| Worst turn, `monster_trap_busy` | 92.773 ms | 92.9643125 ms |
| Margin below 100 ms | 7.227 ms | 7.0356875 ms |
| Production stack | 239/256 | 241/256 |
| Benchmark stack | 243/256 | 245/256 |
| Unresolved stack-analysis gaps | 0 | 0 |
| `.saved` | 812 | 837 |
| `.data` | 108 | 108 |
| Application static bytes (`.saved` + `.data`) | 920 | 945 |
| AVM `.text` | 89,895 | 90,463 |
| AVM `.rodata` | 10,110 | 10,198 |
| Native AVR interpreter flash data records | 29,560 | 29,560 |
| Interpreter HEX file bytes | 83,157 | 83,157 |
| FX data bytes | 101,376 | 102,144 |
| FX save bytes | 4,096 | 4,096 |
| Rebuilt `.arduboy` file bytes | 189,276 | 190,044 |

The four added AVM cases are `drink_sight` 53.82525 ms, `throw_sight` 56.067125 ms, `sight_expire_dense` 82.934 ms and `sight_cursed_trap` 52.122 ms. All old cases remain in the suite. Production and benchmark stack analyses are complete and below 256. FX data grows by 768 bytes; the native interpreter, save-area size and package metadata are unchanged. The rebuilt root `ardurogue2.arduboy` contains the selected firmware.

<!-- SEALS_START -->
| Artifact | SHA-256 |
| --- | --- |
| package | f62cd45d869fd235c693da30c00ffd4df3d3c03d6d029c81dfec85ce63de7042 |
| frozen_explorer_executable | 32b47ad5794328d15a4e5dde445fc8826fcf5b95d28a14e05e3e75c02a55265c |
| arena_csv | 14b8ac190d3fe01e827d0b79ab7603a7588e69a7ad5c075e0e524987072bf12c |
| configurations | 1feeff7f9b4b08960f79e2d038e6290697e378e6c25ed8672bd0506e6a41c0cf |
| info.json | ad2070e1662aa6cae1f0af8207c8e65ac9fc72dc8e46fa46c794403a60712800 |
| interp.hex | f328d846c782bbfe4fc3e5fa91976446780690ef64c93824d5dc544388058430 |
| fxdata.bin | aec6bf143865879add9c73e78722ef3ba8362cc75063122b4c0a0941931ae213 |
| fxsave.bin | f47a8ec3e9aff2318d896942282ad4fe37d6391c82914f54a5da8a37de1300c6 |
<!-- SEALS_END -->

## Reproduction and retained evidence

Use the repository's existing generators and compilers, initialized matching Visual Studio environment when required, and **RelWithDebInfo** for every target. This run used the already configured `build/sim-clang` and `build/avm-ninja`. `build/game-balance/build-native.cmd` locates Visual Studio with `vswhere`, initializes the matching developer environment, and drives CMake. On a clean setup, follow the repository CMake/toolchain setup first rather than changing a configured compiler or generator.

```powershell
cmake --build build/sim-clang --config RelWithDebInfo --parallel 8
ctest --test-dir build/sim-clang -C RelWithDebInfo --output-on-failure -j 8
cmake --build build/avm-ninja --config RelWithDebInfo --target ardurogue2 ardurogue2_bench --parallel 8

$balancePython = 'build/balance-venv/Scripts/python.exe'
$balanceSim = 'build/sim-clang/sim/ardurogue2_sim.exe'
& $balancePython sim/game_balance_experiments.py --simulator $balanceSim `
  --spec sim/game_balance_configurations.json --seeds 1:65535 --jobs 8 --concurrent 2 `
  --agent explorer-v1 --max-actions 100000 --output build/reproduce-explorer-census
& $balancePython sim/game_balance_experiments.py --simulator $balanceSim `
  --spec sim/game_balance_configurations.json --seeds 1:65535 --jobs 8 --concurrent 2 `
  --agent omniscient-v2 --max-actions 100000 --output build/reproduce-oracle-census
& $balancePython sim/game_balance_statistics.py build/reproduce-explorer-census `
  --output build/reproduce-explorer-census/paired.json
& $balancePython sim/game_balance_validate.py build/reproduce-explorer-census/selected `
  --output build/reproduce-explorer-census/selected-validation.json

& $balancePython bench/profile_turns.py --elf build/ardurogue2-bench.elf `
  --sdk-root ../../build/avm-sdk-install --output build/reproduce-avm-bench --check

& $balancePython sim/game_balance_experiments.py --simulator $balanceSim `
  --seeds 1:2000 --jobs 8 --output build/reproduce-isolated
& $balancePython sim/game_balance_statistics.py build/reproduce-isolated `
  --output build/reproduce-isolated/paired.json
& build/sim-clang/sim/game_balance_arenas.exe build/reproduce-arenas.csv 512
```

Every output root must be fresh. The current generic isolated driver uses the repaired policy and the ORC threshold, so it is an updated screening recipe, not an assertion that it reproduces the older audited INCUBUS-threshold policy's numbers. Exact historical commands, executable hashes and full controls are retained in each `*-spec.json`; use those frozen executables to reproduce historical rows. For small detailed comparisons, `sim/balance.py compare BASELINE CANDIDATE --output OUTPUT` reconciles full sidecars. The bounded run-level statistics script and streamed aggregate summaries avoid loading complete census sidecars into a single dataframe.

AVM reproduction uses the normal `bench/profile_turns.py` interface with the built benchmark ELF/image/interpreter, and a fresh retained profile directory; the exact commands and complete benchmark identity fields are in the retained run. The native build and profiler logs record configuration and toolchain. No backend/code-generator changes are required for this balance pass.

Raw evidence is retained under ignored `build/game-balance`: original snapshots in `reference`; audited isolated arms in `audited-screen`; species and combined development in `later-screen`, `refine-screen`, and `long-screen`; old promising comparisons in `promising-10000`; hardware in `bench-final/20261011T231205Z-p4zuan7k`; fresh primary arms in `repaired/reference-2000` and `repaired/explorer-census`; oracle arms in `final/oracle-census`; repaired contact comparison in `repaired/contact-2000`; replacement experiments in `repaired/explorer-v1-sight-replacement-2000` and `repaired/omniscient-v2-sight-replacement-2000`; final arenas in `repaired/equal-start-final-512.csv`. Exact available paths and compact numerical results are recorded in `GAME_BALANCE_RESULTS.json`. Each arm retains manifests, full control specifications, commands, raw CSVs and SHA-256 seals. Interrupted and invalid data is preserved but excluded from accepted estimates.

## Limits and gameplay tradeoffs

These are deterministic-agent measurements on the finite effective-seed population, not human escape probabilities. The oracle's large improvement shows a material difficulty reduction for a player with perfect information and planning; this deserves human playtesting. The explorer still faces unknown items, equipment-dependent light, contact uncertainty, traps, hunger, resource decisions and a demanding ascent. The numerical target was not achieved by altering agent weights after selection or by reducing every monster's strength by half.

Development comparisons were numerous and share analyzed seeds; FDR families and paired intervals help interpret screens but do not make selection unbiased. A full census removes seed-sampling uncertainty for the frozen agents and configurations, not uncertainty about other policies, human play, or telemetry definitions. The potion has demonstrable encounter benefits and a measurable resource cost. More slots and healing create longer runs and more accumulated inventory pressure as well as better preparation. Saved-format compatibility changes are deliberate. All source, test, report and package changes are left **uncommitted**, with no push.
