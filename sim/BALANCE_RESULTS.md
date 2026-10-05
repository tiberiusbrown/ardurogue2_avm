# Balance framework milestone: frozen-v2 demonstrations

Validated 2026-10-05 on Windows, Clang 22.1.3, RelWithDebInfo. This milestone
adds host telemetry, statistics and controlled experiments. It makes **no
production balance changes** and leaves `omniscient-v2`, `Game`, persistence and
the ordinary AVM package unchanged. All changes are uncommitted.

## Architecture and files

`sim/metrics.*` collects typed sparse visits by taking differences of the existing
event counters at floor boundaries; per-visit host arrays add no saved state.
Entry state uses production accessors/definitions; generation diagnostics are
existing host observations. `csv_streams` is the shared seven-stream descriptor
table for writing and parallel merging. `main.cpp` adds all-seed selection,
duplicate effective-seed rejection, manifests and intervention options.

`experiment.hpp/.cpp` provides a host-only virtual extension interface and four
deterministic slot-order operations. `src/world_gen.cpp` calls its guarded hook
after population and before `FloorEntered`; `src/sim_hooks.hpp` declares it only
for simulator builds. `simulator.*` attaches experiment options to the collector.
`manifest.hpp/.cpp`, `build_metadata.hpp.in` and CMake record provenance.

`balance.py` implements summarize/factors/compare/ab; the optional
`requirements-balance.txt` names NumPy, pandas, SciPy and statsmodels.
`test_balance.py`, `test_balance_cli.py`, expanded `test_cli.py` and `tests.cpp`
cover inference, pairing, telemetry and interventions. `AGENTS.md` adds the
required balance workflow; `README.md` links [BALANCE.md](BALANCE.md), which
contains schemas, exact methods, reproducible commands and agent compatibility
controls. `.gitignore` excludes Python cache files as well as existing build data.

Telemetry schema is **2**, manifest schema **1**. Full schemas and the exact
covariate/model/CI definitions are in [BALANCE.md](BALANCE.md). All final CSVs,
reports, manifests, hashes, archived executable and traces remain under `build/`.
The manifest records the configure-time git SHA
`a56e033f7d87b40bb0809425c66991783a7b975d`, dirty state, agent, seed population,
experiment/variant, compiler/build/options, and ordered rules. Python additionally
records executable/CSV/specification SHA-256, dependency versions and an identity
hash excluding paths, wall time, job count and command spelling.

## Frozen baseline: seeds 1..10000

Final raw run: [balance-baseline-final](../build/balance-baseline-final/manifest.json).
Full exploratory report: [factors.md](../build/balance-baseline-final/factors/factors.md),
[factors.csv](../build/balance-baseline-final/factors/factors.csv),
[direct burden](../build/balance-baseline-final/factors/monster_burden.csv),
[selection associations](../build/balance-baseline-final/factors/selection_associations.csv).

| Measurement | Result |
| --- | ---: |
| Escaped | 5,293 / 10,000 (52.93%) |
| Deaths | 4,707 |
| Simulator failures | 0 |
| Reached floor 12 / 15 | 7,078 / 5,989 |
| Lord killed / Yendor acquired | 5,582 / 5,582 |
| Post-Yendor deaths | 289 |
| Entered visits | 196,225 |
| Eligible descent visits | 116,191 |
| Item modeled visits after support restriction | 109,076 |
| Deterministic CSV data | 322,371,719 bytes |

All existing frozen-reference outcomes, action hashes and telemetry counters
match exactly, including byte-identical `runs.csv` SHA-256
`a784d0663a4bc34bd3610a1185da6e25b61f2dd3b16b21d42cffaca0dfef8c0e`.
Seven-stream serial/parallel byte equality holds over the entire 10k population.
Every event-based typed visit counter and floor activity/count reconciles.

### Observational item availability findings

**Adjusted observational association is not proof of causation. Controlled A/B
interventions are preferred for causal balance conclusions.** These are
percentage-point differences in **conditional modeled visit survival**, not
changes in whole-run win rate. BH FDR spans the 71 successfully estimated
adjusted item/monster/floor hypotheses; eight other factors are explicitly flagged.

| Generated presence | Adjusted difference, pp | 95% difference CI, pp | BH q |
| --- | ---: | --- | ---: |
| RING_INVISIBILITY | +0.985 | +0.570 to +1.400 | 0.000325 |
| WAND_STRIKING | +0.871 | +0.581 to +1.160 | 0.00000101 |
| EXPERIENCE | +0.810 | +0.595 to +1.025 | 0.0000000000454 |
| HEALING | +0.713 | +0.497 to +0.930 | 0.00000000901 |
| WAND_ICE | +0.665 | +0.343 to +0.987 | 0.00134 |
| FOOD | -1.489 | -2.469 to -0.509 | 0.0638 |
| RING_DEXTERITY | -0.712 | -1.213 to -0.211 | 0.0210 |
| AMULET_CLARITY | -0.640 | -1.152 to -0.129 | 0.0470 |
| HARMING | -0.346 | -0.568 to -0.124 | 0.0148 |

Food has the largest negative estimated item association, but is **not notable
at q <= .05** and is generated on almost every eligible visit. Its absence has
limited support. These negative estimates do not establish harmful item mechanics:
fixed supply slots, instance quality, accessibility and unmeasured conditional
state can explain composition/selection effects. No constants were retuned.

### Monsters and dungeon geometry

PHANTOM is the strongest estimable negative presence association: **-1.269 pp**,
95% difference CI -1.876 to -0.662, OR 0.458 (95% 0.280–0.746), q=0.0138,
on 14,957 supported visits. ORC's estimate is -0.202 pp with a CI spanning zero
and q=0.364. GRIFFIN is positively associated (+1.149 pp, q=0.000325), but that
does not imply adding griffins helps: the fixed encounter slots can replace
other, more dangerous threats. Restricting exposure contrasts by floor/direction
prevents unrelated early absence from supplying a spurious late-game comparator.

DRAGON and ANGEL presence estimates are flagged for sparse outcome/exposure cells;
BAT, GOBLIN, RATTLESNAKE lack adequate unexposed support; SNAKE and LORD lack
estimable within-stratum presence variation. High total encounter counts do not
make their binary availability estimates reliable. Yendor is excluded because its
generation is a consequence of Lord defeat.

Direct descriptive burden identifies different priorities:

| Monster | Player damage | Deaths caused | Damage/engagement | Deaths/engagement |
| --- | ---: | ---: | ---: | ---: |
| DRAGON, including fire | 1,029,780 | 1,125 | 12.55 | 1.37% |
| INCUBUS | 558,327 | 75 | 4.37 | 0.059% |
| LORD | 502,738 | 355 | 84.38 | 5.96% |
| PHANTOM | 434,650 | 210 | 3.21 | 0.155% |
| SNAKE | 211,437 | 2,273 | 1.46 | 1.57% |

Dragon accounts for the most damage; snakes cause the most immediate deaths;
the Lord has the highest damage and death rate per engagement. These denominators
are behavior-selected and do not constitute adjusted causal content effects.

No floor/generation factor meets q <= .05. Loops associate with +0.155 pp per
SD (95% +0.026 to +0.284, q=0.0817). Archetype contrasts are small and uncertain:
WARREN vs CHAMBERS +0.317 pp (q=.409), FORTRESS -0.192 (q=.491), RUINS +0.072
(q=.756). There is no supported basis here for a geometry balance change.

### Where whole-run interpretation differs

Whole-run generated FOOD has Spearman r=+0.608 with escape and FOOD use r=+0.857,
while adjusted visit-generated presence is -1.489 pp and not FDR-notable. Whole-run
RING_DEXTERITY generation r=+0.315 and AMULET_CLARITY generation r=+0.296 oppose
their negative adjusted visit associations above. Longer survival supplies more
generation and use opportunities; neither the whole-run nor adjusted direction
is causal proof. The synthetic survivorship test independently constructs a
late monster with positive naïve whole-run association but a correctly negative
within-visit adjusted association.

## Controlled proof experiment: seeds 1..1000

[Experiment specification](../build/balance-proof-final/experiment.json),
[paired report](../build/balance-proof-final/comparison/compare.md),
[comparison JSON](../build/balance-proof-final/comparison/comparison.json),
[intervention ledger](../build/balance-proof-final/treatment/interventions.csv),
[discordant seeds](../build/balance-proof-final/comparison/discordant_seeds.csv).

The exact same archived executable and agent run in both variants. Control has
no intervention. Treatment applies:

```text
replace-item:HEALING:FOOD:floor=0:direction=descent:max=1
```

Default instance state is one food unit; each potion slot's position is retained.
The rule affects only floor 0 descent, before generated-content scanning, in
stable slot order, with no gameplay RNG consumption. **352 seeds receive exactly
one substitution**; the ledger reconciles with -352 initial healing units/+352
initial food units. All seven serial/parallel treatment CSVs match exactly over
1,000 seeds. No simulator failure occurs in either variant.

| Paired outcome | Result |
| --- | ---: |
| Control escaped | 541 / 1,000 (54.1%) |
| Treatment escaped | 524 / 1,000 (52.4%) |
| Absolute escape difference | -1.7 percentage points |
| Relative difference | -3.142% |
| Control escape / treatment death | 40 |
| Control death / treatment escape | 23 |
| Both escaped / both died | 501 / 436 |
| Deterministic paired bootstrap 95% CI | -3.3 to -0.2 pp |
| Exact two-sided McNemar/binomial p | 0.0429565 |

The estimate is the effect of this **specified replacement under omniscient-v2**,
including changed loot routing and later gameplay. It is a proof experiment,
not a request or recommendation to replace healing. Whole-run generated food
can decrease despite adding one initial food unit because shorter runs enter fewer
floors. The ledger isolates the intervention from those downstream changes.
This 1,000-seed population is sampled, not the all-seed census.

Secondary paired reach changes are floor 12 -2.5 pp and floor 15 -2.2 pp;
Lord/Yendor -2.3 pp. Mean depth changes -0.329, score -161.2, actions -122.1,
turns -121.4, damage taken -18.17, damage dealt -72.85 and consumables -2.359.
Paired median differences are zero; many seeds are untouched. Full secondary
intervals, conditional/unconditional survival curves, item/monster shifts and
immediate death shifts are in the linked report. Secondary inference is descriptive.

### Trace audit in both discordant directions

* **Seed 4, escape → snake death:** at A34 the control seeks healing at (26,7),
  while treatment seeks an amulet at (36,21). The first different mechanical
  action is A36 at 16/18 HP: west vs south. Treatment dies to a snake after
  108 actions on floor 0; control escapes after 6,416 actions. See
  [control trace](../build/balance-proof-final/traces/control-4.trace) and
  [treatment trace](../build/balance-proof-final/traces/treatment-4.trace).
* **Seed 221, snake death → escape:** the loot target changes at A34; the first
  different action is A42 at 14/18 HP: control goes north toward healing,
  treatment east toward food. Control meets additional snakes and dies after
  57 actions; treatment collects supplies on its different route and escapes
  after 5,490 actions. See
  [control trace](../build/balance-proof-final/traces/control-221.trace) and
  [treatment trace](../build/balance-proof-final/traces/treatment-221.trace).

These examples illustrate why matched starting content does not mean identical
future rolls/actions. No RNG resynchronization is attempted. Audit metadata and
trace context are in [audit.json](../build/balance-proof-final/traces/audit.json).

## Validation and performance

**13/13 CTest checks pass**: all nine ordinary native/generation/UI/rendering/
combat/visibility/benchmark-helper checks, simulator C++ checks, simulator CLI,
balance statistics and balance CLI. The Python synthetic suite has **12 passing
test methods**, covering helpful/harmful factors, BH's known example, sparse and
separated data, clustered seed covariance, survivorship reversal, identical pairs,
known discordance, ordering, incompatibilities/failures/duplicates, stable
bootstrap, full census and hash tampering.
Validation record: [balance-validation.md](../build/balance-validation.md).
Latest targeted Python rerun: [LastTest.log](../build/sim-clang/Testing/Temporary/LastTest.log).

C++ tests verify an exact no-op, forbidden experiment RNG mutation, intended
positions/default and compatible item encodings, monster health/status/Mimic
initialization, cap/removal/scoping, pre-scan telemetry and ledger reconciliation.
CLI checks exercise serial/parallel streams (including empty ledgers), treatment
metadata/reconciliation, seed-zero aliases, invalid rules/options and **exact
enumeration of all 65,535 unique nonzero effective seeds** using an intentionally
limited run. Census inference is separately tested synthetically. A full-seed
complete-game A/B was not needed for this unchanged-content framework milestone.

Ordinary AVM and benchmark builds pass. All **30/30 turn benchmarks** remain under
100 ms, worst **75.340 ms**. See
[benchmark summary](../build/balance-turn-benchmarks/20261005T190947Z-ep2wa9us/summary.md).
Optimized ordinary AVM IR is **byte-identical after path normalization** for state,
combat, items and world generation versus the frozen source revision, with no
simulator symbols: [zero-cost proof](../build/balance-zero-cost/proof.txt).
`Game` remains 774 native/773 AVM bytes and save version 23; model/persistence,
frozen policy sources and `ardurogue2.arduboy` have no diff.

The 10k seven-stream run took approximately **41.42 s with eight workers** and
**260.38 s serially**, compared with the prior four-stream 40.84/230.99 s. Times
are local file creation-to-close estimates, include output writing/merging, and
are not controlled performance benchmarks. Parallel throughput remains close
to the validated baseline; serial overhead is about 13% while adding typed visits.
See [hash/reconciliation/performance verification](../build/balance-verification.json).
Exact dependencies used: NumPy 2.5.3, pandas 3.0.6, SciPy 1.18.1, statsmodels 0.15.0.

## Limits and follow-up use

Do not trust the flagged sparse/structurally constant item/monster estimates.
Even estimable associations retain composition, position, instance-quality and
entry-selection confounding. Clustered standard errors fix within-run dependence,
not omitted-variable bias. BH correction addresses exploratory multiplicity under
its dependence assumptions, not causality. Fixed seed ranges need not be random
population samples. Bootstrap/Wald intervals have boundary/sparsity limitations;
secondary A/B intervals/tests are descriptive. Findings are policy-specific,
not measurements of human play. POSIX launching is not validated here.

Future substantial content changes should use the routine 10k paired comparison,
then full-seed census when practical, and controlled same-location substitutions
to isolate new content. New agent support must first pass an old-content disabled
compatibility control against frozen results/action hashes/relevant telemetry;
material old-content differences require a new agent version and reference.
The complete commands and updated workflow are in [BALANCE.md](BALANCE.md) and
[AGENTS.md](../AGENTS.md).
