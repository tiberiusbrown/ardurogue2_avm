# ardurogue2_avm

This project builds an AVM image using an installed AVM SDK. It is a small
turn-based dungeon crawl for a 128x64 monochrome screen.

## Game

Descend through 16 dungeon floors, defeat the Lord of Darkness on the last
floor, pick up the amulet, then climb back to the surface. Returning without
the amulet ends the run. Death also ends the run.

Each floor has connected rooms, corridors, doors, monsters, food, healing,
weapons, and armor. Explore the dungeon, fight by walking into enemies, manage
hunger, and gain levels. Explored rooms, opened doors, collected items, and
defeated monsters remain recorded when you revisit a floor. The game rebuilds
each floor from a seed and compact progress flags to conserve RAM.

### Controls

| Screen | Control | Action |
| --- | --- | --- |
| Title | A | Start a run or continue a saved run |
| Title with a save | B | Start a new run |
| Dungeon | Direction pad | Move, attack, or open a door |
| Dungeon | A | Pick up an item, use stairs, repeat the last inventory action, or wait |
| Dungeon | B | Open the action menu |
| Status prompt | A | Continue a long message after `[more]` |
| Action menu | Up/Down, A | Choose wait, inventory, full map, save and exit, or abandon |
| Inventory | Up/Down, A | Select and use or equip an item |
| Inventory | Right | Drop the selected item |
| Inventory, full map | B | Return to the dungeon |

Saving exits to the title screen. Continuing consumes the save so a death
cannot be undone by reloading it. A completed or abandoned run updates the
best score. The inventory has 16 slots; up to eight dropped items can persist
across floors.

The dungeon screen shows dungeon depth, player level, and health above a
word-wrapped status area. Messages from one action, including enemy responses,
accumulate there; the next action clears them. Longer messages pause at
`[more]` until A is pressed.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DAVM_SDK_ROOT=/path/to/avm-sdk
cmake --build build --config RelWithDebInfo --target ardurogue2
```

The Arduboy FX package is written to `projects/ardurogue2/ardurogue2.arduboy`.
When built from the parent `avm` repository, CMake builds and installs the SDK
automatically before building this project.

## Source layout

`src/world.cpp` generates floors and handles map visibility. `src/game.cpp`
implements turns, combat, inventory, and the quest. Both use only standard C++
and the shared declarations in `src/game.hpp`, so they can be compiled into a
native test program. `src/main.cpp` owns AVM input, display, random seed
generation, and persistence calls. The AVM stack is separate from its 1024
bytes of global memory.

The native game checks build independently of the AVM SDK:

```sh
cmake -S tests -B build/native -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/native --config RelWithDebInfo
ctest --test-dir build/native -C RelWithDebInfo --output-on-failure
```
