# `src/agents/is_mcts/info_set` — a key for the searcher's information set

**Files:** [`src/agents/is_mcts/info_set.hpp`](../../../../src/agents/is_mcts/info_set.hpp), [`src/agents/is_mcts/info_set.cpp`](../../../../src/agents/is_mcts/info_set.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

IS-MCTS (M8) builds one tree over many determinizations. A node is an information set of the
searching player: everything they could see at that point. So a node's identity is the searcher's
[observation](../../engine/observation/observation.md), and this file reduces one to a 64-bit key.

```cpp
const std::uint64_t key = hash_observation(observe(world_state, searcher));
```

Equal observations give equal keys. The key is also equal for observations that differ only in how
cards are numbered, which is what makes it an information-set key rather than a hash of the bytes.

## Cards by what they are, not their number

Within one search every determinization ŵ is dealt from the same observation, but the opponent's
unseen cards get different numbers in each. When the opponent attaches a Basic {D} Energy, it may be
card 70 in one world and card 85 in another. Those observations differ, yet to the searcher it's the
same move, and both must lead to the same node.

So every card is hashed by its definition: an FNV-1a hash of the card id (`TWM-128`). A card the
viewer hasn't seen hashes as "unseen", and `kNoCard` as "none". That covers every place a card number
appears: the zones, each Pokémon's stack, Energy and Tool, and the Pokémon ids (the number of the
card at the bottom of the stack) that effects, lasting effects, Abilities used and the history point
at. Each card's key is worked out once per call.

## What's hashed

Everything an `Observation` holds: the turn, phase, flags, history, lasting effects, the decision
stack, the choice owner, the effects resolving, the result, both `PlayerView`s (counts, hand, the
deck cards whose place is known, discard and lost zone, the Pokémon in play with their HP, the
resolving cards and the Stadium), `deck_or_prizes` and `opponent_seen`.

- **Sorted:** the hand, `deck_or_prizes` and `opponent_seen`, whose order means nothing to the viewer
  (as `hash_state` ignores the hand's order).
- **In order:** everything else, the discard pile and the bench included.
- **Not hashed:** the `defs` array itself; each card's definition goes in through its key.

## Limits

- **Copies of the same card are interchangeable.** With two Dreepy on the Bench, a lasting effect or a
  target on one hashes the same as on the other, since both point at `TWM-128`. Each Pokémon is still
  hashed in its own Bench position with its own damage, so the two states rarely meet.
- **A 64-bit hash can collide;** two different information sets sharing a node would only blur their
  statistics.

## Tests

[`tests/test_info_set.cpp`](../../../../tests/test_info_set.cpp), tag `[is_mcts]`, on the real decks
(skipped without generated card data):

- equal observations hashing the same, whatever the order of the hand;
- two worlds dealt from the same observation, where the opponent attaches differently numbered copies
  of the same Energy: different observations, the same key;
- a change the viewer can see (damage, the opponent's hand size, the viewer, the turn) changing it;
- a different card in the hand, or a card the viewer hasn't seen, changing it.
