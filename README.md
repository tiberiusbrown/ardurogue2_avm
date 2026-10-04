# ardurogue2_avm

This project builds an AVM image using an installed AVM SDK. It is a small
turn-based dungeon crawl for a 128x64 monochrome screen.

## Game

Descend through 16 dungeon depths, defeat the Lord of Darkness on the last
depth, pick up the Yendor Amulet, then climb back to the surface to win.
Upward travel is blocked until the Amulet is acquired. After that, downward
travel is blocked. Death also ends the run.

Each floor has connected rooms, corridors, doors, monsters, food, potions,
scrolls, weapons, armor, rings, and amulets. Explore the dungeon, fight by
walking into enemies, manage hunger, and gain levels. Only the active floor
exists. Descending permanently discards it. On the return journey, each depth
is a newly generated floor,
distinct from the floor at that depth during descent. Ascent floors have fresh
terrain, doors, and monsters, but no ordinary random loot. This follows the
classic Rogue nonpersistent-floor model and saves 112 bytes of AVM RAM.
The active floor's explored map uses one bit per tile, and a spare bit in each
door coordinate records whether that door is open. Each inventory item uses two
bytes: a type byte and an info byte. Ordinary items use six value bits, a cursed
bit, and an identified bit. Wands use four charge bits, three modifier bits,
and an individual identification bit. Ground slots store coordinates and a
complete item.
Save version 22 stores one active floor in an 821-byte AVM `Game` (unchanged
in size). Weapons and armor each occupy one contiguous `ItemType` range.
Long sword keeps the former sword ID; inserting the other weapons and armor
shifts the armor and subsequent item IDs. Long sword and chain mail preserve
the former generic equipment baselines. Equipment generation selects weighted
subtypes with independent enchantment and curse rolls. Version 21 and older
saves are incompatible; no migration is attempted. The former attack and cached
defense bytes store strength and magic resistance at the same offsets.
It derives potion, scroll, ring, amulet, and wand appearances from the run seed instead of storing 42 mapping bytes. Six bytes store their
discoveries. It stores monster potion effects,
enemy aggression and disguises, player speed, and accessory slots.

The ten potions from ArduRogue are healing, strength, dexterity, experience,
invisibility, harming, poison, confusion, paralysis, and slowing. Every new
run assigns each type a different color. Potions of the same type keep that
color until drinking one reveals its effect for the rest of the run. Healing
also removes poison's weakening, while strength removes weakening before it
can increase strength. Strength potions cap permanent base STR at 12; strength
rings and weakness adjust effective STR separately. Confusion, paralysis, slowing, and invisibility wear off
after several turns.

The nine scrolls from ArduRogue identify or enchant an item, remove a curse,
teleport the player, map the floor, or affect visible monsters with fear,
torment, confusion, or poison. Reading a scroll reveals its type. Each run
randomly assigns unknown scroll descriptions, and scrolls of the same type
stack in inventory.

Rings and amulets are separate item types, with eight variants of each. Rings
can be worn two at a time, and one amulet can be worn. Their effects include
bonuses to combat, visibility, speed, physical protection, health, and experience, plus
sustenance, regeneration, life drain, clarity, conservation, ironblood, and
invisibility. Cursed accessories reverse applicable bonuses and cannot be
removed once equipped. Fire immunity protects against dragon breath, while a
cursed fire ring doubles its damage before magic resistance.
Their unknown descriptions are independently permuted each run. Equipping
weapons, armor, rings, or amulets identifies them; until then, item text hides
equipment bonuses and the true types of jewelry.

All fifteen regular enemy species and the Lord of Darkness use ArduRogue's strength,
dexterity, speed, physical protection, health, XP, flags, and floor encounter weights.
Bats wander until attacked, mimics appear as items and stay put until attacked,
phantoms are invisible, and capable enemies open doors. Trolls and the Lord
regenerate. Rattlesnakes and the Lord can poison; tarantulas, fallen angels,
and the Lord can paralyze; incubi, fallen angels, and the Lord can confuse.
Dragon breath travels in a straight line up to five tiles and bursts over a
three-by-three area. Walls, closed doors, and other monsters block the line.
The player has a base speed of 4. Enemy speed values are turn costs, as in
ArduRogue: lower values act more often. Slowing doubles an enemy's turn cost;
a slowed player gives enemies more turns.

The seven wands are force, teleportation, digging, fire, striking, ice, and
polymorph. Each type has a permuted unknown appearance (long, short, slender,
thick, twisted, curved, or glossy). Using or identifying a wand reveals that
appearance's type for the run. Every individual wand also has one modifier:
normal, cursed, unreliable, spreading, powerful, or overpowered. Its modifier
and exact charges remain unknown until that specific wand is used or identified,
even when its type is already known. A wand starts with 3–10 charges, never
stacks, and crumbles after its last charge. Enchanting adds four charges, up to
15. Remove Curse changes cursed and unreliable wands to normal without
identifying them. Wands appear only during descent.

Choose **Use Item** and select a wand. Normal and powerful wands ask for a
direction; B cancels without spending a charge or turn. Cursed wands immediately
target the player, unreliable wands immediately choose a random cardinal
direction, and spreading wands fire in all four directions at once. Powerful
wands amplify their effect; overpowered wands combine spreading and powerful.
Each activation spends one charge and one turn. Non-digging shots show
ArduRogue's accumulating animated ray. Fire wands add an animated three-by-three
burst, or five-by-five when powerful. Dragon breath keeps its five-tile range,
ordinary three-by-three burst, and fire immunity rules.

Select **Throw Potion** from the action menu, choose a potion, then press a
direction. It travels up to eight tiles and shatters on the first monster,
closed door, or wall. A hit applies the potion to that monster and identifies
its type; a miss consumes the potion without revealing it. Harming can kill a
monster, while healing and strength cure poison. Confusion, paralysis,
slowing, and invisibility affect monsters temporarily. Dexterity and
experience have no effect on monsters. Status messages announce when these
conditions begin, expire, or are cured.
Identify, enchant, and remove curse scrolls open a second item selection.
Canceling that selection still consumes the scroll, as in ArduRogue.

### Combat rules

New players start with 18 HP, STR 5, DEX 4, magic resistance (MR) 2, and
speed 4. Level-ups add 3 maximum HP and restore health. Every fourth level
adds 1 MR; `(level - 1) / 3` adds physical accuracy. Levels do not increase
STR or raw damage. Speed still controls action frequency.

Both sides use the same physical hit rule: roll `0..(2 * accuracy + evasion)`
and hit if the roll is at least evasion. Each input caps at 84 so the RNG's
byte-sized range never overflows. Monster DEX supplies accuracy and evasion.
Player effective DEX is base DEX plus dexterity rings, clamped to 0..84;
it supplies evasion and, with the level bonus, weapon type accuracy modifier,
and attack rings, accuracy. Long sword adds no accuracy. Cursed attack rings
reduce accuracy. Weapon enchantment never changes accuracy.
DEX affects neither damage, absorption, nor MR.

Five mundane weapons share a single weapon slot and icon:

| Weapon | Damage | Accuracy | Generation weight |
| --- | --- | --- | --- |
| Dagger | 1..4 | +2 | 25 |
| Spear | 2..5 | +1 | 20 |
| Long sword | 2..6 | 0 | 30 |
| Mace | 3..7 | -1 | 15 |
| Two-handed sword | 4..8 | -2 | 10 |

Long sword preserves the old generic sword's **2..6**, zero-accuracy baseline.
Two-handed sword is a balance profile using the same slot, with no hand
restrictions or additional abilities. Unarmed attacks roll **1..3**.
`weapon_damage_roll(minimum, maximum, signed_enchant)` keeps that range fixed.
At +k it keeps the highest of 1+k rolls; at -k it keeps the lowest. Effective enchantment caps at -5..+5 (at most six rolls). Each extra
roll has diminishing returns, and both endpoints remain possible even at the
cap. For the long sword, expected means at +0/+1/+2/+3 are approximately
4.00/4.80/5.20/5.43. Neither enchantment nor curse state adds accuracy.

Effective STR is base STR plus strength rings minus weakness, clamped to
1..255. Its damage modifier is -2 at STR 1-2, -1 at 3-4, 0 at 5-6, +1 at
7-8, +2 at 9-10, and +3 at 11 or above. After a successful accuracy roll,
player raw melee damage is `max(1, weapon roll + STR modifier)`, followed by
monster armor absorption and the landed-hit minimum. STR does not change the
weapon's roll distribution. Attack rings do not add damage. Monster raw damage
remains `strength + roll(3)`, halved upward while weakened.

Armor is derived from current equipment on each hit, with no cached defense.
Six mundane armor types share a single armor slot and icon:

| Armor | Rating | Absorption | Generation weight |
| --- | --- | --- | --- |
| Leather armor | 1 | 0..1 | 25 |
| Ring mail | 2 | 1..2 | 20 |
| Scale mail | 3 | 1..3 | 20 |
| Chain mail | 4 | 2..4 | 15 |
| Splint mail | 5 | 2..5 | 12 |
| Plate mail | 6 | 3..6 | 8 |

Chain mail preserves the old generic armor's rating-4 baseline. Each type's
rating is fixed regardless of enchantment, curse, instance info, or dungeon
depth. Protection rings add to the equipped armor's unsigned rating (or zero
when unarmored). Cursed protection lowers the rating; the result clamps to
0..255. The resulting rating N absorbs `floor(N/2) + roll(ceil(N/2) + 1)`.
Thus rating 6 blocks 3..6, and rating 8 blocks 4..8. Monster armor uses the
previous defense table values unchanged. A landed physical attack always deals
`max(1, raw - absorption)`. Protection is never a separate flat subtraction,
and physical armor does not affect evasion or magic.

`armor_absorption(rating, signed_enchant)` keeps this range fixed. At +k it
keeps the highest of 1+k rolls; at -k it keeps the lowest. Effective enchantment
caps at -5..+5 (at most six rolls). Each extra roll has diminishing returns;
maximum absorption remains probabilistic. For rating 6, measured means at
+0/+1/+2/+3 are approximately 4.50/5.13/5.44/5.62. At +5 the chance of blocking
6 is approximately 82.2%, and every value 3..6 remains possible.

Live armor absorption uses the equipped armor's signed enchantment, including
when protection rings adjust the effective rating. Unequipped armor supplies no
enchantment. The neutral `biased_range_roll` primitive implements repeated
rolls; `weapon_damage_roll` and `armor_absorption` are thin semantic wrappers.

`Item` remains two bytes. All weapons and armor store only
`enchant + MAX_EQUIPMENT_ENCHANT` (0..10) in the six value bits of `info`.
Shared retrieval and modification logic keeps enchantment in -5..+5 for all
equipment types. The curse and identification bits remain independent.
`weapon_definition` and `armor_definition` look up inherent damage, accuracy,
and armor rating using
`Item::type`, making those lookups the single source of inherent equipment
stats. Never derive capability from `info`. Definitions take the type directly,
with no instance rating API. Use `equipment_enchant`, `set_equipment_enchant`,
and `make_equipment` for instance state rather than treating `item_value` as a
combat scalar. The absorption helper still supports ratings 0..255 for monsters
and effective ratings adjusted by rings.

Both generated weapons and armor use the same enchantment distribution:
-2 at 5%, -1 at 10%, 0 at 70%, +1 at 10%, and +2 at 5%. A separate 1-in-8
roll decides curse state, allowing curses at any enchantment and removable
negative non-cursed equipment. Dungeon depth does not scale enchantment or
inherent equipment capability. After choosing the original weapon or armor
loot category (each 8/72), generation independently selects a subtype with the
weights above, totaling 100 per category. Other loot category probabilities
stay unchanged, and there is no depth gating. A separate seeded equipment
stream avoids bias from terrain and encounter retry loops; an output mixer
reduces correlations between successive draws from the small RNG. Neither
requires extra saved state.
Enchant scrolls increase equipment enchantment up to +5, preserving inherent
rating/range, identification, and curse state. Cursed gear can have positive,
zero, or negative enchantment and cannot be dropped, exchanged, or replaced
while equipped. Remove Curse clears the curse while preserving enchantment and
armor rating. Identified item text displays signed enchantment after the type's
name, with an independent curse prefix; ordinary labels do not expose a numeric
armor rating. For example, labels include `dagger`, `long sword +2`,
`cursed mace -1`, `plate mail`, and `cursed chain mail +1`. All subtypes appear
under Weapons or Armor in inventory. No encumbrance or speed penalties are
introduced.

Equipment type defines capability; enchantment defines reliability within
that capability. A dagger +5 still rolls only 1..4, while long sword +0 can
roll up to 6. Leather armor +5 still blocks only 0..1, while chain mail -3
blocks only 2..4 and plate mail +0 blocks 3..6. Enchant scrolls never change
weapon accuracy, damage limits, or armor rating, and Remove Curse preserves
type and enchantment.

`is_weapon`, `is_armor`, and `is_equipment` govern inventory grouping, equipping,
repeat actions, enchantment, formatting, and shared equipment icons. Adding
equipment types should extend those predicates, definitions, generation choices,
and name lookup rather than duplicating generic mechanics. Keep each item
group contiguous in `ItemType` so its predicate remains a simple range test.

Mimics store one of five `MimicAppearance` categories: scroll, potion, amulet,
ring, or wand. Explicit appearance bits are separate from aggression and fear.
Rendering maps categories to shared icons through `mimic_icon`; `item_icon`
also maps ordinary item categories to shared icons. Neither the stored mimic
appearance nor the icon table depends on `ItemType` numeric ordering or roster
size. The shared icon table remains in flash, and `Monster` remains eight bytes.

Magic bypasses physical armor, protection, and DEX. MR saves when
`roll(resistance + power + 1) < resistance`; each input caps at 127 to keep
the range safe. A save halves damage rounded upward; failure takes full damage.
Dragon breath uses power 12. Wand fire/ice use power 8, and cursed striking and
digging use power 12. Monster wand damage remains direct with no new monster
MR field. `player_take_fire_damage` first handles fire rings: positive immunity
prevents damage completely; cursed immunity doubles damage with saturation at
255, then applies the normal MR save. Dragon breath and fire-wand splash use
this same entry point. Starvation, potion harming, torment, life drain, and
status effects retain their special behavior.

The small integer helpers in `src/combat_math.hpp` depend only on the seeded
RNG and can be reused by a native balance harness. These mechanics establish a
measurable first pass; they do not claim final balance. For identical raw damage
7, rating 8 flat armor previously dealt 1 HP; randomized armor now averages
about 1.6 HP per landed hit. No broad monster-stat retuning is included.

### Controls

| Screen | Control | Action |
| --- | --- | --- |
| Title | A | Start a run or continue a saved run |
| Title with a save | B | Start a new run |
| Dungeon | Direction pad | Move, attack, or open a door |
| Dungeon | Move onto an item | Prompt to pick up each item on the tile, topmost first |
| Dungeon | Move onto stairs | Prompt to take the stairs |
| Dungeon | A | Repeat the last equipment action or wait |
| Pickup or stairs prompt | A/B | Confirm or cancel the action |
| Dungeon | B | Open the action menu |
| Status prompt | A | Continue a long message after `[more]` |
| Action menu | Up/Down, A | Choose wait, use item, drop item, throw potion, full map, save and exit, or abandon |
| Item selection | Up/Down, A | Select an item for the chosen action |
| Item selection | B | Cancel selection |
| Throw selection | Up/Down, A | Choose a potion from inventory |
| Wand direction | Direction pad | Fire the selected wand |
| Wand direction | B | Cancel without spending a charge or turn |
| Throw direction | Direction pad | Throw the selected potion |
| Confirmation | A/B | Confirm or cancel abandoning the game |
| Throw direction, full map | B | Return to the dungeon |

Saving exits to the title screen. Continuing consumes the save so a death
cannot be undone by reloading it. A completed or abandoned run updates the
best score. The inventory has 16 slots. Dropped items occupy an available empty
ground slot on the active floor; one slot remains reserved for the Lord's
Amulet drop until it dies. Compatible food, potions, and scrolls merge into
ground stacks when possible. If the ground cannot hold an item, the game asks for an
explicit discard confirmation. The amulet cannot be dropped.

The dungeon screen shows dungeon depth, player level, and health above a
word-wrapped status area. Messages from one action, including enemy responses,
accumulate there; the next action clears them. Longer messages pause at
`[more]` until A is pressed.

## Build

```sh
cmake -S . -B build
cmake --build build --config RelWithDebInfo --target ardurogue2
```

The build finds `avm-clang`, `avm-ld`, and `avm-image` on `PATH` and uses the
compiler's SDK directory for headers, libraries, and interpreter files. You can
set `-DAVM_SDK_ROOT=/path/to/avm-sdk` to choose a specific installation.
Open this project folder in VS Code to use the default build task.

The Arduboy FX package is written to `projects/ardurogue2/ardurogue2.arduboy`.
When built from the parent `avm` repository, CMake builds and installs the SDK
automatically before building this project.

## Source layout

`src/model.hpp` defines the saved game layout. `src/game.hpp` exposes gameplay
actions, and `src/game_internal.hpp` shares helpers between gameplay modules.
`src/state.cpp`, `src/combat.cpp`, and `src/items.cpp` implement run state,
combat and turns, and inventory behavior. `src/combat_math.cpp` contains the
shared hit, weapon-roll, STR, absorption, and MR arithmetic. `src/world.cpp` generates floors and
handles map visibility. These modules compile into the native test program.
`src/persistence.cpp` handles save policy, `src/status.cpp` formats messages,
`src/render.cpp` draws the display, and `src/ui.cpp` handles controls and modes.
`src/main.cpp` initializes the app and runs the outer event loop. The AVM has
a separate 256-byte stack and 1024 bytes of global memory.

The native game checks build independently of the AVM SDK:

```sh
cmake -S tests -B build/native -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/native --config RelWithDebInfo
ctest --test-dir build/native -C RelWithDebInfo --output-on-failure
```

The native suite includes deterministic range/overflow checks, signed weapon and
armor enchantment sampling with 100,000 samples per distribution, diminishing
returns and symmetry, paired-seed melee/ring/MR/fire checks, equipment encoding,
all eleven equipment definitions, bounded rolls at every enchantment,
scroll/curse and pickup/drop checks, generic predicates and grouping, strength
potion caps, generation distributions and curse independence across 4,096 seeds
and all depths, subtype weighting and subtype/enchantment independence, all
equipment names and shared icons, mimic appearance/flag checks and rendering
comparisons, and save/layout tests.
Assertions use integer totals with tolerances; decimal means are diagnostic
output only. To inspect sword and armor distributions and compare randomized
damage against flat armor:

```sh
ctest --test-dir build/native -C RelWithDebInfo -V -R combat_distributions
```

The native executable also accepts `--combat-distributions` for this report;
the existing `--armor-distributions` mode remains available for armor alone.

The AVM build retains the 821-byte saved layout and two-byte items, eight-byte
monsters, and four-byte ground items. With the current SDK, the revamped build
reports a complete maximum stack bound of 252 bytes and zero analysis gaps,
on the wand/dragon animation/rendering path. This fits the 256-byte VM stack
with four bytes to spare; future changes should continue checking the linker
report.
