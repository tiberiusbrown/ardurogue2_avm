# Multi-spell books and mana recovery: implementation report

Validated 2026-10-13. Four fixed books, individual spell selection, retained partial books, mana potions, Meditation, and silent passive regeneration are implemented in gameplay and both agents. Every spell applies a measurable effect in natural runs for each agent. Automated escape rates nevertheless fall relative to the previous system; the explorer 2–5% target remains unmet. These results support improved acquisition and broader spell use, not an escape-rate improvement.

## Gameplay and architecture

| Area | Main files |
| --- | --- |
| Metadata, learning, casting and generation | `src/model.hpp`, `src/spells.hpp`, `src/spells.cpp` |
| Inventory modal and rendering | `src/inventory_view.hpp`, `src/ui.cpp`, `src/render.cpp` |
| Items, appearances and recovery | `src/items.cpp`, `src/state.cpp`, `src/status.cpp`, `src/combat.cpp`, `src/world_gen_population.cpp` |
| Agents and observation boundary | `sim/spell_preferences.hpp`, both agent sources, explorer observation/protocol and boundary checks |
| Telemetry and reports | `src/sim_hooks.hpp`, `sim/metrics.*`, `sim/manifest.cpp`, `sim/spell_report.py` |
| Correctness and timing | `tests/book_mana_checks.cpp`, existing native suites, `sim/test_spell_report.py`, `bench/bench.cpp`, `bench/profile_turns.py` |

The four memberships, depth weights and controls are documented in [SPELLS.md](SPELLS.md). Compact constexpr masks validate coverage, uniqueness, IDs and exclusion of starting Magic Dart. `Item` remains two bytes; `info` holds the public book ID. The known-spell mask is the sole persistence for partially learned books. Each successful study teaches one member; only the final unknown member consumes the book. The shared `begin_study(slot, spell)` rejects invalid, unrelated or already-known selections before spending resources. Final learning clears study references and completes removal before callbacks. Dropping, pickup and save/resume retain book identity without partial-session progress.

The inventory-style modal uses the existing directional repeat, seven-row scrolling helper and flash text. Known rows are skipped; A confirms and B cancels without gameplay effects. Selection returns through the existing pending action before gameplay starts, avoiding overlapping modal/gameplay/render frames. Rendering captures were inspected, including the full five-character Known label.

Generation uses a local unavailable byte and an isolated book stream. Each 6% attempt draws one depth-weighted ID; ineligible draws preserve ordinary loot. Carried, completed and already-generated books are excluded without retries. Food, arrows, lights, artifacts/Yendor, ascent and the special all-phantom fifth floor keep their exclusions. Ordinary loot positions and unrelated generation draws are preserved. Mana and Meditation use separate slot-local 8% replacement draws within their ordinary categories; other subtypes retain relative weights. Replacement trades away some existing items rather than adding supplies.

Mana potions use rejection sampling over the nonzero 16-bit RNG stream to draw an inclusive `ceil(max MP / 2)..max MP` amount. Addition uses a wider integer and clamps. Drinking reuses identification, stacking, conservation and action rules; throwing leaves monster state, aggression and attack telemetry unchanged through the existing player-only-potion path. Meditation changes the eight-bit clock divisor immediately to 8, 16 or cursed 32. Hunger, full capacity, death and ended games suppress ticks. HP Regeneration keeps its roll and effect; passive HP/MP recovery emits no gameplay status.

## Authorized tuning and agent strategy

The user authorized spell-related costs and study tuning. Effects, damage ranges and the 3 HP Blood Bolt payment remain unchanged. Starting Dart and Confound retain their MP cost. Frostbind, Repulsion, Sense Traps and Blood Bolt cost 2 MP; Piercing Bolt costs 3; the remaining learned spells cost 4. Piercing Bolt study is 40 turns; Exchange, Fireburst and Chain Lightning are shortened to 60. Other studies retain their original durations. Book weights, 6% attempts and the two 8% recovery replacement rates are retained.

Both policies choose the highest utility per study turn across all carried books, with role redundancy and depth discounts. A public depth budget permits further study only below `2 + floor / 2` additional learned spells (zero-based floor); this is agent strategy, not a player restriction. HP, quiet surroundings, exits, fuel, hunger, spare food, interruption retry and ascent precautions remain. Known mana potions are used at at least two-thirds missing MP when casting/escape has value. Meditation and curses affect discretionary reserves and compete with other amulets.

Explorer protocol 9 exposes inspected/carried book IDs and a public experimental cost profile. Remote books remain category-only. Hidden monster HP/status, map cells, identities and timers remain excluded. The policy compiles separately from production Game declarations, and the boundary/symbol checks pass. Omniscient retains its deliberate knowledge advantages and uses the same authoritative study and casting rules.

Development cohorts exposed excessive study and zero usage for redundant spells. Duration-normalized value, staged complementary learning, lower situational costs, global selection and the depth budget addressed those defects. The initial original-cost 10,000-seed candidate had 0.69% explorer and 42.59% oracle escapes; later cost/policy cohorts had broader use. Those policies differ, so cross-version comparisons are descriptive. The final comparison below establishes a fresh reference with one frozen policy per agent.

## Memory, code size and hardware

| Measure | Before | Final | Change |
| --- | --- | --- | --- |
| Saved Game | 844 B | 845 B | +1 B |
| Identifiable types / bitmap | 63 / 8 B | 65 / 9 B | +2 types / +1 B |
| Item | 2 B | 2 B | 0 |
| Transient global .data | 125 B | 125 B | 0 |
| AVM .text | 104,524 B | 107,372 B | +2,848 B |
| AVM .rodata | 12,368 B | 12,373 B | +5 B |
| Bytecode + rodata | 116,892 B | 119,745 B | +2,853 B |
| Packaged .arduboy | 206,172 B | 208,988 B | +2,816 B |
| Complete stack bound | 247 B documented | 249 B | 7 B below 256 B |

Save version 32 rejects older layouts without migration. No extra saved book, study, timer, inventory or map storage is added. The book view has a four-byte automatic mask model; existing pending action fields carry selection. Host observations and telemetry grow only in native simulator memory. Production and bench stack analyses both report a complete 249-byte bound and zero gaps.

All 129 benchmark cases pass the unchanged 100 ms threshold at 16 MHz. Worst: monster_trap_busy at 94.698813 ms. Original cases are retained, with six new book/recovery cases. Generation and animation exclusions follow the existing profiler contract.

| Case | Worst ms |
| --- | --- |
| book_open | 7.045875 |
| book_down | 6.766938 |
| book_start | 40.202625 |
| book_cancel | 33.822500 |
| drink_mana | 47.557938 |
| equip_meditation | 46.061375 |
| study_turn | 38.090625 |
| study_interrupt | 48.079937 |

## Correctness validation

All 30 CTests pass. Coverage includes all 15 book members, masks/names, partial/final learning, durations, interruption/cancel, invalid selection, save identity, drop/pickup/full inventory, modal controls and scrolling, randomized appearances and identification, all nonzero seeds for mana range endpoints, conservation, throwing, clock wrap/equipment/stairs/resume/starvation/death/full MP, silent HP/MP during study, both agents and strict observation boundaries. The generator checks 65,535 seeds in each depth band with statistical tolerances, plus duplicate, essential-loot, artifact, ascent and phantom exclusions.

Reporting tests distinguish destroyed books from completed studies and reconcile per-floor learning/casting/effect exports. Full experimental visit/run item, monster, ranged, action and turn counters reconcile. The original-content compatibility comparison disables magic and new recovery content and restores the old HP text: both agents match the previous executable on all 1,000 seeds, including outcome, actions, turns, score and action hash, with no simulator failures.

## Paired final experiment

Seeds 1–10,000, both agents, two conditions, eight jobs, 60,000-action limit, full telemetry and entry-state sidecar: 40,000 final runs. No SIM_STUCK or SIM_ERROR. Each agent uses identical policy hashes and the identical frozen executable in both arms. CSV/executable hashes and experiment specifications are sealed. The comparator restores single-spell books, original spell costs/studies, no new recovery items and the old HP-regeneration text using:

```text
balance:multi-spell-books=0
balance:legacy-spell-rules=1
balance:mana-potion-percent=0
balance:meditation-percent=0
balance:silent-regeneration=0
```

The candidate uses production defaults. The cost profile is public to both agents. Random streams are never resynchronized after routes, study or casting diverge. Primary tests use exact paired McNemar/binomial p-values, paired percentile-bootstrap 95% intervals and BH q-values across the two agents. Exploratory floor-exit tests adjust all 64 agent/floor/direction comparisons together.

| Agent | Old escapes | New escapes | Delta pp | 95% CI pp | Losses / gains | q |
| --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | 141 (1.41%) | 89 (0.89%) | -0.52 | -0.78..-0.26 | 117 / 65 | 0.000142 |
| omniscient-v2 | 5,130 (51.30%) | 4,569 (45.69%) | -5.61 | -6.57..-4.71 | 1,505 / 944 | 1.24e-29 |

These are meaningful regressions under the maintained agents. The depth budget limits commitment, but the new system still entails additional study, retained inventory and different acquisition routes. Recovery replaces other items. The bundled comparison cannot isolate how much each component contributes; lower escape rates must not be presented as evidence that more learned spells improve survival. The 2–5% explorer target remains a separate unresolved balance objective.

## Acquisition, study and resources

| Measure | Explorer old | Explorer new | Oracle old | Oracle new |
| --- | --- | --- | --- | --- |
| Books generated | 70,990 | 46,149 | 108,230 | 71,906 |
| Book pickup transactions | 28,670 | 33,077 | 51,650 | 75,035 |
| Books completed | 21,408 | 3,219 | 44,523 | 5,975 |
| Additional spells / run | 2.1408 | 3.8223 | 4.4523 | 7.1846 |
| Study attempts | 26,703 | 46,285 | 49,807 | 78,670 |
| Studies completed | 21,408 | 38,223 | 44,523 | 71,846 |
| Interrupted | 5,295 | 8,062 | 5,284 | 6,824 |
| Study turns | 1,166,777 | 2,155,374 | 2,498,175 | 3,986,239 |
| MP spent | 973,241 | 1,048,797 | 2,714,442 | 2,922,678 |
| MP regenerated | 935,403 | 937,758 | 2,695,629 | 2,581,510 |
| MP restored by potion | 0 | 71,955 | 0 | 316,181 |
| MP while Meditation worn | 0 | 38,422 | 0 | 173,722 |
| HP from regeneration amulet | 57,252 | 52,006 | 33,042 | 29,131 |
| Deaths during / within 16 turns after study | 0 / 3 | 0 / 2 | 0 / 2 | 0 / 2 |
| Food use actions | 147,027 | 142,274 | 145,696 | 145,077 |
| Fuel exhaustions | 9,910 | 9,835 | 6,633 | 7,314 |
| Potion availability on visited floors | 420,946 | 413,247 | 631,698 | 635,548 |
| Healing + Extra Healing generated | 80,512 | 73,491 | 113,781 | 105,996 |
| Meditation equipped turns | 0 | 2,095,345 | 0 | 2,339,130 |
| HP regeneration amulet equipped turns | 2,944,358 | 2,590,024 | 1,477,751 | 1,306,937 |

Pickup counts include repicking dropped books and can exceed generation. Member exposure counts include every contained spell; they are not book counts. Completion is recorded only by final study, independently of destruction/sacrifice. Starting Dart is excluded from learned additions. Death windows include attempts/interrupted studies and are descriptive; zero immediate deaths does not rule out cumulative food/light/opportunity costs. Generation totals reflect visited floors and changing survival, not independent availability effects.

Direct spell kills remain a small part of offense:

| Agent | Old direct spell kill share | New share | Arrows fired old / new | Wand charges old / new |
| --- | --- | --- | --- | --- |
| explorer-v1 | 8.70% | 8.74% | 11,096 / 11,340 | 31,778 / 28,491 |
| omniscient-v2 | 8.30% | 8.57% | 139,658 / 128,984 | 231,405 / 205,796 |

There is no indication here of direct-damage spell dominance over weapons, wands or consumables. Control/mobility effects are additional contributions, so kill share alone is not a complete measure of magical power; these policies also do not represent every human strategy.

## Visible utility for every spell

The following counts come from natural candidate runs, not forced demonstrations. Casts and applied effects are separate: misses, empty surveys and failed landing predictions remain visible. Assigned status/stun durations are not measured enemy actions or HP prevented.

| Spell | Learned explorer / oracle | Casts explorer / oracle | Applied effect explorer / oracle |
| --- | --- | --- | --- |
| Magic Dart | 0 / 0 | 251,455 / 1,151,635 | 643,398 / 3,289,613 damage; 80,652 / 142,645 kills |
| Frostbind | 1,879 / 3,241 | 8,527 / 19,995 | 8,522 / 19,995 slow/confuse applications |
| Repulsion | 1,681 / 2,966 | 3,448 / 5,336 | 2,925 / 4,610 successful displacements |
| Confound | 6,824 / 9,039 | 68,971 / 69,869 | 68,964 / 69,869 slow/confuse applications |
| Sense Traps | 2,466 / 5,032 | 49,459 / 28,591 | 3,493 / 1,974 traps revealed |
| Blink | 5,840 / 7,927 | 8,144 / 1,269 | 8,144 / 1,269 successful displacements |
| Shockwave | 3,776 / 7,350 | 1,449 / 2,561 | 2,514 / 6,010 successful displacements |
| Piercing Bolt | 1,684 / 4,063 | 548 / 5,355 | 4,167 / 41,024 damage; 182 / 1,330 kills |
| Banish | 2,755 / 6,171 | 11,646 / 4,797 | 11,644 / 4,797 successful displacements |
| Exchange | 2,804 / 4,294 | 412 / 846 | 412 / 846 successful displacements |
| Cleanse | 4,235 / 7,876 | 17,503 / 40,616 | 17,503 / 40,616 conditions removed |
| Fireburst | 638 / 3,212 | 54 / 1,917 | 459 / 16,126 damage; 12 / 562 kills |
| Phase Step | 1,687 / 4,076 | 9,021 / 6,589 | 8,992 / 6,589 successful displacements |
| Chain Lightning | 1,229 / 5,302 | 1,444 / 10,052 | 10,521 / 72,605 damage; 270 / 1,909 kills |
| Slumber | 429 / 659 | 3,911 / 2,694 | 3,911 / 2,694 targets stunned |
| Blood Bolt | 296 / 638 | 63 / 264 | 452 / 1,475 damage; 2 / 264 kills |

Fireburst and Blood Bolt remain situational, particularly for explorer, but both deal damage and produce kills. Blood Bolt pays 3 HP per cast; no natural sacrifice deaths occur. Oracle uses it for guaranteed efficient finishes; explorer uses public species/range and conservative HP/resource thresholds. Sense Traps has many empty casts because hidden trap positions are not supplied to the explorer.

## Floor outcomes and retained artifacts

| Floor | Explorer entered old / new | Explorer exited old / new | Oracle entered old / new | Oracle exited old / new |
| --- | --- | --- | --- | --- |
| 1 | 10,000 / 10,000 | 9,766 / 9,752 | 10,000 / 10,000 | 9,781 / 9,796 |
| 2 | 9,766 / 9,752 | 9,495 / 9,450 | 9,781 / 9,796 | 9,739 / 9,754 |
| 3 | 9,495 / 9,450 | 8,590 / 8,512 | 9,739 / 9,754 | 9,635 / 9,662 |
| 4 | 8,590 / 8,512 | 8,003 / 7,940 | 9,635 / 9,662 | 9,583 / 9,606 |
| 5 | 8,003 / 7,940 | 7,639 / 7,566 | 9,583 / 9,606 | 9,567 / 9,579 |
| 6 | 7,639 / 7,566 | 7,217 / 7,055 | 9,567 / 9,579 | 9,526 / 9,520 |
| 7 | 7,217 / 7,055 | 6,566 / 6,347 | 9,526 / 9,520 | 9,477 / 9,445 |
| 8 | 6,566 / 6,347 | 6,194 / 5,975 | 9,477 / 9,445 | 9,445 / 9,393 |
| 9 | 6,194 / 5,975 | 5,643 / 5,391 | 9,445 / 9,393 | 9,360 / 9,312 |
| 10 | 5,643 / 5,391 | 5,377 / 5,068 | 9,360 / 9,312 | 9,330 / 9,267 |
| 11 | 5,377 / 5,068 | 4,444 / 4,014 | 9,330 / 9,267 | 9,126 / 8,962 |
| 12 | 4,444 / 4,014 | 3,501 / 2,976 | 9,126 / 8,962 | 8,790 / 8,532 |
| 13 | 3,501 / 2,976 | 2,533 / 2,064 | 8,790 / 8,532 | 8,424 / 8,110 |
| 14 | 2,533 / 2,064 | 1,584 / 1,150 | 8,424 / 8,110 | 7,704 / 7,260 |
| 15 | 1,584 / 1,150 | 900 / 638 | 7,704 / 7,260 | 6,781 / 6,167 |
| 16 | 900 / 638 | 576 / 397 | 6,781 / 6,167 | 6,325 / 5,754 |

The [small validated summary](MULTI-SPELL-BOOKS-10000.json) retains both directions, corrected floor q-values, death causes, learning cohorts, resource counts and full policy/executable identities. [Per-floor spell counts](MULTI-SPELL-BOOKS-SPELLS-BY-FLOOR.csv) retain all four arms. Conditional learning/entry cohorts are survival-selected and are not causal evidence about a spell or book.

Representative final discordant seeds were traced in both directions and their action hashes verified against the full datasets: explorer gain 58/loss 126; oracle gain 22/loss 8. Explorer 58 first diverges at a mana-versus-extra-healing potion experiment; explorer 126 learns Confound instead of legacy Repulsion and later dies to a frost trap. Oracle 22 and 8 first diverge at book acquisition routes; the gain escapes and the loss dies to dragon fire. These demonstrate expected route/study/content divergence, not a unique cause for the population effect. Earlier unexpected cohorts were also traced before tuning.

Raw final data, sealed specifications, frozen executable and reports: `build/multispell/verified-10k` and `build/multispell/verified-sim.exe`. Full final profiles: `build/multispell/verified-profiles/20261013T175411Z-db597k3f`. Compatibility and traces are retained under ignored `build/multispell/`; huge raw datasets are not committed.

Reproduce a condition with the native simulator from the documented build, using one frozen executable:

```text
<sim> --agent explorer-v1 --seeds 1:10000 --jobs 8 --max-actions 60000 --entry-state --output build/magic-candidate
<sim> --agent explorer-v1 --seeds 1:10000 --jobs 8 --max-actions 60000 --entry-state --output build/magic-control --intervention balance:multi-spell-books=0 --intervention balance:legacy-spell-rules=1 --intervention balance:mana-potion-percent=0 --intervention balance:meditation-percent=0 --intervention balance:silent-regeneration=0
```

Repeat for omniscient-v2; seal manifests with `sim/balance.py`, compare paired runs with matching policy/executable identities, and generate effect/depth exports with `sim/spell_report.py`. Hardware uses the standard CMake SDK build and full `bench/profile_turns.py --check` workflow from AGENTS.md.

## Remaining limits

The final all-65,535-seed census was not run. Multiple development cohorts and 40,000 final paired runs support the measured effects, but do not exhaust the seed population. Physical-device testing was not performed; native, AVM stack analysis, deterministic timing profiles and rendered captures are the validation scope. Automated escape regressions and the unmet explorer target are the main balance limitation. Acquisition/inventory opportunity costs and recovery-versus-healing availability merit controlled component experiments before further balance claims. Older saves remain intentionally incompatible.
