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
Save version 16 stores one active floor in an 821-byte AVM `Game` (unchanged
from version 15). It derives potion, scroll, ring, amulet, and wand appearances from
the run seed instead of storing 42 mapping bytes. Six bytes store their
discoveries. It stores monster potion effects,
enemy aggression and disguises, player speed, and accessory slots; older saves
are not compatible.

The ten potions from ArduRogue are healing, strength, dexterity, experience,
invisibility, harming, poison, confusion, paralysis, and slowing. Every new
run assigns each type a different color. Potions of the same type keep that
color until drinking one reveals its effect for the rest of the run. Healing
also removes poison's weakening, while strength removes weakening before it
can increase attack. Confusion, paralysis, slowing, and invisibility wear off
after several turns.

The nine scrolls from ArduRogue identify or enchant an item, remove a curse,
teleport the player, map the floor, or affect visible monsters with fear,
torment, confusion, or poison. Reading a scroll reveals its type. Each run
randomly assigns unknown scroll descriptions, and scrolls of the same type
stack in inventory.

Rings and amulets are separate item types, with eight variants of each. Rings
can be worn two at a time, and one amulet can be worn. Their effects include
bonuses to combat, visibility, speed, defense, health, and experience, plus
sustenance, regeneration, life drain, clarity, conservation, ironblood, and
invisibility. Cursed accessories reverse applicable bonuses and cannot be
removed once equipped. Fire immunity protects against dragon breath, while a
cursed fire ring doubles its damage.
Their unknown descriptions are independently permuted each run. Equipping
weapons, armor, rings, or amulets identifies them; until then, item text hides
equipment bonuses and the true types of jewelry.

All fifteen regular enemy species and the Lord of Darkness use ArduRogue's strength,
dexterity, speed, defense, health, XP, flags, and floor encounter weights.
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
combat and turns, and inventory behavior. `src/world.cpp` generates floors and
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
