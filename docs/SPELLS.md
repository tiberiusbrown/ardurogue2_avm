# Learned magic

Every character starts knowing Magic Dart, with 6/6 MP. Maximum mana is
`min(16, 6 + level / 3)`. One MP regenerates on every sixteenth ordinary turn,
including waits and study, unless starving. The eight-bit turn clock wraps
without changing that cadence. Spells use ordinary player/enemy action costs,
hunger, fuel and effect aging. Paralysis prevents casting. There are no classes,
spell failures, proficiency levels, intelligence/wisdom attributes or new saved
player buff timers.

## Spells

| Spell | MP | Study turns | Effect |
| --- | ---: | ---: | --- |
| Magic Dart | 2 | 40 | Cardinal ray, range 5; first monster takes 2–4 magic damage. Known initially. |
| Frostbind | 3 | 40 | Cardinal ray, range 5; first monster is slowed for 4–6 turns. |
| Repulsion | 3 | 40 | Cardinal ray, range 5; push the first monster 1–2 tiles. |
| Confound | 3 | 40 | Cardinal ray, range 5; first monster is confused for 4–6 turns. |
| Sense Traps | 3 | 40 | Discover hidden traps within circular radius 4 and mark their exact tiles on the map; traps remain active. |
| Blink | 4 | 60 | Random valid tile within circular radius 3, including across walls and on traps. |
| Shockwave | 5 | 60 | Snapshot all eight adjacent positions; push their monsters directly outward 2–3 tiles each. |
| Piercing Bolt | 5 | 60 | Cardinal ray, total range 5; first two monsters each take 3–5 magic damage. |
| Banish | 5 | 60 | Cardinal ray, range 5; teleport the first monster using ordinary monster teleport placement. |
| Exchange | 5 | 80 | Swap with a selected visible monster within circular radius 3. |
| Cleanse | 5 | 60 | Remove exactly one condition: blindness, then confusion, then slowing. |
| Fireburst | 6 | 80 | Range-5 projectile; radius-1 square explosion at first monster or last clear tile, 3–6 fire damage to each monster and the player. Dragons ignore fire; player fire protection and MR apply. |
| Phase Step | 5 | 60 | Cardinal teleport: prefer exactly 3 tiles, otherwise 2. Cross monsters and intermediate traps, never walls/closed doors/edges; occupied landings are invalid. Destination traps activate normally. |
| Chain Lightning | 6 | 80 | Range-5 first hit for 3–5 magic damage; one visible secondary within Chebyshev 2 takes 3–4. Clear geometric jump path required, nearest target then lowest slot. Invisible monsters cannot receive secondary arcs; lightning can strike and reveal a disguised mimic. |
| Slumber | 5 | 60 | Range-4 ray; first monster stunned for 3–4 enemy actions, refreshing to the greater duration. The immediate enemy response consumes a stun action. |
| Blood Bolt | 3 + **3 HP** | 60 | Range-5 first hit for 6–9 magic damage. Sacrifice current HP before firing, even on a miss; bypasses all mitigation, can kill the caster. Phoenix Heart uses its ordinary resurrection lifecycle. No bolt or enemy turn after final death. |

Names and descriptions are flash strings. A single `SPELL_DEFINITIONS` roster defines IDs, mana/HP costs, study durations, targeting modes and weighted generation categories. IDs 0–10 remain unchanged; IDs 11–15 fill the existing 16-bit known-spell mask. Compile-time checks guard coverage and the two-byte Item layout. Identify and Remove Curse remain future possibilities, with no
spell IDs or effects implemented. Illumination, Arcane Ward, Stone to Mud,
Survey and Fetch are absent.

Rays share wand scanning, collision, animation and ordinary magic death/reward
handling. Walls, map edges and closed doors block them. Lighting does not change
their five-tile range. Like wand rays, a chosen bearing can hit an unseen monster,
but hit text respects invisibility. Exchange additionally requires actual sight:
lighting, blindness, invisibility and line of sight all apply. The existing model
has no monster magic-resistance stat; player MR and ordinary wand rules remain.
Monster conditions age during the following ordinary enemy response, as existing
potion conditions do.

Knockback resolves every intermediate tile in stable monster-slot order. Walls,
closed doors, map boundaries, occupied tiles and blocked diagonal corners stop
movement. Obstruction grants at most two existing stun ticks; recasting does not
accumulate stun. Intermediate traps activate once per entry. Death or trap
teleportation stops that monster's original push. Shockwave snapshots membership
before movement or summoning, so newly summoned monsters are not added to the
wave. Immobility flags do not prevent forced movement. Direct spell kills grant
ordinary rewards, including the Lord's drop. Environmental trap kills and
summoned-monster reward restrictions retain their existing rules.

Blink uses bounded reservoir sampling over at most 28 candidate tiles. It can
cross walls, rejects blockers and monsters, and resolves ordinary landing traps.
If no destination exists it spends the usual MP/action with feedback and no
destination random draw. Banish preserves monster teleport semantics: a landing
trap does not activate until a later entry. Exchange moves both participants
first, then resolves the monster's trap at the player's origin and the player's
trap at the destination. Fatal/displacement traps do not cause duplicate entry
or retain dead-monster references. Empty Cleanse and rays hitting nothing still
spend their normal MP/action.

## Books and study

Books occupy ordinary slots and do not stack. A two-byte `Item` stores `SPELLBOOK`
and its spell ID in the full `info` byte. Book names are public, with no randomized
appearances, curse bits or unidentified variants. Successful learning consumes
one book and sets a permanent bit for the run. Interrupted books remain intact;
learned duplicates offer no benefit and may be dropped/discarded normally.

Use Item starts study without advancing a turn. Gameplay then advances exactly
one ordinary turn per update, paced at 90 ms between updates. Every completed
turn refreshes the dungeon and right-hand study panel: spell name, progress bar,
numeric progress and a four-phase animated rune. B cancels between turns.
Movement, inventory use and casting cannot run concurrently with study.

`STUDY_HUNGRY = 60` defines the hungry region. Starting reserve must be at least
`60 + STUDY_FOOD_MARGIN (12) + study duration`: 112, 132 or 152 hunger units.
This covers even the worst ordinary one-food-unit-per-turn rate, including cursed
Sustenance; normal hunger consumption is slower. There is no extra study food
penalty. Hunger below 60 interrupts study. Monsters act, light burns, temporary
effects age and mana regenerates normally throughout.

Any nonempty gameplay status word, including number-only output, flags an
interruption. Attack misses, expiration messages and hunger warnings all count.
Study drawing bypasses the status stream. The first gameplay word clears the
study panel; triggering text remains in normal paginated status, followed by
“You lose your concentration.” Progress drawing does not overwrite it. Interruption wins
even on the final required turn. Displacement, death, floor changes, paralysis,
blindness, confusion and lifecycle transitions also stop study. Progress resets
to zero; ordinary turns, damage and resource costs remain spent.

Progress is transient session state. Save/exit and resume cancel study while
retaining the book. MP and known spells persist. Save version 31 rejects older
versions under the existing no-migration policy.

## Controls

Open the action menu with B and scroll to **CAST SPELL**. The eight-entry action
menu scrolls through seven rows. The spell menu contains only learned spells;
each shows its cost, with `!` for insufficient MP. A confirms; B cancels freely.
Directional spells request a cardinal direction. Exchange offers eligible visible
monsters, highlights the selected tile and uses direction buttons to cycle them.
B cancels without MP, turns or RNG use. Spells without a target resolve after
confirmation. Gameplay resolution occurs after selection frames unwind. The HUD
shows current/maximum MP, sight radius and temporary See Invisible's `S` marker.

## Distribution and agents

An isolated stream replaces 6% of eligible nonessential descent loot with books.
Food, arrows, all lights, artifacts and Yendor are excluded. Loot positions,
ordinary generation streams and artifact schedules are preserved. No fresh books
appear on ascent or the special all-phantom floor (zero-based floor 4). Explicit
metadata pools replace ordinal ID comparisons. Frostbind, Repulsion, Confound
and Sense Traps form the basic pool. The deeper pool contains the original six
other book spells with weight 3 each; Phase Step, Slumber and Blood Bolt have
weight 2 each; Fireburst and Chain Lightning have weight 1 each. Intermediate
and advanced categories currently share this deeper pool. Deeper-pool probability is `min(16, floor + 3) / 16`, starting at 18.75% and
reaching 100% on zero-based floor 13. Magic Dart books are not generated.
The ordinary early-light guarantee still runs.

Both agents value unknown books, ignore learned duplicates, prepare food/light
and study only with sufficient health, exits and quiet surroundings. Interrupted
attempts are deferred at least 128 simulated turns. Explorer-v1 uses only public
observations and its own memory: visible monsters, reported attacks, known
terrain/hazards, public book identities, learned spells/MP and study progress.
It does not receive hidden positions, traps, timers or monster HP. Its policy
object compiles without production `Game` declarations. Observation protocol 7
withholds a remote spellbook's identity until standing pickup inspection; inventory
and previously examined names remain public. Exact hidden item identities never
enter the observation interface.

Policies support all sixteen effects: emergency displacement/teleportation,
trap-directed Repulsion, dangerous-monster control, two-target bolts, conservative
dart attacks, local trap detection and harmful-condition cleansing. Omniscient-v2
retains its deliberate knowledge advantages while obeying the same actual action,
resource and study rules. Weapons, modifier wands, area scrolls, healing potions,
food and light retain distinct roles. Fireburst requires a useful cluster and
safe splash. Phase Step evaluates preferred known landings and requires a safer
position than walking. Lightning predictions use observed monsters and known
obstructions. Slumber provides emergency single-target control. Blood Bolt
requires a substantial HP reserve and a healing/escape resource; explorer also
limits repeated sacrifices with a 32-turn policy cooldown. Oracle uses actual
remaining enemy HP to identify efficient kills. These are native policy fields,
not saved gameplay state.

## Measurements

Build sizes, test results, policy identities, paired outcomes and design risks
are in [SPELLS-RESULTS.md](SPELLS-RESULTS.md) for the historical eleven-spell system and [MAGIC-EXPANSION-RESULTS.md](MAGIC-EXPANSION-RESULTS.md) for this expansion. Raw datasets and AVM profiles belong
in ignored `build/`. `sim/spell_report.py` reports spell use, learning cohorts,
resources, death causes and floor survival from validated datasets. `spells.csv`
has one row per run/spell, including direct/chained lightning damage, Slumber disables and rolled stun actions, HP sacrificed and self-inflicted Blood Bolt deaths. Core telemetry is version 10; observation is 7 and exploration telemetry remains 6.
