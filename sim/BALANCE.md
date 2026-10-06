# Balance experiments

The native simulator executes production mechanics with maintained `omniscient-v2`.
Python performs analysis and experiment orchestration. No production balance or
agent-policy change is part of this framework. See [README.md](README.md) for
simulator mechanics and [BALANCE_RESULTS.md](BALANCE_RESULTS.md) for the milestone
demonstrations. Raw outputs belong in ignored `build/`.

## Build and dependencies

Run from the ArduRogue 2 repository root. Python dependencies are optional for
the C++ game/simulator. Use Python 3.10+ and a virtual environment:

```sh
python -m venv build/balance-venv
# Activate first: Windows .\build\balance-venv\Scripts\Activate.ps1
#                 POSIX source build/balance-venv/bin/activate
# Windows: build/balance-venv/Scripts/python.exe
# POSIX:   build/balance-venv/bin/python
python -m pip install -r sim/requirements-balance.txt
cmake -S . -B build/sim-native -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/sim-native --config RelWithDebInfo --parallel
ctest --test-dir build/sim-native -C RelWithDebInfo --output-on-failure
python sim/test_balance.py
python sim/test_balance_cli.py build/sim-native/sim/ardurogue2_sim
```

Activate the environment or replace `python` with its full path for every analysis
command, including installation. Append `.exe` on Windows; Visual Studio builds
may place executables under `sim/RelWithDebInfo/`. Keep an existing generator and
compiler when reconfiguring. For Ninja/MSVC, use its matching developer shell.
Optional CTest integration: configure `-DBALANCE_PYTHON=/absolute/path/to/venv/python`
to register `balance_statistics` and `balance_cli`. Without those packages,
ordinary native tests remain available.

## Human-readable 10k scorecard

After installing Python 3.10+, CMake and a native C++ compiler, install the
analysis dependencies once (or use the balance virtual environment above):

```sh
python -m pip install -r sim/requirements-balance.txt
```

Then run this from the repository root:

```sh
python sim/check_balance.py
```

The script uses `build/balance-venv` automatically when the invoking Python
lacks the analysis dependencies. It configures and rebuilds the **current
production simulator**, then runs exactly seeds **1..10000**, with eight worker
processes. Windows Visual Studio's developer environment is loaded automatically;
its bundled Clang and Ninja are used when both are available. The default native
build is `build/balance-check-native`, independent of the AVM SDK/build.

It prints the full report and saves `report/summary.md`, `summary.txt`,
`summary.json`, `floors.csv`, `death_causes.csv`, `items_per_run.csv` and
`monsters_per_run.csv` under a fresh timestamped `build/balance-check-runs/`
directory. Raw telemetry and hashed provenance are retained there.

The scorecard covers outcomes, all 16 descent and 15 ascent floors, floor-0 death
concentration, starvation, entry HP/max HP/level/stats/armor, food/healing/control
and usable wand reserves, heavy weapon/Plate occupancy, worn Invisibility/Speed
accessories, effective invisibility/speed, typed item activity and monster burden.
Floor CSVs include means, medians, reach/survival, Wilson mortality intervals,
actions, turns, damage and consumables. The additional host-only `--entry-state`
sidecar has its own schema version 1; schema 3 adds ranged counters and three item IDs; the entry sidecar remains version 1. Old runs without this sidecar must be rerun for a full
scorecard. Extra probes used by the detailed rebalance audit (such as level-up
healing attribution) are outside this script's telemetry.

Descent mortality uses the requested broad design bands, inclusive at both ends:

| Floors | Mortality per entered visit |
| --- | --- |
| 0 | 8–15% |
| 1–4 | 1–3% |
| 5–8 | 1–2.5% |
| 9–11 | 1.5–3% |
| 12 | 2–4% |
| 13 | 3–6% |
| 14 | 4–7% |
| 15 | 5–9% |

Each descent floor is marked **LOW**, **HIGH**, **IN BAND**, or **NO DATA** using
the unrounded point estimate. The 95% Wilson intervals provide sampling context;
bands are design goals, not statistical acceptance tests. Ascent is marked
**REVIEW** because no numerical target was specified. Overall escape, resources
and equipment have no invented bands. Floor 0 accounting for at least half of
all deaths is a separate review flag, and starvation deaths are reported.
Invalid/stuck/error runs invalidate the report rather than counting as deaths.

Optional commands:

```sh
# Use a different configured native build; it is rebuilt before running.
python sim/check_balance.py --build-dir build/sim-native
# Fresh named output, four workers, and a paired comparison with a prior 10k run.
python sim/check_balance.py --jobs 4 --output build/balance/check-next --baseline build/balance/check-previous
# Reprint a completed report without simulation.
python sim/check_balance.py --summarize-only build/balance/check-next
# CI-friendly status: exit 2 for out-of-band descent; exit 1 for invalid data/errors.
python sim/check_balance.py --strict-bands
```

Normal execution exits 0 even when design bands are missed. `--simulator EXE`
skips compilation when the caller has already built the executable; this option
requires ensuring the binary contains the intended current mechanics and supports
`--entry-state`. Output directories must be fresh; datasets are never overwritten.
`--baseline` uses the existing paired framework with matched effective seeds,
McNemar's test, a deterministic paired bootstrap and detailed comparison exports.

## Routine 10k and full-seed workflows

```sh
build/sim-native/sim/ardurogue2_sim --seeds 1:10000 --jobs 8 --output build/balance/reference-10k
python sim/balance.py summarize build/balance/reference-10k --simulator build/sim-native/sim/ardurogue2_sim
python sim/balance.py factors build/balance/reference-10k --output build/balance/reference-10k/factors

build/sim-native/sim/ardurogue2_sim --all-seeds --jobs 8 --output build/balance/reference-all
python sim/balance.py summarize build/balance/reference-all --simulator build/sim-native/sim/ardurogue2_sim
python sim/balance.py factors build/balance/reference-all
```

`--all-seeds` means exactly requested/effective seeds 1..65535. Requested 0 maps
to 44257 (`0xace1`), so it is excluded. Selections containing both requested 0
and 44257 are rejected. Analysis rejects duplicate effective seeds. Outputs
containing existing `runs.csv` cannot be overwritten; choose fresh directories.
`summarize` reconciles visit counters and floor counts against whole-run totals,
and seals raw CSVs with SHA-256 hashes. `--simulator` additionally records the
executable path/hash. `factors` defaults to `RUN/factors/`.

## Telemetry schema 3

All eight deterministic streams merge in requested-seed order and are identical
between serial and parallel execution. A descriptor table supplies headers,
writers and merging; header-only sparse streams are valid.

| Stream | Observation and fields |
| --- | --- |
| runs.csv | One run: requested/effective seed, agent, escaped/death or distinct simulator failure, actions/turns/score/depth, Yendor, immediate cause, action hash |
| floors.csv | Entered visit keyed by seed/effective_seed/agent/visit/floor/direction, exited flag and activity, HP/max HP/level, effective strength/dexterity/speed, hunger, armor rating/enchantment, food/healing units, weapon/armor type/enchantment, archetype, tile/features/corridors/loops/connections, 16 feature-family counts |
| items.csv | Whole-run typed item aggregates, including final carried units and distinct wand pickup/activation accounting |
| monsters.csv | Whole-run typed generated/encountered/engaged/killed, attacks/damage/hits/deaths/status aggregates |
| visit_items.csv | Sparse typed visit activity: generated/reached/picked_up/used/consumed/equipped/dropped/discarded, charges/drunk/thrown/read/equipped turns and wand identities |
| visit_monsters.csv | Sparse typed visit activity: generated/encountered/engaged/killed, player_attacks/damage_to_monster, monster_attacks/monster_hits/damage_to_player/deaths_caused, poison/confusion/paralysis/fire |
| interventions.csv | Sparse applied replacements/removals with visit, experiment, variant, operation, from/to type and count |

Generated item units retain production stack semantics; equipment/wands are
objects, not charges. Lord's Yendor drop remains a generation event, but is
outcome-selected and excluded from item-availability inference. `carried` is a
final-run snapshot and has no visit counterpart (visit field is zero); every
other event counter reconciles. Monster encounters/engagements retain existing
definitions, including identity reset after polymorph. Source of damage and
actual HP loss retain existing instrumentation. Schema 2 adds effective seeds
to typed/floor streams and names monster fields by damage direction; legacy
schema-less datasets must be rerun. Increment `TELEMETRY_SCHEMA_VERSION` for any
incompatible CSV semantics change.

Entry values use production accessors/item units and the exact effective-speed
budget in `end_turn`. Smaller speed budgets mean faster player turns. Generation
diagnostics are existing host observations and add no saved/AVM state. A visit
exits successfully when production changes floor or the player escapes; a death
does not exit. A simulator failure is never used as an outcome by analysis.

## Factor model and interpretation

**Adjusted observational association is not proof of causation. Controlled A/B
interventions are preferred for causal balance conclusions.**

Each candidate gets a separate statsmodels binomial GLM with logit link:

```text
visit_exited ~ intercept + C(floor:direction) + entry_hp/entry_max_hp
  + entry_level + entry_strength + entry_dexterity + entry_speed + entry_hunger
  + entry_armor_rating + entry_armor_enchant + entry_food_units + entry_healing_units
  + C(archetype) + floor_tiles + major_features + corridors + loops + open_connections
  + factor
```

Numeric controls are standardized; no arbitrary interactions are introduced.
Rank-revealing QR removes redundant columns without changing their span.
Covariance is sandwich/cluster-robust by **effective seed**, with statsmodels'
finite-cluster correction and normal Wald coefficient intervals. Visits from
one seed are never treated as independent runs. Reports contain odds ratios,
coefficient/OR 95% intervals, adjusted percentage-point differences and their
delta-method 95% intervals, raw p-values and BH q-values. Probability differences
average predictions toggling exposure across the modeled observations.

Primary item/monster factors are **generated presence > 0**. Ordinary item
analysis uses descent visits only, avoiding structural ascent zeroes. Only
floor/direction strata with exposure variation and both exit outcomes contribute
to a model. All-success/all-failure strata cannot yield finite nuisance
intercepts and provide no within-stratum contrast; both eligible and modeled
counts are reported. Generated monster absence on unrelated early floors is
never used to infer a late monster's benefit. Default minimum: 100 exposed and
100 unexposed visits, also after support restriction; at least 30 seed clusters,
and five exits/failures per exposure arm. Change `--min-exposure` deliberately.
Sparse cells, separation, full confounding and nonconvergence are flagged rather
than dressed up as authoritative estimates.

Floor counts compare mean to mean + one SD, omitting that variable from its own
adjustment controls. Archetypes compare each category to the alphabetically
first reference on their pairwise eligible population. Other geometry controls
are retained; collinearity can weaken interpretability. Feature-family counts
are emitted for further work, not all added as first-milestone hypotheses.

Benjamini-Hochberg FDR is applied jointly to all successfully estimated adjusted
item/monster/floor hypotheses. Default `--q-threshold .05` marks statistically
notable evidence. Rank by effect magnitude and exposure/sample size, not p-value
alone. Small effects remain small even at low q. Descriptive whole-run item
Spearman associations with escape (generated, picked_up, used, equipped,
turns_equipped) use a separate BH family and are explicitly selection-biased.
Healing use can mark an already struggling run. Longer survival creates more
opportunities to generate, find and equip content. No pickup/use/equip analysis
supports causal claims.

Outputs: `factors.csv`, `factors.md`, `monster_burden.csv`,
`selection_associations.csv`, `analysis.json`. Direct burden uses total encounters,
engagements, damage per engagement, hits per attack, deaths/statuses per engagement.
These are descriptive and policy-selected, even when useful to diagnose mechanisms.

## Paired two-binary A/B

Build baseline and candidate separately in independent checkouts/worktrees and
build directories. Keep the maintained agent identical. For example, from each
checkout run `cmake -S . -B build/sim-native -DCMAKE_BUILD_TYPE=RelWithDebInfo`,
then its CMake build command above. Preserve binaries and use absolute paths.
Python does not check out or compile revisions.

```sh
python sim/balance.py ab --control-exe /baseline/build/sim-native/sim/ardurogue2_sim \
  --treatment-exe /candidate/build/sim-native/sim/ardurogue2_sim \
  --seeds 1:10000 --jobs 8 --experiment content-change --output build/balance/content-change-10k

python sim/balance.py ab --control-exe /baseline/sim --treatment-exe /candidate/sim \
  --all-seeds --jobs 8 --experiment content-change --output build/balance/content-change-all

python sim/balance.py compare build/balance/old-run build/balance/new-run --output build/balance/comparison
```

The tool uses subprocess argv arrays, including paths with spaces. It retains
control/treatment raw outputs/manifests, `experiment.json`, and automatic
`comparison/compare.md`/JSON/tables. It refuses unequal seed sets, duplicates,
different agent/schema versions, differing action limits, missing telemetry,
unfinished/failed simulations, inconsistent visit keys and sealed hash mismatches.
There are deliberately no permissive overrides in this first version.

Primary escape difference: paired absolute percentage points and relative delta,
both discordant directions and both concordant outcomes. Samples use a two-sided
**exact McNemar test**, equivalently `scipy.stats.binomtest(gains, gains+losses,.5)`.
No discordance gives p=1. 95% intervals use **2,000 deterministic paired seed
bootstrap draws**, NumPy PRNG seed 20261005; `--bootstrap` changes the draw count.
Bootstrap resamples differences after pairing, never independent variant rows.

Exactly effective seeds 1..65535 is a deterministic population census: report
exact effects and omit sampling confidence intervals/p-values. For a fixed
sampled range, inferential generalization still assumes representativeness.
Secondary binary outcomes include reaching floor 12/15, Lord kill, Yendor and
post-Yendor death (among all runs, not only acquisitions). Continuous outcomes
include depth, score, actions, turns, damage and consumables: baseline/candidate
means/medians, paired mean/median differences and separate bootstrap intervals.
Secondary inference is descriptive, without a multiplicity claim.

`floor_survival.csv` separates unconditional reach/exit over every seed from
conditional exit over entered visits by floor/direction. `death_causes.csv`
highlights immediate cause shifts per 1,000 runs; it is not the full causal
chain. Item generation/pickup/use and monster generation/encounters/damage/deaths
are normalized per run, with monster per-engagement rates separately named.
`discordant_seeds.csv` and report examples identify the best trace-audit set.

## Controlled same-binary interventions

Production calls a host-only hook after normal population and before floor-entry
telemetry. It disappears completely without `ARDUROGUE2_SIM`. `Experiment::apply`
is the extension interface; `RuleExperiment` implements ordered slot substitutions
without any PRNG. Rules are repeatable CLI arguments:

```text
replace-item:FROM:TO[:floor=N][:direction=descent|ascent][:max=N][:info=default|preserve|BYTE]
remove-item:FROM[:floor=N][:direction=descent|ascent][:max=N]
replace-monster:FROM:TO[:floor=N][:direction=descent|ascent][:max=N]
remove-monster:FROM[:floor=N][:direction=descent|ascent][:max=N]
```

Names are enum names; potion enums use `POTION_` prefixes, for example
`replace-item:POTION_HEALING:FOOD`. The original potion spellings such as
`HEALING` remain accepted, and schema-2 CSV labels retain those original
spellings so existing datasets stay comparable. Floors are 0..15. Omitted scope matches every floor and
direction; omitted cap is unlimited within each visit. Rules execute in supplied
order and can compose; later rules see prior replacements. Removed item slots
retain their position but become empty. Replacements preserve positions.
Item `info=default` means unenchanted equipment, one supply/accessory unit, or
five normal wand charges. `preserve` is permitted only within the same encoding
group (weapon, armor, potion, scroll, food, ring, amulet, wand); modifiers,
curse/identification/enchantment are carried appropriately. `BYTE` is an explicit
raw instance encoding for expert experiments. Cross-group preservation is rejected.
Monster replacement uses production health and a clean status/state aggregate;
Mimics use the production appearance setter with fixed `MIMIC_SCROLL`. Caps count
replaced slots, while item generated telemetry counts units. Ledger counts slots.

Example proof (existing content, no production balance change):

```sh
python sim/balance.py ab --control-exe build/sim-native/sim/ardurogue2_sim \
  --treatment-exe build/sim-native/sim/ardurogue2_sim --seeds 1:1000 --jobs 8 \
  --experiment healing-food-proof \
  --treatment-intervention replace-item:HEALING:FOOD:floor=0:direction=descent:max=1 \
  --output build/balance/healing-food-proof
```

For a future `NEW_WEAPON` in a candidate binary, control may use
`--control-intervention replace-item:NEW_WEAPON:OLD_COMPARATOR:info=preserve`
and treatment may leave it unchanged. Or control leaves comparator slots intact
and treatment uses `replace-item:OLD_COMPARATOR:NEW_WEAPON:info=preserve`.
Cross-group designs should specify default/fixed instance state. These answer
replacement at identical locations, not pure addition. Addition requires a
distinct, explicitly designed custom experiment.

The hook verifies unchanged `game.random_state`. Custom experiments must never
call `roll()` or consume/synchronize gameplay RNG. If stochastic selection is
needed later, derive a separate host PRNG from effective seed, experiment ID,
floor and direction. The current operations use deterministic slot order only.
After player actions diverge, normal gameplay RNG consumption is allowed to
diverge. Do not resynchronize attack rolls or monster behavior.

## Discordant traces and agent compatibility

Replay each variant with its original binary and intervention options:

```sh
build/sim-native/sim/ardurogue2_sim --seed SEED --trace --experiment healing-food-proof --variant control
build/sim-native/sim/ardurogue2_sim --seed SEED --trace --experiment healing-food-proof --variant treatment \
  --intervention replace-item:HEALING:FOOD:floor=0:direction=descent:max=1
```

Inspect first differing action, current HP/resources, actual events and ledger.
Audit examples in both win/loss directions before changing balance constants.
Represent new mechanics competently in maintained `omniscient-v2` before
drawing balance conclusions. Extend it in place, then disable/replace new content
and compare old-content action hashes, outcomes and relevant telemetry against
the previous reference. Behaviorally unreachable support should match exactly.
Document necessary infrastructure differences. Establish a fresh reference with
the new automatic policy hash. Every paired content experiment must use identical
policy hashes; never attribute a different policy's gain to content.

## Reproducibility manifest

Every simulator output directory contains `manifest.json` schema 1:
telemetry_schema_version, git_sha/git_dirty (configure-time; unknown if unavailable),
agent and automatic agent_policy_hash, requested seed selection/effective count, experiment/variant, build type,
compiler, command/options, ordered intervention rules. Reconfigure before final
validation to refresh build provenance.

Python `ab` additionally records executable absolute path/SHA-256, hashes of all
eight CSVs, experiment specification and its SHA-256, dependency versions, and
deterministic result SHA-256. `summarize --simulator` can seal direct simulator
runs equivalently. Paths, timestamps, command spelling and job count are excluded
from result identity. Effective population, executable, behavior options,
experiment/variant/rules and CSV content determine it. Comparisons validate
manifests and verify hashes when present. Build hashes identify uncommitted
builds even when git_sha alone is insufficient.

## Remaining limitations

Models are exploratory and policy-specific. Residual entry/position/gear/status
confounding, correlated generated content, adaptive decisions and conditioning
on entry remain. Bootstrap/Wald intervals can be imperfect near boundaries;
separation is rejected, not solved with arbitrary regularization. Presence hides
quantity/instance quality. Fixed generation slots create composition confounding;
positive monster associations can reflect other threats displaced. Full-seed
effects are exact for this build, not claims about humans or future builds.
POSIX process launching is implemented but this
milestone's demonstrations run on Windows. Large raw tables consume disk/memory;
analysis uses bounded bootstrap batches, sparse visit rows and small per-run
host arrays. Paired comparison hashes typed visits without loading their unused
rows; factor/summarize analysis loads them. GLM fitting data is not streamed.


## Bow validation workflow

Start at 17 Food / 3 Ammo outcomes, bundles 3..5, Short Bow 2..5/+1/range 5
and Long Bow 3..6/0/range 6; obtain the initial 1..10000 measurement before
tuning. Validate agent correctness first, then availability/droughts, damage,
accuracy, subtype distinction, bundle size and subtype frequency. Only then
adjust unrelated progression with one measured change at a time. Use the same
effective seeds, executable and policy hash for causal bow substitution:

```text
--intervention replace-item:SHORT_BOW:SPEAR:info=preserve
--intervention replace-item:LONG_BOW:LONG_SWORD:info=preserve
--intervention replace-item:ARROWS:FOOD:info=1
```

The deterministic one-ration replacement reconstructs the original displaced
Food opportunity; it never copies ammo's raw info bits. Preserve bow enchantment
and curse. Review discordant traces in both directions. After policy extension,
run the no-bow control against the pre-extension reference as a compatibility
audit, then establish the maintained policy's new reference. Final validation
uses all 65535 unique effective seeds, ordinary scorecard, factors, controlled
bow A/B, native tests, serial/parallel byte equality, stream isolation and AVM
size/complete stack/100 ms turn benchmarks. Historical documents are retained
with their original data and agent identity.
