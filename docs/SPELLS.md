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
| Frostbind | 2 | 40 | Cardinal ray, range 5; first monster is slowed for 4–6 turns. |
| Repulsion | 2 | 40 | Cardinal ray, range 5; push the first monster 1–2 tiles. |
| Confound | 3 | 40 | Cardinal ray, range 5; first monster is confused for 4–6 turns. |
| Sense Traps | 2 | 40 | Discover hidden traps within circular radius 4 and mark their exact tiles on the map; traps remain active. |
| Blink | 4 | 60 | Random valid tile within circular radius 3, including across walls and on traps. |
| Shockwave | 4 | 60 | Snapshot all eight adjacent positions; push their monsters directly outward 2–3 tiles each. |
| Piercing Bolt | 3 | 40 | Cardinal ray, total range 5; first two monsters each take 3–5 magic damage. |
| Banish | 4 | 60 | Cardinal ray, range 5; teleport the first monster using ordinary monster teleport placement. |
| Exchange | 4 | 60 | Swap with a selected visible monster within circular radius 3. |
| Cleanse | 4 | 60 | Remove exactly one condition: blindness, then confusion, then slowing. |
| Fireburst | 4 | 60 | Range-5 projectile; radius-1 square explosion at first monster or last clear tile, 3–6 fire damage to each monster and the player. Dragons ignore fire; player fire protection and MR apply. |
| Phase Step | 4 | 60 | Cardinal teleport: prefer exactly 3 tiles, otherwise 2. Cross monsters and intermediate traps, never walls/closed doors/edges; occupied landings are invalid. Destination traps activate normally. |
| Chain Lightning | 4 | 60 | Range-5 first hit for 3–5 magic damage; one visible secondary within Chebyshev 2 takes 3–4. Clear geometric jump path required, nearest target then lowest slot. Invisible monsters cannot receive secondary arcs; lightning can strike and reveal a disguised mimic. |
| Slumber | 4 | 60 | Range-4 ray; first monster stunned for 3–4 enemy actions, refreshing to the greater duration. The immediate enemy response consumes a stun action. |
| Blood Bolt | 2 + **3 HP** | 60 | Range-5 first hit for 6–9 magic damage. Sacrifice current HP before firing, even on a miss; bypasses all mitigation, can kill the caster. Phoenix Heart uses its ordinary resurrection lifecycle. No bolt or enemy turn after final death. |

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

The authorized balance pass lowers selected MP costs and shortens Piercing Bolt, Exchange, Fireburst and Chain Lightning study. Effects and Blood Bolt's 3 HP payment remain unchanged. Hardware uses the table above; host `balance:legacy-spell-rules=1` exposes the former costs and durations to both agents for comparison.

## Books and study

Four fixed, public books cover the fifteen learnable spells. Magic Dart is known
initially and appears in no book.

| Book | Contents |
| --- | --- |
| Book of Control | Frostbind, Confound, Slumber, Banish |
| Book of Displacement | Repulsion, Blink, Shockwave, Phase Step |
| Book of Survival | Sense Traps, Cleanse, Exchange |
| Book of Destruction | Piercing Bolt, Blood Bolt, Fireburst, Chain Lightning |

Books occupy ordinary inventory slots and do not stack. The two-byte `Item`
stores `SPELLBOOK` and a book ID in `info`. Names and memberships never randomize.
Choose **Use Item**, select a book, then use up/down to select an unknown spell.
The full-screen inventory-style view retains the seven-row scrolling convention;
these books have only three or four rows. Learned members show **Known** and are
skipped by navigation; unknown members show their individual study duration.
A confirms and B cancels without turns, MP, hunger or random draws. Invalid or
fully learned books return safely without an empty selection loop.

Each spell needs its own 40 or 60-turn session, in any order. Learning sets
only that spell's permanent known bit. The book remains until every member is
known, when the final successful study consumes it. Dropping, picking up and
saving a partially learned book preserve its public ID; the known mask supplies
all progress. Interrupted sessions teach nothing and retain the book. There is
no saved partial-session progress.

The shared `begin_study(slot, spell)` validates both the inventory item and its
selected unknown member. `use_inventory(slot, target)` uses `target` as the chosen
spell for books; simulator actions transmit it without graphical interaction.
The selection modal returns before gameplay begins, preserving the AVM stack
boundary. Study starts without a turn; each subsequent update advances exactly
one ordinary turn, paced at 90 ms. The dungeon refreshes with a progress bar,
numeric count and animated rune. B cancels between turns. Movement, inventory use
and casting cannot run concurrently with study. Final learning clears the active
study references, updates the known mask and removes a completed book before
emitting completion events or status text.

`STUDY_HUNGRY = 60` defines the hungry region. Starting reserve must be at least
`60 + STUDY_FOOD_MARGIN (12) + study duration`: 112 or 132 hunger units.
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
retaining the book. MP and known spells persist. Save version 32 rejects older
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

An isolated stream attempts to replace 6% of eligible nonessential descent loot.
Food, arrows, lights, artifacts and Yendor are excluded. Loot counts, positions,
ordinary generation streams, artifact schedules and early light guarantees stay
intact. Ascent and the special all-phantom floor (one-based floor 5) receive no
new books. A local byte excludes books already generated on that floor, carried
books and books whose entire membership is known. Each attempt draws once; an
ineligible result leaves the original loot. It never retries or redistributes its
weight to another book.

| One-based floors | Control | Displacement | Survival | Destruction |
| --- | ---: | ---: | ---: | ---: |
| 1–3 | 50% | 35% | 12% | 3% |
| 4–7 | 35% | 35% | 22% | 8% |
| 8–11 | 20% | 30% | 30% | 20% |
| 12–16 | 10% | 20% | 30% | 40% |

These are draw weights; duplicate/knowledge exclusions reduce realized book
frequency. Advanced books remain possible from floor 1.

Mana regenerates silently every 16 turns, every 8 with an equipped ordinary
**Amulet of Meditation**, or every 32 when that amulet is cursed. Intervals divide
the saved 256-turn clock; equipment changes apply immediately and stairs/resume
retain cadence. Starvation, death, ended games and full MP forfeit the tick. No
extra timer is saved. The HP Regeneration amulet retains its original 1-in-20
healing effect and now emits no message, so passive HP/MP recovery does not break
study. Other gameplay messages still interrupt it.

**Potion of Mana Restoration** restores a uniformly drawn whole-number amount
from `ceil(max MP / 2)` through `max MP`, clamped to capacity. It spends an ordinary
action and follows potion appearance, discovery, stacking and conservation rules.
It does not heal or change statuses, and has no thrown monster effect. Its
slot-local 8% potion replacement and Meditation's 8% amulet replacement preserve
ordinary generation draws and the other types' relative weights.

Both agents value remaining spells by tactical usefulness, prepare food/light
and study with adequate HP, exits and quiet surroundings. They select individual
members across all carried books by utility per study turn, discount redundant control/mobility, defer
costly situational spells until deeper floors, preserve a food reserve and avoid
study on ascent. A public depth budget allows at most `2 + zero-based floor / 2` additional learned spells before another study; this is agent strategy, not a player restriction. Interruptions retain the 128-turn retry delay. They avoid
carried duplicates and useless completed books, recognize that learning may
leave a book, and use mana potions when at least two-thirds of MP is missing and
casting/escape has value. Known Meditation and curses affect casting reserves;
other amulets still compete through equipment scores.

Explorer-v1 uses only public observations and memory: visible monsters, printed
attacks, known terrain/hazards, legitimately inspected book IDs, learned spells,
displayed MP and study progress. Observation protocol 9 withholds remote book
identities until standing pickup inspection; carried and inspected books expose
public membership through compile-time rules. Hidden item identities, monster
HP/statuses, map cells and timers remain inaccessible. Its policy compiles
independently of production `Game` declarations. Omniscient-v2 retains its
deliberate knowledge advantages and uses the same authoritative gameplay API.

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
are in [SPELLS-RESULTS.md](SPELLS-RESULTS.md) for the historical eleven-spell system and [MAGIC-EXPANSION-RESULTS.md](MAGIC-EXPANSION-RESULTS.md) for the five-spell expansion; [MULTI-SPELL-BOOKS-RESULTS.md](MULTI-SPELL-BOOKS-RESULTS.md) covers themed books and mana recovery. Raw datasets and AVM profiles belong
in ignored `build/`. `sim/spell_report.py` reports spell use, learning cohorts,
resources, death causes and floor survival from validated datasets. Sidecar schema 2 also records actual status applications, trap reveals, cleansing and successful movement, so every spell has measurable effect counters. `spells.csv`
has one row per run/spell, including direct/chained lightning damage, Slumber disables and rolled stun actions, HP sacrificed and self-inflicted Blood Bolt deaths. Core telemetry is version 10; observation is 9 and exploration telemetry remains 6.
