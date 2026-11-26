# `tools/import/translate` — card text to effects

**Files:** [`tools/import/translate.hpp`](../../../tools/import/translate.hpp), [`tools/import/translate.cpp`](../../../tools/import/translate.cpp)
**Library:** `pokebot_import_lib` · **Namespace:** `pokebot::import`

Reads a card's text and writes its effects in the [effect language](../../src/cards/effects/program.md), the same JSON
as a hand-written [effects file](../../data/cards/README.md#effectsidjson--hand-written-effects). It's M3b's way to
cover Standard without writing every card by hand. So far it reads attacks, with the vocabulary the engine has today.

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
   Resistance for Benched Pokémon", "Pokémon ex, Pokémon V, etc. have Rule Boxes"), turns curly apostrophes and
   non-breaking spaces into plain ones, and collapses spaces.
2. **`split_sentences`** splits at each full stop followed by a space or the end.
3. **Each sentence is matched by its rules,** tried in order; one must use every word. A sentence none of them reads
   whole is tried as two joined by "and" or ", and".
4. **Sentences are joined** the way card text does: "If heads, …", "If tails, …" (the `else` of a flip), "You may …",
   "If you do, …" (into the "may" before it), "Then, …".
5. **The damage step goes after** the last block that changes the damage (`set_damage`, `add_damage`,
   `ignore_defender_effects`); without those it's first, as the compiler puts it.

The parts of a sentence it reads:

| Part | Reads | Becomes |
|---|---|---|
| How many | "a", "an", "2", "up to 3", "any number of", "any amount of", "all" | `count`, `up_to` |
| Cards | "Basic {G} Energy card", "{D} Pokémon", "Pokémon ex", "Stage 2 Pokémon", "Evolution Pokémon", "N's Pokémon", "Basic Pokémon with 70 HP or less", "Pokémon that doesn't have a Rule Box", "Supporter card", "Item card", "Pokémon Tool card", "Stadium card", "Trainer card", "card" | a `filter` |
| Opposing Pokémon | "1 of your opponent's Pokémon", "2 of your opponent's Benched Pokémon" | `targets`, `on` |
| Counts after "for each" | heads; damage counters on this or that Pokémon; cards in a hand; Prize cards taken; Benched Pokémon (yours, theirs or both); Energy attached to this, the opponent's or both Active Pokémon; cards discarded in this way; cards in a discard pile | `for_each` |
| Conditions after "If" | heads, both or either of them heads, tails; a Knock Out (by an attack) last turn; a Stadium in play; more Prize cards left; Pokémon sharing a type; the opponent at N or fewer Prize cards; exactly N Prize cards left; this Pokémon with a type of Energy | `condition` |

The sentences it reads: drawing; discarding or shuffling the hand; searching the deck (revealing, to the hand or the Bench);
attaching Energy from the deck, the hand or the discard pile; discarding Energy from this Pokémon or any of yours; putting
Energy attached to this Pokémon into the hand; damage formulas; damage to chosen opposing Pokémon; flipping coins; Special
Conditions on either Active Pokémon; can't attack, can't use this attack, can't retreat and no Items next turn; switching,
gusting, Tuck Tail and shuffling this Pokémon into the deck; damage counters, placed or on chosen Pokémon; using a Benched
Pokémon's attack; and ignoring effects on the defender.

Two shapes follow how our hand-written effects say them, so the programs compare equal:

- "You may shuffle N Energy attached to this Pokémon into your deck. If you do, …" checks that the Pokémon has N Energy
  first, then asks.
- "You may discard any amount of …" doesn't ask: choosing none is the same as saying no.

Inside an attached-Energy choice, a filter leaves out `"kind": "energy"`, since everything attached there is Energy. From
the deck, the hand or the discard pile it's kept, so "Basic Energy" doesn't also match a Basic Pokémon.

## Types and functions

| Name | What it is |
|---|---|
| `AttackContext` | The attack's `name` (for "can't use <name>") and printed `damage_mod` |
| `Translation` | `ok`, the `effects` list, and the first sentence `unread` when it isn't ok |
| `normalize_text(text)` | The cleaned text |
| `split_sentences(text)` | Its sentences, without their full stops |
| `translate_attack(text, context)` | The attack's effects, or why not |

## Checking it

`pokebot-import --check-translations` (see [the command](main.md#--check-translations)) translates every hand-written
attack and compares the compiled programs, then reports how many of Standard's attack texts it reads and the
sentences it most often can't. Programs compare with `==` on [`Step`](../../src/cards/effects/program.md#steps-what-the-engine-runs).

## Tests

[`tests/test_translate.cpp`](../../../tests/test_translate.cpp), tag `[translate]`, with made-up texts in the cards'
wording: cleaning and splitting; damage formulas, flips and conditions; damage to chosen Pokémon and damage counters;
cards from the deck, the discard pile and Energy; lasting effects, switching and other Pokémon's attacks; texts it
mustn't read; and, on the real cards, every hand-written attack it reads compiling to the same program.
