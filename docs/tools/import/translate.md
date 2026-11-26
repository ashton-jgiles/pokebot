# `tools/import/translate` — card text to effects

**Files:** [`tools/import/translate.hpp`](../../../tools/import/translate.hpp), [`tools/import/translate.cpp`](../../../tools/import/translate.cpp)
**Library:** `pokebot_import_lib` · **Namespace:** `pokebot::import`

Reads a card's text and writes its effects in the [effect language](../../src/cards/effects/program.md), the same JSON
as a hand-written [effects file](../../data/cards/README.md#effectsidjson--hand-written-effects). It's M3b's way to
cover Standard without writing every card by hand. It reads attacks, Abilities, Items, Supporters, Tools, Stadiums and
Special Energy, with the vocabulary the engine has today.

```cpp
const Translation t = translate_attack("Flip a coin. If heads, draw a card.", AttackContext{"Lucky Draw", DamageMod::None});
// t.ok, t.effects == [{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "draw", "count": 1}]}]
```

## Strict: all of a text or nothing

A translation uses every word of every sentence, or it fails and names the first sentence it couldn't read
(`unread`). It never guesses at the rest, so a card it can't read is missing its effects instead of playing
wrong. The attack's printed damage is checked too: a "×" attack must set its damage with `for each`, a "+" attack
must add to it, and a plain one must do neither.

## How it reads

1. **`normalize_text`** removes HTML tags and the reminder texts that change nothing ("Don't apply Weakness and
   Resistance for Benched Pokémon", "Pokémon ex, Pokémon V, etc. have Rule Boxes", "Apply Weakness as ×2"), turns curly
   apostrophes, curly quotes and non-breaking spaces into plain ones, and collapses spaces.
2. **`split_sentences`** splits at each full stop followed by a space or the end.
3. **Each sentence is matched by its rules,** tried in order; one must use every word. A sentence none of them reads
   whole is tried as two joined by "and" or ", and".
4. **Sentences are joined** the way card text does: "If heads, …", "If tails, …" (the `else` of a flip), "You may …",
   "If you do, …" (into the "may" before it), "Then, …".
5. **The damage step goes after** the last block that changes the damage (`set_damage`, `add_damage`,
   `ignore_defender_effects`, `ignore_weakness`, `ignore_resistance`); without those it's first, as the compiler puts it.
6. **"If tails, this attack does nothing"** must come just after a flip. Everything after it, with the damage step,
   goes into an `if` that needs enough heads: 1 for "if tails" after one coin or "if both of them are tails", every coin
   for "if either of them is tails" or "if any of them are tails".

The parts of a sentence it reads:

| Part | Reads | Becomes |
|---|---|---|
| How many | "a", "an", "2", "up to 3", "any number of", "any amount of", "all" | `count`, `up_to` |
| Cards | "Basic {G} Energy card", "{D} Pokémon", "Pokémon ex", "Stage 2 Pokémon", "Evolution Pokémon", "N's Pokémon", "Basic Pokémon with 70 HP or less", "Pokémon that doesn't have a Rule Box", "Supporter card", "Item card", "Pokémon Tool card", "Stadium card", "Trainer card", "card" | a `filter` |
| Opposing Pokémon | "1 of your opponent's Pokémon", "2 of your opponent's Benched Pokémon" | `targets`, `on` |
| Every Pokémon at once | "each of your opponent's Pokémon", "each of your opponent's Benched Pokémon", "each of your Benched Pokémon" | `each`, `on`, `player` |
| Counts after "for each" | heads; damage counters on this or that Pokémon; cards in a hand; Prize cards taken; Benched Pokémon (yours, theirs or both); Energy attached to this, the opponent's or both Active Pokémon; cards discarded in this way; cards in a discard pile; damage counters on the opponent's Active Pokémon; your or the opponent's Pokémon in play; Pokémon Tools attached to all of your Pokémon; Energy attached to all of your or the opponent's Pokémon | `for_each` |
| Conditions after "If" | heads, both or either of them heads, tails; a Knock Out (by an attack) last turn; a Stadium in play; more Prize cards left; Pokémon sharing a type; the opponent at N or fewer Prize cards; exactly N Prize cards left; this Pokémon with a type of Energy, with damage counters or with a Pokémon Tool; the opponent's Active Pokémon a Pokémon ex (or ex or V), an Evolution, a Basic, of a type, with a Special Condition, or with damage counters | `condition` |

The sentences it reads: drawing; discarding or shuffling the hand; searching the deck (revealing, to the hand or the Bench);
attaching Energy from the deck, the hand or the discard pile; discarding Energy from this Pokémon or any of yours; putting
Energy attached to this Pokémon into the hand; damage formulas; damage to chosen opposing Pokémon; flipping coins; Special
Conditions on either Active Pokémon; can't attack, can't use this attack, can't retreat and no Items next turn; switching,
gusting, Tuck Tail and shuffling this Pokémon into the deck; damage counters, placed or on chosen Pokémon; using a Benched
Pokémon's attack; ignoring effects on the defender, Weakness or Resistance; healing this Pokémon, 1 of your (Benched)
Pokémon or each of them; damage to itself; damage or damage counters to every Pokémon at once; and discarding an Energy from
the opponent's Active Pokémon. An Item or Supporter that heals 1 of your Pokémon, or each of them, needs a damaged Pokémon
(`has_damaged`).

Lasting effects from attacks: "During your opponent's next turn, this Pokémon takes N less damage from attacks (after
applying Weakness and Resistance)"; "… prevent all damage from and effects of attacks done to this Pokémon"; "… prevent all
damage done to this Pokémon by attacks" (with "from Basic Pokémon" or "if that damage is N or less"); "… attacks used by the
Defending Pokémon do N less damage (before …)"; "During your next turn, the Defending Pokémon takes N more damage from
attacks (after …)"; "During your next turn, attacks used by this Pokémon do N more damage to your opponent's Active
Pokémon (before …)"; "… the Defending Pokémon can't use attacks"; and "… if the Defending Pokémon tries to use an attack,
your opponent flips a coin. If tails, that attack doesn't happen", whose second sentence is read only after the first.

Also: "Switch out your opponent's Active Pokémon to the Bench" (the reminder "(Your opponent chooses the new Active
Pokémon.)" is removed as text), with "this attack does N damage to the new Active Pokémon" after it; "Draw cards until you
have N cards in your hand"; "Before doing damage, discard all Pokémon Tools from your opponent's Active Pokémon", which places
the damage step itself, and "If you discarded a Pokémon Tool in this way"; "(For each heads,) discard a random card from your
opponent's hand"; "(For each heads,) discard the top (N) card(s) of your (opponent's) deck"; "If your Benched Pokémon have any
damage counters on them"; "During your next turn, this Pokémon can't retreat"; "Heal N damage from your Active Pokémon"; the
Ability triggers "Once during your turn, when you play this Pokémon from your hand to evolve 1 of your Pokémon, you may use this
Ability" and "When you play this Pokémon from your hand to evolve 1 of your Pokémon during your turn, you may …"; and "If this
Pokémon is in the Active Spot and is damaged by an attack from your opponent's Pokémon (even if this Pokémon is Knocked Out), the
Attacking Pokémon is now <a Special Condition>" or "… put N damage counters on the Attacking Pokémon".

Passives on Abilities and Tools: "This Pokémon takes N less damage from attacks (after …)" and "The Pokémon this card is
attached to takes N less …"; "Prevent all damage done to this Pokémon by attacks from your opponent's Pokémon ex"; "As long
as this Pokémon is on your Bench, prevent all damage from and effects of attacks from your opponent's Pokémon done to this
Pokémon"; "As long as this Pokémon is in the Active Spot, attacks used by your opponent's Active Pokémon do N less damage
(before …)"; and "Prevent all effects of attacks used by your opponent's Pokémon done to this Pokémon".

Two shapes follow how our hand-written effects say them, so the programs compare equal:

- "You may shuffle N Energy attached to this Pokémon into your deck. If you do, …" checks that the Pokémon has N Energy
  first, then asks.
- "You may discard any amount of …" doesn't ask: choosing none is the same as saying no.

Inside an attached-Energy choice, a filter leaves out `"kind": "energy"`, since everything attached there is Energy. From
the deck, the hand or the discard pile it's kept, so "Basic Energy" doesn't also match a Basic Pokémon.

## Cards other than attacks

`translate_card` reads every text of a card, each with its own rules, and returns what reads as an effects-file overlay:

| Text | Reads | Becomes |
|---|---|---|
| Item, Supporter | the effect sentences above and the ones below; "You can use this card only if …" | `effects`, `requires` |
| Ability | "Once during your turn, [if this Pokémon is in the Active Spot, \| if …,] you may use this Ability." or "… you may <effect>"; "… you may discard <cards> from your hand in order to use this Ability"; "You must discard … in order to use this Ability"; "When you play this Pokémon from your hand onto your Bench, …"; "You can't use more than 1 <name> Ability each turn" (or one that has a word of its name); "As often as you like during your turn …"; passives | `effects`, `requires`, `active_only`, `trigger`, `one_per_name`, `once_per_turn`, `passive` |
| Tool | "The Pokémon this card is attached to gets +N HP", "… is {C}{C} less" (Retreat Cost), "… has no Retreat Cost", "Attacks used by the [Poisoned] Pokémon this card is attached to do N more damage …" | `passive` |
| Stadium | "Once during each player's turn, that player may …" (their turn read as yours); "Whenever any player puts a … Pokémon onto their Bench during their turn, place N damage counters on that Pokémon"; "… Pokémon in play (both yours and your opponent's) have no Retreat Cost" | `effects`, `requires`, `trigger`, `passive` |
| Special Energy | "As long as this card is attached to a Pokémon, it provides {X} Energy"; "If this card is attached to a … Pokémon, it provides … instead" or "… provides every type of Energy but provides only N Energy at a time"; "… discard it at the end of your turn" | `provides`, `provides_when`, `discard_at_end_of_turn` |

The sentences added for these: drawing for either player, each player shuffling their hand in, the opponent's hand to the
bottom of their deck, switching your Active Pokémon (with or without a type on both), switching a Benched Pokémon (", except
any <name>,") with your Active one, from the discard pile into the hand (a list joined by "or", or "in any combination of"),
moving an Energy between your Pokémon, searching for a list of cards (one search each) or for one of several (one search),
searching cards onto the top of the deck, looking at the top cards, a damage bonus for this turn, attaching to each chosen
Pokémon, discarding a Stadium, discarding an Energy from 1 of the opponent's (Benched) Pokémon, and "the new Active
Pokémon is now …". "If …, … instead" replaces the sentence before with an
`if` whose `else` is that sentence.

**What a card needs to be played** comes from its effects, as our hand-written files say it, so a card that would do nothing
isn't offered. Effects after "If you do" add nothing, since the card can be played without them (Samurott can switch its
own Active when the opponent has no Bench):

| Effect | Requires |
|---|---|
| Cards from the discard pile, or attaching from it | the discard pile has one; for attaching, a Benched Pokémon that matches (`bench_has`) or one in play (`in_play_has`) |
| Switching your Active Pokémon | a Benched Pokémon (that matches, if a type is named); a filtered Active Pokémon is required as written |
| Switching in the opponent's Benched Pokémon | the opponent has one |
| Moving an Energy between your Pokémon | one of your Pokémon has a matching Energy, and you have a Benched Pokémon |
| Putting Pokémon onto the Bench | room on the Bench |
| Attaching from the hand | a matching card in the hand |
| Discarding a Stadium | a Stadium in play |
| A cost: discarding N cards, or a named card, from the hand | N other cards in the hand, or a matching one |

Requirements count as the same whatever their order. In passives, Pokémon filters leave out `"kind": "pokemon"`, since a
passive only ever looks at Pokémon in play.

## Types and functions

| Name | What it is |
|---|---|
| `AttackContext` | The attack's `name` (for "can't use <name>") and printed `damage_mod` |
| `Translation` | `ok`, the `effects` list, and the first sentence `unread` when it isn't ok |
| `normalize_text(text)` | The cleaned text |
| `split_sentences(text)` | Its sentences, without their full stops |
| `translate_attack(text, context)` | The attack's effects, or why not |
| `TextPart` | `Trainer` (Item or Supporter), `Stadium`, `Tool`, `Ability`, `SpecialEnergy` |
| `translate_text(text, part, name)` | One text's fields, such as `effects` and `requires`, in `effects` (an object), or why not |
| `CardTranslation` | The `overlay` (an effects file: `attacks`, `abilities` and the card's own fields), the parts `unread` with why, and how many `parts` have text |
| `translate_card(card)` | Every text of the card, the ones read and the ones not |
| `translation_differences(written, translated, overlay)` | The parts in the overlay where two loaded cards differ: programs, requirements, passives, Ability flags, triggers and what Energy provides |

## Checking it

`pokebot-import --check-translations` (see [the command](main.md#--check-translations)) translates every text of every
hand-written card and compares what loads, then reports how much of Standard it reads and the sentences it most often
can't. The importer [stores what it reads](standard.md#translations) for cards without effects, and
`pokebot-import --audit` ([the command](main.md#--audit-a-sample-of-translations)) prints a random sample of those
texts to check by hand. Programs compare with `==` on [`Step`](../../src/cards/effects/program.md#steps-what-the-engine-runs).

## Tests

[`tests/test_translate.cpp`](../../../tests/test_translate.cpp), tag `[translate]`, with made-up texts in the cards'
wording: cleaning and splitting; damage formulas, flips and conditions; damage to chosen Pokémon and damage counters;
cards from the deck, the discard pile and Energy; lasting effects, switching and other Pokémon's attacks; texts it
mustn't read; Items and Supporters with their requirements; Abilities; Tools, Stadiums and Special Energy; and, on the real
cards, every hand-written text it reads loading the same as the hand-written one, and exactly the known texts left
unread, so a text that stops being read fails it.
