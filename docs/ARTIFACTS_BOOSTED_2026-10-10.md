> Historical report: strong artifacts at the former 1/8 spawn rate.
> The final 1/128 rarity change and current measurements are in [ARTIFACTS.md](ARTIFACTS.md).

# Ten rare artifacts: strengthened implementation

Updated 2026-10-10 in the uncommitted working tree based on
`e9762262c2be3c7389b0e3d343e917e5ab5f216b`. The nine new artifacts have been
strengthened, and the existing Ring of Invisibility is now the tenth artifact.
Cursed drawbacks remain. The packaged game has been rebuilt. No commits or
pushes were made. The [initial report](ARTIFACTS_INITIAL_2026-10-10.md) retains
original mechanics and measurements before tuning.

## Current behavior

| Artifact | Uncursed effect | Cursed effect |
| --- | --- | --- |
| Stormbringer | Melee 6–10, accuracy +2; orthogonal 3–6 physical splash; 1/16 chance of one self-damage after a successful hit | Same offense; 1–2 self-damage on every hit |
| Glass Sword | Melee 8–14, accuracy +4; shatters after damage on 1/256 successful hits | Shatters on 1/16 successful hits |
| Hammer of Ruin | Melee 6–10, accuracy +1; 1/3 chance to double post-armor damage and force surviving target away | Same heavy hits; independent 1/4 player recoil opposite attack |
| Dragonhide | Armor 7; fire immunity | Same armor; fire vulnerability, doubled once before saving throw |
| Titan Plate | Armor 12; no speed penalty | Effective speed cost +4 |
| Ring of Invisibility | Permanent invisibility while equipped | Cancels temporary invisibility; no concealment |
| Ring of Reprisal | +2 armor; 1/2 chance of a turn-free normal melee counterattack after damaging monster melee | No passive armor/counters; 1/4 chance of one extra self-damage |
| Ring of the Hunt | +2 armor; arrows gain +8 accuracy and +6 raw damage, with or without a bow | No passive armor; −6 accuracy and −4 raw arrow damage, clamped to at least one |
| Phoenix Heart | +20 maximum HP; effective speed cost −2; consumed on lethal damage, restoring full effective maximum HP after removal | No passive benefits or resurrection |
| Heart of the Giant | +40 maximum HP; effective speed cost −2 | Maximum HP −12; no speed benefit |

Reprisal and Hunt do not confer invisibility. Artifact jewelry modifiers are
inherent fixed magnitudes rather than inferred enchantments. Increasing maximum
HP does not heal current HP; removing equipment clamps current HP. HP stays
within 1–255, and speed arithmetic is bounded and combines with slowing and
ordinary speed amulets without changing `game.speed`.

Successful-hit effects keep their explicit order. Primary damage precedes
splash/heavy damage and target movement, followed by life/break/recoil effects.
Splash applies physical armor, awards normal kills, uses the primary target's
original position and never recurses. Force wands and Hammer share movement and
collision helpers. Reprisal spends no additional turn, applies only to actual
monster melee HP loss, and safely handles a killed attacker before regeneration.
Positive fire immunity overrides all vulnerabilities; multiple vulnerabilities
double at most once. A shared lethal-damage path handles melee, magic/fire,
starvation, harming potions and item self-damage. Destruction clears the whole
Item, equipment slot and repeat reference, including for self-consuming curses.

## Rarity, formatting and saves

All ten types independently have a 1/8 run-wide selection probability and one
deterministic floor index in 4–14, derived from run seed/type in the dedicated
artifact domain. Each appears at most once during normal play. Post-processing
replaces distinct accessible ordinary ground slots, prefers non-Food/non-Arrows
placements, skips ascent and preserves the Yendor reservation. It consumes
neither gameplay RNG nor ordinary generation draws.

Invisibility no longer belongs to the ordinary ring roster. Its former 2%
ordinary subtype outcome now yields protection with the same instance generation
and RNG draws; the other ordinary subtype thresholds remain unchanged. Its
unique artifact placement follows the same 1/8 schedule as the other types.
This intentional availability change is separate from capability comparisons.

Weapon/armor enchantments retain −2/−1/0/+1/+2 at 5/10/70/10/5%, with independent
1/8 curses. Jewelry keeps a 1/8 curse draw and unidentified appearances. Existing
icons are reused. Ten jewelry descriptions preserve appearance permutations.
Names reside in flash and status messages use item-aware formatting. The new
rule in `AGENTS.md` prohibits hardcoded item names in status strings; messages
that describe a consumed/broken item copy it before destruction.

`Item` remains two bytes and AVM `Game` remains 773 bytes (native 774 with tail
padding). All layout/offset assertions remain. The six-byte identification set
contains 46 types. `SAVE_VERSION` remains 25 from the enum expansion; converting
an existing ring to an artifact requires no additional enum or layout change.
No new persistent RAM or production simulator instrumentation was added.

## Correctness and performance

Native Clang 22.1.3, C++17, RelWithDebInfo: **15/15 CTests passed**, 23.57 seconds.
This includes full game/generation, artifact distributions and behavior,
formatting/rendering/UI, benchmark fixtures, simulator, serial/parallel telemetry
and balance-analysis checks. All ten artifacts are tested as equipment upgrades
against ordinary top-tier gear, including Hunt without ammunition. Full ring
slots require the normal removal turn before equipping the upgrade.

Artifact tests cover normal/cursed behavior, destruction/repeat cleanup, effect
ordering, rewards, force collisions, fire precedence, speed/HP bounds, every
resurrection damage path, later lethal damage, arrow modifiers and unaffected
wand/potion paths. Scheduling and equipment frequencies use all 65,535 effective
seeds; real floor generation uses 2,048 seeds across eleven eligible floors to
check accessibility, deterministic placement, collisions, uniqueness, ordinary
stream isolation and absence of artifacts in ordinary generation. Appearance,
identification, ascent, Yendor and saved-layout checks pass. The floor snapshot
for seed ffff changed as expected with invisibility reclassification.

Both final AVM builds have a complete **254/256-byte stack bound**, zero analysis
gaps. Item formatting is divided into small category helpers; dragon fire
resolution runs after its AI decision frame unwinds, preserving decision/RNG
order while allowing item-aware resurrection messages within the stack budget.

| Size | Pre-artifact reference | Current image | Increase |
| --- | ---: | ---: | ---: |
| Packaged `.arduboy` | 159,068 B | 166,748 B | 7,680 B (+4.83%) |
| `fxdata.bin` | 71,168 B | 78,848 B | 7,680 B (+10.79%) |
| `interp.hex` | 83,157 B | 83,157 B | 0 |
| `fxsave.bin` | 4,096 B | 4,096 B | 0 |

**42/42 final-ELF benchmarks meet 100 ms.** Input, computation and final rendering
are included; generation/animation are excluded. Worst case remains arrow kill,
95.14856 ms versus 94.92494 ms before artifacts (+0.22362 ms). The added cases
measure Storm splash 61.241 ms, Glass break 55.261 ms, Hammer heavy 59.101 ms,
Reprisal 57.708 ms, Phoenix 57.154 ms and Titan speed 43.240 ms.

Evidence: [native checks](../build/artifacts/boost-ten-native-tests-final.log),
[AVM build and stack](../build/artifacts/boost-ten-avm-build.log),
[42-case final benchmark report](../build/artifacts/boost-ten-turn-benchmarks/20261010T185204Z-kjks39j1/summary.md).

## Controlled balance validation

All ten artifacts improve aggregate escape rate over their ordinary counterparts
across all 65,535 nonzero effective seeds, by +0.526 to +1.659 percentage points.
This meets the requested uncursed aggregate survival criterion under the maintained
policy; cursed drawbacks remain. Every capability
comparison uses the same executable, effective seeds and maintained
`omniscient-v2` policy hash
`0adcf5360d3783f385a7f67054143e99cbf07b52c35b3440a7a680eeb41df28b`.
Only one artifact is replaced per control; all others remain natural. Controls
preserve location, instance enchantment, identification and curse. Comparators
are Two-handed sword for all weapons, Plate mail for both armor types,
Protection for the three rings and Speed for both amulets. The policy rejects
cursed equipment, so these comparisons measure the uncursed power requirement;
native checks provide cursed behavior evidence. No gameplay RNG is resynchronized
after divergence.

The ordinary-items control disables only the artifact placement pass in the
current roster. A separate old-content compatibility audit maps legacy ordinary
invisibility to protection before comparing action hashes/outcomes/telemetry.
The ledger differs by design because only the legacy executable performs the
host replacement; zero-valued new item rows and shifted numeric IDs are normalized.
These compatibility checks are separate from causal same-policy content comparisons.
The 10,000-seed audit matches all seven behavioral streams exactly after this
normalization: outcomes, action hashes, floors, items, monsters, typed visits and
ranged activity (4,066 escapes in both builds).
Evidence: [compatibility audit](../build/artifacts/boost-ten-10k/ordinary/compatibility-audit.json).

The exact individual effects below compare natural artifacts to a control
replacing only the named type. All comparisons passed manifest/hash validation,
with zero simulator failures. No sampling intervals or p-values apply to this
complete deterministic population.

| Artifact | Ordinary counterpart | Escape delta (pp) | Gained escapes | Lost escapes |
| --- | --- | ---: | ---: | ---: |
| Stormbringer | Two-handed sword | +0.896 | 621 | 34 |
| Glass Sword | Two-handed sword | +0.858 | 676 | 114 |
| Hammer of Ruin | Two-handed sword | +1.602 | 1,056 | 6 |
| Dragonhide | Plate mail | +0.743 | 570 | 83 |
| Titan Plate | Plate mail | +1.498 | 1,011 | 29 |
| Ring of Invisibility | Protection | +1.659 | 1,103 | 16 |
| Ring of Reprisal | Protection | +0.775 | 579 | 71 |
| Ring of the Hunt | Protection | +0.526 | 459 | 114 |
| Phoenix Heart | Speed | +0.797 | 542 | 20 |
| Heart of the Giant | Speed | +0.829 | 566 | 23 |

Evidence: [all ten paired census comparisons](../build/artifacts/boost-ten-all/results.json).
The 10,000-seed runs are identical ordered prefixes of the census in all eight
behavioral/entry streams; the intervention ledger differs only in its experiment
label. This also verifies reproducibility across four versus six native workers.
[Prefix audit](../build/artifacts/boost-ten-all/prefix-audit.json).

The availability census includes every nonzero effective seed: 38,873 escapes
with natural artifacts (59.316%), versus 26,648 (40.662%) with the current
ordinary roster. The exact +18.654-point improvement comprises 12,555 gained
escapes and 330 lost, with zero simulator failures. Floor-15 reach improves
9.821 points, post-Yendor deaths fall 5.542 points among all runs, and mean HP
damage taken falls 66.999 per run. These secondary summaries are descriptive;
immediate death causes and individual traces are not the whole causal chain.

Availability counts below are unique runs, except consumption events. Generated
means the scheduled floor was visited; selected is the full run-wide schedule.
Equipped counts ignore repeated switches between melee weapons and bows. Every
generated artifact occurred at most once per run.

| Artifact | Selected | Generated | Reached | Picked up | Equipped runs |
| --- | ---: | ---: | ---: | ---: | ---: |
| STORMBRINGER | 8,185 | 6,331 | 5,410 | 4,968 | 4,968 |
| GLASS_SWORD | 8,216 | 6,368 | 5,615 | 5,455 | 5,455 |
| HAMMER_OF_RUIN | 8,234 | 6,313 | 5,387 | 5,167 | 5,167 |
| DRAGONHIDE | 8,439 | 6,472 | 5,686 | 5,233 | 5,232 |
| TITAN_PLATE | 8,228 | 6,290 | 5,602 | 5,452 | 5,451 |
| RING_INVISIBILITY | 8,170 | 6,265 | 5,616 | 5,366 | 5,354 |
| RING_REPRISAL | 8,249 | 6,360 | 5,672 | 5,505 | 5,503 |
| RING_HUNT | 8,241 | 6,385 | 5,689 | 5,524 | 5,520 |
| AMULET_PHOENIX_HEART | 8,146 | 6,288 | 5,671 | 5,223 | 5,223 |
| AMULET_HEART_OF_GIANT | 8,120 | 6,252 | 5,600 | 5,449 | 5,449 |

Glass shatters 3,554 times and Phoenix resurrects 144 times in the natural census. These usage counts are not causal rankings of acquired items.

Evidence: [availability census](../build/artifacts/boost-ten-all/ordinary-comparison/compare.md),
[generation and usage](../build/artifacts/boost-ten-all/validation-metrics.json).

The 1..10,000 prefix gives the following paired estimates. The intervals and
McNemar tests assume the deterministic prefix represents the wider population;
BH correction applies jointly to the ten artifact comparisons. The census
uses exact population effects without sampling intervals or p-values.

| Artifact versus comparator | Escape delta (pp) | Paired 95% interval (pp) | BH escape q |
| --- | ---: | --- | ---: |
| Stormbringer / Two-handed sword | +0.99 | +0.78 to +1.20 | 7.68e-23 |
| Glass Sword / Two-handed sword | +0.99 | +0.77 to +1.23 | 2.17e-17 |
| Hammer of Ruin / Two-handed sword | +1.54 | +1.31 to +1.78 | 4.67e-42 |
| Dragonhide / Plate mail | +0.83 | +0.64 to +1.05 | 3.87e-17 |
| Titan Plate / Plate mail | +1.40 | +1.16 to +1.65 | 1.09e-34 |
| Ring of Invisibility / Protection | +1.59 | +1.34 to +1.85 | 3.2e-43 |
| Ring of Reprisal / Protection | +0.85 | +0.66 to +1.06 | 7.8e-19 |
| Ring of the Hunt / Protection | +0.43 | +0.24 to +0.63 | 7.35e-06 |
| Phoenix Heart / Speed | +0.83 | +0.65 to +1.02 | 3.77e-21 |
| Heart of the Giant / Speed | +0.84 | +0.66 to +1.03 | 3.93e-22 |

Natural artifacts escape in 5,951/10,000 runs (59.51%), versus 4,066 (40.66%)
with the current ordinary roster: +18.85 percentage points, 1,935 gained escapes
and 50 lost. This availability result includes all artifact replacements and is
separate from each type's same-location capability comparison. Zero simulator
failures occurred in every variant. No starvation deaths occurred; Stormbringer
self-damage caused three deaths in the natural prefix.

Representative discordant seeds were replayed in both directions for each type:
Storm 58/950, Glass 46/170, Hammer 188/826, Dragonhide 39/109, Titan 204/109,
Invisibility 41/3252, Reprisal 142/81, Hunt 389/7, Phoenix 87/1643 and Giant
79/3788. They show normal acquisition/equipment and later combat/path/RNG
variation. Hammer loss seed 826 dies while traveling to the artifact before
acquisition; equipment preference can change exposure as well as combat.
Phoenix seed 1071 additionally demonstrates a single consumption, full HP
restoration and eventual escape against the Speed replacement's death.
No recursive resurrection, stuck run or simulator error was observed.
Aggregate improvements do not imply that every individual seed or human choice
wins: replayed losses remain in the reported totals and no RNG is resynchronized.

Evidence: [paired prefix results](../build/artifacts/boost-ten-10k/results.json),
[trace review](../build/artifacts/boost-ten-10k/trace-review.json),
[Phoenix revival trace](../build/artifacts/boost-ten-10k/trace-phoenix-revival-natural-1071.txt).

## Reproduction and files

Use existing CMake generators/compilers and matching Windows developer setup.
All configurations are RelWithDebInfo. Manifests retain exact commands, policy
identity, executable/stream hashes and experiment specifications. Raw data,
profiles, entry states and representative discordant traces remain in ignored
`build/artifacts/`. Interrupted earlier runs are obsolete and excluded.

```text
cmake --build build/bow-native --config RelWithDebInfo --parallel
ctest --test-dir build/bow-native -C RelWithDebInfo --output-on-failure --parallel 8
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check
build/bow-native/sim/ardurogue2_sim.exe --all-seeds --jobs 6 --entry-state --experiment strengthened-artifacts --variant natural --output build/artifacts/natural
```

For controls append the corresponding intervention from
[sim/BALANCE.md](../sim/BALANCE.md). For ordinary generation append
`--intervention ordinary-items`. `boost_runs.py`, `boost_analyze_incremental.py`, `boost_prefix_audit.py` and the
compatibility/trace audit scripts retain the executed orchestration under build.

Changed files, relative to this project:

- Production: `src/model.hpp`, `src/state.cpp`, `src/combat.cpp`, `src/items.cpp`,
  `src/game_internal.hpp`, `src/world_gen.hpp`, `src/world_gen_population.cpp`,
  `src/status.cpp`, `src/ui.cpp`, `src/sim_hooks.hpp`.
- Simulator: `sim/experiment.cpp`, `sim/experiment.hpp`, `sim/metrics.cpp`,
  `sim/omniscient_agent.cpp`, `sim/policy_checks.cpp`, `sim/tests.cpp`.
- Tests: `tests/artifact_checks.cpp`, `tests/CMakeLists.txt`,
  `tests/bench_checks.cpp`, `tests/bow_checks.cpp`, `tests/format_native.cpp`,
  `tests/game_native.cpp`, `tests/weapon_checks.cpp`.
- Benchmarks: `bench/bench.cpp`, `bench/profile_turns.py`, `bench/test_profile_turns.py`.
- Documentation: `AGENTS.md`, `sim/BALANCE.md`, `docs/ARTIFACTS.md`,
  `docs/ARTIFACTS_INITIAL_2026-10-10.md`.
- Rebuilt game: `ardurogue2.arduboy`.
