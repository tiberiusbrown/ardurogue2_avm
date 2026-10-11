# Explorer invisible contact

This is a native explorer-policy change. Production combat, phantom definitions,
awareness, lighting, firmware, saved structures and gameplay RNG are unchanged.
The default remains named `explorer-v1`; `omniscient-v2` remains the balance oracle.

## Failure and correction

The previous policy updated `shadow` after a legitimate melee hit or miss, then
could immediately clear it and block the square after 12 stationary turns or a
damage reassessment. This conflated evidence that a square had contained a foe
with whether fighting that foe was worthwhile. It could discard fresh contact
in a favorable fight. Its rotating probe counter did not record failed hypotheses.

`InvisibleContact` now retains one primary square, attack origin, confirmation
time, public hit/miss result, tested cardinal directions, recent displayed HP
loss and bounded encounter costs. It contains no monster identity, live HP,
unseen motion or internal object handle. The original visible-combat action
history remains available independently of this unseen-contact memory.

| State | Evidence and next action |
| --- | --- |
| None | Ordinary exploration and visible combat. |
| Suspected | Anonymous incoming attack, named but currently unseen attacker, or recent legitimate encounter; direction remains unknown. Test untried plausible squares if viable. |
| Confirmed | Own non-confused melee hit/miss establishes the attempted square **at attack time**. Prefer another strike there while viable. Enemy turns may already have changed occupancy. |
| Uncertain | Noncombat action or printed displacement/form/death feedback weakens the hypothesis. A brief retry may be reasonable; certainty is not retained. |
| Disengaging | Survival or productivity overrides commitment. Use existing healing/control/escape and known movement; retain a separate route cooldown. |

## Public feedback and event order

Protocol 3 adds `MeleeFeedback`, attack-time `contact_origin`, counts of printed
incoming/anonymous attacks, and conservative encounter-change/summoning flags.
The native adapter still sanitizes each monster name through the production
visibility predicates. A printed “something” supplies species zero.

Both “You hit” and “You miss,” followed by the production entity status call,
confirm contact for a cardinal Move prepared while not confused. A roll miss
is positional evidence, even though it proves no damage. Empty movement,
wall/door bumps, ranged feedback and confused intentions cannot confirm that
square. Incoming attacks provide danger and attack count, never a bearing.
Several printed attacks may come from one fast creature; they are not an
inferred count of distinct hidden monsters.

A killing melee blow prints “You defeat” without “You hit.” Defeat is deliberately
not associated with the attempted square: the same prose may be produced by
splash, reprisal or holy damage during an otherwise empty move. It clears or
weakens an existing hypothesis and records observed progress, without inventing
a new bearing. Later splash/reprisal deaths do not overwrite an earlier primary
hit/miss result. Printed knockback, disappearance, transformation or teleport
trap messages weaken contact conservatively, without disclosing a destination
or assuming the message names the primary opponent.

Confirmation is processed after the complete production action, including enemy
turns. If the player is no longer at the attack origin, the square is not accepted
as current adjacent contact. Player movement, confusion, a failed repeated strike,
a visible replacement/revelation and more than three turns without confirmation
invalidate the square. A noncombat action normally reduces it to uncertain for
a short retry. Six quiet turns without credible threat end the episode. A
completed retreat releases tactical disengagement while route avoidance remains;
the cooldown cannot force 64 turns of unnecessary wandering.

## Commitment, probing and loop protection

Fresh favorable contact refreshes the hypothesis, including consecutive roll
misses and fights exceeding 12 turns. The old `stationary_contact_turns` preference
does not govern the new policy. SurvivalRisk remains the main danger estimator:
known armor, public species when printed/visible, statuses, HP and one/two-action
damage estimates inform the existing healing, control, teleport and retreat
decisions. Anonymous pressure is retained alongside visible enemies. Recent
unseen pressure can remain relevant for three turns without a new message.

At low HP, large observed loss over the last eight action observations can
justify retreat even before the instantaneous estimate is lethal. Repeated
healing plus cumulative observed loss, depleted hunger without food, repeated
observable summons/replacements near a discovered summoning hazard, or prolonged
absence of successful hit feedback can also make commitment unproductive.
No inference uses hidden damage totals or live opponent HP.

The four-bit tested mask belongs to the current player position. Known traversable
directions precede unknown tactile probes; known walls, closed doors, features
and visible occupants are excluded. Failed directions are not immediately
repeated. Successful empty movement discards the old square and avoids an
immediate reverse probe. Exhausting local candidates causes a bounded
repositioning interval rather than another blind rotation. Confusion suppresses
positional probing. Multiple attackers preserve a primary hypothesis while
raising conservative pressure; visible urgent threats and survival actions
can take priority. There is no hidden multi-monster tracking model.

Separate safeguards are 64 misses without successful hit/progress feedback,
observable replenishment over 32 turns, two healing actions with at least two
max-HP bars of displayed loss, starvation pressure, and a final 512-turn episode
bound, plus 1,024 stationary action observations across episode/death resets.
Hits reset the no-progress measure. These are strategic/final guards,
not a 12-turn stationary occupancy rule. Known previously entered summoning
traps receive increasing route cost. Dangerous abandoned squares and the
departure square have a temporary route cooldown. Retreat attempts share the existing four-step tactical budget so continuing
pressure cannot produce a long detour loop. If no known retreat exists or this
budget is exhausted, the explorer can keep defending credible contact instead of waiting to die.

## Telemetry schema and limitations

Core streams remain version 7. Exploration schema 5 appends these per-visit
fields; `invisible_contact_experiments.py` retains totals and raw sealed CSVs.

| Fields | Definition and information boundary |
| --- | --- |
| `unseen_encounters`, `unseen_located` | Reporting episodes begin at an initially unobserved attack/contact; located episodes receive anonymous hit/miss contact. Episodes end after six turns without evidence or on floor closure. |
| `unseen_probes`, `unseen_probes_to_first_contact` | Policy-labelled probes, and their sum in located episodes before first confirmation. Divide the latter by located episodes; unlocated episodes are excluded. |
| `unseen_confirmations`, `unseen_contact_hits`, `unseen_contact_misses` | Public anonymous hit/miss feedback; confirmations reconcile to hits plus misses. |
| `unseen_max_consecutive`, `unseen_contact_turns` | Maximum consecutive confirming observations at the same origin/square, and total confirming observations. These measure contact duration, not opponent identity or exact lifetime. |
| `unseen_contact_losses` | A previously confirming observation is followed by nonconfirming feedback; includes deliberate noncombat actions and defeat. Not proof the monster physically moved. |
| `unseen_premature_abandonments` | Moving elsewhere after fresh anonymous contact, without printed encounter change or serious modeled danger, excluding explicit strategic disengagement. A diagnostic proxy, not a counterfactual claim. |
| `unseen_damage_total`, `unseen_healing`, `unseen_kills` | Privileged production damage/healing-or-experience-potion units consumed during episodes, and actual kills of reporting slots initially involved unseen. Slots clear on death/replacement; no identity reaches policy. |
| `unseen_disengagements` | A retreat/detour/teleport episode survives until six quiet turns. Does not prove the movement caused survival. |
| `unseen_over_12/32/64/128` | Episodes whose total production-turn duration exceeds each bound, including the quiet closure interval. |
| `unseen_summons`, `unseen_hazard_encounters` | Actual player/monster summoning activations and episodes associated with a nearby authoritative summoning trap or activation. Hidden trap proximity is reporting only and does not prove causal involvement. |
| `unseen_ordinary_damage/kills`, `unseen_hazard_damage/kills` | Episode totals split by the above privileged hazard association; reconcile to total episode damage/kills. |
| `unseen_stationary_starvation`, `phantom_deaths` | Privileged deaths attributed to starvation during consecutive stationary contact, and all PHANTOM-attributed deaths on that visit. |

Aggregate damage, kills and resource expenditure depend on exposure and survival.
A deeper-running policy can incur more of each. Hazard groups are associations,
not controlled trap interventions. The oracle's new contact fields remain zero.
Telemetry and trace objects are never supplied to the independently compiled
policy, and disabling telemetry preserves gameplay/actions.

## Tests and implementation scope

The native suite passes all 23 CTests, including 621 explorer checks and the
1,792-case renderer/observer comparison. Added checks cover first anonymous
production melee miss/hit, killing feedback, empty movement and wall bumps;
48 successive favorable misses and 160 productive hits; loss, movement,
displacement/form messages, healing uncertainty/expiry, floor transition,
unidentified incoming attacks, multiple/mixed threats, confusion, risky escape,
observable replenishment, replacement deaths resetting episodes, and prolonged
no-progress safeguards. Newly confirmed contact during disengagement updates
the square to avoid, and a regression verifies it is not selected as an escape
step. Repeated escape attempts share the existing four-step retreat budget; a
constructed pressure sequence verifies that contact cannot reset that budget.
Production dispatch and event collection reconcile unseen episode/kill counters. Identical
contact histories remain identical after hidden position/HP/RNG changes.
Existing full replay, no-telemetry parity, serial/parallel CSV parity, production
state/RNG noninterference and policy import-boundary tests pass.

All nine oracle core CSVs are byte-identical to the frozen executable over seeds
1:1000, with unchanged oracle policy hash. The reconstructed legacy explorer
matches all nine frozen core CSVs and every common exploration field over
1:1000; the final executable also repeats the 32-seed compatibility check. Firmware package SHA-256 is unchanged:
`5556c4658b484e98bfdae07726c8a34c5e53b16277ef1ba4f9d0db33ccfa2d0c`.
All 78 AVM timing cases pass, worst 99.299 ms. MSBuild's file tracker encountered
an environment permission error; the existing SDK build succeeded with Ninja.
No SDK installation or firmware source change was needed. Host ExplorerAgent
size is 540,120 bytes; no saved RAM/layout/version changes.

Changed native files are `explorer.hpp`, `explorer_agent.cpp`,
`explorer_parameters.hpp`, `explorer_events.{hpp,cpp}`, `explorer_checks.cpp`,
`metrics.{hpp,cpp}`, `simulator.cpp`, `trace.cpp`, `test_explorer_cli.py` and
`invisible_contact_experiments.py`. Documentation changes are this report,
`EXPLORER_AGENT.md`, `sim/README.md` and `sim/BALANCE.md`.

## Frozen comparisons

The previous executable and original source/default snapshots are retained in
`build/invisible-contact/reference/`; executable SHA-256 is
`8b11166c60ffd280f83a268ded99620826df68565f05aa04a36b85e68fc2ea3d`.
The candidate retains `contact_aware=0` for the original policy and combines it
with `invisible_response=0` for the original ablation. That ablation disables
the older reassessment, not all probing or contact repetition. Only these
comparison switches differ; all 101 preexisting defaults and production rules
remain fixed. Code/parameter/observation hashes are recorded in each manifest.
These are descriptive policy comparisons, not same-policy gameplay interventions.

Exploration used seeds 1:1000. The first prototype improved phantom mortality
but reduced mean depth to 4.200, below both comparators. Paired trace review and
code inspection found overly persistent disengagement and visible-contact
misclassification; these were structural corrections, not threshold optimization.
Visible-combat history was then preserved independently. Historical census
escape seeds were not used to optimize the policy. The final candidate was
refrozen after a constructed replacement/death loop test and paired trace review
found a newly confirmed retreat square was still being selected as an escape
step. Every revision and the repeated expanded 10001:20000 comparison are
retained. These seeds have historical exposure and are not claimed to be
globally untouched holdout data. No constants were fitted to rare escape seeds.

The final movement correction shares the existing four-step retreat budget.
The preceding reviewed candidate reached mean depth 4.4770 versus the ablation's
4.4847 on the expanded range; trace/movement review exposed persistent detours
under continuing pressure. The final candidate reaches 4.4901. This is a
structural bound on escape attempts, with all unrelated preferences unchanged;
it is not evidence that one marginal depth contrast proves universal superiority.
The earlier candidate had fewer phantom-attributed deaths (2,653) than the
selected candidate (2,707); both revisions remain available for review.

| Final exploratory policy, 1,000 seeds | Phantom deaths | Mean/median deepest floor | Escapes | Failures |
| --- | ---: | ---: | ---: | ---: |
| Legacy | 284 | 4.340 / 4 | 0 | 0 |
| Disabled-response ablation | 276 | 4.330 / 4 | 0 | 0 |
| Contact-aware | 283 | 4.378 / 4 | 0 | 0 |

The exploratory candidate has 10,785 initially unseen kills versus 10,574
legacy, 12 favorable-abandonment proxies versus 505, and a maximum actual
confirmation streak of 15 versus 12. Phantom mortality itself is essentially
unchanged on this small range. Constructed tests supply the controlled evidence
for persistent favorable contact beyond twelve turns.

### Expanded frozen comparison

The final comparison uses 10,000 identical seeds, 10001:20000, per policy,
equipment-light production rules, eight workers and a 50,000-action guard.
Floors below are one-based. None of the three policies escapes.

| Policy | Phantom deaths | Mean/median deepest floor | Mean actions | Escapes | SIM_STUCK / SIM_ERROR |
| --- | ---: | ---: | ---: | ---: | ---: |
| Legacy | 2,815 | 4.4463 / 4 | 1914.12 | 0 | 0 / 0 |
| Disabled-response ablation | 2,678 | 4.4847 / 4 | 1930.47 | 0 | 0 / 0 |
| Contact-aware | 2,707 | 4.4901 / 4 | 1941.94 | 0 | 0 / 0 |

Relative to legacy, phantom attribution falls from 28.15% to 27.07%, a 1.08
percentage-point difference (3.84% relative). The paired contrast has 1,011
legacy phantom deaths replaced by another outcome and 903 new phantom deaths;
exact McNemar p=0.0144, BH q=0.0289 within the two phantom-attribution contrasts.
The mean-depth difference is +0.0438 floors, paired bootstrap 95% interval
[+0.0119, +0.0761]. The intervals use 5,000 resamples of paired seeds and offline
analysis RNG seed 712; this RNG never enters gameplay.

Against the disabled-response ablation, depth is effectively tied: +0.0054
floors, interval [-0.0271, +0.0388]. Phantom deaths are 29 higher; paired discordance
is 927 avoided versus 956 new, p/q=0.5188. These results **do not establish
superiority to that ablation**. All policies eventually die in this range;
changing attributed death cause is not itself a rescued run. The evidence for
adopting the contact model is the controlled reasoning correction, measurable
contact/pressure diagnostics, and modest broader progression gain over legacy.

All medians are floor 4. Legacy/disabled/contact respectively kill the Lord and
obtain Yendor in 2/1/2 runs, but all die during the return. With zero escapes in
10,000 runs, each policy's two-sided exact 95% binomial upper escape-rate bound
is about 0.0369%. This is no evidence of a completion-rate improvement.
Maximum action counts are 12,818 / 10,739 / 14,557, all below the guard; there
are no simulator failures or censored excessive-action runs.

Conditional descent survival is exited/entered on each floor:

| Floor | Legacy | Disabled | Contact-aware |
| --- | ---: | ---: | ---: |
| 1 | 8,686/10,000 (86.9%) | 8,684/10,000 (86.8%) | 8,685/10,000 (86.8%) |
| 2 | 7,711/8,686 (88.8%) | 7,712/8,684 (88.8%) | 7,709/8,685 (88.8%) |
| 3 | 5,520/7,711 (71.6%) | 5,600/7,712 (72.6%) | 5,552/7,709 (72.0%) |
| 4 | 3,970/5,520 (71.9%) | 4,015/5,600 (71.7%) | 3,963/5,552 (71.4%) |
| 5 | 2,710/3,970 (68.3%) | 2,808/4,015 (69.9%) | 2,782/3,963 (70.2%) |
| 6 | 1,981/2,710 (73.1%) | 2,040/2,808 (72.6%) | 2,053/2,782 (73.8%) |
| 7 | 1,234/1,981 (62.3%) | 1,262/2,040 (61.9%) | 1,309/2,053 (63.8%) |
| 8 | 934/1,234 (75.7%) | 959/1,262 (76.0%) | 992/1,309 (75.8%) |
| 9 | 622/934 (66.6%) | 630/959 (65.7%) | 680/992 (68.5%) |
| 10 | 523/622 (84.1%) | 525/630 (83.3%) | 564/680 (82.9%) |
| 11 | 295/523 (56.4%) | 314/525 (59.8%) | 304/564 (53.9%) |
| 12 | 155/295 (52.5%) | 162/314 (51.6%) | 161/304 (53.0%) |
| 13 | 77/155 (49.7%) | 86/162 (53.1%) | 93/161 (57.8%) |
| 14 | 36/77 (46.8%) | 41/86 (47.7%) | 41/93 (44.1%) |
| 15 | 9/36 (25.0%) | 9/41 (22.0%) | 13/41 (31.7%) |
| 16 | 2/9 (22.2%) | 1/9 (11.1%) | 2/13 (15.4%) |

These conditional groups have different surviving players and equipment;
their contrasts are descriptive rather than same-entry-condition interventions.
Later floors have very small denominators.

### Encounter diagnostics and costs

Totals are over all 10,000 runs. Unseen episodes include all initially unseen
species and mixed visible/unseen combat, not only phantoms. Privileged phantom
rows below are actual production classifications. They are not policy inputs.

| Metric | Legacy | Disabled | Contact-aware |
| --- | ---: | ---: | ---: |
| Unseen episodes | 116,150 | 115,997 | 114,705 |
| Located episodes | 43,581 | 43,605 | 43,909 |
| Directional probes | 25,755 | 25,599 | 55,160 |
| Probe sum before first contact in located episodes | 17,210 | 17,179 | 39,865 |
| Confirmations / confirming observations | 152,287 | 157,413 | 154,159 |
| Confirmed attack-roll hits | 110,339 | 113,897 | 111,568 |
| Confirmed attack-roll misses | 41,948 | 43,516 | 42,591 |
| Maximum consecutive confirmations | 12 | 12 | 25 |
| Contact-loss proxies | 54,822 | 53,085 | 57,656 |
| Favorable-abandonment proxies | 5,052 | 661 | 201 |
| Damage during unseen episodes | 571,149 | 578,955 | 567,963 |
| Healing/experience units in unseen episodes | 5,192 | 5,226 | 5,121 |
| Disengagements followed by quiet survival | 8,485 | 2,543 | 9,940 |
| Initially unseen enemies killed | 110,270 | 112,313 | 110,690 |
| Episodes exceeding 12 turns | 15,818 | 15,632 | 17,348 |
| Episodes exceeding 32 turns | 320 | 261 | 283 |
| Episodes exceeding 64 turns | 6 | 5 | 6 |
| Episodes exceeding 128 turns | 0 | 0 | 0 |
| Summoning activations during episodes | 1,579 | 1,499 | 1,291 |
| Hazard-associated episodes | 2,420 | 2,432 | 2,293 |
| Ordinary-associated episode damage | 547,670 | 555,433 | 546,083 |
| Hazard-associated episode damage | 23,479 | 23,522 | 21,880 |
| Ordinary-associated unseen kills | 107,565 | 109,512 | 107,993 |
| Hazard-associated unseen kills | 2,705 | 2,801 | 2,697 |
| Stationary-contact starvation deaths | 0 | 6 | 2 |
| All combat turns | 1,578,798 | 1,605,647 | 1,610,298 |
| Exploring turns | 15,339,516 | 15,476,013 | 15,521,387 |
| Travel | 17,038,114 | 17,171,799 | 17,264,584 |
| Repeated tile visits | 5,263,753 | 5,287,626 | 5,350,414 |
| Actual phantom kills | 52,220 | 53,545 | 52,970 |
| Actual phantom damage | 327,857 | 328,858 | 319,871 |

Located episodes are 37.52% / 37.59% / 38.28% of unseen episodes. Probe sum per
located episode is 0.395 / 0.394 / 0.908; increased deliberate probing does not
establish faster location. The maximum live confirmation streak rises from 12
to 25, while controlled histories retain 48 misses and 160 hits. Favorable
abandonment proxies fall sharply, but the metric excludes explicit strategic
disengagement by definition and does not establish that every retreat was wise.
Contact-loss proxies include healing and kills and are higher in the new policy.

Actual phantom damage is 2.44% below legacy and 2.73% below disabled. Actual
phantom kills are 1.44% above legacy, but 1.07% below disabled. Exposure, changed
routes and survival confound these aggregates. No individual metric proves a
causal phantom-combat advantage. Total starvation deaths are 719 / 718 / 738;
the new policy still has two stationary-contact starvation deaths. The safeguards
prevent constructed loops and the observed simulator guard failures, not every
unproductive fight or starvation.

| Units consumed, whole run | Legacy | Disabled | Contact-aware |
| --- | ---: | ---: | ---: |
| Food | 51,235 | 51,780 | 52,086 |
| Healing | 16,244 | 16,341 | 16,349 |
| Experience | 3,796 | 3,781 | 3,796 |
| Teleport | 4,526 | 4,560 | 4,510 |
| Fear | 4,391 | 4,394 | 4,427 |

Mean actions rise by 27.82 versus legacy and 11.47 versus disabled. Repeated
visits/travel are 30.89% / 30.79% / 30.99%. More combat/probing and food use are
real costs; the final policy does not show a general exploration-efficiency
improvement. Resource and damage totals must be read with progression/exposure.

### Paired trace review

`trace-review.json` verifies that all twelve selected trace run rows, action
hashes and policy hashes exactly reproduce their corresponding batch rows.
Review compares mechanical actions, not merely changed goal labels. Privileged
`monster_hp` annotations in action traces never reach the separate `OBS` input.

- **10011, phantom attribution avoided:** at A1065, after a quiet recovery-feature
  detour, legacy continues a contact-avoidance movement cycle. The candidate
  collects the observed scroll instead. Both reach floor 4; legacy dies to a
  phantom, candidate later to an amulet effect. This is improved route progress,
  not proof of a better isolated phantom fight.
- **10034, better locating but worse progression:** after an east probe succeeds
  as empty movement, at A1321 the candidate tests south instead of immediately
  reversing west. Anonymous hit/miss contact confirms (31,7); it repeats south,
  defeats the foe and resumes exploration. The candidate nevertheless starves
  on floor 4, while legacy reaches floor 5 and dies to a phantom. Correct local
  reasoning does not ensure a better whole-run result.
- **10008, new phantom attribution:** at A513, after blindness expires and a
  visible bat survives a miss, legacy's stationary cutoff detours east. The
  candidate finishes the visible foe. Candidate reaches floor 5 and dies to a
  phantom, versus legacy's orc death on floor 4. Removing the old generic cutoff
  also changes visible melee history; this is not isolated phantom causation.
- **10013, retreat regression:** a contact hit at A1339 costs five displayed HP,
  then healing is followed by another anonymous attack. Candidate retreats west
  at A1341 based on recent pressure, then takes three further retreat steps;
  legacy stays, kills the opponent and levels. Candidate eventually dies to a
  phantom on floor 5 versus legacy's starvation on floor 6. Conservative pressure
  after healing can overestimate the value of abandoning a finishable fight.

The recorded full-light replenishment case **9145** completes without a
simulator failure for all variants. Legacy dies to an orc on floor 6 after
2,220 actions; disabled dies to a phantom on floor 5 after 1,964; the candidate
reaches floor 13 and dies to dragon fire after 7,462. Longest stationary action
streaks are 16 / 11 / 25, respectively; none recreates the historical endless
replenishment loop. The older policy already avoids that historical loop under
current production rules, so this is not a claim that this pass uniquely fixed it.

The occupied-retreat-square regression found in the earlier paired review is
covered by a constructed test. The final tests also verify that continuing
anonymous pressure cannot reset the four-step escape-attempt budget. Both
improvement and regression traces remain retained; no historical census escape
seed was used for policy optimization.

### Reproduction and identities

Final raw data/configurations/executable/manifests/seals are under
`build/invisible-contact/bounded-1000/` and `bounded-10000/`. Statistical analysis
is `bounded-10000/analysis.json`; inspected action/OBS excerpts and parity are in
`bounded-10000/trace-review.json`. The comparison workflow is standard-library
Python; the offline bootstrap/McNemar calculation uses the existing balance
venv's NumPy/SciPy. All prototype and intermediate expanded outputs remain
under ignored `build/invisible-contact/`. The final changed-source snapshot,
patch and file hashes are retained in `bounded-10000/source/`, `source.patch`
and `source-seals.json`.

```sh
python sim/invisible_contact_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --reference build/invisible-contact/reference/ardurogue2_sim.exe --seeds 1:1000 --jobs 8 --output build/invisible-contact/reproduce-1000
python sim/invisible_contact_experiments.py --simulator build/competence/native/sim/ardurogue2_sim.exe --seeds 10001:20000 --jobs 8 --traces --output build/invisible-contact/reproduce-10000
```

Output directories must be new. On this Windows host, the MSYS UCRT runtime
DLL directory must be on PATH. Defaults, original source and executable are
already frozen in `reference/`; no production intervention is used in the main
comparisons. Only the historical loop trace uses `full-light`.

Final executable SHA-256: `b79a07c0155bb8067be3bee327ca72622cd8fc2a51a2a30537aeccca037bb126`.

| Policy | Exact policy hash |
| --- | --- |
| Legacy | `d78bb48918441844d5c992f308ec9ee342471d9b85312d1c92c1f21ec64f03e1` |
| Disabled-response ablation | `95eb1e440e24ae3a3f5b969901b8df07d245809905a3c2fa7a6bbda17ea1f1bd` |
| Contact-aware | `2940acd0c0155138dcbc9b4692538952db6139abfaf911764d9c6d9811e0073f` |

Code hash: `217e06da0752f454b2642f742e2cf071e2708e5da2950414b48a71fc867f9deb`.
Observation protocol 3 hash: `b2e8d5fcb20b0316373430e290ee416e915f0759a94b984953eb090c59f6d699`.
Complete parameter hashes and all 102 values are retained in the manifests.

## Remaining weaknesses

One primary square cannot resolve several independent invisible attackers.
Attack counts may overestimate numbers when an enemy is fast; HP differences
underestimate damage concealed by healing/regeneration. A printed hit does not
quantify damage, and generic displacement/death prose cannot identify its target.
Retreat squares are known terrain, not guaranteed free of unseen enemies.
Direction probing can move the player and change the encounter. The policy may
still spend resources on an unfavorable fight, revisit a hazard when no route
avoids it, or die while cornered. Simulation-loop guards are not survival proofs.
Rare escapes and survivor-dependent aggregate measures do not establish human
completion probability or a statistically significant escape benefit.
