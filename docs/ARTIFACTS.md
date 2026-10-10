# Ten rare artifacts: final rarity tuning

Updated 2026-10-10 in the uncommitted working tree based on
`e9762262c2be3c7389b0e3d343e917e5ab5f216b`. All ten strengthened artifacts,
including the reclassified Ring of Invisibility, retain their benefits and cursed
drawbacks. Only their independent run-wide selection rate changes from 1/8 to
**1/128**, making each type sixteen times rarer. The packaged game is rebuilt.
No commits or pushes were made.

The [initial report](ARTIFACTS_INITIAL_2026-10-10.md) retains the original nine
artifacts. The [strengthening report](ARTIFACTS_BOOSTED_2026-10-10.md) preserves
the prior 1/8 implementation, individual capability comparisons and full-seed
balance evidence. Its availability measurements are historical.

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

All ten types independently have a 1/128 run-wide selection probability
(0.78125%, sixteen times rarer) and one
deterministic floor index in 4–14, derived from run seed/type in the dedicated
artifact domain. Each appears at most once during normal play. Post-processing
replaces distinct accessible ordinary ground slots, prefers non-Food/non-Arrows
placements, skips ascent and preserves the Yendor reservation. It consumes
neither gameplay RNG nor ordinary generation draws.

Invisibility no longer belongs to the ordinary ring roster. Its former 2%
ordinary subtype outcome now yields protection with the same instance generation
and RNG draws; the other ordinary subtype thresholds remain unchanged. Its
unique artifact placement follows the same 1/128 schedule as the other types.
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

Native Clang 22.1.3, C++17, RelWithDebInfo: **15/15 CTests passed**, 33.65 seconds.
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
identification, ascent, Yendor and saved-layout checks pass. The four descent
floor snapshots changed as expected with reduced spawning;
ascent snapshots remain identical. Frequency checks use sample-size-aware
binomial bounds because the rarer spawns produce smaller item-state samples.

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

Evidence: [native checks](../build/artifacts/rare-artifacts-native-tests-final.log),
[AVM build and stack](../build/artifacts/rare-artifacts-avm-build.log),
[42-case final benchmark report](../build/artifacts/rare-artifacts-turn-benchmarks/20261010T194321Z-oebh8rzo/summary.md).

## Rarity verification

Per the user's request, final balance verification uses **1..10,000** matched
effective seeds only. One candidate executable runs both natural artifacts and
the `ordinary-items` control, which disables only artifact post-processing.
Both use maintained `omniscient-v2`, eight workers and the unchanged policy hash
`0adcf5360d3783f385a7f67054143e99cbf07b52c35b3440a7a680eeb41df28b`.
The candidate executable SHA-256 is
`c6410dfefa89e9aae59126402d7eb9ecd4d158b070eced618b147ed48ef5cec3`.
Manifests seal the experiment, executable and telemetry hashes.

| Variant | Escapes / runs | Escape rate |
| --- | ---: | ---: |
| Ordinary equipment | 4,066 / 10,000 | 40.66% |
| Strong artifacts at 1/128 | 4,200 / 10,000 | 42.00% |
| Prior strong artifacts at 1/8, historical | 5,951 / 10,000 | 59.51% |

The current **+1.34 percentage-point** increase meets the requested 1–2-point
target. There are 141 gained escapes and seven lost; zero simulator failures.
The paired bootstrap 95% interval is +1.11 to +1.59 points, conditional on the
deterministic seed prefix representing the wider population. Floor-15 reach
increases 0.69 points, post-Yendor deaths decrease 0.50 points among all runs,
and mean damage taken decreases 4.375 HP per run. Secondary metrics are descriptive.

At least one artifact is scheduled in 765/10,000 runs (7.65%); 594 runs (5.94%)
actually visit a floor generating one. Every generated type remains unique per
run. Selection, floor assignment and item state use the same dedicated artifact
streams. Floors 4–14, 1/8 curses, enchantments, effects, equipment preferences,
ordinary generation and gameplay RNG are unchanged.

| Artifact | Selected | Generated | Reached | Picked up | Equipped runs |
| --- | ---: | ---: | ---: | ---: | ---: |
| Stormbringer | 87 | 73 | 62 | 62 | 62 |
| Glass Sword | 67 | 54 | 48 | 46 | 46 |
| Hammer of Ruin | 89 | 61 | 53 | 52 | 52 |
| Dragonhide | 75 | 50 | 39 | 38 | 38 |
| Titan Plate | 83 | 60 | 57 | 54 | 54 |
| Ring of Invisibility | 90 | 72 | 60 | 58 | 58 |
| Ring of Reprisal | 67 | 58 | 48 | 47 | 47 |
| Ring of the Hunt | 92 | 74 | 65 | 65 | 65 |
| Phoenix Heart | 71 | 56 | 46 | 46 | 46 |
| Heart of the Giant | 69 | 54 | 49 | 48 | 48 |

Glass shatters 30 times and Phoenix resurrects seven times. These observational
counts are not causal rankings of artifact capability. Individual capability
balance was already verified at the former spawn rate and is retained in the
historical strengthening report; rarity tuning does not weaken any artifact.

The ordinary control matches the previous 10,000-seed ordinary reference byte
for byte in all nine CSV streams, including action hashes, entry states and the
empty intervention ledger. Representative gain seed 35 and loss seed 1024 were
replayed in both variants; losses remain in the aggregate and RNG is never
resynchronized after divergence.

Evidence: [paired comparison](../build/artifacts/rare-artifacts-10k/ordinary-comparison/compare.md),
[results](../build/artifacts/rare-artifacts-10k/results.json),
[availability, compatibility and trace audit](../build/artifacts/rare-artifacts-10k/validation-metrics.json).

## Reproduction

Use existing generators/compilers, matching Windows developer setup, and
RelWithDebInfo for every build. Raw data, manifests, profiles and traces remain
in ignored `build/artifacts/`. `rarity_runs.py`, `rarity_audit.py` and
`boost_analyze_incremental.py` retain the executed orchestration there.

```text
cmake --build build/bow-native --config RelWithDebInfo --parallel
ctest --test-dir build/bow-native -C RelWithDebInfo --output-on-failure --parallel 8
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check
build/bow-native/sim/ardurogue2_sim.exe --seeds 1:10000 --jobs 8 --entry-state --experiment rare-artifacts --variant natural --output build/artifacts/natural
```

For the ordinary control append `--intervention ordinary-items` and use a fresh
output folder. The rarity follow-up changes `src/world_gen.hpp`, the three
generation/distribution test files, this report and the rebuilt
`ardurogue2.arduboy`. The full implementation's changed-file inventory remains in
the historical strengthening report.
