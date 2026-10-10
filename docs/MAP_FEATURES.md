# Map features

Twenty positioned features share twelve fixed slots, lookup, rendering and
activation. They never obstruct movement, sight or light. Generation resets the
current array on every floor visit, including regenerated ascent floors.

## Storage and discovery

`MapFeature` is exactly three bytes: two-byte `Position` followed by `id`.
The array appends at offset 773 in `Game`; existing offsets and the two-byte
items, eight-byte monsters and four-byte ground items are unchanged. AVM
`Game` grows from 773 to 809 bytes, exactly **36 saved bytes**. Native alignment
makes `sizeof(Game)` 810. Save version **26** rejects versions 25 and older;
there is no migration.

Zero is an empty slot. Types are the even IDs 2 through 40. Bit 0 (`0x01`)
means hidden, so hidden spike ID 3 becomes visible spike ID 2. Every trap
starts hidden. Player entry reveals it before effects; monster entry reveals it
only when `can_see()` confirms the tile is currently visible. Wait/Search gives each
hidden trap in the eight neighboring cells an independent 1/4 discovery chance.
Searching consumes one normal turn, does not activate traps, and consumes no
search RNG when no eligible trap is nearby. Revealed traps stay visible and
active for the current floor, including through save/continue.

The flash 4x4 atlas uses the complete ID as its frame number. Frame zero and
every odd frame are empty; all twenty even frames have distinct silhouettes.
Explored features draw after terrain and before items, monsters and the player.
The full map filters hidden features. No discovery bitset, per-cell feature
array, rendering cache or dynamic allocation is added to the game.

## Roster

The table gives floor 4+ effects; the depth rules below override early traps.
Raw damage is before the existing protection/armor/MR helpers. Status durations
are set before the movement's normal turn ticks, just like existing effects.

| ID | Trap | Effect on each entry |
| --- | --- | --- |
| 2 | Spike | 3–7 physical damage, normal armor absorption |
| 4 | Poison dart | 1–3 physical damage and one weakness, capped at three |
| 6 | Flame | 5–9 fire damage at power 6; existing fire immunity, vulnerability and MR |
| 8 | Frost | 3–6 magic damage at power 6; separate failed MR save applies 4–7 slowing |
| 10 | Snare | 2–3 paralysis; positive Ironblood prevents it |
| 12 | Teleport | Shared safe player teleport; excludes the original position and does not chain arrivals |
| 14 | Confusion | 5–8 confusion; positive Clarity prevents it |
| 16 | Magic drain | Random charged wand loses 1–2 charges; otherwise 3–6 magic damage at power 6 |
| 18 | Summoning | Up to two floor-roster hostile monsters in nearby visible empty tiles |
| 20 | Rust | Random equipped weapon/armor loses one bounded enchantment; otherwise 1–2 physical damage |

All ten traps remain active indefinitely. Wand draining preserves modifier and
identification bits. Rust preserves curse/identification. Trap summons use the
spare `MON_TRAP_SUMMONED` state bit and grant no XP or score, including holy
kills; ordinary combat, death and slot reuse remain intact. Capacity or placement
failure simply limits the number summoned. Phoenix Heart uses normal lethal
damage handling. Teleport's bounded random search falls back to deterministic
passable tiles, relaxing stair/item avoidance only if necessary; closed doors,
walls, monsters and the original position remain excluded.

### Monster trap effects

Monsters activate traps on successful ordinary movement, including pursuit,
wandering, confusion and fear. They use their existing local movement decisions
and never deliberately avoid hidden or discovered traps. Stationary attacks,
stun, blocked moves, opening a door, generation, summoning, teleport arrivals and
stationary polymorph do not activate traps. Leaving and returning activates the
same permanent trap again. Wand and Hammer knockback activates only the final
landing tile, after the movement/combat scratch frames unwind; spreading force
rays defer their landing effects with a temporary local mask.

| Trap | Monster effect |
| --- | --- |
| Spike | Same depth-tuned raw damage as the player; inherent armor absorption |
| Poison dart | Same depth-tuned physical damage and armor; surviving monster gains existing permanent weakened effect |
| Flame | Same depth-tuned raw fire damage; dragons immune; no player equipment or MR |
| Frost | Same depth-tuned raw damage, bypassing armor; survivor slowed for 6–10 status ticks |
| Snare | Stun for 2–3 monster actions; preserve longer stun |
| Teleport | Same-floor passable unoccupied destination, excluding player/origin; avoid stairs where possible; bounded fallback; preserve HP/effects, no wand confusion or arrival activation |
| Confusion | 5–8 packed status ticks, preserving longer confusion |
| Magic drain | 3–6 magical backlash damage, bypassing physical armor |
| Summoning | Attempt 1–2 nearby floor-roster hostile monsters centered on the trigger; respect terrain/capacity; no arrival activation |
| Rust | 2–4 physical corrosive damage with inherent armor absorption; no armor mutation |

Monster slow/confusion durations use the existing four-bit fields; weakness uses
existing permanent weakness semantics. Snare uses the existing stun byte. No new
persistent state, avoidance memory or per-cell lookup table is required.

Trap damage is environmental: `damage_monster(index, damage, false)` records
ordinary monster damage/death once, without PlayerAttack, aggression, XP or kill
score. Lord death retains the normal Yendor drop. `MON_TRAP_SUMMONED` monsters
remain worth zero XP/score for every death source. A local live-slot mask stops
killed monsters' remaining actions/status aging/regeneration and prevents newly
summoned monsters from acting in reused slots during the same enemy turn.

Visible tile activation permanently clears exactly hidden bit 0, even for an
invisible monster. Offscreen activation keeps hidden traps hidden and produces
no trap feedback. Named feedback also checks monster detection; an undetectable
monster on a visible tile produces only “A trap activates!” All witnessed status
messages are retained, with normal `[more]` paging when the panel fills.

Each interactive object asks its own yes/no question on entry. Forge, pool and
sacrifice also use a filtered inventory picker. Declining or canceling preserves
the object. Valid acceptance clears the whole slot before effects, so the reward
cannot repeat, then emits a disappearance message. Healing uses effective max HP.

| ID | Object | Accepted outcome |
| --- | --- | --- |
| 22 | Fountain | 25% heal 1/4 max HP; 15% +40 hunger; 15% cure; 15% confusion; 10% weakness; 5% +1 STR/DEX (cap 12); 5% hostile summon; 5% identify; 5% teleport |
| 24 | Healing spring | Heal 1/2 max HP and cure weakness, confusion and slowing |
| 26 | Altar | 3/8 heal 1/4 max HP; 2/8 random uncurse; 1/8 identify; 1/8 weakness/confusion; 1/8 hostile summon |
| 28 | Sacrificial altar | Destroy selected object/entire stack; equal chances of healing, XP, random uncurse or +1 equipped weapon/armor enchantment |
| 30 | Enchanted forge | Weapon/armor: 3/4 +1 enchantment, 1/8 −1, 1/8 curse |
| 32 | Statue | 1/2 hostile summon; 1/4 useful item; 1/4 healing and cure |
| 34 | Sarcophagus | 1/2 zombie (depth <4) or phantom; 1/4 useful item; 1/4 +25 score |
| 36 | Runestone | Equal chances: heal 1/4 max HP, cure, +1 MR (cap 127), or 3–6 magic damage at power 6 |
| 38 | Alchemical pool | Five equal rolls: +1/−1 equipment enchantment, uncurse, curse, or improve bounded value |

Sacrifice value is 3 for artifacts, 2 for weapon/armor/wand or a stack of at
least three units, otherwise 1. Its healing is value/4 of max HP and XP is
5 × value. Yendor and equipped cursed objects cannot be sacrificed. Existing
removal checks and item destruction safely clear equipment references.
Forge enchantments clamp to existing limits, including improvements at the cap.
The pool accepts weapons, armor, wands, rings and non-quest amulets; arrows and
consumables are excluded. Inapplicable enchantment rolls become value improvement:
wands gain three charges capped at 15, equipment gains one enchantment, and
accessory magnitude gains one within its packed limit. Curse changes use the
wand modifier helpers where appropriate. Missing identify/uncurse/summon/item
targets fall back to healing; no occupied ground or monster slot is overwritten.

**ID 40, holy symbol**, is visible and reusable. Entry pulses 3–5 damage
(bypassing physical armor) within Chebyshev radius two to zombies, phantoms,
incubi, fallen angels and the Lord. Surviving eligible monsters fear the player;
the Lord is fear-immune. Ordinary kills award normal XP/score. Waiting on the
symbol does not pulse; later re-entry does. Ordinary monsters and the player
are unaffected.

## Generation

Features populate after monsters, items and the initial player position using
the dedicated `MAP_FEATURE_DOMAIN = 0x37e9` floor stream. Gameplay RNG and
ordinary layout, population, supplies, equipment, ammunition and artifact streams
remain isolated. The depth rules are:

| Floors | Total feature budget | Trap share of non-holy slots | Eligible traps | Early effect overrides |
| --- | --- | --- | --- | --- |
| 0–1 | 2–4 | 30% | Spike, dart, confusion | Spike 2–4; dart 1–2 plus normal weakness; confusion 3–5 |
| 2–3 | 3–5 | 40% | Above plus flame, frost, snare, teleport | Flame 3–5; frost 2–4 plus normal slowing/MR; snare 1–2 |
| 4–15 | 4–8 | 55/95 = 57.895% | All ten | Original full-strength effects |

Once per generated floor, select a holy symbol with probability
`min(30, 4 + 2 × actual generated undead/demonic count)` percent. It consumes
one budget slot, uses the existing geometry and fallback, and is never selected
again on that floor. Remaining slots use the depth's trap/interactive split.
Trap and interactive types are uniform within their eligible categories. A
failed placement leaves the slot empty; it does not grant a replacement reward.

Placement traverses a deterministic permutation of the 2,048 map cells. It
first honors geometry and feature spacing (squared distance at least nine),
then relaxes spacing, then geometry. All passes exclude walls, doors, stairs,
player, existing items/monsters/features; traps also exclude Manhattan distance
three or less from the player. Valid floor geometry is already connected.

Teleport candidates additionally require a conservative connectivity proof.
In a 5×5 window, remove the candidate and flood the passable cells using two
32-bit masks. All its passable cardinal neighbors must reconnect within that
window. Closed doors count as traversable, as ordinary movement opens them.
This proves a path in the full map without requiring a map-sized flood buffer.
It may reject safe tiles whose alternate route leaves the window. Every fallback
retains the proof; if no safe tile exists, omit that teleport.

| Feature | Preferred geometry |
| --- | --- |
| Spike, poison dart, frost, confusion, rust | Corridor |
| Flame | Junction or corridor |
| Snare | Corridor or corner |
| Teleport | Open or junction, always with connectivity proof |
| Magic drain | Dead end or corridor |
| Summoning | Open or junction |
| Fountain, altar, statue, pool | Open |
| Spring, sacrifice, forge, sarcophagus | Dead end |
| Runestone | Near wall or dead end |
| Holy symbol | Open or roomy |

## Movement and simulation

`begin_player_move` resolves ordinary movement and immediate traps. When an
item and feature share the entered tile, it first defers entry for pickup
offers. Pickups and full-pack swaps finish before the feature; declined items
stay on the tile. Movement and accepted pickups retain their normal turn costs,
which run after entry resolution. Yendor remains at the Lord's drop location
and can be picked up before a teleport displaces the player. For an
interactive entry it defers the enemy turn using temporary `Session` state.
The frontend gathers the explicit `FeatureChoice` after the combat frame
unwinds, then `complete_player_move` resolves it and completes the movement turn
plus any accepted pickup turns.
Cancellation still completes that movement turn. Fatal traps stop turn processing.
Knockback queues its final landing activation until the combat frame unwinds,
without an extra turn or teleport recursion. Monster entry activates traps as
described below; monsters do not activate interactive objects or holy symbols.
The five feature/pickup session bytes are unsaved; telemetry adds no AVM RAM.

The simulator uses the same resolver. `DecisionContext` erases both ID and
position of every hidden feature; `observed_features` provides the same filtered
view. The maintained policy only reads filtered feature observations. Regression
tests change hidden type and position independently and require unchanged actions.
It treats known teleports as impassable in ordinary routes, regardless of detour
length. Only an unreachable mandatory goal permits a bounded relaxed route;
other known traps retain weighted routing. An eight-position host-only history
abandons repeating optional loot/feature detours and resumes stair progress.
History only records observed movement and known features. The policy
uses nearby beneficial objects with hunger and detour
budgets, and limits intentional holy entries. It does not deliberately farm
summoning traps. New feature curses exposed two bow-switch loops; guards now
respect a cursed equipped bow/weapon and available curse removal.

Telemetry schema 5 retains sparse `features.csv`, per visit and even feature type:
generated, discovered, activated, declined, damage, deaths, spawned and holy
damage, repeated activations, maximum entries on one trap per visit, and actual
teleport displacement. `floors.csv` adds exit hunger, move attempts, generated
unholy population and available holy geometry. Trap/feature damage scopes distinguish
direct causes; summons reset host monster identity. `ranged.csv` distinguishes
entire arrow stacks sacrificed from arrows fired/thrown, preserving conservation.
Schema 5 appends separate `monster_activated`, `monster_damage`, `monster_killed`,
`monster_teleported`, `monster_status` and `monster_spawned` counters. Existing
activation/damage/death/spawn/repetition/displacement counters retain their player
meaning; `monster_status` counts successful extensions/additions. Ordinary typed
monster totals include these environmental events once. Run records append final
residual XP and hunger. Counters and policy scratch live only in native host structures.

## Monster interaction validation

See [the monster-trap report](MONSTER_TRAPS_2026-10-10.md) for the current
implementation, complete stack proof, 66 workloads and paired balance results.

## Current tuning validation

The 2026-10-10 pickup-first revision and controlled results are documented in
[the tuning report](MAP_FEATURE_TUNING_2026-10-10.md). The following sections
preserve the initial feature implementation's measurements and policy identity.

## Historical initial validation

The final native suite passes **16/16** tests, including all twenty outcomes,
protection, forced entry, Phoenix, item packing/selection, save round trips,
hidden observation, render priority, UI turn accounting, deterministic generation,
conditional holy frequency and sparse-capacity fallback. Generation feature
checks cover 512 floors across all four archetypes and 1,000 conditional-weight
samples. Serial/parallel telemetry and arrow conservation checks pass.

Production AVM compilation reports a complete **256-byte** stack bound and
zero analysis gaps; the benchmark image reports 254 bytes. The production bound
uses the full VM stack, so future edits must recheck it. All **58/58** final
turn workloads pass 100 ms, slowest **96.076 ms** (arrow kill). Feature holy,
dense, forge, search and Phoenix cases take 70.065, 76.246, 47.973, 55.729 and
60.594 ms respectively. Profiles are retained under
`build/map-features-turn-benchmarks/20261010T204637Z-tb8n9c70/`.

Against the saved pre-feature ELF, text grows 68,982 → 79,329 bytes and rodata
8,488 → 9,546: **11,405 additional flash bytes**. The packaged image grows
166,748 → 178,268 bytes (**11,520 bytes**). Saved plus ordinary data is
809 + 106 = **915 / 1,024 bytes**, a 39-byte total increase (36 saved + 3 session).

Paired balance artifacts and analysis are described below. Huge raw datasets and
profiles stay in ignored `build/`; the source, tests and small report are tracked.

## Historical initial paired balance and review

Both variants use one RelWithDebInfo executable, maintained `omniscient-v2`
policy hash `76e57893905404906ab59197ddb62915da5c453aea23a6f0f208281ca13a9e7e`,
20,000-action limit and eight isolated workers. Control disables only map-feature
generation via `no-map-features`. RNG is never resynchronized after divergence.
The measured executable is retained as `build/map-features-validated.exe`, SHA-256
`89abb4f2d4b7c7eec15293f626bfa150ae91ead92b4b08b8d2ee070f74abcc38`.
Manifests seal all nine CSV streams. Routine artifacts are under
`build/map-features-validated-10k/{control,treatment,comparison,feature-analysis}`.

For seeds **1–10,000**, control escapes **4,200 (42.00%)**, treatment
**2,312 (23.12%)**: **−18.88 percentage points**, paired bootstrap 95% interval
**[−19.80, −17.95]**. There are 2,214 control-only wins and 326 treatment-only
wins, with **zero SIM_STUCK/SIM_ERROR**. Floor-zero exits fall from 90.58% to
80.07%; reaching floor 12 falls from 72.31% to 47.36%, and reaching floor 15
from 59.31% to 35.33%. Full paired floor survival and FDR-adjusted item/monster
shifts are retained in `comparison/`.

The final **1–65,535 seed census** confirms the shift: **27,585 / 65,535
(42.092%)** control escapes versus **15,015 / 65,535 (22.911%)** treatment,
an exact population difference of **−19.181 percentage points**. There are
14,728 control-only wins and 2,158 treatment-only wins. Both variants have
**zero simulator failures**. Sampling intervals/p-values are unnecessary for
this complete deterministic seed population. Census artifacts are retained in
`build/map-features-validated-all/` with the same policy hash as the routine pair.

Census treatment records **6,387 direct trap deaths**, **2,820 starvation
deaths**, **2,346,297 HP** of trap damage and **81,918 HP** of holy damage.
Arrow conservation and floor/run action/turn reconciliation pass in both
variants; there are **zero single-use activation violations**. Detailed census
type totals are in `feature-analysis/feature_totals.csv`.

Routine **1–10,000 seed treatment** direct trap deaths total **932**: flame 397, frost 211, magic drain 143,
spike 139, dart 40 and rust 2. Effective trap damage totals **358,420 HP**.
Starvation rises from zero to **396** deaths; no direct interactive-feature
deaths occur in this batch. Teleport traps activate **121,390** times after
23,576 discoveries, indicating costly repeated crossings. The worst visit has
more than 150 teleport activations. Confusion traps activate 32,202 times.
These totals describe the maintained policy, not a human win-rate estimate.

The nine single-use types each activate roughly 13,700–18,200 times; healing
springs activate 15,498 times, forges 17,788, sacrifices 13,741 and pools 17,920.
No visit/type has more accepted single-use interactions than generated objects.
Summoning traps create 10,952 marked monsters and award no XP/score. Holy symbols
activate 21,309 times and remove **12,456 monster HP**. Many incidental pulses
have no nearby unholy target; this is not evidence of an independently measured
survival benefit. Native tests verify boss fear immunity and normal kill rewards.

Among successful floor-zero exits, mean exit HP is 20.98 → 19.18 and hunger
133.86 → 138.44. At floor 12, mean exit HP is 78.84 → 77.74 and hunger
151.09 → 150.58. Low-hunger exits (<40) rise from 4 to 433 across all visits.
`floor_transitions.csv` contains every floor/direction; these are survivor-selected
means. `generated_exposure_by_floor.csv` compares feature generation on entered
visits within floor/direction and reports entry HP/max HP, hunger and armor.
Different entry conditions and prior survival prevent causal attribution to
individual feature types. The paired removal measures the whole feature system.

An exploratory binomial model uses generated-on-visit presence for all twenty
types simultaneously, floor/direction strata, entry health/max HP/level/hunger,
armor/STR/DEX/speed, food/healing reserves, generated unholy count and floor
size. Covariance is clustered by run seed and twenty feature tests use BH FDR.
Adjusted visit-mortality odds ratios are teleport **1.55**, flame **1.42**,
frost **1.28**, and healing spring **0.81** (all q < 0.05). Holy symbols show
**0.95**, q = 0.106, so this analysis does not establish a survival benefit.
These associations remain noncausal despite entry-condition adjustment;
`generated_exposure_adjusted.csv` and `association_model.txt` retain all estimates
and uncertainty. They prioritize future controlled tuning experiments.

Curses can now enter equipped slots through forge/pool use, reducing effective
equipment capability and preventing removal until cured. Native tests cover
curse preservation, cursed equipped sacrifice rejection, safe slot clearing,
uncursing and bounded wand/accessory handling. The policy uses available
remove-curse scrolls and random altar benefits, and guards all cursed weapon
switches. This repairs the feature-induced bow loops found during exploratory
runs; invalid exploratory runs are excluded from the accepted comparison.
There is no separate causal curse-only effect estimate.

Representative traces are retained as `build/map-features-audit-{5,44}-{control,treatment}.trace`.
Seed 5 changes from an opening-floor bat death to a full escape after altered
early movement/combat and later feature benefits. Seed 44 changes from escape
to floor-zero starvation after 29 teleport and 27 confusion activations. The
loss is a repeated-crossing hazard, not a simulator failure or XP farming.

The no-feature control matches all 10,000 historical rare-artifact-reference
outcomes. **9,984** action hashes and every common column in all eight legacy
streams match exactly. All **16** differing runs first diverge immediately
after a player teleport scroll: the shared helper now rejects its original
position and has a bounded fallback. Individual first-divergence contexts and
unchanged-stream checks are retained in `feature-analysis/`; this required
teleport safety improvement explains the small compatibility exception.

The requested starting weights and effects are implemented without compensating
changes to unrelated combat or supply systems. The large difficulty shift
merits a separate tuning decision: first review reusable teleport choke points,
early flame/frost lethality and hunger costs of repeated trap crossings. Keep
the full-stack bound under review when changing effects or status messages.

## Changed files

- Gameplay/storage: `src/model.hpp`, `features.hpp`, `features.cpp`,
  `feature_icons.hpp`, `combat.cpp`, `items.cpp`, `game_internal.hpp`.
- Generation: `src/world_gen.hpp`, `world_gen.cpp`, `world_gen_population.cpp`.
- Frontend: `src/render.cpp`, `ui.hpp`, `ui.cpp`, `main.cpp`.
- Simulation: `src/sim_hooks.hpp`; `sim/agent.hpp`, `omniscient_agent.cpp`,
  `simulator.cpp`, `experiment.hpp`, `experiment.cpp`, `metrics.hpp`,
  `metrics.cpp`, `trace.cpp`, `ranged_report.py`, `balance.py`, `main.cpp` (CLI help).
- Native checks: `tests/feature_checks.cpp`, `game_native.cpp`,
  `render_native.cpp`, `ui_native.cpp`, `artifact_checks.cpp`, `bow_checks.cpp`;
  `sim/tests.cpp`, `policy_checks.cpp`, `test_cli.py`, `test_balance_cli.py`,
  `test_check_balance.py`.
- Build/performance: root, test and sim `CMakeLists.txt`; `bench/bench.cpp`,
  `profile_turns.py`, `test_profile_turns.py`; rebuilt `ardurogue2.arduboy`.
- Documentation: root `README.md`, `docs/MAP_FEATURES.md`,
  `docs/floor-generation.md`, `sim/README.md`, `sim/BALANCE.md`, `bench/README.md`.
