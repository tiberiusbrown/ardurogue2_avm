# String storage

Use flash strings for constant text and format strings in gameplay code rather
than storing them in RAM. Wrap literals in `F(...)` (or use `PROGMEM` /
`AVM_PROGMEM` declarations). The AVM runtime library provides C++ overloads
that accept flash strings through the ordinary API names, such as `snprintf`
and `avm_draw_text`; use those overloads. ArduRogue 2's own `status_word` also
provides a flash-string overload. RAM buffers remain appropriate for text
constructed at runtime.

# Equipment

Equipment type/subtype definitions determine inherent damage range, accuracy,
and armor rating. `Item::info` stores only per-instance state such as signed
enchantment, curse, and identification. Never derive inherent capability from
`info` or pack ratings/tiers together with enchantment. Enchantment changes only
the roll distribution within the type's fixed range.

Keep all items of the same group contiguous in the `ItemType` enum so group
predicates such as `is_weapon`, `is_armor`, `is_potion`, `is_ring`, `is_amulet`,
`is_scroll`, and `is_wand` can use simple inclusive range tests.

# Turn performance

Use this project's ignored `build/` directory for objects, `ardurogue2.elf`,
`ardurogue2-bench.elf`, and benchmark results; keep `ardurogue2.arduboy` at the project root.
After development, run from this project's directory (`<sdk>` is the AVM SDK):

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=<sdk>
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check --output build/turn-benchmarks
```

All cases must take at most **100 ms** in their worst sample, from submitted
input through computation and final rendering to readiness for the next input;
generation and animation are excluded. Use the saved avm-lldb profiles to
optimize failures while preserving gameplay and benchmark coverage. Run
correctness tests and rerun the full suite on the final ELF until all cases pass.

# Balance simulation

Gameplay/content/balance changes affecting items, equipment, monsters, combat,
player stats, hunger, statuses, generation/population, rewards, progression,
stairs or resource availability must run native correctness tests and a paired
simulator comparison using [sim/BALANCE.md](sim/BALANCE.md).

Routine validation uses **10,000 fixed effective seeds (1..10000)**, the same
frozen **omniscient-v2** agent, and the same effective seed population in both
variants. For intentional substantial balance/content changes, ambiguous
results or final validation, use **all 65,535 unique effective seeds (1..65535)**
when practical. Never compare unrelated random samples. Never compare different
agent versions and attribute the difference to game content. Simulator failures
(`SIM_STUCK`/`SIM_ERROR`) invalidate experimental data; they are not deaths.

Inspect practical effect size before p-values. Exploratory multi-factor reports
use FDR-adjusted q-values (default q <= 0.05). Do not infer causality from
pickup/use/equip correlations. Recognize survivorship bias for late items and
monsters; prefer generated-on-visit exposure adjusted for entry condition and
floor/direction, followed by controlled intervention experiments.

For new content, prefer both a normal paired game comparison where applicable
and a controlled same-location/same-seed replacement/removal A/B. Use one
candidate executable in both variants when isolating new-content effects:
replace new content with a comparator in control, or substitute new content
into comparator slots in treatment. Pure additive availability is a distinct
experiment. Do not resynchronize gameplay RNG after variants diverge.

omniscient-v2 is frozen. If new-content agent support is behaviorally unreachable
with that content disabled, run a compatibility control with all new content
removed/replaced. Old-content results, action hashes and relevant telemetry must
match the frozen reference. If old-content behavior changes materially, increment
the agent version and establish a new reference before attributing effects to
content. Document an agent defect; do not silently repair the frozen policy.

After unexpected A/B results, trace representative discordant seeds from both
directions before changing balance constants. Retain experiment specifications,
manifests and reports. Generated CSV/report artifacts belong under ignored
`build/`; never commit huge datasets. Small reference summaries may be committed
to document an intentional baseline, subject to repository commit instructions.
