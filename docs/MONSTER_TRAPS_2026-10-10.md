# Monster trap interactions — 2026-10-10

All ten existing traps now resolve monster entry. Feature generation, player
trap tuning, artifacts (including the intentionally rare Ring of Invisibility),
save version 26 and the persistent layout are unchanged.

## Effects

Raw physical rolls pass through existing inherent monster armor absorption.
Fire, frost and magical backlash bypass physical armor. No player equipment or
magic resistance participates in monster damage.

| Trap | Monster effect on entry |
| --- | --- |
| Spike | Player's depth-tuned raw range: 2–4 on floors 0–1, otherwise 3–7; physical armor |
| Poison dart | Player's depth-tuned raw range: 1–2 on floors 0–1, otherwise 1–3; physical armor; surviving monster weakened with existing permanent semantics |
| Flame | Player's depth-tuned range: 3–5 below floor 4, otherwise 5–9; dragons immune |
| Frost | Player's depth-tuned range: 2–4 below floor 4, otherwise 3–6; surviving monster slowed 6–10 status ticks |
| Snare | Stun for 2–3 monster actions; preserve a longer existing stun |
| Teleport | Random legal same-floor destination; preserve HP/state/effects; no wand confusion or destination activation |
| Confusion | 5–8 four-bit status ticks; preserve longer confusion |
| Magic drain | 3–6 unarmored magical backlash damage |
| Summoning | Attempt 1–2 level-appropriate hostile monsters within two cells of the triggering monster |
| Rust | 2–4 physical corrosive damage, applying inherent armor; no armor mutation |

Slow/confusion preserve longer existing effects. Weakness uses `MON_WEAKENED`
with the existing permanent value 15. Status ticks and speed calculations retain
existing mechanics; the ordinary entered-tile action still ages statuses normally.
Summons respect the 16 monster slots, valid terrain, player occupancy and other
monsters, and carry `MON_TRAP_SUMMONED`. Capacity/placement failure limits spawns.

## Entry, identity and feedback

`move_monster()` returns successful movement; `advance_monster_action()` returns
a movement signal. `enemy_turn()` calls `activate_monster_trap()` after movement
scratch unwinds. Pursuit, wandering, confused and fleeing movements all share
this integration. The AI never inspects traps when selecting movement and never
avoids discovered traps. A stationary attack, stun wait, immobility, blocked move
or door opening performs no trap lookup. Traps remain active, including re-entry.

`force_monster_move()` finishes path/collision calculation before its caller
resolves the final landing trap. Wand rays use a local landing mask, processed
after ray scratch unwinds; Hammer melee similarly returns movement before trap
resolution. Intermediate knockback tiles do not activate. Existing wall/monster
collisions and their stun remain unchanged. No saved or session queue was added.

Generation, summoning, teleport arrivals and stationary polymorph do not activate
traps. Monster teleport reuses the existing legality check, with 100 random
attempts and a bounded floor scan fallback. It excludes walls, closed doors,
player, occupied/original positions, preferring non-stairs. If only stairs are
legal, they are allowed; if no alternative tile exists, placement safely fails.
Teleport does not consume another action or recursively trigger its destination.

A local live-slot mask includes monsters present at enemy-turn entry. Deaths
clear eligibility before forced-entry summoning can reuse slots; a freshly
summoned replacement never inherits a previous monster's budget. Dead monsters
do not regenerate, age effects or act again. Fire/splash deaths of future slots
are cleared before later summoning, with a dedicated regression covering this.

Monster trap damage calls `damage_monster(index, damage, false)`: no PlayerAttack,
new aggression, XP or kill score, and no HP underflow. Lord death still executes
`leave_yendor()` once, leaving the amulet at the death tile. Trap-summoned monsters
remain worth zero XP/score for every death source. Environmental death feedback
uses “dies in a trap!” rather than “You defeat …”.

The exact hidden encoding is unchanged: bit 0 means hidden. `can_see(tile)`
reveals a witnessed activation permanently, even when the triggering monster is
invisible. Offscreen activation retains hidden state and emits no message.
Monster-aware flash text names only detectable monsters; otherwise the visible
activation says “A trap activates!” All witnessed messages remain enabled and use
normal `[more]` paging. The maintained agent's filtered hidden-feature observation
and pre-activation decision restrictions remain intact. A census-exposed bow/wand
inventory valuation defect was repaired in the maintained policy for both variants;
monster navigation and hidden-feature observation policy were not changed.

## Host telemetry and controls

Telemetry schema **5** appends six visit/type counters to `features.csv`:
`monster_activated`, `monster_damage` (effective HP lost), `monster_killed`,
`monster_teleported` (successful relocations), `monster_status` (actual packed
status additions/extensions or longer stun), and `monster_spawned` (successful
monster-triggered summons). Original activation, player damage/death, spawn,
repeat-entry, displacement and feature-use counters retain their meanings.
Ordinary MonsterDamage/MonsterKilled events are emitted once; trap counters are
attributed subsets. Floor `damage_dealt` remains player-attributed damage, while
typed monster damage and kill totals include all environmental sources.

`runs.csv` appends final residual `xp` and `final_hunger`. XP should be interpreted
alongside final level, because level-ups spend accumulated XP. All telemetry and
experiment state remain host-only. Schema readers, exporters, CLI fixtures and
feature aggregation were updated; serial/parallel tests pass.

The host rule `no-monster-traps` disables only monster activation. It leaves
feature generation, player effects and all ordinary generation streams intact.
Both variants use the same frozen executable, policy and effective seeds; gameplay
RNG is not resynchronized after divergence. Supplementary metrics stream through
`sim/monster_trap_report.py`. Reproduction commands are in `sim/BALANCE.md`.

## Memory, stack and performance

| Production measurement | Tuned baseline | Final monster traps |
| --- | ---: | ---: |
| Saved Game | 809 bytes | 809 bytes |
| Ordinary data | 108 bytes | 108 bytes |
| MapFeature / slots | 3 bytes / 12 | 3 bytes / 12 |
| Monster | 8 bytes | 8 bytes |
| Save version | 26 | 26 |
| Text | 79,863 bytes | 83,129 bytes |
| Read-only data | 9,546 bytes | 9,782 bytes |
| Packaged image | 178,780 bytes | 182,108 bytes |
| Complete production stack bound | 236 bytes | 246 bytes |
| Stack analysis gaps | 0 | 0 |
| Original 58-workload worst response | 83.466 ms | 83.572 ms |
| Entire suite worst response | 83.466 ms / 58 cases | 97.121 ms / 66 cases |

Saved/persistent RAM grows by **zero bytes**. Text plus read-only data grows by
3,502 bytes; the rebuilt package grows by 3,328 bytes. Production and benchmark
images both have complete 246-byte proofs with zero gaps. The 256-byte hard limit
passes; **10 bytes** of headroom remains, below the preferred 16-byte target.
The maximum path involves Reprisal melee/life cost/Phoenix status paging.
An extra helper phase increased the proof, so that optimization was discarded.
No recursion, shared scratch lifetime or new persistent hazard state was introduced.

All **66/66** input responses pass 100 ms. All original 58 remain below 90 ms;
65 of 66 total cases are below 90 ms. The busy twelve-monster workload is the
remaining 97.121 ms case. The eight additions are:

| Workload | Worst response ms | Input segments |
| --- | ---: | ---: |
| monster_trap_multi | 62.750 | 3 |
| monster_trap_kills | 62.245 | 2 |
| monster_trap_teleport | 55.674 | 1 |
| monster_trap_summon | 50.599 | 1 |
| monster_trap_force | 71.403 | 2 |
| monster_trap_hammer | 39.459 | 2 |
| monster_trap_busy | 97.121 | 2 |
| monster_trap_spread | 63.854 | 5 |

Crowded feedback creates additional input boundaries. The profiler measures each
input through a displayed `[more]` prompt or final readiness, excludes human
acknowledgment waits, and validates the fully resolved turn after all pages.
Schema-3 benchmark JSON retains every segment and total computation. For example,
spreading force takes five segments totaling 222.798 ms, with a worst segment of
63.854 ms; the busy turn totals 136.119 ms across two segments. These totals are
not single input-response latency. Messages are never suppressed. Production ray
animations remain intact; BENCH-only animation drawing/delays are excluded.

Final profiles: `build/monster-traps/final-benchmarks/20261010T232043Z-qgk87ygp/`.
The final ELF identity matches the rebuilt benchmark image. Two additional
paging regressions validate the runner's separate boundary/final snapshots.

## Native verification

**16/16 CTest tests pass**. The suite includes 12,800 monster trap/type/depth/seed
resolutions; armor, immunities, packed effects, stun, capacity, stale-slot reuse,
teleport legality/fallback/non-recursion, all movement modes, repeat activation,
blocked/stationary/door cases, final-only forced entry, wand/Hammer integration,
stationary polymorph, environmental attribution, zero HP, Lord/Yendor, and hidden
or invisible feedback. Multi-action tests prevent dead monsters continuing or
regenerating and verify snare interruptions. Fire-killed future-slot reuse is
checked across more than 64 seeds. A further 2,048-seed sweep covers Reprisal,
cursed Hammer player knockback, Holy deaths and subsequent slot reuse. Existing feature generation, UI, artifacts,
combat distributions, persistence, rendering, policy and telemetry tests pass.
The benchmark runner also passes **10** Python checks. The final production
ELF reaches its rendered title/input boundary, and the package passes ZIP CRC
validation for metadata, interpreter, game image and save area.

No-feature enabled/disabled games are byte-identical with identical telemetry,
actions and RNG across 32 full native runs. The disabled candidate matches the
previous tuned simulator on shared run telemetry and action hashes for seeds
1–256. The final frozen census binary and current native simulator have identical
SHA-256 hashes. All nine final control CSV streams match the initial control
census byte-for-byte across all 65,535 seeds; the policy repair and slot refresh
introduce no control-run differences. Its escape count also matches the
historical tuned census.

## Paired balance: routine 10,000 seeds

The maintained `omniscient-v2` policy hash is
`dcd81ab7bceb7d9ef08e546fa70a454973d2fd05270194602d6b5260b33049d1`.
Seeds 1–10,000: control **3,341** escapes (33.41%), monster traps **3,324**
(33.24%). Difference **−0.170 percentage points**, paired bootstrap 95% CI
[−0.95, +0.63], exact McNemar p=0.681. There are 749 gained escapes and 766 lost
escapes. Both variants have **zero SIM_STUCK/SIM_ERROR**.

Monster traps contribute **188,764 activations**, **343,850 HP damage**,
**8,534 environmental kills**, **10,521 teleports**, **70,957 status extensions**,
and **32,977 new monsters** from monster-triggered summoning. Typed kill totals
include environmental deaths once; none of the 8,534 trap kills earns XP/score.
The following are whole-run means, including deaths and survivors:

| Metric | Control | Monster traps |
| --- | ---: | ---: |
| score | 4860.572 | 4843.539 |
| level | 27.224 | 27.238 |
| xp | 46.612 | 46.319 |
| actions | 3968.866 | 3983.898 |
| turns | 3952.436 | 3967.476 |
| final_hunger | 155.481 | 155.014 |
| food rations consumed | 10.845 | 10.876 |
| monsters_generated | 208.258 | 211.365 |
| monsters_encountered | 165.442 | 167.286 |
| monsters_killed | 134.956 | 137.068 |
| damage_dealt | 2254.948 | 2267.841 |
| damage_taken | 648.327 | 643.765 |

Lord deaths change 4,257 → 4,185 and Yendor acquisition 4,256 → 4,185. Escapes
already require Yendor; post-acquisition deaths change 915 → 861. Death-cause
changes include Lord melee 458 → 519, dragon melee 492 → 543, dragon fire
742 → 695, troll 840 → 793, starvation 117 → 128 and direct player trap deaths
207 → 194. Detailed death causes and paired floor outcomes remain in the report.
No large overall escape shift appeared and no compensating balance changes were
made. Monster damage can reduce threat, while teleportation, lost credited kills,
extra summons and diverging gameplay RNG also change encounters. These are
maintained-agent results, not a human success-rate estimate. Discordant traces
include seed 13 (control troll death → escape) and seed 7 (escape → dragon death).

### Routine monster-triggered totals by type

| Trap | Activations | HP damage | Kills | Teleports | Status extensions | Summons |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Spike | 18218 | 59636 | 2988 | 0 | 0 | 0 |
| Poison dart | 19662 | 26947 | 1301 | 0 | 11874 | 0 |
| Flame | 16785 | 98769 | 2078 | 0 | 0 | 0 |
| Frost | 16570 | 69725 | 1085 | 0 | 15361 | 0 |
| Snare | 13390 | 0 | 0 | 0 | 13369 | 0 |
| Teleport | 10521 | 0 | 0 | 10521 | 0 | 0 |
| Confusion | 31297 | 0 | 0 | 0 | 30353 | 0 |
| Magic drain | 14313 | 62811 | 816 | 0 | 0 | 0 |
| Summoning | 33549 | 0 | 0 | 0 | 0 | 32977 |
| Rust | 14459 | 25962 | 266 | 0 | 0 | 0 |

## Initial census defect and corrected-policy validation

The initial full census had 21,698 control escapes and 21,550 treatment escapes,
but the treatment had one **SIM_STUCK**, so that census is rejected as balance
evidence. Seed 54808 repeatedly exchanged a Short Bow and a Wand of Fire in a full
pack without arrows. The agent valued retaining the bow at 95 but reacquiring it
at 100 + bow score, creating an inconsistent reverse preference. Retention now
uses at least 100 + bow score. A genuinely more valuable wand can still replace
it, and the reverse trade is refused. Native policy tests cover both directions;
the complete seed now ends normally (dragon fire death on ascent).

Both variants are rerun with the identical corrected maintained `omniscient-v2`
policy. The initial 10k results above are repeated exactly under the corrected policy: all
nine CSV streams are byte-identical in both variants, including action hashes.
The corrected-policy comparison confirms 3,341 versus 3,324 escapes, the same
−0.170 percentage-point difference and confidence interval, and zero failures.
Both final manifests record policy hash
`dcd81ab7bceb7d9ef08e546fa70a454973d2fd05270194602d6b5260b33049d1`.
No production game mechanics or artifact constants were changed to compensate
for the policy defect.

## Paired balance: final 65,535-seed census

Both variants completed all nonzero effective seeds with **zero SIM_STUCK/SIM_ERROR**. Control escapes **21,698** (33.109%), monster traps **21,550** (32.883%). The exact population change is **-0.226 percentage points** (-0.682% relative). Sampling intervals and p-values are unnecessary for this complete deterministic seed population.

There are 4,802 gained escapes, 4,950 lost escapes, 16,748 escapes in both variants and 39,035 deaths in both. Control deaths total 43,837; monster-trap deaths total 43,985.

Monster traps caused **1,236,621 activations**, **2,256,349 effective HP damage**, **56,489 environmental kills**, **68,067 teleports**, **465,254 status additions/extensions**, and **211,476 additional monsters**. Environmental kills remain excluded from player XP/score.

| Metric (whole-run mean) | Control | Monster traps |
| --- | ---: | ---: |
| score | 4848.895 | 4828.173 |
| level | 27.163 | 27.170 |
| xp | 46.327 | 46.291 |
| actions | 3953.348 | 3968.826 |
| turns | 3936.988 | 3952.472 |
| final_hunger | 155.815 | 155.414 |
| food_rations_consumed | 10.818 | 10.853 |
| floors_entered | 17.010 | 17.007 |
| monsters_generated | 207.481 | 210.568 |
| monsters_encountered | 164.912 | 166.731 |
| monsters_engaged | 143.010 | 144.478 |
| monsters_killed | 134.602 | 136.644 |
| monsters_player_attacks | 567.742 | 570.787 |
| monsters_damage_to_monster | 2258.195 | 2301.869 |
| monsters_monster_attacks | 351.223 | 352.029 |
| monsters_damage_to_player | 610.107 | 609.430 |
| damage_dealt | 2249.797 | 2258.663 |
| damage_taken | 645.398 | 644.636 |

| Paired outcome | Control runs | Monster-trap runs | Change pp |
| --- | ---: | ---: | ---: |
| reached_floor_12 | 42276 | 42201 | -0.114 |
| reached_floor_15 | 31817 | 31559 | -0.394 |
| lord_killed | 27557 | 27333 | -0.342 |
| yendor_acquired | 27553 | 27332 | -0.337 |
| post_yendor_death | 5855 | 5782 | -0.111 |

The census shows a small overall escape decrease, similar to the routine sample. Mean turns rise by 15.485 and score falls by 20.723. Extra summons increase generated/encountered populations, while environmental kills reduce some threats without credited rewards. Player-attributed damage and all-source monster damage are reported separately. Teleportation, status changes and diverging gameplay RNG also change encounters. No compensating difficulty or artifact tuning was applied.

### Census monster-triggered totals by type

| Trap | Activations | HP damage | Kills | Teleports | Status additions/extensions | Summons |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Spike | 121,878 | 396,870 | 19,677 | 0 | 0 | 0 |
| Poison dart | 130,048 | 178,662 | 8,801 | 0 | 78,713 | 0 |
| Flame | 109,657 | 644,170 | 13,823 | 0 | 0 | 0 |
| Frost | 107,984 | 453,630 | 7,149 | 0 | 100,117 | 0 |
| Snare | 86,788 | 0 | 0 | 0 | 86,655 | 0 |
| Teleport | 68,067 | 0 | 0 | 68,067 | 0 | 0 |
| Confusion | 206,117 | 0 | 0 | 0 | 199,769 | 0 |
| Magic drain | 93,222 | 410,340 | 5,454 | 0 | 0 | 0 |
| Summoning | 217,327 | 0 | 0 | 0 | 0 | 211,476 |
| Rust | 95,533 | 172,677 | 1,585 | 0 | 0 | 0 |

### Census player death causes

These are immediate causes, not the full causal chain. The largest shift is Mimic deaths (+265, or +4.044 per 1,000 seeds); starvation rises by 91 (+1.389 per 1,000). Overall escape shifts remain small. Detailed floor survival separates reach from conditional exit and ascent from descent in the paired comparison.

| Immediate cause | Control | Monster traps |
| --- | ---: | ---: |
| MIMIC | 2,643 | 2,908 |
| SNAKE | 6,724 | 6,519 |
| HOBGOBLIN | 2,318 | 2,411 |
| starvation | 674 | 765 |
| fire:DRAGON | 4,772 | 4,843 |
| PHANTOM | 3,248 | 3,184 |
| TROLL | 5,363 | 5,425 |
| trap:8 | 306 | 252 |
| GRIFFIN | 675 | 632 |
| INCUBUS | 3,004 | 2,962 |
| DRAGON | 3,402 | 3,428 |
| LORD | 3,163 | 3,144 |
| ORC | 1,702 | 1,688 |
| trap:6 | 451 | 464 |
| item:STORMBRINGER | 23 | 35 |
| BAT | 495 | 484 |
| GOBLIN | 910 | 899 |
| ANGEL | 1,033 | 1,023 |
| TARANTULA | 392 | 382 |
| trap:2 | 249 | 241 |
| ZOMBIE | 1,377 | 1,382 |
| RATTLESNAKE | 692 | 696 |
| trap:4 | 125 | 121 |
| trap:16 | 93 | 94 |
| item:AMULET_VAMPIRE | 3 | 3 |

Direct player trap deaths total 1,224 → 1,172. Successful escapes still require Yendor. The final corrected-policy seed 54808 ends in an ordinary dragon-fire death, and the complete census has no simulator failures.

Final evidence: `build/monster-traps/paired-verified-all/comparison/` and `build/monster-traps/paired-verified-all/monster-report/`. The corresponding `paired-verified-10k/` directories retain routine results. Both final manifests record the same executable and maintained policy hash.

## Files and retained evidence

Gameplay changes: `src/features.cpp`, `features.hpp`, `combat.cpp`, `items.cpp`,
`game_internal.hpp`, `model.hpp`, `sim_hooks.hpp`; BENCH-only ray animation guards
in `render.cpp`. No world-generation or artifact constants changed.
Host changes: `sim/metrics.*`, `experiment.*`, `main.cpp`, `simulator.cpp`,
`trace.cpp`, `balance.py`, `feature_experiments.py`, `monster_trap_report.py`,
`tests.cpp`, `test_cli.py`, `omniscient_agent.cpp` and `policy_checks.cpp`.
Native coverage: `tests/feature_checks.cpp` and
`game_native.cpp`. Bench coverage/runner: `bench/bench.cpp`, `profile_turns.py`,
`test_profile_turns.py`. Documentation: project README, map-feature/tuning reports,
this report, simulator README/BALANCE and benchmark README. The final production
package is `ardurogue2.arduboy` at the project root.

Ignored evidence lives under `build/monster-traps/`: paired-verified-10k and paired-verified-all
CSV/manifests/comparisons (initial runs are preserved separately), supplementary monster reports, frozen simulator
binaries, final AVM profiles, paging regressions, compatibility CSVs and traces.
`build/monster-traps-avm.log` contains complete linker stack proofs;
`build/monster-traps/tests-final.log` records all native checks. Large datasets
are intentionally untracked. `final-provenance.json` seals final sources,
artifacts, verification logs and report paths.

Remaining limitations: preferred 16-byte stack headroom and the optional 90 ms
ceiling are not met by the 246-byte proof and one busy workload, respectively.
The required 256-byte/100 ms limits pass. No outstanding correctness failures or
persistent-layout changes remain.
