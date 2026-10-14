# Persistent companions

Up to four creatures follow you, fight nearby enemies, and survive stairs and
save/resume. They have no expiry, equipment, hunger, levels, or resurrection.
Open **COMPANIONS** in the action menu to inspect their species, HP/max HP,
summoned/tamed origin, and active conditions. Up/down selects a creature; A
opens permanent dismissal confirmation, A confirms, and B cancels. Browsing and
canceling are free. Dismissal costs one ordinary action and awards nothing.

## Summoned species

These statistics stay fixed at every player level. Speed is action cost: a
smaller number acts more frequently under the ordinary monster scheduler.

| Species / glyph | HP | STR | DEX | Speed | Armor | MP | Role |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Hound / `h` | 14 | 4 | 5 | 3 | 1 | 7 | Fast melee; scent detects invisible enemies |
| Spider / `s` | 18 | 4 | 4 | 4 | 1 | 8 | Melee; successful hits can weaken or slow for 4 ticks |
| Wisp / `w` | 10 | 3 | 4 | 4 | 0 | 10 | Fragile ranged attacker; 2–4 magic damage, range 3; sees invisible enemies |
| Golem / `g` | 36 | 7 | 3 | 7 | 5 | 12 | Slow physical blocker; opens doors; successful hits can stun for 2 ticks |

Spider weakening, Spider slowing, and Golem stun each have a 25% application
chance when that condition is absent. Weakening reduces ordinary damage; it
does not add a separate poison damage clock and remains until an ordinary
restorative effect clears it. Physical attacks use the existing
accuracy, strength, armor, and weakening formulas. Wisp magic bypasses armor
and requires a clear terrain line. Scent never gives the player See Invisible
or discloses an unseen enemy's location to explorer-v1.

**Summon Hound, Spider, Wisp, and Golem** each require 60 study turns. The fifth
fixed spellbook, **Book of Companions**, contains all four. Learn each member
separately; the book is consumed after the last member is learned. Summoning
costs the listed MP and one ordinary action. It rejects a full roster, an
already active summoned species, insufficient MP, or lack of a safe landing
without spending MP or a turn. Only one living companion of each new species
may exist. Tamed ordinary species have no duplicate-species restriction.

On each of the first three physical descent floors (indices 0–2), a missing,
unfinished Companions book replaces one eligible ordinary loot slot whenever
one exists. The same floor contains at most one copy. The guarantee retries if
you leave the book behind; it never gives the book directly to your inventory.
Later eligible descent floors have a 35% scheduling chance. It does not replace
food, ammunition, lights, healing/extra-healing/mana potions, artifacts, Yendor,
or a pre-existing book. Carrying the book or knowing all four members suppresses
it. The all-phantom fifth floor and ascent generate no new books. Book scheduling
uses a separate seed domain, consumes no gameplay RNG, and keeps the ordinary
book replacement draws and duplicate decisions intact.

## Following, swapping, and recall

Companions make bounded local decisions. They prefer your recently vacated
position, remain still when adjacent, avoid revealed traps, and can exchange
places with a stationary leading companion to clear a corridor. Species with
the ordinary door-opening ability open closed doors. Others eventually recall
when separated. There is no per-turn full-map search or recursive pathfinding.

Walking into a companion swaps your positions instead of attacking it. This
costs normal movement time and applies both creatures' destination traps once.
Your usual pickup, feature, visibility, and stairs behavior follows the swap.
Tamed creatures retain their original glyph, with no friendliness overlay.

Within three Manhattan tiles, companions defend you. Farther away, following
takes priority over combat. Melee companions can take one local step toward a
nearby detected threat, including around your occupied square, while staying
within that defensive radius. A fast creature can repeat an attack against the
same surviving target; moving does not buy additional attacks that turn.
Recall first attempts ordinary movement. A stranded creature can recall after
two failed follow actions at distance eight or six failed actions otherwise.
Bright-light recalls try eight nearby cells hidden around corners; dim-light
recalls prefer cells outside the light circle. A safe visible fallback is allowed
after ten failed actions or at distance twelve. Nearby creatures with a clear
terrain connection do not recall merely because their formation is crowded.

Landing searches examine a fixed radius-three neighborhood, prefer behind you,
and reject walls, closed doors, occupied cells, your cell, and every trap,
including hidden traps. A visible fallback appears in the normal dungeon redraw;
it adds no glyph marker or extra timed animation. Recall is silent and preserves
HP, conditions, stun, and identity. An eight-turn cooldown prevents loops.
Forced displacement starts six ordinary turns of grace; a saved state bit
restarts conservative grace after resume so loading cannot immediately undo a
push or teleport. Following continues during grace.

Floor generation keeps the four eight-byte instances in their reserved slots,
invalidates their old positions before generation scratch is used, and places
survivors safely near the new entrance afterward. If a custom constrained map
has too few safe nearby cells, an unplaced survivor is retained and retries
recall when space becomes available. No creature is deleted or placed on another
creature to force a transition.

## Combat, healing, and friendly fire

Hostiles continue pursuing the player. A detecting hostile attacks a companion
blocking its next movement cell instead of becoming stuck. Dragons can breathe
at a blocking companion; ordinary burst damage can also affect nearby creatures
and the player. Hostiles never search the map for distant companions. Companions
never deliberately attack another companion or the player. Stun, slowing,
blindness, confusion, fear, weakening, invisibility, and species abilities use
the existing monster fields and effects.

Pet kills of eligible hostiles give full player XP and ordinary score, including
Wisdom bonuses and the Lord's Yendor drop. Rewardless summoned enemies remain
rewardless. Killing or dismissing a companion gives no XP, score, mana, or loot.
Death immediately frees its reserved slot.

Every applicable ray hits the first creature, including your companion. Piercing
Bolt and lightning chains include friendly creatures; Fireburst, wand bursts,
Shockwave, scroll effects, arrows, and thrown potions also obey ordinary occupancy
and effects. Banish, Exchange, teleportation, and force can displace companions.
Damage and hostile conditions do not change allegiance. Friendly fire can kill
them permanently. Use thrown healing and ordinary restorative potion effects
to help an injured companion.

Natural pet recovery gives one HP every 32 ordinary turns while not fighting,
never above maximum HP. A pet that attacked or was attacked during that turn
cannot recover even if its last opponent died. Nearby detected enemies also
prevent recovery. Recovery is silent. A tamed Troll additionally retains its
ordinary species regeneration. Routine following and recall do not interrupt
study. Visible fighting, traps, deaths, and other meaningful messages do.
Routine creature attacks emit at most one battle message per turn and reuse
existing action feedback when present; deaths and traps still report separately.

## Taming

All three methods use one authoritative conversion function. Success copies
species, HP, stun, conditions, and state into a free companion slot, then clears
the hostile slot. It is permanent. Bat, Snake, Rattlesnake, Goblin, Orc,
Tarantula, and Hobgoblin are ordinarily tameable. Troll and Griffin resist.
Zombie, Phantom, Mimic, Incubus, Dragon, Fallen Angel, Lord, rewardless hostile
summons, and already friendly creatures are immune.

| Target condition | Scroll | Friendship potion | Domination wand |
| --- | ---: | ---: | ---: |
| Ordinary, above half HP | 100% | 50% | 75% |
| Ordinary, at most half HP | 100% | 65% | 90% |
| Ordinary, at most quarter HP | 100% | 75% | 100% |
| Troll/Griffin, above half HP | 50% | 5% | 25% |
| Troll/Griffin, at most half HP | 50% | 15% | 40% |
| Troll/Griffin, at most quarter HP | 50% | 25% | 50% |

For potion/wand attempts, weakening grants the same ten percentage-point bonus
as quarter HP, once rather than cumulatively. The half-HP bonus is independent.
Scrolls ignore injury bonuses. Eligible outcomes cap at 100%, subtract strong
species' 50-point resistance, and floor at 5%. Immune targets always have zero
chance and consume no probability draw.

**Scroll of Taming** selects an actually visible, revealed creature within a
circular radius four and clear terrain line. B cancels freely. A valid attempt
consumes one scroll and one action, including resistance failure. A full party
rejects before consumption. **Potion of Friendship** uses the normal thrown
potion projectile, impact, appearance, identification, and stack rules. Drinking
it reports that it has no useful effect. **Wand of Domination** uses range-six
rays, ordinary charges and identification. Failed attempts spend a charge;
full-party activation rejects before a charge or turn. Powerful resolves a
radius-one impact area; spreading uses N/E/S/W order, and overpowered combines
them. Conversions use stable slot order, skip new friends, and stop at the cap.
Cursed domination confuses the user unless Clarity protects them; unreliable
uses the usual random cardinal bearing. Enchantment and Remove Curse retain
their existing wand behavior.

Independent per-slot replacement chances are 4% for Friendship among ordinary
potion results, 6% for Taming among scroll results, and 6% for Domination among
wand results. Original generation draws and placements remain intact.

An ordinary altar's favorable companion branch has a 1/8 chance: Hound with
weight 5/8, Spider 2/8, Wisp 1/8. It costs no MP, is permanent, and respects safe
placement and species/cap restrictions. A failed grant falls back to its usual
blessing. The altar remains one-shot, with its other rewards and hostile
summoning risks intact. It grants no Golems.

## Persistence and simulation

Save version **33** stores an **879-byte AVM Game**, up 34 bytes: 32 for four
monsters and two for the expanded spell mask. Monster instances remain eight
bytes. Slots 0–11 are hostile; slots 12–15 derive friendly allegiance from their
indices. Hostile generation and summoning retain twelve slots. Spell IDs 0–15
are preserved; IDs 16–19 use a 32-bit knowledge mask. Old saves are rejected
under the existing no-migration policy. Item groups remain contiguous; the
appearance system covers 20 potions, 16 scrolls, and nine wands.

Both maintained agents navigate through friendly swaps, avoid friendly ray,
burst, chain, and displacement targets, use summoning and eligible taming, and
can dismiss a weak full-party member for a substantially better ordinary
creature. They preserve ordinary combat and utility magic. Summoning is quiet,
has a 96-turn replacement cooldown, and keeps two MP, or four when learned
emergency displacement is available. Preferred species change with depth;
additional summon study competes conservatively with control and mobility.
The ordinary study budget remains unchanged, and a summon is not studied before
the character can afford its MP cost. Dismissal has a 128-turn replacement guard.

Explorer protocol **10** admits sixteen visible creatures and forty observed
ground objects/disguises. Visible friends carry public allegiance. A separate
four-entry companion roster supplies only the screen's identity, HP/max HP,
origin, and active condition flags: no offscreen positions, enemy HP, hidden
timers, aggression, terrain, traps, or scent coordinates. Telemetry never enters
the policy. Oracle keeps its deliberate information advantage.

Core telemetry **11** adds `pets.csv`, keyed by seed, floor visit, direction, and
species. It records source acquisitions, method-specific attempts/successes,
attacks/hits, damage, controls, kills/base and awarded XP, damage absorbed,
friendly damage, death causes, observed completed lifetimes, active turns,
recalls/distances/visibility, healing, dismissal, entry/exit roster, spell
interceptions, friendly effects, and pet-related study interruptions. Completed
lifetimes exclude living/censored pets; active-turn counts include them. See
[the implementation and validation report](PETS-RESULTS.md) for measured limits,
paired outcomes, provenance, and remaining balance concerns.
