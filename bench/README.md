# Complete-turn latency benchmarks

The initial goal is **no more than 150 ms** (2,400,000 emulated AVR cycles at
16 MHz) from submitting an action's input to the completed view being ready
for the next input. The goal applies to the worst sample of every benchmark.
It is an initial performance target, not a claim that the current game meets it.

The [initial baseline](BASELINE.md) records 63 samples across the 21 cases:
9 cases meet the goal and 12 exceed it.

`profile_turns.py` uses Python 3's standard library and the installed SDK's
`avm-lldb`. It profiles the ordinary production ELF built with `-g -O2 -flto`;
there are no benchmark hooks, alternate gameplay implementations, or added
guest RAM/stack allocations. Each sample launches a fresh emulator, starts a
game through actual button input, and installs a deterministic 821-byte fixture.
Saved-layout offsets are verified against the ELF's DWARF. UI preparation also
uses real buttons, avoiding writes to globals that LTO may split into fragments.

## Run

Build the game and run all 21 cases, three samples each:

```text
cmake --build <game-build> --config RelWithDebInfo --target ardurogue2
python bench/profile_turns.py --elf <game-build>/ardurogue2.elf --sdk-root <sdk> --output <results>
```

Or, when Python is available at CMake configure time:

```text
cmake --build <game-build> --config RelWithDebInfo --target ardurogue2_turn_benchmarks
```

Select individual cases, collect native AVR hotspots, and produce HTML reports:

```text
python bench/profile_turns.py --elf <game.elf> --lldb <avm-lldb> --benchmark move_dense --benchmark equip_armor --repeat 3 --native --html --output <results>
```

Native hotspot collection requires an `interp.elf` matching the SDK's
`interp.hex`. The SDK rejects mismatched firmware. For development,
`AVM_LLDB_INTERP_ELF` can select a matching interpreter ELF without changing
the game image or measured firmware. Regular source profiling needs no
interpreter ELF. HTML reports use the SDK's sibling `avm-prof` executable.

`--list` lists the cases. `--check` exits 2 if any completed sample exceeds the
goal; without it, over-budget timings are reported and exit status is 0.
An invalid/incomplete measurement always exits 1. `--goal-ms` changes the target;
the separate `--deadline-ms` (default 10 seconds of emulated time) bounds setup
and execution, while `--timeout` bounds host debugger runtime.

Every invocation creates a unique directory under `--output`. It retains
per-sample fixtures, pre/post game state, debugger transcripts, controller
frame captures, generated `.lldb` command files, and `.avmp` source profiles.
`summary.json`, `summary.csv`, and `summary.md` contain median/worst timings,
pass/over status, exact cycle boundaries, and profiler launch identities.
Profiles contain ELF/interpreter hashes and source/instruction hotspots for
comparisons across optimizations. Fixture/model hashes identify workload changes.

Generate scripts without running them with `--emit-only`, then use any script
directly (once; its output files must not already exist):

```text
avm-lldb --batch --source <sample>/turn.lldb <game.elf>
avm-prof report <sample>/turn.avmp --html <sample>/profile.html
avm-prof diff <before>/turn.avmp <after>/turn.avmp --html <diff.html>
```

## Measurement boundary

Each window measures the complete response between consecutive player inputs.
It starts at the coherent AVM boundary where the action button is submitted
and includes interrupt wakeup/polling, input dispatch, player movement/combat/
item effects, monster turns, status updates, every intermediate render,
the final `render()`/display transfer, and return to the main input loop's
`avm_idle()` call. No post-render idle interval is included. The stop PC must
match the ready main-loop PC, the profile must contain the final renderer, its
cycle totals must reconcile, and the post-state must prove the intended action.
A deadline, fault, incomplete turn or unexpected modal stop cannot pass the goal.

Menu navigation and choosing a slot happen before the measured window. Item
use/drop starts with the inventory A confirmation; digging starts with
the direction; pickup starts with the yes/no A confirmation. The pickup approach
step and its rendered question belong to the preceding input response and
are outside the pickup-confirmation measurement. Movement cases avoid
tiles that request a further decision.

Each measured response receives exactly one submitted input. No acknowledgement
presses are injected during it. Fixtures avoid status pagination that would
require another input, while retaining ordinary status updates. Floor generation
and animated actions are outside this suite. Digging is retained because it
does not animate. The runner also rejects profiles containing generation or
animation functions, so those costs cannot silently enter a turn measurement.

## Workloads

- Movement: lit room, branching corridor, map corner clipping, explored maze
  with 12 active enemies.
- Waiting: empty room and the same dense enemy/door/item workload.
- Combat: deterministic melee hit, miss, lethal hit with XP, and opening a door.
- Items: food, healing, weapon/armor/ring equipment, mapping, teleport,
  dropping, and pickup.
- Curse discovery: equip an unidentified cursed amulet of speed. Both its type
  knowledge and instance identification start unknown; the resulting state must
  show it equipped with its type and curse revealed. The confirmation response
  includes the curse warning and completes without a pagination acknowledgement.
- Wands: digging through blocked terrain without animation.

These are controlled workload fixtures, not a statistical claim about all
possible floors or turns. Repeat samples reset the same state/RNG and can be
identical in emulated cycles. Host wall-clock time is not the game metric.
Source line breakpoints are discovered from the matching source tree;
`--source-dir` must correspond to the ELF. Update the saved-layout contract
and action checks when gameplay/save structures change.

Run the helper/measurement rejection tests with:

```text
python bench/test_profile_turns.py
```
