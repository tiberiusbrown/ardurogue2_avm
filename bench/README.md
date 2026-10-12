# Complete-turn latency benchmarks

The limit is **no more than 100 ms** (1,600,000 emulated AVR cycles at
16 MHz) from submitting an action's input to the completed view being ready
for the next input. Each benchmark runs once because emulated time is deterministic.
The target applies to the controlled scenarios listed below.

Dungeon generation is measured separately, including its loading feedback.
Eleven isolated optimization trials and the retained combination are documented
in [the generation results](RESULTS-GENERATION.md). The combined nine-case mean
fell from 9.853 to 5.795 seconds with unchanged generated states and permanent RAM.

The [baseline](BASELINE.md) and [first optimization results](RESULTS.md)
record earlier measurements against the former 150 ms limit.
The next round implemented shared ray prefixes and row-based exploration
separately, benchmarking each; see the [second-round results](RESULTS-100MS.md).
Removing persistent room metadata then saved 48 RAM bytes and simplified
visibility; see the [room metadata results](RESULTS-ROOMS.md). The original 21
cases met the current 100 ms limit. Nine inventory browsing stress cases now
extend the suite to 30 cases. Inventory lookups and modal state reuse were then
optimized in six separately benchmarked steps; all 30 cases now pass. See the
[inventory profiling analysis](PROPOSALS-INVENTORY.md) and
[inventory optimization results](RESULTS-INVENTORY.md). The runner defaults to
the same limit.

`profile_turns.py` uses Python 3's standard library and the installed SDK's
`avm-lldb`. The separate `ardurogue2-bench.elf` links [bench.cpp](bench.cpp) with
the gameplay objects and the normal main input loop, built with `-g -O2 -flto`.
The benchmark startup selects a scenario and prepares it through C++ fields,
constructors, and game helpers. It bypasses save loading and floor generation;
the scenarios do not create saves or depend on save versions or byte offsets.
The benchmark adds one guest byte for the case selector. Setup returns before
the initial render; no benchmark code runs inside a measured input response.
The normal `ardurogue2.elf` and root `.arduboy` do not include the benchmark code.

Magic adds eleven cast workloads and two real study updates (ordinary progress
and a gameplay-message interruption). Shockwave captures four adjacent monsters,
pushes multiple tiles and enters an intermediate trap. Cast confirmation uses the
actual scrolling action menu and deferred targeting flow. Study benchmarks exclude
the intentional 90 ms presentation pacing and measure gameplay plus rendering.
The [magic report](../docs/SPELLS-RESULTS.md) records the historical 115-case run.
The [expansion report](../docs/MAGIC-EXPANSION-RESULTS.md) records the current
123-case run, including five additional spells and three lightning wand workloads.

## Run

Build the game and run all 123 cases once:

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
Inventory browsing also captures `before.pgm`; both of its frames use logical
display orientation for highlight and scrolling validation.
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
the final renderer/display transfer, and return to the next `avm_idle()` call.
Gameplay actions return to the main input loop; inventory opening and scrolling
return to the inventory modal's input loop. No post-render idle interval is
included. The stop PC must match the expected input boundary, the profile must
contain the appropriate final renderer, its cycle totals must reconcile, and
the post-state must prove the intended action.
A deadline, fault, incomplete turn or unexpected modal stop cannot pass the goal.

Menu navigation and preliminary slot navigation happen before the measured window. Item
use/drop starts with the inventory A confirmation; digging starts with
the direction; pickup starts with the yes/no A confirmation. The pickup approach
step and its rendered question belong to the preceding input response and
are outside the pickup-confirmation measurement. Movement cases avoid
tiles that request a further decision. Inventory opening starts with the menu's
A confirmation. Inventory scrolling starts with one UP or DOWN input after the
manifest's preliminary DOWN and UP presses have positioned the selection.
Browsing must leave the captured game state and every inventory item unchanged
and consume no game turn. The runner checks the selection highlight and, for
scrolling, the shift of overlapping rendered rows in the logical frame captures.
This avoids depending on optimized modal locals being available in DWARF.

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
- Inventory browsing: opening, scrolling down near the end, and scrolling up
  from the top of a scrolled viewport for each of these layouts:
  - A full 16-slot pack with all eleven groups interleaved (27 rows including
    headers), identified equipment and accessories, and long consumable names.
  - A full 16-slot group containing all eight identified wand types, alternating
    overpowered and unreliable modifiers, and 10–15 charges. Scrolled views
    render seven item names rather than spending a row on the group header.
  - All eleven groups with one item each (22 rows). The eleven items occupy the last
    eleven slots in reverse group order, maximizing headers and scanning past
    empty leading slots. Scrolling crosses group headers near the bottom.

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

Bow integration extends the suite to 36 cases: arrow hit/miss/kill/empty, bow equipping and the Throw/Shoot picker. The benchmark arrow renderer retains production render setup and excludes the timed animation frames. The ordinary image keeps the complete 60 ms/tile animation.

Map features extend the final suite to **58 cases**: all ten traps, a holy pulse
with twelve unholy targets, healing-spring acceptance, forge item selection,
searching eight adjacent traps, dense feature rendering and a lethal trap with
Phoenix revival. Choices are prepared before timing; submitted acceptance or
item selection includes the production effect, enemy turn and final rendering.
All 58 final cases meet 100 ms; see [the validation report](../docs/MAP_FEATURES.md).

## Monster trap workloads and status pages

Eight additional cases cover four monster entries, four environmental kills,
teleport, summoning, force-wand knockback, Hammer knockback, a busy late-floor
turn with twelve monsters, and four spreading-force landings. Production keeps
all witnessed status messages. The profiler measures each input through the
next displayed `[more]` prompt or final main-loop readiness, then acknowledges
pages using actual release/A inputs and validates the completed turn. The summary
reports the worst segment, with every segment and total computation time retained
in schema-3 JSON. A displayed prompt is an input boundary; waiting for human
acknowledgment is excluded. Total computation across multiple pages can exceed
100 ms while each response remains within the input-response budget.

Each page is replayed deterministically from the compiled fixture in a fresh
batch debugger session before its profile window. This avoids relying on LLDB
Python bindings or asynchronous debugger behavior. Captures and scripts are
retained for every page. Animation drawing/delays for wand rays are omitted only
in BENCH builds, as for other excluded animation; production animation is intact.
All original 58 cases retain their existing final-render endpoints. See
[the monster-trap measurements](../docs/MONSTER_TRAPS_2026-10-10.md).

The lighting integration adds eight cases for maximum sight, blinded dense turns,
fuel exhaustion, blindness recovery, offhand equipping, and drinking/throwing
blindness. Existing dense and forced-movement cases equip permanent illumination
to retain radius-six coverage. Their item prompts are prepared with real button
presses before profiling begins. Final measurements are recorded in
[the lighting report](../docs/LIGHTING_OFFHAND_BLINDNESS.md).

## Hazard-routing performance pass

That visibility change expanded the suite to 81 cases: all 78 prior fixtures are retained, followed by
hidden-monster, cursed-sight-ring and blind-player monster trap entries. They
exercise the visibility proof reused by trap status formatting. Profiling also
captures `game.random_state` and the entire `sizeof(game)` via DWARF to `game.bin`
after final readiness. Capture commands run outside the timing window.

```text
python bench/compare_turns.py REFERENCE_RUN CANDIDATE_RUN --output build/turn-comparison.csv
```

This rejects differing case manifests, timing failures, gameplay fields, full
saved state/RNG, endpoints, final display, or pagination framebuffers and exports
absolute and percentage timing differences. The final suite improves the worst
case from 99.299 to 92.773 ms with 81/81 equivalent cases and a complete
239/256-byte stack bound. See the [full timing table and hotspot report](../docs/EXPLORER_HAZARD_ROUTING_AND_AVM_OPTIMIZATION.md).

## Current gameplay-balance fixtures

The current suite has 87 cases: the earlier 81, four temporary-sight cases, and `natural_recovery` plus `recovery_dense`. The recovery fixtures start at turn 255 with injured HP, exercise the saved clock wrapping to zero, and run through ordinary input, enemy actions, recovery feedback, and rendering. Native checks verify the quiet fixture restores exactly one HP; the profiler verifies that completed outcome as well.

The natural-regeneration revision passes all 87 cases, with a 92.9854375 ms worst turn, complete 241-byte production and 245-byte benchmark stack bounds, and no new saved bytes. See [the current balance report](../docs/GAME_BALANCE_2_TO_5_PERCENT.md) for selected gameplay, profiles, package measurements and census results.


## Magic expansion cases

Eight additional workloads bring the suite to 123: clustered/self-damaging
Fireburst, Phase Step onto a hidden spike trap, two-target Chain Lightning,
Slumber, Blood Bolt, normal Lightning, powerful three-target Lightning, and
overpowered spreading Lightning around twelve monsters. Each uses the actual
deferred directional or modifier-aware wand UI. Outcome checks assert effects
and costs as well as the ordinary turn. Deliberate animations are excluded;
gameplay, status paging and final rendering remain measured. The linker proves
the complete stack bound for production and benchmark ELFs. Results are in
[MAGIC-EXPANSION-RESULTS.md](../docs/MAGIC-EXPANSION-RESULTS.md).
