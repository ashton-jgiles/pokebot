# `src/agents/world` — a game consistent with what a player can see

**Files:** [`src/agents/world.hpp`](../../../src/agents/world.hpp), [`src/agents/world.cpp`](../../../src/agents/world.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Agents get only an [`Observation`](../engine/observation/observation.md), but to look ahead they
need a `GameState` to try actions on. A `World` is one: every card the viewer can see is where they
see it, and the cards they can't place are dealt at random. The greedy agent (M5) tries each action
in a few worlds; the turn planner (M6) searches in one. Given a [deck model](flat_mc/deck_model.md),
a world also deals the opponent's unseen cards from a decklist guessed from the cards seen (M7):
that's a determinization ŵ.

```cpp
World world(observation, rng);
GameState next = world.game(); // a copy to try an action on
apply_legal(next, action);

World sampled(observation, rng, &model); // the opponent's unseen cards dealt from a list the belief draws
sampled.list();                           // that list
```

## What gets dealt

| Cards | Dealt from | Into |
|---|---|---|
| The viewer's own | Their cards not in view (the same cards as `deck_or_prizes`) | The deck places whose card they don't know, then the Prize cards |
| The opponent's | The opponent's cards not in view: ones seen before (shuffled back, say) keep their definition; the rest are `unknown_card()`, or with a deck model, cards of the list drawn | The deck places they don't know, the hidden part of the hand, then the Prize cards |

Everything else is copied from the observation: the Pokémon in play, the open zones, the deck cards
whose place is known, the turn, flags, history, lasting effects, the pending decisions and the
effects resolving. The `World` gets its own `rng` (from the one passed in), so it can't foresee the
real game's coin flips or shuffles.

`unknown_card()` is an Item that does nothing. The opponent's unseen cards are never in play after
setup, so only their hand, deck and Prize cards hold it.

### With a deck model

`World(observation, rng, &model)` gives the opponent's never-seen cards real definitions:

1. The [belief](flat_mc/deck_model.md#the-belief) is worked out from the observation, and one list
   is drawn from it with `rng`. `list()` returns it (`nullptr` without a model).
2. That list's cards, less the ones seen (counted by card id), are shuffled and given to the
   never-seen card numbers. Cards seen that the list doesn't explain weren't taken off, so the list
   has that many cards too many, and the shuffle leaves out that many at random.
3. Dealing then goes on as without a model: the same shuffle of card numbers decides which go into
   the hand, the deck and the Prize cards, so each keeps its observed size.

Seen cards keep the definition the viewer saw, and the viewer still hasn't seen the dealt ones, so
the world still observes back to the same observation. In it the opponent knows its own (sampled)
hand, while the viewer's unseen cards stay `unknown_card()`.

Without a model (`nullptr`, the default) nothing changes, and `rng` is used exactly as before, so
Greedy and the Planner play the same games as at their sign-offs. The `DeckModel`, its library and
the `CardDB` must outlive the world, whose card table points at the library's definitions.

**What the viewer knows** is rebuilt to match: they know where every card in view is and have seen
every card in `opponent_seen`. The opponent knows only what's in the open and their own hand.

**A card an effect chose that the viewer can't see** (a `kNoCard` in an `EffectRun`'s `chosen`, such
as the opponent's pick in a search they don't reveal) is filled in with one of the cards dealt into
the zone the effect is choosing from: that player's hand (discarding for Ultra Ball) or deck (a
search). Which card it is doesn't show to the viewer, so any of them will do.

## The guarantee

`observe(world.game(), viewer) == observation`: a world looks exactly like the game it came from to
the player it was built for. So the deciding player is offered the same actions in it, and an action
that uses no chance and reveals nothing (attaching Energy, benching a Pokémon, retreating) looks the
same afterwards as in the real game. Draws, searches, shuffles and flips come out differently in each
world, which is why the greedy agent averages over several.

## Limits

- **Only after setup and before the game is over.** During setup the opponent's Pokémon are face
  down, so there is nothing to put in their places; the constructor throws `std::invalid_argument`.
- **A `World` can't be copied or moved,** because its `GameState` points at its own `CardTable`.
  Copies of `game()` are fine while the `World` lives.
- An observation whose hidden cards don't add up (it can't happen from `observe`) throws `std::logic_error`.

`CardTable` gained a constructor from an array of definitions for this
([game_state.md](../engine/game/game_state.md#cardtable)).

## Tests

[`tests/test_world.cpp`](../../../tests/test_world.cpp), tag `[world]`:

- in random games on sample cards (with a search that isn't revealed and a look at the top of the
  deck) and on the real decks, at every action after setup and for both players: the world observes
  back to the same observation, passes the invariant checker, and offers the deciding player the same
  actions;
- your deck and Prize cards dealt from the same cards, differently each time, with your hand as it is;
- the opponent's unseen cards are `unknown_card()` and seen ones keep their definition;
- attaching Energy, benching and retreating in a world look the same afterwards as in the game;
- a card the opponent is choosing from their hand stood in for by one dealt into their hand;
- no world during setup;
- with a deck model, on the real decks with the opponent playing each of the 12 archetype lists,
  every 5th action after setup and from both sides: the world observes back, passes the invariant
  checker and offers the same actions; seen opponent cards keep their definitions, and each unseen
  card is dealt from the list drawn, no more copies than it has left after the ones seen; worlds
  draw more than 4 different lists; and with the opponent's exact list left out of the library, no
  world draws it;
- without a deck model, exactly the same world and the same use of `rng` as before.
