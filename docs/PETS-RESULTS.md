# Persistent companions: implementation and validation

Validated on 2026-10-14 UTC against starting commit `f2083c6a39a7bcd086fef10f91292076ec6364eb` on `codex/multi-spell-books-mana`. The gameplay guide is [PETS.md](PETS.md). This report describes the final candidate and fresh matched-policy controls, rather than pooling development cohorts.

Persistent companions now participate in ordinary occupancy, turns, combat, traps, spells, stairs and saving. Native correctness and all AVM performance checks pass. The experiment evaluates the complete availability package, including replacement loot, learning time, mana, inventory decisions and combat. It does not estimate a pet-only damage bonus while holding those opportunity costs fixed.

## Gameplay and design decisions

There are twelve hostile slots and four friendly slots. Allegiance is derived from slot index; each Monster remains eight bytes. Hostile population and hostile summons keep twelve slots. Taming moves an intact instance into the friendly range. Death and confirmed dismissal free a slot; neither grants rewards. Eligible pet kills award full ordinary XP and score, preserve Wisdom bonuses and the Lord's Yendor drop, and exclude rewardless enemies.

Four fixed-stat summons cost 7/8/10/12 MP and one action. They never expire, scale with level, carry equipment, eat, or resurrect. One living member of each new species is allowed, including altar grants. Ordinary tamed species can repeat. Insufficient MP, a full or duplicate party, and unsafe placement reject before MP or turn consumption. Maximum mana is unchanged; there is no reservation or upkeep. Agents keep emergency MP voluntarily.

| Species / glyph | HP | STR | DEX | Action cost | Armor | MP | Ability |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Hound / h | 14 | 4 | 5 | 3 | 1 | 7 | Scent; fast melee |
| Spider / s | 18 | 4 | 4 | 4 | 1 | 8 | 25% weakening; 25% four-tick slow |
| Wisp / w | 10 | 3 | 4 | 4 | 0 | 10 | 2–4 armor-bypassing damage; range three; sees invisible |
| Golem / g | 36 | 7 | 3 | 7 | 5 | 12 | Opens doors; 25% two-tick stun |

Smaller action cost means faster scheduling. Physical creature attacks use existing accuracy, strength and absorption arithmetic. Weakening halves outgoing damage and uses the ordinary poison marker, not a new damage-over-time system; it persists until restoration. Conditions do not stack without bounds. Wisp attacks require terrain line of effect and never hit friends or the player. Hound/Wisp detection does not disclose unseen enemies to explorer-v1.

The fifth book contains all four summons, with sixty study turns each. Spell IDs 0–15 are preserved and summons append 16–19 in a 32-bit knowledge mask. On descent floors 0–2, an eligible ordinary slot becomes a missing unfinished Companions book; leaving it behind permits a retry on the next eligible floor. Later eligible floors schedule it at 35%. Carried/completed books, same-floor duplicates, ascent and the all-phantom floor are excluded. Essential supplies, artifacts, Yendor and existing books are protected. The schedule uses an isolated seed domain, preserves ordinary book draws, and changes no ground-item count.

Following uses six bounded local choices and a recent-player-position preference. Friendly swaps apply each destination trap once; door opening without movement does not retrigger the current trap. Defensive melee approach remains within three tiles of the player and does not grant an extra attack that turn. Beyond that range, returning takes priority. Automatic recall tries normal movement and safe offscreen corner/light-circle landings first. Severe stranding permits a visible normal redraw, with no extra timed animation. A fixed 24-candidate radius-three search rejects all traps, closed doors, walls, occupants and the player. Eight ordinary turns of cooldown and six turns of displacement grace prevent immediate recall exploits.

Stairs keep the four instances in place while invalidating old positions; only hostile storage becomes generation scratch. Safe entrance placement restores survivors on both descent and ascent. A constrained custom map retains any temporarily unplaced pet for a later retry. Idle recovery restores one HP every 32 turns, capped at maximum and disabled during fighting or nearby detected threats. Thrown healing and ordinary restorative effects still work.

Applicable spell and projectile rays hit the first creature. Piercing, chains, area damage, knockback, teleportation and conditions include friendly creatures. Friendly fire can kill a companion without changing its allegiance. Hostiles pursue the player and attack a blocking pet rather than searching the dungeon for distant pets. Companions never deliberately attack friends. Battle feedback is limited to one routine creature-combat message per ordinary turn; meaningful deaths and traps still report and can interrupt study.

The nine-entry action menu adds COMPANIONS: species, HP/max HP, origin and active conditions for zero to four pets, selection and permanent dismissal confirmation. Browsing/canceling costs nothing; confirmed dismissal costs one action. New species have four unique 4×4 glyph sprites. Tamed species keep their original glyph without an allegiance marker.

## Taming and generation

| Target HP | Ordinary scroll / potion / wand | Troll or Griffin scroll / potion / wand |
| --- | --- | --- |
| Above half | 100% / 50% / 75% | 50% / 5% / 25% |
| At most half | 100% / 65% / 90% | 50% / 15% / 40% |
| At most quarter | 100% / 75% / 100% | 50% / 25% / 50% |

Weakening supplies the same ten-point bonus as quarter HP, once; the half-HP bonus is independent. Scrolls ignore injury bonuses. Bat, Snake, Rattlesnake, Goblin, Orc, Tarantula and Hobgoblin are ordinary eligible targets. Troll and Griffin subtract fifty points of resistance. Zombie, Phantom, Mimic, Incubus, Dragon, Fallen Angel, Lord, rewardless hostile summons and friends are immune.

Scroll targeting requires actual visibility, radius four and clear effect; cancellation is free. Friendship uses normal thrown-potion impact, consumption and appearance/identification rules. Domination uses range-six rays, ordinary charges, cursed/unreliable modifiers and deterministic area/spreading order; conversion stops at four and skips new friends. The shared authoritative roll preserves species, HP, conditions and stun. A failed valid attempt consumes its resource; full-party rejection occurs before scroll or wand expenditure.

Independent same-slot generation replaces 4% of ordinary potion results with Friendship, 6% of scroll results with Taming and 6% of wand results with Domination. It keeps original generation draws/placements. The ordinary altar has a one-in-eight companion branch, weighted Hound 5/8, Spider 2/8, Wisp 1/8. Invalid grants fall back to an ordinary blessing. Other one-shot rewards and summoning risks remain.

## RAM, flash, stack and execution cost

| Metric | Starting build | Companion build | Difference |
| --- | --- | --- | --- |
| Saved Game (.saved) | 845 | 879 | +34 bytes |
| Other static RAM (.data) | 125 | 144 | +19 bytes |
| Total production static RAM | 970 | 1023 | +53 bytes; one byte free |
| AVM .text | 108132 | 124825 | +16,693 bytes |
| AVM .rodata | 12359 | 13547 | +1,188 bytes |
| Program .text + .rodata | 120491 | 138372 | +17,881 bytes |
| Packaged .arduboy | 209756 | 227676 | +17,920 bytes |
| Proven maximum stack | 249 | 256 | +7 bytes; complete bound |

The saved increase is exactly four eight-byte creatures plus two additional spell-mask bytes. Transient growth is six Session trail/leash bytes, two placement-cache bytes, two lazy-target-cache bytes, one per-turn combat flag byte, and two four-byte row-presence filters. The benchmark selector uses the last global byte: benchmark static RAM is 1,024 bytes. The stack is a separate 256-byte region. Save version 33 rejects incompatible older raw saves; Item remains two bytes, Monster eight, GroundItem four, and identified-item knowledge occupies nine bytes.

Both production and benchmark linkers prove a complete 256-byte bound with zero analysis gaps. The witness runs through spell knockback, a monster trap, death and status rendering. This leaves no stack headroom. The generated package was built with the AVM SDK, warnings as errors, optimized LTO. No hooks or telemetry add production saved fields.

All 140 cases pass the unchanged 100 ms criterion, including the existing suite and eleven companion cases with twelve hostiles and four pets. Timing uses emulated 16 MHz cycles from submitted input to readiness or an ordinary displayed [more] page, validates completed gameplay, and acknowledges every page. Generation and presentation-only animation remain excluded under the existing harness definition. The maximum is **99.568 ms, pets_summon**.

| Companion case | Worst ms |
| --- | --- |
| pets_dense | 92.427 |
| pets_swap | 97.918 |
| pets_recall | 95.371 |
| pets_summon | 99.568 |
| pets_study | 97.821 |
| pets_domination | 85.855 |
| pets_friendly_fire | 98.413 |
| pets_magic | 92.427 |
| pets_traps | 94.558 |
| pets_recovery | 92.888 |
| pets_rest | 66.103 |

Localized performance changes include packed occupancy comparison, fixed row-presence filters for doors/features, lazy cached pet targeting, skipping empty effect aging and unnecessary regeneration lookups, incremental sixteen-bit scheduler bits, shallow decision/attack and UI/action dispatch, one-read flash ability lookup, direct flash font advances, and the existing terrain renderer's fixed-row unroll. They retain gameplay rather than reducing hostile population or weakening workloads. Dense combat, traps, recall, friendly fire, actual idle healing and recovery denial have result assertions.

Native validation: **32/32 CTest suites pass**, 38.65 seconds in the final parallel run. Coverage includes all summons/cost rejection, taming odds/immunity/modifiers, capacity and duplicate rules, save/resume, 64 full-party floor transitions, traps/swaps/doors, safe/silent recall and grace, creature combat/rewards/Lord, interception/chains/area spells, healing/study, early generation across 1,000 seeds, UI/icons and both agents' information/tactics/attribution. Explorer boundary and renderer visibility checks pass. Git whitespace validation is clean.

Two historical explorer fixtures needed the balance run's 100,000-action budget: seed 19 legitimately escapes after 21,314 actions, and seed 6725 with old consumables legitimately starves after 20,473. Rejection, repeated-swap, path and identical-state stall guards remain enabled. A real oracle failure at seed 3170 was corrected: crossing the reserve-fuel threshold formerly made candle acquisition exceed retention and reverse-swap a useful book forever. A deterministic threshold fixture covers the defect; the corrected seed escapes. Failed development cohorts are excluded.

## Paired experiment and provenance

Each agent runs effective seeds **1–10,000**, pets disabled/enabled, from the same frozen candidate executable, identical policy within each pair, four workers per arm, full telemetry plus entry state, and a 100,000-action cap. All 40,000 runs must terminate as escaped or death; stalls/errors invalidate a cohort. Disabled removes new availability, restores the same-slot ordinary loot comparator, omits the independent book schedule and retains ordinary altar outcomes. Gameplay RNG is never resynchronized after paths diverge.

Frozen executable SHA-256: `18c2a4b6db1ca4066779172329646f049a6208444e9bd125f3cf5d05f5d3025c`.

Every arm seals all sixteen CSV streams, executable identity, source snapshot, complete experiment specification and deterministic result hash. Raw datasets, profiles, traces and manifests stay under ignored `build/pets/`; only this compact report is committed. Core telemetry is 11, public observations 10, spell/magic sidecars 2. The public companion roster gives explorer-v1 its own screen-visible identity, HP/max HP, origin and active flags, while withholding offscreen positions, enemy HP, hidden timers, terrain, traps and scent locations.

Policies distinguish friends in navigation and risk, swap through them, avoid unsafe rays/areas/chains/displacement, use summons and each taming method, and can dismiss a weak full-party member for a substantial upgrade. Summons are quiet, depth-prioritized, capped conservatively at one/two/three in early/middle/late policy stages, guarded by a 96-turn replacement cooldown and emergency MP reserves. The ordinary 2 + floor/2 study budget remains; summons cannot be studied before their mana cost is affordable. Neither agent receives pet telemetry.

| Agent | Disabled escape | Enabled escape | Delta, pp | 95% paired CI, pp | Loss / gain | Failures |
| --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | 92/10,000 (0.92%) | 109/10,000 (1.09%) | +0.17 | [-0.07, 0.43] | 77 / 94 | 0 |
| omniscient-v2 | 4530/10,000 (45.30%) | 3607/10,000 (36.07%) | -9.23 | [-10.17, -8.29] | 1665 / 742 | 0 |

The finite contiguous seed range gives an exact result for these 10,000 starting seeds. The paired percentile bootstrap intervals (2,000 fixed-PRNG resamples) and exact McNemar tests assume the range is representative when extrapolating. They are not a full 65,535-seed census. Primary escape is prespecified; secondary binary outcomes and all floor reach/exit comparisons receive Benjamini–Hochberg FDR adjustment in the retained analysis. Resource totals, reached-floor species and completed lifetimes are descriptive, affected by route choice and survivor selection.

Explorer's +0.17-point escape difference has p=0.221 and a confidence interval spanning zero; it is not convincing evidence of a general improvement. Oracle's −9.23-point difference is a 20.38% relative reduction, p=8.74×10⁻⁸¹. Its floor-12 reach is broadly similar, but floor-15 reach and Lord kills fall materially. The decline is concentrated in later combat, rather than the special phantom floor or an invalid simulator population.

| Agent | Secondary outcome | Off | On | Delta, pp | FDR q |
| --- | --- | --- | --- | --- | --- |
| explorer-v1 | reached_floor_12 | 33.66% | 37.14% | +3.48 | 4.37e-10 |
| explorer-v1 | reached_floor_15 | 8.04% | 8.39% | +0.35 | 0.31 |
| explorer-v1 | lord_killed | 5.38% | 6.45% | +1.07 | 0.000592 |
| explorer-v1 | yendor_acquired | 5.37% | 6.45% | +1.08 | 0.000592 |
| explorer-v1 | post_yendor_death | 4.45% | 5.36% | +0.91 | 0.00158 |
| omniscient-v2 | reached_floor_12 | 85.23% | 84.73% | -0.50 | 0.203 |
| omniscient-v2 | reached_floor_15 | 62.02% | 53.89% | -8.13 | 4.6e-60 |
| omniscient-v2 | lord_killed | 57.55% | 49.27% | -8.28 | 4.33e-60 |
| omniscient-v2 | yendor_acquired | 57.55% | 49.27% | -8.28 | 4.33e-60 |
| omniscient-v2 | post_yendor_death | 12.25% | 13.20% | +0.95 | 0.0336 |

## Replay review and old-content compatibility

| Agent | Seed | Direction | Off | On | On terminal cause | On pet damage | On pet kills | On summon MP |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | 19 | gain | death | escaped | escaped | 403 | 40 | 217 |
| explorer-v1 | 122 | loss | escaped | death | fire:DRAGON | 533 | 28 | 112 |
| omniscient-v2 | 21 | gain | death | escaped | escaped | 56 | 5 | 21 |
| omniscient-v2 | 13 | loss | escaped | death | DRAGON | 150 | 14 | 28 |

All eight replays match the corresponding cohort action hash and terminal result. Explorer seed 19 changes Polymorph availability into Domination before later acquisition; its pets deal 403 damage and kill forty enemies, and the run escapes instead of dying to the Lord. Seed 122 changes an early study/light decision, has more study and less ordinary casting, and eventually dies to dragon fire despite 533 pet damage. Oracle seed 21 initially replaces Haste with the guaranteed book; routes and combat draws diverge and the enabled run escapes instead of dying to a Rattlesnake. Oracle seed 13 pursues a nearer guaranteed book, later has fewer ordinary casts, and dies to an ascent Dragon despite fourteen pet kills. These are plausible complete-package gains/losses, not identified causal contributions of one species or one MP cost. No RNG reset makes later dice artificially match.

The preceding same-core disabled development reference is reproduced exactly for explorer-v1: all 10,000 action hashes and terminal outcomes match. The oracle reserve-light fix changes 62 hashes and seven terminal outcomes, moving 4,529 escapes to 4,530; it establishes the fresh maintained-policy reference used in both final arms. Earlier multi-book archives used a dirty 679f0cb build before the current f2083c6 base, older observations and policies. Against that historical candidate, 9,847 explorer and 8,267 oracle terminal results match, while exact hashes match only one and 978 respectively. The older 0.89% / 45.69% rates therefore are context, not the disabled arm of this experiment. No exact backwards-compatibility claim is made for changed appearances/protocols, policy or prior build state.

## Acquisition, combat, deaths and recalls

Counts below are totals across the 10,000 enabled runs per agent, including repeated acquisitions. Lifetime averages use completed pet deaths only; living pets and dismissed pets are not counted as completed deaths. Damage absorbed is applied damage, including friendly damage. Control ticks/strengths are assignments, not measured enemy turns or HP prevented.

| Metric | explorer-v1 | omniscient-v2 |
| --- | --- | --- |
| Runs acquiring a pet | 6,587 | 9,581 |
| Runs casting a summon | 6,075 | 8,798 |
| Total acquisitions | 30,715 | 58,596 |
| Mean completed lifetime, turns | 1059.382 | 710.054 |
| Hit rate | 0.775 | 0.738 |
| Mean recall distance | 6.043 | 7.108 |
| Spell acquisitions | 27,106 | 26,961 |
| Scroll acquisitions | 402 | 11,092 |
| Potion acquisitions | 837 | 5,344 |
| Wand acquisitions | 1,198 | 13,188 |
| Altar acquisitions | 1,172 | 2,011 |
| Scroll attempts | 422 | 12,610 |
| Scroll successes | 402 | 11,092 |
| Potion attempts | 1,756 | 11,883 |
| Potion successes | 837 | 5,344 |
| Wand attempts | 1,710 | 21,668 |
| Wand successes | 1,198 | 13,188 |
| Pet attacks | 807,413 | 688,508 |
| Pet hits | 625,758 | 508,265 |
| Pet damage dealt | 1,668,140 | 1,429,371 |
| Pet kills | 131,136 | 122,853 |
| Base XP from pet kills | 1,419,471 | 1,420,576 |
| Actual XP awarded | 1,467,811 | 1,448,406 |
| Control applications | 5,667 | 7,161 |
| Assigned control ticks/strength | 51,200 | 48,126 |
| Damage absorbed | 562,337 | 884,869 |
| Friendly damage | 12,938 | 3,420 |
| Pet deaths | 29,203 | 54,544 |
| Hostile-caused deaths | 28,473 | 53,047 |
| Player-caused deaths | 530 | 185 |
| Trap-caused deaths | 200 | 1,312 |
| Other deaths | 0 | 0 |
| Dismissals | 0 | 15 |
| Recalls | 290,595 | 930,073 |
| Visible recalls | 47,980 | 923,281 |
| Idle HP restored | 173,468 | 274,383 |
| Player spell ray interceptions | 274 | 0 |
| Player spell chain hits | 0 | 0 |
| Friendly spell control/displacement events | 28 | 0 |
| Pet-event study interruptions | 380 | 1,055 |

Offscreen recall succeeds much more often in explorer play. Visible recalls are 16.51% of explorer recalls and 99.27% of oracle recalls. Bright lighting leaves few hidden radius-three landing cells, and fast travel can outrun slow companions; safe visible fallback prevents permanent separation but can be noticeable. This is a quality-of-life limitation even though recalls are silent and cooldown-bound. The highest observed entry/exit roster is four in both agents, and every visit stays within the cap.

## Species distribution

Acquisitions are source-summed counts. Completed mean lifetime and active pet-turn occupancy describe different populations; a surviving pet can contribute many active turns without entering the lifetime mean.

| Agent | Species | Acquired | Damage | Kills | Deaths | Mean completed lifetime | Active turns |
| --- | --- | --- | --- | --- | --- | --- | --- |
| explorer-v1 | BAT | 54 | 152 | 38 | 47 | 1493.0 | 82,314 |
| explorer-v1 | GOBLIN | 1,192 | 30,333 | 3,180 | 1,114 | 1536.9 | 1,847,043 |
| explorer-v1 | GOLEM | 158 | 3,475 | 205 | 119 | 603.8 | 240,886 |
| explorer-v1 | GRIFFIN | 5 | 161 | 8 | 5 | 480.2 | 2,401 |
| explorer-v1 | HOBGOBLIN | 365 | 19,848 | 1,715 | 334 | 1510.8 | 566,593 |
| explorer-v1 | HOUND | 24,171 | 1,216,898 | 102,461 | 23,122 | 1036.9 | 26,200,830 |
| explorer-v1 | ORC | 439 | 20,525 | 1,946 | 386 | 1819.2 | 806,620 |
| explorer-v1 | RATTLESNAKE | 75 | 669 | 137 | 72 | 945.9 | 69,603 |
| explorer-v1 | SNAKE | 144 | 1,019 | 208 | 137 | 1029.0 | 145,294 |
| explorer-v1 | SPIDER | 1,164 | 39,536 | 3,181 | 1,069 | 1240.2 | 1,544,507 |
| explorer-v1 | TARANTULA | 151 | 3,046 | 256 | 144 | 883.8 | 134,534 |
| explorer-v1 | TROLL | 12 | 1,046 | 52 | 7 | 819.3 | 12,396 |
| explorer-v1 | WISP | 2,785 | 331,432 | 17,749 | 2,647 | 842.4 | 2,697,300 |
| omniscient-v2 | BAT | 983 | 1,922 | 365 | 952 | 1160.5 | 1,142,314 |
| omniscient-v2 | GOBLIN | 10,085 | 189,363 | 20,218 | 9,943 | 972.3 | 10,008,301 |
| omniscient-v2 | GOLEM | 33 | 989 | 48 | 21 | 562.9 | 32,816 |
| omniscient-v2 | GRIFFIN | 1,415 | 12,859 | 670 | 1,263 | 296.5 | 594,015 |
| omniscient-v2 | HOBGOBLIN | 5,449 | 150,487 | 12,504 | 5,275 | 800.9 | 4,535,218 |
| omniscient-v2 | HOUND | 25,208 | 634,074 | 55,209 | 22,452 | 577.9 | 14,803,097 |
| omniscient-v2 | ORC | 3,861 | 114,855 | 11,029 | 3,757 | 1056.4 | 4,193,135 |
| omniscient-v2 | RATTLESNAKE | 715 | 5,715 | 1,065 | 705 | 807.9 | 576,058 |
| omniscient-v2 | SNAKE | 1,721 | 11,956 | 2,516 | 1,697 | 860.4 | 1,476,885 |
| omniscient-v2 | SPIDER | 1,499 | 26,761 | 2,124 | 1,351 | 621.9 | 1,012,528 |
| omniscient-v2 | TARANTULA | 3,282 | 44,978 | 3,695 | 3,232 | 588.0 | 1,968,275 |
| omniscient-v2 | TROLL | 2,113 | 76,618 | 4,931 | 1,804 | 432.1 | 1,246,234 |
| omniscient-v2 | WISP | 2,232 | 158,794 | 8,479 | 2,092 | 406.9 | 1,032,684 |

## Ordinary magic, weapons, consumables and time

| Metric | Explorer off | Explorer on | Oracle off | Oracle on |
| --- | --- | --- | --- | --- |
| Mean actions/run | 5,489.0 | 5,966.0 | 5,190.1 | 4,805.5 |
| Median actions/run | 5,413.0 | 6,117.0 | 5,484.0 | 4,738.5 |
| Mean turns/run | 5,428.6 | 5,777.6 | 5,161.3 | 4,778.2 |
| Median turns/run | 5,373.0 | 5,908.5 | 5,454.0 | 4,713.0 |
| Ordinary casts | 458,286 | 409,405 | 1,349,093 | 1,137,532 |
| Ordinary spell MP | 1,116,090 | 992,932 | 2,916,425 | 2,458,872 |
| Summon casts | 0 | 27,106 | 0 | 26,961 |
| Summon MP | 0 | 198,957 | 0 | 195,738 |
| Study turns | 2,256,981 | 2,439,083 | 3,977,772 | 4,209,778 |
| Study interruptions | 8,529 | 9,895 | 6,706 | 7,934 |
| Melee/bow unscoped physical damage | 8,706,131 | 7,660,669 | 15,091,304 | 12,732,085 |
| Melee-weapon equipped turns | 51,176,294 | 54,321,145 | 49,320,798 | 45,210,694 |
| Player attack events (all methods) | 3,933,547 | 3,571,712 | 6,469,770 | 5,588,236 |
| Arrows fired | 12,248 | 11,078 | 127,702 | 126,328 |
| Bow shot damage | 40,826 | 37,103 | 505,721 | 496,387 |
| Ordinary potions drunk | 163,827 | 140,749 | 283,764 | 249,485 |
| Ordinary potions thrown | 11,181 | 7,064 | 65,593 | 44,009 |
| Ordinary scrolls read | 96,659 | 79,781 | 73,763 | 53,856 |
| Ordinary wand activations | 8,120 | 5,921 | 39,482 | 30,328 |
| Ordinary wand charges | 29,598 | 19,764 | 205,609 | 143,693 |
| All consumed-use events | 434,695 | 404,359 | 695,634 | 631,336 |

Primary melee and bow damage use the existing unscoped attribution. The physical subtotal subtracts independently attributed pet, spell and item damage from player-credit floor damage; it includes ordinary melee and bow shots. Bow rows use actual shot telemetry. Equipment turns describe weapon use duration, not attack frequency. Player attack events include projectile/item attacks and are not pure melee counts. Exact melee-only attack counts are a remaining telemetry limitation. Per-spell detail follows, so a fall in aggregate ordinary casts can be checked against specific emergency/control spells rather than attributed automatically to companions replacing all magic.

| Spell, casts / MP | Explorer off | Explorer on | Oracle off | Oracle on |
| --- | --- | --- | --- | --- |
| Magic Dart | 253,100 / 506,200 | 248,508 / 497,016 | 1,147,824 / 2,295,648 | 980,496 / 1,960,992 |
| Frostbind | 9,235 / 18,470 | 3,287 / 6,574 | 20,897 / 41,794 | 9,981 / 19,962 |
| Confound | 3,633 / 7,266 | 1,553 / 3,106 | 5,227 / 10,454 | 3,008 / 6,016 |
| Repulsion | 73,519 / 220,557 | 84,396 / 253,188 | 69,098 / 207,294 | 73,742 / 221,226 |
| Blink | 55,434 / 110,868 | 26,540 / 53,080 | 28,588 / 57,176 | 13,411 / 26,822 |
| Piercing Bolt | 8,386 / 33,544 | 9,016 / 36,064 | 1,350 / 5,400 | 1,780 / 7,120 |
| Sense Traps | 1,991 / 7,964 | 1,342 / 5,368 | 2,492 / 9,968 | 1,206 / 4,824 |
| Cleanse | 587 / 1,761 | 454 / 1,362 | 5,305 / 15,915 | 3,562 / 10,686 |
| Banish | 11,771 / 47,084 | 8,483 / 33,932 | 4,916 / 19,664 | 5,528 / 22,112 |
| Shockwave | 2,599 / 10,396 | 2,155 / 8,620 | 874 / 3,496 | 915 / 3,660 |
| Exchange | 19,168 / 76,672 | 14,198 / 56,792 | 40,279 / 161,116 | 32,407 / 129,628 |
| Fireburst | 64 / 256 | 45 / 180 | 1,885 / 7,540 | 705 / 2,820 |
| Phase Step | 10,639 / 42,556 | 5,447 / 21,788 | 6,800 / 27,200 | 3,940 / 15,760 |
| Chain Lightning | 2,989 / 11,956 | 2,499 / 9,996 | 10,726 / 42,904 | 6,063 / 24,252 |
| Slumber | 5,099 / 20,396 | 1,451 / 5,804 | 2,596 / 10,384 | 708 / 2,832 |
| Blood Bolt | 72 / 144 | 31 / 62 | 236 / 472 | 80 / 160 |
| Summon Hound | 0 / 0 | 23,631 / 165,417 | 0 / 0 | 23,996 / 167,972 |
| Summon Spider | 0 / 0 | 763 / 6,104 | 0 / 0 | 975 / 7,800 |
| Summon Wisp | 0 / 0 | 2,554 / 25,540 | 0 / 0 | 1,957 / 19,570 |
| Summon Golem | 0 / 0 | 158 / 1,896 | 0 / 0 | 33 / 396 |

## Floor survival, phantom floor and ascent

Floors use zero-based indices. Entered/exited are unique runs, not visit totals; paired reach/exit rates use all 10,000 seeds. Mean entry pets and time-weighted pets use reached visits and are conditional descriptive quantities. The all-phantom floor remains index 4 with twelve Phantom generation slots; it is excluded from the book guarantee. Ascent still generates no new books.

### explorer-v1

| Direction | Floor | Off entered / exited | On entered / exited | On mean entry pets | On time-weighted pets |
| --- | --- | --- | --- | --- | --- |
| descent | 0 | 10,000 / 9,754 | 10,000 / 9,738 | 0.00 | 0.01 |
| descent | 1 | 9,754 / 9,462 | 9,738 / 9,418 | 0.02 | 0.04 |
| descent | 2 | 9,462 / 8,556 | 9,418 / 8,423 | 0.05 | 0.11 |
| descent | 3 | 8,556 / 7,978 | 8,423 / 7,800 | 0.17 | 0.35 |
| descent | 4 | 7,978 / 7,595 | 7,800 / 7,395 | 0.42 | 0.47 |
| descent | 5 | 7,595 / 7,144 | 7,395 / 6,945 | 0.44 | 0.57 |
| descent | 6 | 7,144 / 6,456 | 6,945 / 6,353 | 0.56 | 0.74 |
| descent | 7 | 6,456 / 6,079 | 6,353 / 6,023 | 0.85 | 0.96 |
| descent | 8 | 6,079 / 5,507 | 6,023 / 5,604 | 0.98 | 0.97 |
| descent | 9 | 5,507 / 5,173 | 5,604 / 5,369 | 0.98 | 1.00 |
| descent | 10 | 5,173 / 4,252 | 5,369 / 4,576 | 0.99 | 0.95 |
| descent | 11 | 4,252 / 3,366 | 4,576 / 3,714 | 0.94 | 0.98 |
| descent | 12 | 3,366 / 2,383 | 3,714 / 2,742 | 1.04 | 0.93 |
| descent | 13 | 2,383 / 1,435 | 2,742 / 1,630 | 0.95 | 0.80 |
| descent | 14 | 1,435 / 804 | 1,630 / 839 | 0.83 | 0.64 |
| descent | 15 | 804 / 537 | 839 / 644 | 0.66 | 1.03 |
| ascent | 0 | 109 / 92 | 121 / 109 | 1.33 | 1.34 |
| ascent | 1 | 122 / 109 | 146 / 121 | 1.33 | 1.36 |
| ascent | 2 | 137 / 122 | 161 / 146 | 1.34 | 1.34 |
| ascent | 3 | 152 / 137 | 177 / 161 | 1.35 | 1.36 |
| ascent | 4 | 169 / 152 | 209 / 177 | 1.45 | 1.39 |
| ascent | 5 | 186 / 169 | 240 / 209 | 1.53 | 1.49 |
| ascent | 6 | 199 / 186 | 261 / 240 | 1.56 | 1.58 |
| ascent | 7 | 215 / 199 | 286 / 261 | 1.52 | 1.57 |
| ascent | 8 | 227 / 215 | 307 / 286 | 1.50 | 1.52 |
| ascent | 9 | 236 / 227 | 323 / 307 | 1.41 | 1.50 |
| ascent | 10 | 249 / 236 | 340 / 323 | 1.36 | 1.41 |
| ascent | 11 | 261 / 249 | 353 / 340 | 1.11 | 1.29 |
| ascent | 12 | 291 / 261 | 386 / 353 | 0.90 | 1.03 |
| ascent | 13 | 350 / 291 | 486 / 386 | 0.65 | 0.77 |
| ascent | 14 | 537 / 350 | 644 / 486 | 1.43 | 0.84 |
| ascent | 15 | 0 / 0 | 0 / 0 | — | — |

### omniscient-v2

| Direction | Floor | Off entered / exited | On entered / exited | On mean entry pets | On time-weighted pets |
| --- | --- | --- | --- | --- | --- |
| descent | 0 | 10,000 / 9,791 | 10,000 / 9,771 | 0.00 | 0.11 |
| descent | 1 | 9,791 / 9,744 | 9,771 / 9,719 | 0.18 | 0.29 |
| descent | 2 | 9,744 / 9,643 | 9,719 / 9,614 | 0.36 | 0.57 |
| descent | 3 | 9,643 / 9,584 | 9,614 / 9,572 | 0.74 | 0.92 |
| descent | 4 | 9,584 / 9,566 | 9,572 / 9,542 | 1.03 | 0.89 |
| descent | 5 | 9,566 / 9,519 | 9,542 / 9,494 | 0.85 | 1.15 |
| descent | 6 | 9,519 / 9,434 | 9,494 / 9,414 | 1.20 | 1.29 |
| descent | 7 | 9,434 / 9,378 | 9,414 / 9,367 | 1.39 | 1.48 |
| descent | 8 | 9,378 / 9,290 | 9,367 / 9,298 | 1.55 | 1.57 |
| descent | 9 | 9,290 / 9,245 | 9,298 / 9,257 | 1.59 | 1.61 |
| descent | 10 | 9,245 / 8,972 | 9,257 / 8,953 | 1.63 | 1.55 |
| descent | 11 | 8,972 / 8,523 | 8,953 / 8,473 | 1.53 | 1.47 |
| descent | 12 | 8,523 / 8,075 | 8,473 / 7,886 | 1.50 | 1.22 |
| descent | 13 | 8,075 / 7,243 | 7,886 / 6,704 | 1.17 | 0.81 |
| descent | 14 | 7,243 / 6,202 | 6,704 / 5,389 | 0.84 | 0.57 |
| descent | 15 | 6,202 / 5,747 | 5,389 / 4,916 | 0.57 | 0.61 |
| ascent | 0 | 4,530 / 4,530 | 3,607 / 3,607 | 0.87 | 0.87 |
| ascent | 1 | 4,530 / 4,530 | 3,608 / 3,607 | 0.85 | 0.86 |
| ascent | 2 | 4,530 / 4,530 | 3,609 / 3,608 | 0.87 | 0.86 |
| ascent | 3 | 4,532 / 4,530 | 3,609 / 3,609 | 0.80 | 0.87 |
| ascent | 4 | 4,533 / 4,532 | 3,614 / 3,609 | 0.40 | 0.50 |
| ascent | 5 | 4,534 / 4,533 | 3,616 / 3,614 | 0.40 | 0.39 |
| ascent | 6 | 4,539 / 4,534 | 3,617 / 3,616 | 0.39 | 0.38 |
| ascent | 7 | 4,540 / 4,539 | 3,623 / 3,617 | 0.39 | 0.38 |
| ascent | 8 | 4,546 / 4,540 | 3,628 / 3,623 | 0.38 | 0.37 |
| ascent | 9 | 4,551 / 4,546 | 3,638 / 3,628 | 0.37 | 0.36 |
| ascent | 10 | 4,597 / 4,551 | 3,671 / 3,638 | 0.36 | 0.36 |
| ascent | 11 | 4,673 / 4,597 | 3,745 / 3,671 | 0.35 | 0.33 |
| ascent | 12 | 4,845 / 4,673 | 3,931 / 3,745 | 0.37 | 0.34 |
| ascent | 13 | 5,245 / 4,845 | 4,357 / 3,931 | 0.40 | 0.36 |
| ascent | 14 | 5,747 / 5,245 | 4,916 / 4,357 | 0.74 | 0.48 |
| ascent | 15 | 0 / 0 | 0 / 0 | — | — |


## Controlled party arenas

A separate production-core arena evaluates 512 seeds × nine parties × seven enemy scenarios = **32,256 cases**. The level-20, 80-HP player wears ordinary plate, sword and bright light, and waits without attacking. Full-health companions are granted for free, with no learning/acquisition cost. Enemies start nearby; cases stop on clear/death or 128 turns. This deliberately favorable party test isolates combat capability rather than ordinary campaign availability.

Success below requires **all enemies cleared and the player alive**. Censored cases are unresolved within 128 turns and are reported separately, never treated as cleared or as simulator failure/death. Four-Wisp and four-Golem parties are deliberately infeasible stress comparisons: production allows only one of each. Four Trolls are possible only after repeated resistant taming.

| Party; clear-alive % (censored %) | 1 Phantom | 4 Phantoms | 12 Phantoms | 1 Dragon | 4 Dragons | 12 Dragons | 1 Lord |
| --- | --- | --- | --- | --- | --- | --- | --- |
| none | 0.00% (10.55%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) |
| hound | 89.84% (1.17%) | 0.00% (1.76%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) |
| spider | 0.00% (21.88%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (13.48%) |
| wisp | 31.64% (10.35%) | 5.27% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) |
| golem | 0.00% (84.77%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) |
| mixed | 100.00% (0.00%) | 73.83% (0.78%) | 0.00% (0.00%) | 27.15% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 99.80% (0.00%) |
| trolls | 0.00% (73.24%) | 0.00% (0.00%) | 0.00% (0.00%) | 57.23% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 99.80% (0.00%) |
| wisps_infeasible | 100.00% (0.00%) | 100.00% (0.00%) | 100.00% (0.00%) | 37.30% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 100.00% (0.00%) |
| golems_infeasible | 0.00% (84.77%) | 0.00% (0.00%) | 0.00% (0.00%) | 54.30% (0.00%) | 0.00% (0.00%) | 0.00% (0.00%) | 94.53% (0.00%) |

One Golem clears none of the Dragon/Lord cases; it cannot independently replace late-game offense in these arenas. A legal mixed party clears the isolated Lord in 511/512 cases while the player waits. This is a real balance concern for a fully assembled healthy party against a lone boss. Four Trolls do similarly, but repeated resistant taming is costly. Full dragon and phantom groups remain lethal to the legal mixed party. Four unsupported Wisps clear even twelve Phantoms, supporting the one-active-species restriction.

## Interpretation and remaining limitations

Companions provide damage, interception, distraction, control and full XP, but their complete availability package does not automatically improve campaign escape rates. The aggregate paired result includes lost ordinary loot, changed inventory choices, learning turns, consumed MP, divergent routes and combat draws. The explorer remains below the longer-term 2–5% target; no unrelated enemy, spell, survival or information-boundary changes force that target.

The oracle decline is material and should not be described as successful difficulty-neutral tuning. Conservative study/summon reserves preserve other magic but still impose an opportunity cost; an agent can exploit this package less efficiently than a human. Isolated mixed-party boss strength and campaign deterioration coexist. This implementation keeps the requested expensive costs and permanent-vulnerable model; it does not hide the tradeoff with arbitrary expiry or max-mana reservation.

A visible recall uses the ordinary redraw without a separate flash animation. Local greedy following relies on bounded recall for complex obstacles. Custom maps with insufficient entrance space can temporarily retain an unplaced living instance. Spider weakening reuses the permanent ordinary poison marker until restoration. Polymorph retains allegiance and ordinary species-change semantics, so a tamed creature may eventually change beyond the normal tameable roster; this remains a possible power route. Pet HP recovery is modest except for a tamed Troll's inherited regeneration.

The final stack bound is exactly 256 and production globals use 1,023/1,024 bytes. The slowest measured turn has about 0.432 ms headroom. Future changes must rerun linker proof and all 140 cases. Validation uses native execution and the AVM interpreter; a physical Arduboy playtest has not been performed. Sampling and these agents cannot rule out rare exploits or optimal human strategies. The arena checks are bounded combat cases, not a substitute for complete campaign outcomes.

## Retained evidence and reproduction

- `build/pets/accepted-specification.json`: source snapshot, controls, executable and arena hashes.
- `build/pets/accepted-ctest.log`: final 32-suite pass.
- `build/pets/avm-build.log`: complete 256-byte linker proofs with no gaps.
- `build/pets/elf-baseline.txt`, `elf-final.txt`: section-size comparisons.
- `build/pets/complete-benchmarks/20261014T031243Z-8v9c6a7v/`: all 140 measurements, outcome checks, profiles and summary.
- `build/pets/accepted-10k/`: four sealed full-telemetry arms, paired comparisons, floor/species/resource summaries and FDR-adjusted floor tests.
- `build/pets/accepted-arenas.csv`, `accepted-arena-summary.csv`: bounded combat cases and censored fractions.
- `build/pets/accepted-traces/`: representative discordant paired replays and action-hash verification.

Rebuild native with `cmake --build build/multispell-native2 --parallel 8` and run `ctest --test-dir build/multispell-native2 --output-on-failure -j 4`. Rebuild AVM targets with the configured SDK and run `python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root <sdk> --check --output build/pets/recheck`. Fresh simulation commands are documented in [sim/README.md](../sim/README.md); keep matched seeds, one executable, matching policy and all stall guards.

## Files changed

### Build and packaged firmware

`CMakeLists.txt`, `README.md`, `ardurogue2.arduboy`

### Gameplay and UI

`src/app_state.hpp`, `src/combat.cpp`, `src/companions.cpp`, `src/companions.hpp`, `src/features.cpp`, `src/features.hpp`, `src/inventory_view.hpp`, `src/items.cpp`, `src/main.cpp`, `src/model.hpp`, `src/render.cpp`, `src/sim_hooks.hpp`, `src/spells.cpp`, `src/spells.hpp`, `src/state.cpp`, `src/status.cpp`, `src/status.hpp`, `src/status_advances.hpp`, `src/ui.cpp`, `src/ui.hpp`, `src/world.cpp`, `src/world.hpp`, `src/world_gen.cpp`, `src/world_gen_population.cpp`

### Native correctness

`tests/CMakeLists.txt`, `tests/artifact_checks.cpp`, `tests/awareness_checks.cpp`, `tests/book_mana_checks.cpp`, `tests/bow_checks.cpp`, `tests/companion_checks.cpp`, `tests/consumable_checks.cpp`, `tests/feature_checks.cpp`, `tests/format_native.cpp`, `tests/game_native.cpp`, `tests/generation_checks.cpp`, `tests/lighting_checks.cpp`, `tests/magic_expansion_checks.cpp`, `tests/render_native.cpp`, `tests/spell_checks.cpp`, `tests/ui_native.cpp`, `tests/weapon_checks.cpp`

### Simulator, policies and analysis

`sim/BALANCE.md`, `sim/CMakeLists.txt`, `sim/README.md`, `sim/agent.hpp`, `sim/balance.py`, `sim/experiment.cpp`, `sim/explorer.hpp`, `sim/explorer_agent.cpp`, `sim/explorer_checks.cpp`, `sim/explorer_observation.cpp`, `sim/frontend.cpp`, `sim/main.cpp`, `sim/metrics.cpp`, `sim/metrics.hpp`, `sim/omniscient_agent.cpp`, `sim/pet_arenas.cpp`, `sim/pet_checks.cpp`, `sim/policy_checks.cpp`, `sim/simulator.cpp`, `sim/spell_preferences.hpp`, `sim/spell_report.py`, `sim/survival_diagnostics.cpp`, `sim/test_balance_cli.py`, `sim/test_check_balance.py`, `sim/test_cli.py`, `sim/test_explorer_cli.py`, `sim/tests.cpp`, `sim/trace.cpp`

### AVM benchmarks

`bench/README.md`, `bench/bench.cpp`, `bench/profile_turns.py`, `bench/test_profile_turns.py`

### Documentation

`docs/EXPLORER_AGENT.md`, `docs/PETS-RESULTS.md`, `docs/PETS.md`, `docs/SPELLS.md`
