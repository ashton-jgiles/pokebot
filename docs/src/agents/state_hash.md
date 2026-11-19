# `src/agents/state_hash` — state hashes and the closed set

**Files:** [`src/agents/state_hash.hpp`](../../../src/agents/state_hash.hpp), [`src/agents/state_hash.cpp`](../../../src/agents/state_hash.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The turn planner (Model 1) keeps a **closed set**: the states it has already expanded in one search.
A child whose state is already in it isn't expanded again. That removes the many orders of the same
independent actions, such as attaching Energy then benching a Pokémon versus benching then attaching
(the Blocks World argument for graph-based search), and in the and-tree mode reuses a value already
worked out.

```cpp
std::uint64_t hash_state(const GameState& game);

ClosedSet closed;
closed.insert(game);   // false if an equal state was already in
closed.contains(game);
```

## What the hash covers

Everything in a `GameState` that decides what happens next: both players' zones (the deck in order,
the Prize cards, discard pile, Lost Zone, the Pokémon in play with their Energy, Tools, damage,
Special Conditions and the turn they came into play, cards being played, the Stadium, mulligans),
whose turn it is and its phase, the turn's flags and Abilities used, the game history, the pending
decisions, the effects resolving, lasting effects, what each player knows, and the result.

It leaves out two things:
- **The random number generator.** Det actions don't use it, so states the planner reaches by
  different Det orders have the same one anyway; leaving it out also merges states whose only
  difference is a generator that will be re-dealt in a sampled world.
- **The order of the cards in each hand.** Nothing in the rules depends on it; two orders of the same
  hand are the same position.

Each value is folded in through SplitMix64's finalizer, so every bit of it reaches every bit of the
hash. A plain hash of the state's bytes wouldn't do: a `FixedVector`'s unused slots can hold anything.

**Collisions:** the closed set keeps only hashes, as the proposal's definition says, so two different
states with the same 64-bit hash would be taken as one. Over one search of at most some thousands of
states, the chance is far below one in a billion. The tests check that equal hashes mean equal states
over thousands of real-deck states.

**If `GameState` gains a field,** it has to be added to `hash_state` too. The real-deck test would
catch a forgotten field only if it ever made two states with the same hash differ.

## Tests

[`tests/test_state_hash.cpp`](../../../tests/test_state_hash.cpp), tag `[planner]`:

- attaching then benching and benching then attaching reach the same state and hash, and so do
  evolving then attaching and attaching then evolving;
- the hash changes with damage, a flag, a card moving, two different deck cards swapping places, a
  pending decision, what a player knows and the turn, and not with the random number generator or the
  hand's order;
- on the real decks, states in random games with the same hash are the same state;
- a closed set holds each state once.
