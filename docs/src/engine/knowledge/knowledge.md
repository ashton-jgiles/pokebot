# `src/engine/knowledge` — who knows what

**Files:** [`knowledge.hpp`](../../../../src/engine/knowledge/knowledge.hpp), [`knowledge.cpp`](../../../../src/engine/knowledge/knowledge.cpp) · **Namespace:** `pokebot` (helpers in `pokebot::detail`)

The engine knows where every card is. A player doesn't. This records what each player knows, so
the observation layer (step 4.2) can show them exactly that and nothing more. It's also what later
search will use to guess hidden cards: everything a player doesn't know is what gets dealt at random.

```cpp
bool knows_where(const GameState& game, int player, CardIndex card);
bool has_seen(const GameState& game, int player, CardIndex card);
template <class Visit> void each_visible_card(const GameState& game, Visit visit); // visit(card, owner, owner_only)
```

## Two sets per player

`GameState::knowledge` holds, for each player, two [`CardSet`](../game/game_state.md#knowledge)s
(one bit per card):

| Set | Means | Cleared |
|---|---|---|
| `located` | The player knows where the card is right now | For every card in a deck when it's shuffled |
| `seen` | The player has seen the card at some point this game | Never |

`seen` is what deck guessing will use: seeing a Dreepy makes a Dragapult deck likely, even after the
Dreepy is shuffled away. `located` is what the observation shows and what search must not re-deal.

## What a player knows

| A card is known to | When | Recorded by |
|---|---|---|
| Both players | It's in the open: in play (with its Energy and Tool), in a discard pile or Lost Zone, the Stadium, or a card being played | `note_visible`, after every action |
| Its owner | It's in their hand | `note_visible` |
| Its owner, during setup | Pokémon put down during setup are face down until both players have set up | `note_visible` (while `turn` is 0) |
| The opponent | A search that says "reveal them" (`"reveal": true`) put it in the hand | `MoveChosen` |
| Its owner | They looked at the top of their deck (Recon Directive), or put it on top themselves (Ciphermaniac's Codebreaking) | `ChooseCards` from the deck top, `MoveChosen` to the deck top |
| Its owner | They searched their deck (Ultra Ball, Nest Ball): they look through all of it, so they also know what's left is their Prize cards | `ChooseCards` from the deck |
| The opponent (seen only) | Its owner mulliganed: the hand is shown, then shuffled back | `new_game` |

A card a player knows stays known when it moves, until a shuffle. So a Pokémon taken from the
discard pile with Night Stretcher is still known to the opponent in its owner's hand, and the cards
Recon Directive put on the bottom of the deck are still known to their owner.

**Prize cards are never known:** they're dealt from a shuffled deck, and the
[invariant checker](../invariants/invariants.md) fails if anyone knows one.

## Card numbers

Players see card numbers, so the numbers mustn't give anything away. [`CardTable`](../game/game_state.md#cardtable)
numbers cards in decklist order, so card 75 being a Dreepy and card 79 a Drakloak would mean four
Dreepy. [`play_game`](../../harness/match.md) therefore plays every game on `cards.shuffled(~seed)`,
which numbers each player's cards in a random order (player 0 still has 0–59, player 1 60–119).

## Functions

| Function | Behavior |
|---|---|
| `knows_where(game, player, card)` | Whether the card is in the player's `located` set |
| `has_seen(game, player, card)` | Whether it's in their `seen` set |
| `each_visible_card(game, visit)` | Calls `visit(card, owner, owner_only)` for every card in a hand (`owner_only`) or in the open |
| `detail::show(game, player, card)` | Adds the card to both of the player's sets |
| `detail::note_visible(game)` | `show`s every visible card to whoever can see it. `apply` calls it after every action, and `new_game` at the end |
| `detail::shuffle_deck(game, player)` | Shuffles the deck and clears `located` for every card in it, for both players. Every deck shuffle goes through it |

**Granularity:** knowledge is updated after each action, not each step. A card that passed through
the open and back out within a single action wouldn't be recorded. No card in our decks does that:
every effect that moves cards out of the open waits for a choice first.

## Tests

[`tests/test_engine_knowledge.cpp`](../../../../tests/test_engine_knowledge.cpp), tag `[knowledge]`: the
open zones and hands, face-down setup, a revealed search known until it's shuffled away, a search
that doesn't reveal, searching the deck showing all of it to its owner until the shuffle, a card
picked up from the discard pile, looking at the top of the deck, a
mulligan showing the hand, and 100 random games checking after every action that every visible card
is recorded and no Prize card is known.
