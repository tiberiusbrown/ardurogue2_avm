# String storage

Use flash strings for constant text and format strings in gameplay code rather
than storing them in RAM. Wrap literals in `F(...)` (or use `PROGMEM` /
`AVM_PROGMEM` declarations). The AVM runtime library provides C++ overloads
that accept flash strings through the ordinary API names, such as `snprintf`
and `avm_draw_text`; use those overloads. ArduRogue 2's own `status_word` also
provides a flash-string overload. RAM buffers remain appropriate for text
constructed at runtime.

Do not hardcode item names into status strings. Use the item-aware status methods
(`status(item)` or `status(item, punctuation)`) to format item names, identification,
curse, enchantment and articles consistently. Copy an Item before destroying it
when a later message must describe the consumed or broken item.

# Equipment

Equipment type/subtype definitions determine inherent damage range, accuracy,
and armor rating. `Item::info` stores only per-instance state such as signed
enchantment, curse, and identification. Never derive inherent capability from
`info` or pack ratings/tiers together with enchantment. Enchantment changes only
the roll distribution within the type's fixed range.

Keep all items of the same group contiguous in the `ItemType` enum so group
predicates such as `is_weapon`, `is_armor`, `is_potion`, `is_ring`, `is_amulet`,
`is_scroll`, and `is_wand` can use simple inclusive range tests.

# Ammunition and projectiles

`ARROWS` uses all eight `Item::info` bits for unsigned quantity, including the
bits otherwise named curse/identification. Use `item_value`, `set_item_value`,
`item_is_cursed`, `item_is_identified`, `maximum_stack` and `is_stackable`; never
mask arrow quantities or OR flags into ammo. Keep the Ammo enum group contiguous.
Bows are equipment in the single weapon slot; their melee and ranged definitions
are separate. Ranged attacks reuse `scan_ray` and physical combat. The bow range
cap is `MAX_BOW_RANGE == MAX_LIGHT_RADIUS`; light does not alter physical range.
Resolve Throw/Shoot after the UI frame unwinds, as for wands. No projectile or
telemetry fields belong in `Game`. Quantity generation uses `AMMO_QUANTITY`,
leaving supplies/equipment/placement and gameplay RNG isolated.

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

For gameplay/content changes affecting difficulty or player power, run native
correctness tests and paired balance validation per [sim/BALANCE.md](sim/BALANCE.md).

- **Seeds:** Use identical effective seeds and maintained `omniscient-v2` in both
  variants: `1..10000` routinely; `1..65535` for substantial changes, ambiguous
  results, or final validation when practical. `SIM_STUCK`/`SIM_ERROR` invalidate
  data; they are not deaths.
- **New content:** Also test same-seed/same-location replacement or removal
  against an appropriate comparator, using one candidate executable for both
  variants. Treat additive availability separately; never resynchronize RNG
  after divergence.
- **Agent compatibility:** With new content removed/replaced, verify old-content
  results, action hashes, and relevant telemetry match the previous reference where mechanically possible.
  Document defects; new mechanics may extend `omniscient-v2` in place. The
  automatic `agent_policy_hash` in each build manifest identifies the exact
  policy. Baseline/candidate content comparisons require identical policy hashes.
  Represent new mechanics competently before trusting balance conclusions;
  after a policy extension, establish a fresh reference balance run.
- **Analysis:** Prioritize practical effect sizes, floor survival, death causes,
  and relevant item/monster metrics. Use FDR-adjusted q-values for exploratory
  reports (default `q <= 0.05`). Observational associations are not causal;
  account for selection/survivorship bias using generated-on-visit exposure
  adjusted for entry condition and floor/direction, then controlled experiments.
- **Review:** Trace representative discordant seeds in both directions after
  unexpected results, before tuning constants. Accept changes only after review
  confirms the difficulty/power shift is intentional.
- **Artifacts:** Retain experiment specifications, manifests, and reports under
  ignored `build/`; never commit huge datasets. Small intentional reference
  summaries may be committed subject to repository commit instructions.
