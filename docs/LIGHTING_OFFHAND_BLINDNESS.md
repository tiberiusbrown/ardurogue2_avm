# Lighting, offhand equipment and blindness

Implemented against repository commit `10a14ac6ee5ecfb72fdcdd00c40d05b0075b2736`.

## Mechanics

Effective sight uses signed arithmetic: `clamp(1 + offhand light + both illumination
rings + brilliance amulet, 0, 6)`. Blindness overrides it with zero. Existing 13×13
compile-time masks, ray blockers and explored-map storage are reused. Remembered
terrain remains; ground items and monsters require current sight. The player's
tile remains visible for interaction. Sight does not change projectile range or
monster AI. No existing accessory grants an independent blindness-proof sense.

| Ordinary item | Inherent property | Instance state |
| --- | --- | --- |
| Candle | +1 light; expected 250 turns | 125 fuel, burn probability 1/2 |
| Torch | +2 light; expected 350 turns | 175 fuel, burn probability 1/2 |
| Lantern | +3 light; expected 900 turns | 150 fuel, burn probability 1/6 |
| Glowing orb | +2 light indefinitely | No fuel |
| Buckler | Defense rating 1 | Ordinary enchantment/curse/knowledge |
| Round shield | Defense rating 2 | Ordinary enchantment/curse/knowledge |
| Tower shield | Defense rating 3 | Ordinary enchantment/curse/knowledge |
| Ring of illumination | +2 light, or −1 cursed | Fixed bonus; randomized ring appearance |
| Amulet of brilliance | +3 light, or −1 cursed | Fixed bonus; randomized amulet appearance |
| Potion of blindness | Drink or throw, `8 + roll(8)` | Ordinary stacking/color/identification |

One persistent offhand inventory index accepts lights or shields. Use equips,
replaces or toggles it in one turn; replaced equipment stays in the pack. Cursed
shields block removal/replacement until uncursed. Bows and two-handed weapons
remain compatible. Destruction, dropping and full-pack swaps clear equipment and
repeat references through the shared lifecycle helpers. Inventory indices are
stable; this checkout does not compact the pack.

Lights are identified, uncursed, unenchantable singletons. All eight `Item.info`
bits hold fuel, with no extra bookkeeping. At turn entry, after incrementing the
turn counter, equipped fuel draws exactly one gameplay RNG roll; successful burn
decrements fuel. Exhaustion immediately removes the item, light and references,
then reports the copied item through item-aware status formatting. Unequipped
lights, drawing and cancelled actions never burn. Wait, search, blindness,
paralysis and slow-player turns burn once, irrespective of monster turn cost.
Inventory estimates are `uint16_t(fuel) * denominator`, formatted as
`lantern ~ 900 t`; estimates are not exact countdowns. The HUD shows `LIGHT n`.

Physical defense is the existing clamped armor rating applied to body rating +
shield rating + protection rings. The absorption enchantment is
`clamp(body enchant + shield enchant, -5, 5)`. Intrinsic ratings never use `info`.
Physical traps share this calculation; shields do not add magic/fire resistance.
Enchant/uncurse scrolls, forges and applicable deterioration/improvement selection
include shields. Existing equipment RNG selection is preserved without a shield.

Drinking replaces the player blindness timer and reports “You are blinded!”.
The drinking turn ages the timer once, leaving 7–14 turns at the next input,
matching existing status timing. Expiration reports “You can see again.” and
restores equipment-derived sight; clarity does not prevent blindness.
A hit monster shares the existing four-bit confusion timer and movement/combat
behavior, with `Monster.state` bit `0x10` classifying blindness. Cause replacement
is centralized: confusion clears the flag; blindness replaces the duration and
sets it. Expiration uses blindness recovery text. Teleport-wand confusion, mass
confusion, confusion traps, polymorph, deaths and resets clear/replace the cause.
No monster timer or AI implementation was added.

## Generation and persistence

Ordinary category rolls use 87 outcomes: food 15, arrows 5, potions 20, scrolls 8,
weapons 8, armor 8, wands 4, rings 2, amulets 2, lights 10 (11.49%), shields 5
(5.75%). Previous category numerators are retained; their shares fall by 17.24%
because ground capacity stays fixed. This includes essential supplies and is an
explicit balance tradeoff, not a claim of unchanged availability.

| One-based floors | Candle | Torch | Lantern | Orb |
| --- | --- | --- | --- | --- |
| 1–3 | 30% | 60% | 10% | 0% |
| 4–8 | 15% | 45% | 35% | 5% |
| 9–16 | 5% | 25% | 60% | 10% |

Shields use 45/35/20 at all depths, with existing ordinary equipment curse and
enchantment rolls. Illumination is 6% of ordinary rings; brilliance is 1/17 of
ordinary amulets. Blindness is one of eleven potion types; existing early healing
substitutions are retained. Every new run equips a fully fueled torch in slot 0
without spending a turn. Floors 2–4 guarantee a ground light after artifact
placement, replacing an ordinary non-food/non-ammo item if none exists, with an
essential-supply fallback only when necessary. Quest/artifact slots are protected;
ascent replenishes no ordinary supplies. The ten artifact seed hashes explicitly
use their previous raw type IDs, preserving their selection and floor schedules.

AVM `Game` is **809 → 812 bytes**: knowledge **6 → 7 bytes** for 49 types,
`offhand_slot` one byte and `blindness` one byte. Features move 773 → 774;
offhand/blindness occupy offsets 810/811. Earlier field offsets are preserved.
Save version **26 → 27** rejects incompatible saves. Native `Game` is 812 bytes
(the old host size was 810 with padding). `Item` remains 2 bytes and `Monster`
8 bytes. No new render buffers or fuel arrays exist.

Production code/constant flash (`.text + .rodata`) grows **92,911 → 98,854
bytes (+5,943)**: `.text` 83,129 → 88,839 and `.rodata` 9,782 → 10,015.
Ordinary `.data` stays **108 bytes**; saved plus ordinary RAM is **917 → 920
of 1,024 bytes (+3)**. Packaged `fxdata.bin` grows **94,208 → 100,352 (+6,144)**
after image headers/alignment; the interpreter HEX and 4,096-byte save image are
unchanged. The root `.arduboy` is rebuilt.

## Validation

Deterministic fuel trials: 4,096 seeded lifetimes per fueled type give means
**250.111 / 350.135 / 900.996** turns. Tests also audit 1,024 exact single-roll
transitions per type, including blindness/paralysis. Generation tests sample
4,096 seeds at each of depths 0/5/12, confirm the requested conditional weights,
raw fuel, all new categories, rare orbs, and early-floor guarantees. Shared tests
cover curse/enchant bounds, forge integration, full pack swaps, save rejection,
visibility/memory, formatting, UI, throwing, effect replacement and reset cleanup.

All **74/74 final-ELF turn benchmarks pass** the 100 ms worst-sample budget.
The slowest is `monster_trap_busy` at **97.907 ms**, leaving 2.093 ms. The eight
new cases range from 40.370 to 77.383 ms. All status pages are acknowledged and
completed outcomes validated; generation/animation are excluded by the existing
runner. Captured AVM frames and native rendering/formatting checks validate the
HUD, blindness, offhand actions and fuel text.

The final complete static stack bound is **250/256 bytes**, with zero analysis
gaps. A real 103.952 ms dense-trap overrun was fixed by reusing trap-entry sight
for feedback and streaming status words in eight-character chunks with one space
width calculation. This uses four additional transient stack bytes, no saved RAM,
and preserves output. Native status/render checks confirm formatting.

The full **17/17 native CTest suite passes** (39.05 seconds), including the
simulator, public CLI, balance statistics/orchestration and scorecard regressions.
Windows initially loaded an incompatible GCC runtime DLL from Meld under CTest;
co-locating the matching GCC 13 runtime DLLs beside executables in ignored build
directories resolved the stalls. No repository/runtime installation change was
needed. Save/load byte preservation and old-version rejection pass.

## Balance and interpretation

Both experiments use the same final candidate executable and maintained
`omniscient-v2` policy hash
`56f6825c65443400482357ea1a3b964c808837f5fdb972c86fdf8c2dff6a485d`.
The radius-six control keeps candidate content/placement and fuel RNG; the same
adaptive policy knows its permanent sight and can favor shields. The shield
control removes shields at their generated locations. Divergent actions/RNG are
never resynchronized. These are controlled effects of the implemented system,
not comparisons against an old executable or old policy.

The full **65,535-seed census** has **13,416 escapes (20.472%)**
with radius-six control and **2,378 (3.629%)** with equipment
lighting: **-16.843 percentage points**. Both arms have **zero
SIM_STUCK/SIM_ERROR**. The first 10,000 seeds separately give **2023/10,000
(20.23%)** control versus **386/10,000 (3.86%)**
candidate. Candidate action hashes match the independent shield experiment for
all 10,000 shared seeds. Census discordances: 11,527 control-only escapes,
489 candidate-only escapes, 1,889 shared escapes and 51,630 shared deaths.

This is a substantial difficulty increase, consistent with constraining sight
and trading light against defense. These controlled runs isolate the implemented
light system plus the same adaptive agent's response, rather than independently
estimating the cost of exploration ignorance. Requested mechanics are retained;
no balance constant was tuned to hide this shift.

Conditional exit survival per entered visit (one-based floors; ascent contains
only runs returning with Yendor):

| Floor | Descent control | Descent candidate | Ascent control | Ascent candidate |
| --- | ---: | ---: | ---: | ---: |
| 1 | 87.16% | 84.48% | 99.99% | 99.96% |
| 2 | 97.03% | 93.80% | 99.92% | 99.92% |
| 3 | 95.56% | 88.83% | 99.99% | 99.96% |
| 4 | 96.58% | 89.37% | 99.97% | 99.96% |
| 5 | 97.24% | 88.94% | 99.97% | 100.00% |
| 6 | 98.04% | 89.74% | 99.96% | 99.92% |
| 7 | 96.58% | 83.50% | 99.93% | 99.83% |
| 8 | 97.88% | 89.98% | 99.87% | 99.96% |
| 9 | 96.61% | 84.58% | 99.76% | 99.83% |
| 10 | 98.75% | 94.56% | 99.85% | 99.71% |
| 11 | 95.19% | 79.75% | 98.77% | 98.81% |
| 12 | 92.45% | 78.91% | 97.93% | 97.87% |
| 13 | 91.26% | 79.82% | 95.42% | 95.28% |
| 14 | 84.37% | 70.67% | 91.40% | 90.42% |
| 15 | 77.76% | 67.55% | 89.94% | 84.49% |
| 16 | 76.93% | 71.93% | — | — |

Deaths before reaching floor 5: **14,385 (21.95%)** control,
**24,307 (37.09%)** candidate. Floor-1 deaths are
8,414 / 10,173. Later-floor death totals also reflect fewer
candidate runs reaching those floors.

Largest immediate cause shifts (counts across all seeds):

| Cause | Control | Candidate |
| --- | ---: | ---: |
| fire:DRAGON | 8,995 | 3,497 |
| PHANTOM | 2,805 | 7,917 |
| HOBGOBLIN | 2,656 | 7,503 |
| ORC | 2,100 | 6,596 |
| LORD | 4,429 | 1,081 |
| DRAGON | 5,010 | 2,468 |
| SNAKE | 7,492 | 9,686 |
| GOBLIN | 1,146 | 3,048 |
| starvation | 733 | 344 |


The 10,000-seed shield removal experiment has **219/10,000 escapes (2.19%)**
without shields and **386/10,000 (3.86%)** with shields: **+1.67 percentage points**,
paired bootstrap 95% interval [1.39975, 1.96]. Discordant pairs are 18 favoring
removal and 185 favoring availability. Both arms have zero simulator failures.

Full-census turn-entry telemetry:

| Metric | Radius-six control | Equipment lighting |
| --- | ---: | ---: |
| Elapsed turns | 220,207,706 | 125,083,093 |
| Fuel exhaustion events | 5,091 | 30,723 |
| Runs with exhaustion | 4,411 | 18,840 |
| Turns with a shield | 202,918,681 | 24,368,212 |
| Turns with a light | 17,150,720 | 100,185,558 |
| Player blind turns | 0 | 0 |
| Deaths while player blind | 0 | 0 |

Equipment-lighting turns by radius (% of all elapsed turns):

| Radius | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Turns % | 0.002 | 0.589 | 1.321 | 44.220 | 48.811 | 1.295 | 3.760 |

Shield/light occupancy is 19.48% / 80.10%; 28.75% of runs exhaust a source.

Candidate ground generation and pickups (descriptive exposure, excludes the
starting torch; pickup rate divides picked-up items by generated-on-visit items):

| Item | Generated | Picked up | Pickup % | Equipped events |
| --- | ---: | ---: | ---: | ---: |
| CANDLE | 159,709 | 50,362 | 31.53 | 36,285 |
| TORCH | 420,671 | 189,420 | 45.03 | 608,109 |
| LANTERN | 272,340 | 131,102 | 48.14 | 277,522 |
| GLOWING_ORB | 37,378 | 5,948 | 15.91 | 6,302 |
| BUCKLER | 193,844 | 6,185 | 3.19 | 6,266 |
| ROUND_SHIELD | 152,044 | 7,891 | 5.19 | 7,969 |
| TOWER_SHIELD | 85,483 | 8,436 | 9.87 | 8,515 |
| RING_ILLUMINATION | 11,523 | 8,636 | 74.95 | 8,634 |
| AMULET_BRILLIANCE | 10,253 | 6,461 | 63.02 | 6,464 |
| BLINDNESS | 160,305 | 114,890 | 71.67 | 0 |

The oracle throws 82,950 blindness potions and drinks 0.


Reviewed final-binary traces in both directions: seed 2 equips a lantern but
still dies to a snake on floor 1 while radius-six control escapes; seed 59 dies
to a goblin on floor 6 under full light but escapes with equipment lighting;
seed 146 dies to a rattlesnake on floor 2 under control but escapes with equipment
lighting. Equipment choice, fuel rolls and action timing change subsequent RNG;
individual discordances do not imply that darkness helps or hurts every seed.

An initial policy comparison was discarded after 60/10,000 reverse-swap failures:
arrow acquisition could displace a wand whose retention value then displaced the
arrows. Bow-owned ammunition retention now exceeds its acquisition value; direct
policy regression and fresh paired references validate the fix. A first full
census then exposed three replenishing summoning fights
(seeds 23490, 46568, 60237): shields plus vampire healing could sustain endless
zero-XP combat. The oracle now bounds stationary summoned combat, uses available
escape/control resources, or steps away from a discovered trap. All three finish
under the repaired policy, and the policy fixture checks the bound. Fresh paired
runs use this repaired policy in both arms. Earlier pre-cleanup and failed-policy
runs are excluded from final measurements. Original-content action hashes
cannot generally match historical runs because starting torch fuel changes combat
RNG each turn, category/potion draws change, and visibility changes decisions.
Layout domains and artifact schedules remain isolated; the no-op intervention
and telemetry/tracing determinism tests pass.

Caveats: the policy knows global terrain and loot, though ranged opportunities
obey sight; it is not a human exploration model. It never drinks blindness, so
player blind turns/deaths are zero in these populations and cannot measure the
potion's human risk. Monster blindness is exercised by throwing and regression
checks. Later-floor generated/picked totals reflect survival and are descriptive,
not causal exposure claims. Large difficulty shifts are reported explicitly;
requested bonuses, curse penalties, fuel probabilities/quantities and shield
ratings are retained without tuning. Blindness hits omit the redundant “potion
shatters” sentence to fit feedback in the existing status area; misses retain it.

Raw experiments, manifests, traces and profiles remain under ignored
`build/lighting/`: `paired-validated-all`, `paired-validated-shields`,
`traces-validated`, and
`turn-benchmarks-final-verified`. Reproduce with the commands in `sim/BALANCE.md` and
`bench/README.md`. Reports reject failed seeds, require matching policy hashes,
and use FDR-adjusted item/monster exploratory results; census percentages are
exact for the finite nonzero 16-bit seed space.
