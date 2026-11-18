# `src/engine/observation` — what one player can see

**Files:** [`observation.hpp`](../../../../src/engine/observation/observation.hpp), [`observation.cpp`](../../../../src/engine/observation/observation.cpp) · **Library:** `pokebot_lib` · **Namespace:** `pokebot`

A `GameState` holds every card, hidden ones included. An `Observation` holds only what one player
could see sitting at the table. It's all an [agent](../../agents/agent.md) gets, so an agent can't
cheat, even by accident.

```cpp
Observation observe(const GameState& game, int viewer);
```

It's built from what [knowledge](../knowledge/knowledge.md) says the viewer knows. Like `GameState`,
it's a plain value (trivially copyable, 3,432 bytes) with `==`.

## What's in it

| Field | Holds |
|---|---|
| `viewer` | Whose view this is. `me()` and `opponent()` pick out the two `PlayerView`s |
| `turn`, `current_player`, `first_player`, `phase`, `flags`, `history`, `lasting`, `result` | Copied from the game: all public |
| `decision` | What's being decided and by whom (not meaningful once the game is over) |
| `effect` | The attack, Trainer or Ability resolving, if any (`effect.card` is `kNoCard` if none) |
| `players` | Each side of the table, as below |
| `deck_or_prizes` | The viewer's own cards whose place they don't know, **sorted**: their deck and Prize cards as one list |
| `opponent_seen` | Every opponent card the viewer has seen this game, wherever it is now, sorted |
| `def(card)` | The card's definition, or `nullptr` for an opponent card the viewer hasn't seen |

Each `PlayerView` has:

| Field | Holds |
|---|---|
| `hand_size`, `deck_size`, `prize_count`, `mulligans` | Counts, which both players can see |
| `hand` | The cards in that hand the viewer knows: all of their own; of the opponent's, only cards they were shown (sorted, so the order doesn't hint at the rest) |
| `deck` | Deck cards whose place the viewer knows, as `DeckCard{card, from_top}`, top first (cards they put there or looked at, until a shuffle) |
| `discard`, `lost_zone`, `resolving`, `stadium` | Copied: all face up |
| `active`, `bench` | Each Pokémon as a `PokemonView`: the `PokemonInPlay` plus its `max_hp` as the engine works it out (Tools and Stadiums can change it). `remaining_hp()` takes the damage off |
| `face_down` | How many Pokémon put down during setup the viewer can't see yet. They're left out of `active` and `bench` |

## Why it's shaped this way

- **Deck and Prize cards are one list.** You know your decklist, so you know which of your cards are
  in the deck or the Prize cards together, but not which six are the prizes. Sorting the list means
  the engine's deck order can't leak either.
- **Card definitions only for seen cards.** Card numbers are [shuffled per game](../knowledge/knowledge.md#card-numbers),
  so an opponent card the viewer hasn't seen is just a number with no definition. The opponent's
  deck can't be read off the card table.
- **`opponent_seen` is what deck guessing will use** (M7): a Dreepy seen once still says Dragapult,
  even after it's shuffled back into the deck.
- **Two games that differ only in cards the viewer can't see give equal observations.** That's the
  test that proves nothing leaks.

## Describing it

`describe(observation)` ([describe.md](../describe/describe.md#an-observation)) writes it as text, with
the opponent's hand as a count and the cards known in it.

## Tests

[`tests/test_observation.cpp`](../../../../tests/test_observation.cpp), tag `[observation]`:

- your own hand and only the size of the opponent's;
- definitions only for seen cards;
- the deck and Prize cards as one sorted list;
- a revealed card in the opponent's view of the hand;
- cards put on the bottom of the deck shown where they are;
- face-down setup Pokémon;
- games that differ only in hidden cards look the same, and a change the viewer can see does show;
- random games on the sample cards and on the real decks, checking after every action that every
  card an observation names is one whose place the viewer knows, with its definition. The real-deck
  games also swap hidden cards every 20 actions and check the viewer sees no difference;
- `describe(observation)`;
- [`play_game`](../../harness/match.md) showing each agent its own player's view, and on the real
  decks, every action offered naming only cards the deciding player knows the place of.
