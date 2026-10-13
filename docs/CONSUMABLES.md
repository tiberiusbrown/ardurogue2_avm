# Scrolls and potions

Every drink, read and successful throw consumes one normal action, including
effects with no targets. Potions retain the Conservation amulet's existing
chance to preserve a dose when drunk; throws always consume one. Consuming an
item identifies its type for the run. A potion hitting a monster identifies it;
Fire also identifies on an observable burst without a direct hit. Canceling a
selection, scroll target or projectile direction consumes neither item nor turn.

## Scrolls

| Scroll | Effect |
| --- | --- |
| Hold Monster | Visible monsters within Chebyshev radius 3 become aggressive and receive 3–5 turns of stun. Both sight and line of sight are required; a longer existing stun is retained. |
| Detect Traps | Discover every hidden trap and explore only those trap cells. Traps remain present and inactive; unrelated terrain and features remain hidden. Inspect their positions on Full Map. |
| Summoning | Create one aggressive, floor-appropriate encounter on a free cardinal neighbor. Four valid candidates are sampled without retries. Full monster capacity or blocked neighbors still consume the scroll. This monster grants no XP or score. Mimics retain their normal disguise initialization. |
| Mass Teleportation | Snapshot eligible visible monsters, then teleport each once through the existing placement routine. Invisible or occluded creatures remain unaffected. Failed placement leaves a monster in place; successful placement retains the existing confusion effect and occupancy checks. |
| Fire | Radius-1 explosion centered on the reader, with the fire wand's 8–15 base damage. Monsters and the reader are processed separately; dragons remain immune, ordinary kills retain rewards, and player fire protection/MR apply. The reader can die. |
| Detect Objects | Show every actual ground-item position, including unknown and unlit items, as off-diagonal pixel pairs on a temporary map. The player's diagonal marker is distinct. Mimics, item identities and unknown terrain are omitted. A/B dismisses the screen without another turn. |

The original nine scrolls retain their effects. Targeted Identify, Enchant and
Remove Curse can now be canceled before reading.

## Potions

| Potion | When drunk | When thrown at a monster |
| --- | --- | --- |
| Extra Healing | Roll from ceil(75% of effective maximum HP) through 100%, then use normal healing modifiers and cap at maximum. Clear confusion and slowing. | Heal by the same proportion of species maximum HP; clear confusion and slowing. |
| Restore Vitality | Clear vampire drain and weakness. Recover effective maximum HP naturally, without healing current HP or increasing permanent stats. | Clear weakness, without healing. |
| Haste | 15–25 turns. Subtract 1 from effective turn cost, after equipment adjustments and slowing's doubling, clamped to 1–255. | Clear slowing; no monster Haste state. |
| Levitation | 15–25 turns. Suppress all ten player entry traps while retaining pickups, stairs and ordinary beneficial features. | No effect. |
| Fire Resistance | 20–30 turns. Complete fire immunity, including flame traps, dragon breath and all item explosions. Overrides cursed fire vulnerability. | No effect. |
| See Invisible | 20–30 turns. See natural and temporary invisibility within ordinary lighting and line of sight, overriding a cursed See Invisible ring. | No effect. |
| Fire | 3–6 base fire damage, with existing fire protection, vulnerability and an MR save (power 6). No area effect. | Shatter at the traced impact/final valid tile, even with no direct target or immediate wall obstruction. Radius-1 burst deals 4–8 base damage, uses power 8 against the player, and can damage multiple creatures and the thrower. |

Beneficial temporary effects refresh to the greater of the old and newly rolled
duration and never stack their bonus. Durations use the existing turn-aging
convention, including the drinking turn. Start and expiration messages describe
each effect; expiration occurs once. Timers survive saving, loading and stairs.
Equipment remains active alongside temporary buffs. Temporary fire immunity
wins over vulnerability; temporary sight wins over cursed sight impairment.

## Generation and appearances

The supply categories remain Food 15, Arrows 5, Potions 20, Scrolls 8, Weapons 8,
Armor 8, Offhand 8 out of 72. Existing accessory/artifact/wand generation and
stable artifact schedule identifiers remain intact. Feature rewards call the
same consumable selectors.

See Invisible retains its independent 6% potion selection (18% on floors 2–4).
Among the remaining potion slots, 75% use the original eleven-type selection
with its existing relative frequencies and depth redirects. The other 25% use:

| New potion | Relative weight in that 25% pool |
| --- | --- |
| Extra Healing | 2 |
| Restore Vitality, Haste, Levitation, Fire Resistance, Fire | 1 each |

Restore Vitality becomes Extra Healing below floor 5. Fire becomes ordinary
Healing on floors 1–4. The original harmful-potion safeguards remain intact.
Thus each weight-1 addition gets about 3.36% of all potion slots outside the
early sight boost, and Extra Healing about 6.71%, before depth redirects.

Scrolls likewise retain original selection in 75% of scroll slots. Their new
25% pool assigns Hold Monster, Detect Traps and Detect Objects weight 2 each;
Summoning, Mass Teleportation and Fire weight 1 each (total 9). Their overall
shares are approximately 5.56% for weight 2 and 2.78% for weight 1. Each original
scroll retains approximately 8.33%. These are explicit relative weights, not
an accidental redistribution from a larger enum range.

All nineteen potion colors and fifteen scroll descriptions are unique within
each run and derived deterministically from the run seed. Names and descriptor
tables remain in flash. Item and Monster representations remain two and eight
bytes respectively.

## Implementation of the earlier consumable expansion

The checked-out baseline already included Blindness, See Invisible, Illumination
and Brilliance, exceeding the counts in the request. Preserving them yields
**18 potions, 15 scrolls, 11 rings, 11 amulets and 7 wands: 62 identifiable types**.
The existing seven-byte identification bitmap becomes eight bytes. Three new
timer bytes follow the already present See Invisible timer. Save version 29
rejects version 28 and earlier, including shifted raw item IDs. There is no
unsafe save migration.

The subsequent magic expansion adds the eighth wand, bringing identification
to 63 types in the same eight-byte bitmap. That expansion used save version 31. The themed-book/mana update uses version 32,
with 19 potions, 15 scrolls, 11 rings, 12 amulets and 8 wands: 65 discoveries in
a nine-byte bitmap. Older saves are rejected.
See the Lightning section below and [the expansion report](MAGIC-EXPANSION-RESULTS.md).

Detect Objects uses a transient Session request and UI mode. The renderer
iterates the existing 24 ground slots directly on the existing Full Map path;
it allocates no detection bitmap and does not modify Game or explore terrain.
Full Map draws exposed walls from packed 16-bit words directly into the existing
framebuffer. A separate floor-rendering frame reuses the ordinary local sight
code; its two 13-word scratch arrays occupy 52 bounded stack bytes, with no
additional static RAM. Pixel-reference tests retain the existing map geometry.
Consumable modals return a deferred input command, so their frames unwind before
gameplay/status/animation. Scroll consumption unwinds before scroll resolution;
monster fire iteration unwinds before player damage and status paging.

Levitation is checked in the shared player tile-entry feature handler before
discovery, activation, telemetry or RNG. This covers walking and forced movement.
Existing player teleportation does not activate destination traps and retains
that behavior. Expiry while standing on a trap does not trigger it; subsequent
entry resumes normal behavior. Monster entry and trap effects are unchanged.

Summoning shares encounter/mimic initialization with existing feature summons.
The existing reward-suppression bit is generalized as MON_SUMMONED, with its old
MON_TRAP_SUMMONED alias retained for existing observers. Existing altar/trap
reward behavior is preserved without an extra monster byte.

Both maintained policies extend in place: **omniscient-v2 and explorer-v1**.
They value restoration, temporary buffs, Hold/Mass Teleport and safe offensive
Fire, and avoid known harmful drinking/reading without a tactical benefit.
Explorer observation interface 5 exposes active buff flags, a public drained-
vitality flag and object positions only while the scroll display is available.
It never exposes remaining timers, item identities or unexplored terrain from
object detection. Remembering those positions is host-only agent memory.
Core telemetry schema 9 records the new types, scroll summons and buff activity/
expiry; historical schema 8 remains readable by the analysis tools.

Explorer-v1 also retains failed tactile probes until new observed terrain
supersedes them, bounds optional frontier/loot attempts using its own knowledge
growth, and uses 16-bit host visit counts. This fixes navigation cycles found
during validation without consulting hidden terrain or changing firmware.

## Validation

The focused native suite covers visibility, range, duration support, all traps,
forced entry, teleporting, saves, appearance uniqueness, full identification,
stacking/conservation, boundaries, blocked/full summons, reward suppression,
fire sources and attribution, renderer secrecy, modal dismissal and both
policies. **All 28 native CTest targets pass**, including the Python balance,
CLI, provenance and reconciliation checks (Clang 22.1.3, Release).

The production and benchmark AVM builds complete with zero stack-analysis gaps.
The packaged `ardurogue2.arduboy` was rebuilt from the production ELF.

| AVM measurement | Baseline | Final | Change |
| --- | ---: | ---: | ---: |
| Saved Game bytes | 837 | 841 | +4 |
| Other static RAM (`.data`) | 108 | 110 | +2 |
| Code (`.text`) | 90,891 | 93,499 | +2,608 |
| Flash constants (`.rodata`) | 10,227 | 10,947 | +720 |
| Code plus constants | 101,118 | 104,446 | +3,328 |
| Production maximum stack | 250 | 250 | 0 |
| Benchmark maximum stack | — | 252 | — |
| `fxdata.bin` | 102,656 | 105,984 | +3,328 |
| `.arduboy` package | 190,556 | 193,884 | +3,328 |

The interpreter image and 4,096-byte save allocation are unchanged. The direct
persistent increase is four bytes rather than the request's estimated six,
because the baseline already had a seven-byte bitmap and See Invisible timer.
Transient RAM grows by two bytes for the object-display request and deferred
inventory target. Item and Monster layout sizes remain unchanged.

**All 102 benchmarks pass the 100 ms limit**, including 87 existing cases and
15 new effect/display cases. The maximum input-to-ready sample is
**94.392625 ms** (`monster_trap_busy`); Detect Objects takes **94.296375 ms**.
Generation and fire animation retain the established benchmark exclusions.
Complete results and LLDB profiles are retained under
`build/consumables-benchmarks-validated/20261012T030315Z-z_dvybj_/`.

## Balance validation

The controlled comparator restores original items at the same generation slots
using `balance:new-consumables-percent=0`. Both variants run the same candidate
executable and maintained omniscient-v2 policy hash
`83a32d3ad36a269f0b8864be2829fa5f4678c01bc7907c3b89836d15f56af86e`.
The independent See Invisible mechanic is 20–30 turns in both arms. Thus the
comparison measures the specified loot substitution under this policy, including
subsequent action/RNG divergence; it does not isolate each item's power.

| Seeds | Original-item control | New content | Escape difference |
| --- | ---: | ---: | ---: |
| 1–10,000 | 5,731 / 10,000 (57.31%) | 4,339 / 10,000 (43.39%) | −13.92 percentage points |
| All 65,535 nonzero seeds | 37,586 / 65,535 (57.3526%) | 28,364 / 65,535 (43.2807%) | −14.0719 percentage points |

Every oracle run finishes with zero simulator failures. The 10k paired bootstrap
95% interval is approximately −14.87 to −12.95 points. The complete census is an
exact deterministic population effect, so no sampling interval or p-value is
reported. Census pairs comprise 24,190 shared escapes, 23,775 shared deaths,
13,396 control-only escapes and 4,174 new-content-only escapes.

This is a **substantial increase in difficulty**, consistent with explicitly
replacing a quarter of the original consumable slots, including permanent-stat
and experience supplies. It is retained as the reviewed content distribution;
no compensating monster, weapon or food tuning is included. The 10k scorecard
flags ten descent floors outside existing broad design bands. These results
should accompany any future generation-weight tuning rather than imply that
the previous difficulty was preserved.

| Census metric, per starting run | Control | New content |
| --- | ---: | ---: |
| Deepest floor | 12.424 | 12.092 |
| Score | 6,082.39 | 5,448.77 |
| Actions | 4,576.53 | 4,344.41 |
| Damage taken | 529.11 | 557.57 |
| Damage dealt | 1,981.93 | 1,787.61 |
| Consumables used | 76.112 | 73.087 |
| Reach floor 12 | 81.176% | 77.118% |
| Reach floor 15 | 68.482% | 59.664% |

Troll, Lord, dragon melee and dragon fire deaths increase by 33.55, 26.72,
25.62 and 22.77 per 1,000 starts respectively. Starvation deaths decrease from
305 to 280; the new content does not remove food pressure. These cause and
resource summaries are descriptive and are not individual-item causal rankings.

In the 10k sample, ordinary Healing use falls from 7.068 to 5.758 doses per
start, with 4.496 Extra Healing doses added. Experience falls from 2.475 to
1.857 and Strength from 4.639 to 3.498. Haste, Levitation and Fire Resistance
are used 1.626, 1.487 and 1.182 times per start respectively. Hold Monster is
used 1.195 times, Mass Teleportation 0.430, and thrown Fire potions 0.948.
Known Summoning scrolls are never deliberately read by the oracle.

Representative discordant trace pairs were reviewed before retaining the
weights. Seeds 14 and 21 change from escape to early death after different
healing/scroll choices reroute exploration; seeds 5 and 15 change from death to
escape after potion/scroll substitutions change combat and routing. Both
directions show legitimate item/action divergence. RNG is never resynchronized.
Traces are retained as `build/consumables-trace-new-{seed}.txt` and
`build/consumables-trace-restore-{seed}.txt` for seeds 5, 14, 15 and 21.

The pristine pre-change executable is a separate compatibility reference,
not the controlled balance comparator. All **1,855 seeds with no See Invisible
dose** match it exactly in result, action hash, actions, turns, score, depth,
level and HP. Their 144,690 original typed item rows, 29,680 monster rows,
56,770 feature rows and 12,783 floor rows also match after translating shifted
item IDs by name. Another 180 seeds share the final run fields despite sight
use; their sight-active counters may differ. All 7,965 run-field divergences
involve See Invisible use, whose requested duration change also consumes a new
random draw. The pristine reference escapes 57.05%; that cross-policy number
is descriptive only.

### Explorer-v1 validation

The same `explorer-v1` name and public-observation boundary are preserved.
Final paired seeds 1–10,000 use one executable, default parameters and policy
hash `f19ddbb862f3f08ab827b3183685788d93859b3c801cd827fbe866a5efb8be80`.
Both arms have **zero simulator failures** and reconciled telemetry.
The original-item control records 220 escapes and 9,780 deaths (2.20% escape);
new content records 95 escapes and 9,905 deaths (0.95%). The paired difference
is −1.25 percentage points, with a 95% bootstrap interval of −1.57 to −0.94;
190 seeds lose an escape and 65 gain one. These rates describe the maintained
player-limited policy, not human players or the omniscient oracle.

Validation exposed actual navigation cycles, fixed by retaining failed probes,
bounding optional goals and preserving visit counts. The final remaining
20,000-action guard hits were investigated individually: new-content seeds
4,127, 5,897 and 6,512 complete at 20,671, 23,049 and 30,312 actions; control
seed 4,624 completes at 22,048. They continue exploring fresh dark ascent
floors rather than repeating indefinitely. Therefore both final arms use the
same 100,000-action guard. Their actual maxima are 30,312 and 22,048; no run
approaches the new guard. Earlier datasets with failures are excluded from
balance conclusions and retained only for investigation.

Raw explorer streams and sealed manifests are in
`build/consumables-explorer-complete-10k` and
`build/consumables-explorer-complete-old-10k`; the paired report is
`build/consumables-explorer-comparison/compare.md`. Use the same census commands
below with `--agent explorer-v1 --seeds 1:10000 --max-actions 100000` in place
of `--all-seeds` to reproduce these runs.

Raw data, executable/CSV hashes, manifests, specifications and comparisons are
retained in ignored `build/`: `consumables-reference-10k`,
`consumables-availability-control-10k`, `consumables-candidate-10k`,
`consumables-control-all`, `consumables-candidate-all`,
`consumables-comparison-all`, `consumables-experiment.json`, `consumables-audit.json` and
`consumables-compatibility.json`. The full comparison is
`build/consumables-comparison-all/compare.md`; the detailed 10k scorecard is
`build/consumables-candidate-10k/report/summary.md`.

To reproduce the controlled census, use one newly built candidate simulator
for both commands, then run the standard analysis (add `.exe` on Windows):

```sh
build/sim-native/sim/ardurogue2_sim --all-seeds --jobs 8 --entry-state --intervention balance:new-consumables-percent=0 --output build/consumables-control-all
build/sim-native/sim/ardurogue2_sim --all-seeds --jobs 8 --entry-state --output build/consumables-candidate-all
python sim/balance.py summarize build/consumables-control-all --simulator build/sim-native/sim/ardurogue2_sim
python sim/balance.py summarize build/consumables-candidate-all --simulator build/sim-native/sim/ardurogue2_sim
python sim/balance.py compare build/consumables-control-all build/consumables-candidate-all --output build/consumables-comparison-all
```


## Wand of Lightning (save version 31)

There are eight wands. The themed-book/mana update brings the full item roster
to 65 identifiable types in a nine-byte bitmap. The new unknown descriptor is **forked**, shuffled bijectively with the
other seven appearances. Item size, four-bit charges and three-bit modifiers
remain unchanged. Lightning replaces 12% of ordinary wand slots using a local
derived draw; the original seven retain their relative weights and supply RNG.

| Modifier | Effect |
| --- | --- |
| Normal | Range 6; 6–10 primary magic damage, one 4–7 arc. |
| Cursed | Player-only 8–12 magic damage, power 8, ordinary MR save. |
| Unreliable | Random cardinal direction with normal damage/chaining. |
| Spreading | N/E/S/W rays with normal chaining and a shared already-hit mask. |
| Powerful | 10–14 primary, 7–10 second, 4–7 third. Each arc starts at the previous hit. |
| Overpowered | Spreading plus Powerful. No monster is hit twice in the activation. |

Every arc requires actual player visibility, Chebyshev distance at most 2 and a
clear geometric supercover path, including diagonal corner obstructions. Nearest
distance wins, then monster slot. Primary death retains its former origin.
Dragons have no lightning immunity. Each activation spends one charge and one
ordinary action, even on a miss. Identification, recharge, curse removal and
depletion use the ordinary wand lifecycle. Flash-resident profiles and a bounded
nonrecursive routine also serve Chain Lightning.

## Mana restoration and Meditation

Potion of Mana Restoration is ordinary and stackable, available from floor 1.
A slot-local 8% replacement of ordinary potion results preserves generation RNG
and the other potion types' relative selection weights. Its new randomized color
is lavender; all 19 potion appearances remain unique per run. Drinking restores
an unbiased integer draw in `ceil(player_max_mana()/2)..player_max_mana()`, capped
at capacity. It spends one ordinary action, identifies normally and obeys potion
conservation. It does not heal HP, remove status effects or grant a buff. Throwing
has no effect on monsters.

Amulet of Meditation is ordinary, nonstackable equipment, using an 8% slot-local
replacement of ordinary amulet results. The twelve amulet descriptors include
amethyst and use their own coprime appearance permutation. Equipping identifies
it. Normal Meditation regenerates one MP on multiples of 8 turns; cursed
Meditation uses multiples of 32. Without it the interval is 16. Its curse locks
removal through the ordinary accessory rules. Starvation, death and full MP
suppress natural recovery. The persistent eight-bit turn clock supplies cadence
across wrap, stairs, save/resume and study, without another timer.

Passive MP recovery and HP Regeneration amulet healing are silent. The original
HP probability/effect and simulator resource events remain. Other effect and
expiration messages still appear and still interrupt study. See
[SPELLS.md](SPELLS.md) and [the implementation report](MULTI-SPELL-BOOKS-RESULTS.md).
