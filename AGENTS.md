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

All cases must take at most **150 ms** in their worst sample, from submitted
input through computation and final rendering to readiness for the next input;
generation and animation are excluded. Use the saved avm-lldb profiles to
optimize failures while preserving gameplay and benchmark coverage. Run
correctness tests and rerun the full suite on the final ELF until all cases pass.
