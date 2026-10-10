# Nine rare artifacts: implementation and validation

Historical report retained before the requested strengthening and invisibility
reclassification. Current behavior and validation are in [ARTIFACTS.md](ARTIFACTS.md).
The original candidate executable and image are preserved as
`build/artifacts/artifacts-initial.exe` and `artifacts-initial.arduboy`.

Validated 2026-10-10 against the uncommitted working tree based on
`e9762262c2be3c7389b0e3d343e917e5ab5f216b`. All requested initial constants are
retained. The packaged `ardurogue2.arduboy` was rebuilt. No commits or pushes were
made.

## Implemented behavior

| Artifact | Normal effect | Cursed effect |
| --- | --- | --- |
| Stormbringer | Melee 4–8, accuracy +1; orthogonal 2–4 physical splash after primary damage; 1/8 chance of 1–2 self-damage | Same offense; life cost on every hit |
| Glass Sword | Melee 7–13, accuracy +4; shatters after damage on 1/256 successful hits | Shatters on 1/16 successful hits |
| Hammer of Ruin | Melee 4–8, accuracy −1; 1/4 chance to double post-armor damage and force surviving target away | Same heavy hits; independent 1/4 player recoil opposite attack |
| Dragonhide | Armor 5; fire immunity | Fire vulnerability, doubled once before magic saving throw |
| Titan Plate | Armor 9; effective speed cost +2 | Effective speed cost +4 |
| Ring of Reprisal | After damaging monster melee, 1/4 chance of immediate turn-free normal melee counterattack | 1/4 chance of one extra self-damage; no counterattack |
| Ring of the Hunt | Arrows gain +6 accuracy, +4 raw damage, with or without a bow | −6 accuracy, −4 raw damage, clamped to at least one |
| Phoenix Heart | Consumed on lethal damage; restores ceil(effective maximum HP / 4) and continues play | No resurrection |
| Heart of the Giant | Effective maximum HP +20 while equipped | Effective maximum HP −12 |

Weapon misses produce no artifact effects. Splash uses the primary target's
original position, applies armor independently, awards normal kill rewards and
does not recurse. Hammer and force wands share movement/collision helpers.
Reprisal follows the original melee status effects and precedes surviving
attacker regeneration/aging; it never starts another player turn. Primary damage
and splash/knockback precede vampire interaction and weapon life/break/recoil
costs. Player damage, including starvation and harming potions, shares lethal
resolution. Destruction clears the whole Item, equipment and repeat reference
before another effect can execute; it can consume cursed equipment.

Any positive equipped fire immunity overrides all vulnerabilities. Multiple
vulnerabilities double damage at most once. Titan speed uses bounded intermediate
arithmetic, combines with speed amulets/slowing and never changes `game.speed`.
Maximum HP remains clamped to 1–255; increasing it does not heal current HP.

## Generation, identification and saves

Each type independently receives a deterministic 1/8 selection draw and one
scheduled floor index in 4–14 from a dedicated run-seed/type domain. Artifact
post-processing replaces distinct accessible ordinary ground slots, preferring
non-Food/non-Arrows slots. It consumes neither gameplay RNG nor ordinary
generation streams and skips ascent and the Yendor reservation. Existing stairs
permit only descent before Yendor and ascent afterward, so a scheduled floor
cannot regenerate an artifact during normal play. No discovery bitset is needed.

Weapon/armor enchantments retain the existing −2/−1/0/+1/+2 distribution
(5/10/70/10/5%) and independent 1/8 curse draw. Jewelry has a 1/8 curse draw.
Named constants in `world_gen.hpp` expose the initial rarity/floor/curse settings.
Artifact jewelry retains unidentified appearances; ten distinct jewelry
descriptors support both expanded groups. Names and messages use flash storage
and existing group icons.

`Item` remains two bytes. The saved AVM `Game` remains 773 bytes, with all field
offset assertions unchanged (native size is 774 due to tail padding). The
six-byte identification bitset holds 46 types and has a capacity assertion.
`SAVE_VERSION` is 25; the existing version check rejects earlier saves whose item
IDs shifted. There are no new persistent fields or AVM simulator hooks.

## Correctness results

Native Clang 22.1.3, C++17, RelWithDebInfo: **15/15 CTests passed**, 12.51 seconds.
Coverage includes game correctness, artifacts, bulk generation, inventory/UI,
formatting/rendering, combat distributions, visibility, benchmark fixtures,
simulator/CLI and balance analysis/scorecard tests. Simulator CLI checks include
serial/parallel byte equality for eight seeds and all telemetry streams.

The new artifact suite checks normal/cursed mechanics, destruction, effect
ordering, rewards, force collisions, fire precedence, speed/HP boundaries,
resurrection sources and repeat deaths, arrow modifiers and unaffected
wand/potion paths. It uses all 65,535 nonzero combat RNG states for probability
checks. Observed normal Stormbringer cost was 12.509%; Glass break rates were
0.391% normal and 6.244% cursed; Hammer heavy/recoil rates were 25.004%/24.949%.
Reprisal damaging counters occurred on 23.472% of damaging incoming hits (the
counterattack can miss); cursed extra damage occurred on 24.974%.

Scheduling and equipment distributions were checked across all 65,535 effective
seeds. Per-type selection rates were 12.39–12.88%. Actual floor generation for
2,048 seeds across all eleven eligible floors checked deterministic placement,
accessibility, collisions, uniqueness and unchanged ordinary slots/streams.
Yendor, ascent, appearance permutations, identification bounds and save layout
also passed.

Evidence: [native test log](../build/artifacts/native-tests-accepted.log),
[artifact statistical log](../build/artifacts/artifact-statistics.log).

## AVM image, stack and turns

AVM production and benchmark builds both passed with a complete stack bound of
**249/256 bytes**, zero analysis gaps. The clean reference bound was 244 bytes.
Remaining stack headroom is seven bytes; further nested combat effects need care.

| Size | Reference | Candidate | Increase |
| --- | ---: | ---: | ---: |
| Packaged `.arduboy` | 159,068 B | 164,188 B | 5,120 B (+3.22%) |
| `fxdata.bin` | 71,168 B | 76,288 B | 5,120 B (+7.19%) |
| `interp.hex` | 83,157 B | 83,157 B | 0 |
| `fxsave.bin` | 4,096 B | 4,096 B | 0 |

**42/42 final-ELF turn benchmarks passed the 100 ms limit.** Timing includes
submitted input, computation and final rendering/display through the next input
wait, at emulated 16 MHz; generation and animation are excluded. The original
36 cases also passed on a clean reference build using the same SDK.

Worst case remains `arrow_kill`: **95.1625 ms**, versus **94.92494 ms** reference
(+0.23756 ms). The largest common-case increase was `move_dense`, +1.77463 ms
(70.68675 → 72.46138 ms). The added cases measured Storm splash 57.421 ms, Glass
break 55.133 ms, Hammer heavy/knockback 58.645 ms, Reprisal 57.930 ms, Phoenix
54.502 ms and Titan speed 43.309 ms. A separate Hammer run also verified the
expected target position after knockback.

Evidence: [AVM build/stack log](../build/artifacts/avm-build-final.log),
[final 42-case report](../build/artifacts/turn-benchmarks-verified/20261010T180129Z-0_wpi793/summary.md),
[clean 36-case reference](../build/artifacts/turn-benchmarks-baseline/20261010T180357Z-v1zrxnl0/summary.md),
[Hammer position check](../build/artifacts/hammer-verified/20261010T180805Z-3n17libz/summary.md).

## Controlled balance results

All content comparisons use the same candidate executable and maintained
`omniscient-v2` policy hash
`0d709d5a3daea34204971b10a84b0ae4ee5db07c93cafce2b4aca9408fbcf040`.
The policy understands artifact equipment, speed/fire effects, ranged bonuses,
Glass fallbacks and Hunt/ammunition complementarity. Cursed equipment remains
excluded by its existing policy; native tests provide the cursed evidence.

With `ordinary-items`, a 10,000-seed compatibility audit matched the pre-extension
reference exactly across outcomes, action hashes and every old-content telemetry
stream after mapping shifted numeric item IDs and omitting zero-valued new rows:
10,000 runs, 195,965 floors, 580,000 item rows, 160,000 monster rows, 2,508,458
visit-item rows, 645,708 visit-monster rows and 10,000 ranged rows. Both escaped
4,430 times, with zero simulator failures. The separate compatibility audit
does not relax official same-policy comparison checks.

The final census covered **all 65,535 unique nonzero effective seeds**:

| Variant | Escapes | Escape rate | Simulator failures |
| --- | ---: | ---: | ---: |
| Ordinary generated items, artifact pass disabled | 28,844 | 44.013% | 0 |
| All nine artifacts replaced by preserved-state mundane comparators | 28,741 | 43.856% | 0 |
| Natural artifacts | 29,311 | 44.726% | 0 |

Natural replacement availability raises escape rate **0.713 percentage points**
over ordinary generation (3,115 gained escapes, 2,648 lost). Capabilities raise
it **0.870 points** over same-location mundane comparators (2,933 gained, 2,363
lost). This is an exact finite seed census, so sampling p-values/intervals are
omitted. No starvation deaths occurred. Relative to ordinary generation,
floor-15 reach increased 0.891 points, mean damage taken fell 4.931 HP/run and
damage dealt increased 27.52 HP/run. Dragon-fire deaths fell by 418; Stormbringer
life costs caused 159 deaths. Immediate death cause is not the complete causal
chain.

### Scheduling, discovery and use

Counts are unique runs in the natural census; generated means a scheduled floor
was actually visited, reached means the player stepped onto the object's tile,
and equipped ignores repeated bow/melee switches. A death before the scheduled
floor explains selection without generation. Every generated type occurred at
most once per run.

| Artifact | Selected | Generated | Reached | Picked up | Equipped |
| --- | ---: | ---: | ---: | ---: | ---: |
| Stormbringer | 8,185 | 6,241 | 5,166 | 4,949 | 4,949 |
| Glass Sword | 8,216 | 6,265 | 5,503 | 5,347 | 5,347 |
| Hammer of Ruin | 8,234 | 6,222 | 5,247 | 5,027 | 5,026 |
| Dragonhide | 8,439 | 6,396 | 5,456 | 5,268 | 5,268 |
| Titan Plate | 8,228 | 6,218 | 4,062 | 3,599 | 3,599 |
| Reprisal | 8,249 | 6,281 | 5,189 | 4,978 | 4,978 |
| Hunt | 8,241 | 6,298 | 4,860 | 4,581 | 4,581 |
| Phoenix Heart | 8,146 | 6,200 | 4,355 | 3,954 | 3,953 |
| Heart of the Giant | 8,120 | 6,149 | 4,209 | 3,784 | 3,784 |

Glass shattered 3,602 times and Phoenix resurrected 2,330 times. These are usage
measurements, not causal rankings of artifacts acquired by surviving players.

### Focused tradeoffs and remaining concerns

Three additional 1..10,000 paired experiments replaced only the named artifact,
keeping the others natural and preserving location/enchantment/curse. Natural
escape was 45.25% for this exact prefix of the census.

| Natural artifact versus comparator | Escape delta (pp) | Paired 95% CI (pp) | BH-adjusted escape q |
| --- | ---: | --- | ---: |
| Glass Sword versus Two-handed sword | +1.47 | +1.220 to +1.730 | 2.31e−31 |
| Titan Plate versus Plate mail | −0.60 | −0.800 to −0.400 | 1.67e−9 |
| Phoenix Heart versus Vitality | −0.50 | −0.690 to −0.320 | 1.14e−7 |

These specified substitutions include the policy's choices and later RNG
divergence. The finite prefix is not a random sample; inferential results assume
representativeness. Secondary outcomes remain descriptive. Glass is deliberately
strong. Titan's speed cost and Phoenix's single-use protection, equipment
opportunity cost and policy valuation warrant future tuning review; neither is
a universal upgrade under this policy. Keep the requested initial mechanics
pending such review.

Discordant traces were reviewed in both directions for all three controls:
Titan seeds 81/2193, Glass 1172/46 and Phoenix 687/129. They show actual equipment
use and subsequent combat/path divergence, with no simulator failure or repeated
resurrection. Phoenix seed 687 consumes its Heart, continues, then dies to a
later dragon-fire hit; seed 129 escapes without consuming it. Overall ordinary
versus natural traces also reviewed seeds 7/21/46/120. Do not resynchronize RNG
after divergence or assign the whole run's outcome to one effect from a trace.

The final 10k scorecard flags five descent mortality targets: floors 2, 14 and 15
HIGH; floors 5 and 9 LOW. The broader progression bands and equipment policy
remain balance concerns, not correctness failures. The aggregate census benefit
is modest and the individual costs are intentional; no unrelated progression
constants were changed.

Evidence: [compatibility audit](../build/artifacts/compatibility-audit.json),
[availability comparison](../build/artifacts/availability-comparison/compare.md),
[capability comparison](../build/artifacts/capability-comparison/compare.md),
[focused results and trace review](../build/artifacts/focused-review.json),
[final scorecard](../build/artifacts/natural-final-sample/report/summary.md),
[population/usage metrics](../build/artifacts/validation-metrics.json).
Raw CSVs, experiment specifications, executable/stream hashes, entry-state hashes,
profiles and trace logs remain in ignored `build/artifacts/`. The 10k natural
dataset is an exact ordered CSV prefix of the census with derivation provenance.

## Reproduction

Use the existing CMake generator/compiler and matching developer environment.
All builds below use RelWithDebInfo. `python` denotes the balance virtual
environment for analysis. The installed SDK is the AVM workspace's
`build/avm-sdk-install`.

```text
cmake --build build/bow-native --config RelWithDebInfo --parallel
ctest --test-dir build/bow-native -C RelWithDebInfo --output-on-failure --parallel 8
cmake --build build --config RelWithDebInfo --target ardurogue2 ardurogue2_bench
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check --output build/artifacts/turn-benchmarks
build/bow-native/sim/ardurogue2_sim.exe --all-seeds --jobs 8 --entry-state --experiment artifacts-final --variant natural-all --output build/artifacts/natural-all
build/bow-native/sim/ardurogue2_sim.exe --all-seeds --jobs 8 --entry-state --experiment artifacts-final --variant ordinary-all --output build/artifacts/ordinary-all --intervention ordinary-items
python sim/balance.py compare build/artifacts/ordinary-all build/artifacts/natural-all --output build/artifacts/availability-comparison
```

For same-location comparator runs, append the nine `replace-item` rules in
[sim/BALANCE.md](../sim/BALANCE.md). Focused controls append only their respective
rule and use `--seeds 1:10000`. Manifests contain the exact executed commands;
retained `seal.py`, `subset.py`, `seal_focused.py`, `focused_review.py` and
`report_metrics.py` record hashing, sample derivation and supplemental audits.

## Changed files

All paths below are relative to the ArduRogue 2 project root.

- Production: `src/model.hpp`, `src/state.cpp`, `src/combat.cpp`, `src/items.cpp`,
  `src/game_internal.hpp`, `src/world_gen.hpp`, `src/world_gen_population.cpp`,
  `src/status.cpp`, `src/sim_hooks.hpp`.
- Simulator: `sim/experiment.cpp`, `sim/experiment.hpp`, `sim/metrics.cpp`,
  `sim/omniscient_agent.cpp`, `sim/policy_checks.cpp`, `sim/tests.cpp`.
- Native tests: `tests/artifact_checks.cpp` (new), `tests/CMakeLists.txt`,
  `tests/bench_checks.cpp`, `tests/bow_checks.cpp`, `tests/format_native.cpp`,
  `tests/game_native.cpp`, `tests/weapon_checks.cpp`.
- Turn benchmarks: `bench/bench.cpp`, `bench/profile_turns.py`,
  `bench/test_profile_turns.py`.
- Documentation: `sim/BALANCE.md`, `docs/ARTIFACTS.md` (new).
- Rebuilt image: `ardurogue2.arduboy`.
