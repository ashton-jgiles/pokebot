# `src/cards/deck_validator` — deck-building rules

**Files:** [`src/cards/deck_validator.hpp`](../../../src/cards/deck_validator.hpp), [`src/cards/deck_validator.cpp`](../../../src/cards/deck_validator.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Checks a `Decklist` against the deck-building rules, using the card database for each card's real
name and rules. `pokebot-import` runs it on every deck it imports. The engine will run it
before a game starts.

## API

```cpp
struct DeckValidation {
    std::vector<std::string> problems; // empty when the deck is legal
    bool legal() const;
};

DeckValidation validate_deck(const Decklist& deck, const CardDB& db);
```

It never throws for an illegal deck. Every problem becomes a message in `problems`, so a
single run reports **all** of them.

## Rules checked

| Rule | Rulebook | Example message |
|---|---|---|
| Every card exists in the card database | — | `unknown card TST-999 ("Mystery Card"); run pokebot-import on this deck` |
| Exactly 60 cards | p. 22 | `deck has 59 cards; it must have exactly 60` |
| At most 4 cards with the same **name**, except Basic Energy | p. 21–22 | `5 cards named "Ultra Ball"; at most 4 are allowed` |
| At least 1 Basic Pokémon | p. 22 | `no Basic Pokemon; a deck needs at least 1` |
| At most 1 ACE SPEC card in total | p. 26 | `2 ACE SPEC cards (Unfair Stamp, Secret Box); at most 1 is allowed` |
| At most 1 Radiant Pokémon | p. 29 | `2 Radiant Pokemon (Radiant Alpha, Radiant Beta); at most 1 is allowed` |

## Details

- **Names come from the card database, not the decklist.** All printings of a name count together:
  3 Greninja ex (TWM-106) plus 2 Greninja ex (30C-21) is 5 cards named "Greninja ex".
- **Basic Energy** is exempt from the 4-copy rule. Special Energy is not.
- **Unknown cards** are reported and skipped for the other checks. The "no Basic Pokémon" check is
  also skipped when any card is unknown, because the missing card might be the Basic.
- **Watch out for names that include a level** ("Gengar LV.43"). The rulebook counts it as the
  same name as "Gengar", but the validator compares names exactly. None of the current decks use
  such cards.

## Tests

[`tests/test_deck_validator.cpp`](../../../tests/test_deck_validator.cpp), tag `[deck_validator]`.
It uses a small made-up card database, with one test per rule and one with several problems
at once. The last test validates every deck in `data/decks/` against the real card data, and is
**skipped** if `data/cards/generated/` doesn't exist yet.
