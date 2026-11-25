# Standard format scan (TEF – DLR)

**Done:** 2026-11-16, corrected 2026-11-25 · **Purpose:** design the effect system (Milestone 3) for every
Standard-legal card, not only the cards in our four decks.

## Corrections (2026-11-25)

This document was lost from the repo and restored from its first version for M3b, with these corrections. The rest
is as it was written on 2026-11-16.

- **The 97.6% isn't a share of translatable texts.** It counts texts that match at least one known mechanic. The 1,455
  texts have 1,374 different sentence templates, only 230 of them used more than once. A prototype phrase grammar
  found that 40.1% of the cards need no new vocabulary and that at most about 73% parse at all (see
  [M3b](../roadmap.md#m3b--standard-coverage-)).
- **Delta Reign is left out, not counted in by hand.** TCGdex still doesn't mark it legal, its data stores every Pokémon ex
  as a Basic, and 16 of its Trainers and Energy have no text.
- **TCGdex has wrong data.** Six Pokémon ex of 30th Celebration are stored as Basic but are evolutions; one, Greninja ex,
  was in our mega-greninja deck. They're fixed in [`corrections.json`](../data/cards/README.md#correctionsjson), as are
  the 52 evolutions that don't say what they evolve from.
- **The count of distinct cards is now the importer's:** `pokebot-import --standard` finds **2,097**
  Standard-legal cards outside Delta Reign (see [the coverage report](../tools/import/standard.md)). The prototype's
  2,109 counted the Basic Energy named "Basic Grass Energy" apart from "Grass Energy" (8 cards), and four corrected
  30th Celebration ex apart from their promo reprints. On 2026-11-25, **373 (17.8%)** of them have all their effects.
- **The scan's Python tool is gone;** the coverage report replaces it.

## What was scanned

Every card in Temporal Forces through Delta Reign, plus the SV and ME promo sets, from TCGdex. A
card is in scope if TCGdex marks it Standard-legal. Delta Reign is the exception: it's the newest
set and TCGdex hasn't flagged it yet, so it's counted in by hand.

| | Count |
|---|---|
| Printings downloaded | 3,639 |
| Standard-legal printings | 3,393 |
| **Distinct cards** (reprints and alternate arts collapsed) | **2,213** |
| Pokémon / Trainers / Energy | 1,933 / 244 / 36 |
| Items / Supporters / Tools / Stadiums | 90 / 83 / 37 / 34 |
| Basic / Stage 1 / Stage 2 Pokémon | 1,089 / 586 / 170 (plus Pokémon ex: 223) |
| **Distinct effect texts** | **1,455** (attacks 1,761, Abilities 300, Trainers 230, Special Energy 6, before de-duplication) |

Not legal and correctly excluded: classic reprints in 30th Celebration and 30th Classic Collection,
older promos, and reprints of rotated cards in Prismatic Evolutions.

**Confirmed absent from Standard:** the Lost Zone (0 mentions), and VSTAR, GX, V-UNION and Radiant
(0 mentions). Nothing needs to be built for them.

## How the texts break down

Each text was tagged with every mechanic it uses, by matching the templated wording card text
always uses. **1,420 of 1,455 distinct texts (97.6%) match at least one known mechanic.** The full
tag counts came from a one-off Python scan (since removed). The headline numbers:

| Area | Most common mechanics (distinct texts) |
|---|---|
| Damage | damage formulas ("for each", "more damage") 480 · attacker/defender damage modifiers 300 · damage to Benched or chosen Pokémon 71 · place damage counters 61 · prevent damage/effects 55 · self-damage 17 · move damage counters 8 |
| Timing | "you may" 212 · once during your turn 128 · "when you play this from your hand" 32 · when damaged 30 · only if Active 26 · when Knocked Out 24 · play conditions 22 · whenever 11 · Pokémon Checkup 10 · end of turn 6 |
| Cards | search deck 155 · draw 74 · prize references 46 · discard from hand 34 · look at top of deck 26 · mill 22 · recover from discard 18 · discard from opponent's hand 16 · look at opponent's hand 15 · shuffle hand into deck 15 · draw until N 14 |
| Energy | discard own Energy 62 · attach from deck 43 · attach from discard 31 · attach from hand 29 · move Energy 24 · "provides" 18 · discard opponent's Energy 15 |
| Board | heal 57 · bench from deck 44 · switch own 25 · gust 23 · return to hand/deck 8 · devolve 7 |
| Conditions | coin flips 125 · Poisoned 32 · Confused 22 · Burned 17 · Paralyzed 14 · Asleep 12 · "attack does nothing" 23 |
| Lasting | "during your/your opponent's next turn" 100 · can't attack/retreat/use 59 · "during this turn" 21 · Item/Supporter lock 7 · Ability lock 7 |
| Passive | Weakness/Resistance changes 97 · rule-box references 56 · tool interactions 27 · Retreat Cost 25 · Stadium interactions 25 · HP changes 20 · Ancient/Future 17 · Tera 13 · copying attacks 7 |

**What this means:** most texts are combinations of a few dozen building blocks. The effort goes
into a solid vocabulary, not one-off code.

## The rules cards change: questions the engine must ask

These are the rules that Stadiums, Tools, Special Energy and the **146 passive Abilities** bend. Each
must be a question the engine asks, with the rulebook's answer as the default, so a card can change
the answer. That was the lesson from Forest of Vitality.

| # | Question | Examples |
|---|---|---|
| 1 | **Maximum HP** | Hero's Cape, Lively Stadium, Gravity Mountain, Ludicolo, Brambleghast, Alolan Exeggutor |
| 2 | **Retreat Cost** | Air Balloon, Rescue Board, Gravity Gemstone, N's Castle, Altaria, Archaludon, Mega Chandelure ex, Wimpod |
| 3 | **Attack cost** | Counter Gain, Sparkling Crystal, Hop's Choice Band, Nighttime Mine, Incineroar ex, Decidueye ex, Kyurem, Grapploct |
| 4 | **Energy a card provides** | Neo Upper, Legacy, Ignition, Prism Energy, Meganium's Wild Growth (Basic {G} provides {G}{G}) |
| 5 | **Weakness and its multiplier** | Lillie's Clefairy ex (Weakness becomes {P}), Illumise (×3) |
| 6 | **Types** | Carbink and Scovillain ex (dual type), Iron Treads |
| 7 | **Damage, attacker side** (before W/R) | Maximum Belt, Binding Mochi, Serperior ex, Postwick, Okidogi, Kingambit |
| 8 | **Damage, defender side** (before or after W/R) | Full Metal Lab, Granite Cave, Berries, Bronzong, Pyroar, Garbodor |
| 9 | **Is the damage or effect prevented?** | Mist Energy, Pikachu's Keep Hidden, Shaymin, Farigiraf ex, Drednaw (≥200), Battle Cage, Neutralization Zone, Tera bench rule |
| 10 | **Are effects on the defender ignored?** | Walking Wake ex, N's Zekrom's Shred |
| 11 | **Does this Pokémon have its Abilities?** | Team Rocket's Watchtower, Flutter Mane, Iron Thorns ex, Gastrodon, Psyduck's Damp |
| 12 | **Do Tools work, and how many fit?** | Jamming Tower, Rotom ex's Multi Adapter |
| 13 | **Can this card be played now?** | Item locks (Tyranitar, Jellicent ex, Budew), Copperajah (Stadiums), Genesect (ACE SPEC), Team Rocket's Arbok, Palafin ex |
| 14 | **Can this Pokémon evolve now, and into what?** | Forest of Vitality, Eevee, Luxio, Shelmet/Karrablast, Eevee ex's Rainbow DNA, Rare Candy, Grand Tree |
| 15 | **Can it attack, and which attacks?** | Team Rocket's Mewtwo ex, Mega Golurk ex, Slaking ex, Meloetta ex (turn 1), Mew ex, Relicanth, Core Memory, Technical Machines, Dipplin (attacks twice) |
| 16 | **Can it retreat, and does the retreat work?** | Clutch, Sob, Goodra (coin flip cancels it) |
| 17 | **Can it get, or recover from, Special Conditions?** | Festival Grounds, Hoothoot, Slowpoke, Dizzying Valley |
| 18 | **How many Prize cards for this Knock Out?** | Lillie's Pearl, Legacy Energy, Shedinja, Mega Gengar ex, Munkidori ex (fewer); Hydreigon ex, Togekiss (more) |
| 19 | **Is it actually Knocked Out?** | Survival Brace, Pikachu ex, Mega Hawlucha ex (stay at 10 HP) |
| 20 | **Where do Knocked Out cards go?** | Gengar's Infinite Shadow (to hand), Huntail (Energy to hand) |
| 21 | **Bench size** | Area Zero Underdepths |
| 22 | **Can it be healed? Can counters move? Can cards go to hand?** | Yveltal, Patrat, Milotic |
| 23 | **Energy attachments per turn** | Iono's Bellibolt ex, Emboar (attach as often as you like) |
| 24 | **Pokémon Checkup damage** | Perilous Jungle, Magmortar, Pecharunt, Froslass, Team Rocket's Tyranitar |
| 25 | **Setup rules** | Cinderace (may start face down as Active) |

## Events: when cards react

Triggered effects need the engine to announce these moments:

| Event | Examples |
|---|---|
| A card is played from hand (Pokémon to Bench, Item, Supporter) | Meowth ex, Iron Leaves ex, Fraxure, Rhyperior |
| A Pokémon is put onto the Bench | Risky Ruins |
| A Pokémon evolves | Team Rocket's Ampharos |
| Energy is attached from hand | Gengar ex's Gnawing Curse, Magearna, Enriching Energy |
| A Pokémon is damaged by an attack (while Active) | Punk Helmet, Deluxe Bomb, Lucky Helmet, Spiky Energy, Berries, Numel, Qwilfish |
| A Pokémon is Knocked Out by an attack | Amulet of Hope, Heavy Baton, Maractus, Gengar ex's Fainting Spell, Photon Cord |
| The opponent's Active moves to the Bench | Team Rocket's Dugtrio, Magcargo, Mismagius ex |
| A Pokémon retreats | Goodra |
| A card is discarded from the deck | Ferrothorn |
| After an attack / end of turn | Boomerang Energy, Ignition Energy, Technical Machines, Powerglass |
| Pokémon Checkup | Snorlax, Perilous Jungle |
| An Ability or Tera Pokémon leaves play | Rotom ex, Area Zero Underdepths (Bench back down to 5) |

## Game history the engine must remember

| Fact | Used by |
|---|---|
| Were any of my Pokémon Knocked Out during the opponent's last turn? | Koraidon ex, Fezandipiti ex, Unfair Stamp |
| Did this Pokémon use attack X last turn? | Miltank, Sylveon ex, "can't use Impact Blow next turn" |
| Which cards did I play this turn (a Supporter? a "Team Rocket" Supporter?) | Team Rocket's Factory, Community Center |
| Who went first / is it my first turn? | Illumise, Meloetta ex, Fan Rotom |
| Prize cards taken | Pecharunt ex, Kingambit, Bloodmoon Ursaluna ex, Brambleghast |
| Energy attached this turn | (already a turn flag) |
| Once-per-game effects used | Legacy Energy |

## The genuinely unusual: candidates for custom C++

About 35 texts match no pattern at all. Most are still expressible with a new building block. These
are the ones that look like real one-offs:

| Card | Why it's unusual |
|---|---|
| N's Zoroark ex, Mew ex, Clefable (Metronome), Team Rocket's Mimikyu | Use another Pokémon's attack. Worth a general "copy attack" block, since there are 7 |
| Dipplin | Attacks twice |
| Shiftry (Expelling Tornado), Sylveon ex (Angelite), Illumise, Chimecho | Shuffle Pokémon in play into the deck |
| N's Vanilluxe, Spiritomb | Double or quadruple the damage counters already on Pokémon |
| Yveltal ex, Glaceon ex | Knock Out directly by condition |
| Delibird, Grafaiai | Effects that act for or on the opponent's resources |
| Transformation Tome | Must play 2 at once; swaps a Pokémon in play with one in the discard pile |
| Grand Tree, Lumiose City, Celebratory Fanfare | Stadium actions that end the turn |
| Palafin ex, Cinderace | Change how a Pokémon enters play or setup |
| Ferrothorn | Reacts to being discarded from the deck |
| Area Zero Underdepths | Bench size that shrinks when its condition ends |

## What this changes in the plan

1. **The effect vocabulary should be designed for all of Standard now**: the 25 questions, the
   events, and the game history above. These are cheap to build into the engine from the start and
   expensive to retrofit.
2. **Writing effects for all 2,213 cards by hand isn't realistic.** Milestone 3 writes effects for
   our four decks and proves the vocabulary. Covering all of Standard is a job for the **text
   translator**, which becomes much more valuable now that we know the wording is 97.6% patterned.
3. **Re-run the scan on every new set** (now `pokebot-import --standard`). New tags with high counts mean new
   vocabulary; untagged texts are the next custom-code candidates.
