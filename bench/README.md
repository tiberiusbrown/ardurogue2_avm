# Complete-turn latency benchmarks

The initial goal is **no more than 150 ms** (2,400,000 emulated AVR cycles at
16 MHz) from submitting an action's input to the completed view being ready
for the next input. Each benchmark runs once because emulated time is deterministic.
The target applies to the controlled scenarios listed below.

The [baseline](BASELINE.md) records one measurement for each of the 21 cases:
9 cases meet the goal and 12 exceed it.
After deferring redundant rendering and batching terrain rows, all 21 cases
meet the goal; see the [optimization results](RESULTS.md) for timings and
correctness comparisons.

`profile_turns.py` uses Python 3's standard library and the installed SDK's
`avm-lldb`. The separate `ardurogue2-bench.elf` links [bench.cpp](bench.cpp) with
the gameplay objects and the normal main input loop, built with `-g -O2 -flto`.
The benchmark startup selects a scenario and prepares it through C++ fields,
constructors, and game helpers. It bypasses save loading and floor generation;
the scenarios do not create saves or depend on save versions or byte offsets.
The benchmark adds one guest byte for the case selector. Setup returns before
the initial render; no benchmark code runs inside a measured input response.
The normal `ardurogue2.elf` and root `.arduboy` do not include the benchmark code.

## Run

Build the game and run all 21 cases once:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=<sdk>
cmake --build build --config RelWithDebInfo --target ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --output build/turn-benchmarks
```

Or, when Python is available at CMake configure time:

```text
cmake --build build --config RelWithDebInfo --target ardurogue2_turn_benchmarks
```

Select individual cases, collect native AVR hotspots, and produce HTML reports:

```text
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --lldb <avm-lldb> --benchmark move_dense --benchmark equip_armor --native --html --output build/turn-benchmarks
```

Native hotspot collection requires an `interp.elf` matching the SDK's
`interp.hex`. The SDK rejects mismatched firmware. For development,
`AVM_LLDB_INTERP_ELF` can select a matching interpreter ELF without changing
the game image or measured firmware. Regular source profiling needs no
interpreter ELF. HTML reports use the SDK's sibling `avm-prof` executable.

`--list` lists the cases. `--check` exits 2 if any benchmark exceeds the
goal; without it, over-budget timings are reported and exit status is 0.
An invalid/incomplete measurement always exits 1. `--goal-ms` changes the target;
the separate `--deadline-ms` (default 10 seconds of emulated time) bounds setup
and execution, while `--timeout` bounds host debugger runtime.

Every invocation creates a unique directory under `--output`. It retains
pre/post named state snapshots (`before.json`/`after.json`), debugger transcripts, controller
frame captures, generated `.lldb` command files, and `.avmp` source profiles.
`summary.json`, `summary.csv`, and `summary.md` contain exact cycle counts and timings,
pass/over status, exact cycle boundaries, and profiler launch identities.
Profiles contain ELF/interpreter hashes and source/instruction hotspots for
comparisons across optimizations. The compiled scenario source hash and initial
state hash identify workload changes. The ELF hash identifies the actual build.

Generate scripts without running them with `--emit-only`, then use any script
directly (once; its output files must not already exist):

```text
avm-lldb --batch --source <sample>/turn.lldb build/ardurogue2-bench.elf
avm-prof report <sample>/turn.avmp --html <sample>/profile.html
avm-prof diff <before>/turn.avmp <after>/turn.avmp --html <diff.html>
```

`bench.cpp` owns the case manifest and a named `bench_<case>` entry point for
each scenario. The script stops at `bench_select`, sets the single `bench_case`
selector, and stops at the chosen case entry before letting its setup run.
`--list` shows selector indices. For example, in an interactive avm-lldb session:

```text
breakpoint set --name bench_select
run
expr bench_case = 15
breakpoint set --name bench_equip_cursed_amulet
continue
```

Use `--emit-only --benchmark equip_cursed_amulet` to generate the
complete setup, input, and profiling commands for that case. All UI navigation
after compiled setup uses actual buttons; the debugger never writes Game or Ui.

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
presses are injected during it. Scenarios avoid status pagination that would
require another input, while retaining ordinary status updates. Floor generation
and animated actions are outside this suite. Digging is retained because it
does not animate. The runner also rejects profiles containing generation or
animation or benchmark setup functions, so those costs cannot silently enter
a turn measurement. State validation uses DWARF field names and enum constants;
the mapping check reads the explored array using its actual `sizeof`.

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

These are controlled scenarios, not a statistical claim about all
possible floors or turns. Each case starts from a fixed state/RNG and produces
a deterministic emulated cycle count. Host wall-clock time is not the game metric.
Source line breakpoints are discovered from the matching source tree;
`--source-dir` must correspond to the ELF. Adding a case to the `TURN_BENCHMARKS`
manifest in `bench.cpp` gives it a selector and breakpoint entry automatically;
add its preparation and result checks as needed. Save-version/layout changes
require no Python fixture updates. The startup differs from the normal ELF, so
keep the benchmark ELF identity with results when comparing builds.

Run the helper/measurement rejection tests with:

```text
python bench/test_profile_turns.py
```
