# `src/agents/endgame/chance` — every way an action can turn out

**Files:** [`src/agents/endgame/chance.hpp`](../../../../src/agents/endgame/chance.hpp), [`src/agents/endgame/chance.cpp`](../../../../src/agents/endgame/chance.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The endgame solver's chance nodes. In a world ŵ, `chance_outcomes` takes a legal action and returns
every way it can turn out, each with its probability, so the solver can take an expectation where
Flat MC and IS-MCTS sample.

```cpp
Rng rng(seed);
for (const Outcome& o : chance_outcomes(world, action, rng)) {
    o.state;        // the game after the action, in this outcome
    o.probability;  // the outcomes' probabilities add up to 1
}
```

## The three kinds of chance

- **Coin flips** run on a script ([`Rng::script_flips`](../../engine/shared/rng.md)): the action is
  played with a sequence of heads and tails, and whenever it asks for more flips than the script
  holds, the sequence branches on heads and tails for the next flip, up to `max_flips`. A sequence
  of length L has probability 2^−L; at the cap, the rest of the flips are tails.
- **Draws and looks at the top of a deck.** One probe run shows which of the deck's top cards the
  action touched: those that left it from the top (drawn, discarded, looked at and taken) and those
  that became known to someone. Every combination of cards for those top slots is placed there
  before the action is replayed, with probability ∏ C(copies, chosen) / C(pool, slots), the pool
  being the deck's cards whose place no one knows. Cards whose place someone knows stay where they
  are. With more than `max_outcomes` combinations, `samples` of them are drawn instead, each with
  weight 1/`samples`. Both players' decks are handled, so ending the turn branches on the
  opponent's draw.
- **A shuffle before a draw** (a hand shuffled into the deck, then a draw): the probe shows the
  deck reordered, so the action is sampled: each sample shuffles the unseen cards' places and
  reseeds the game's generator.

Outcomes that look the same to both players (by [`hash_observation`](../is_mcts/info_set.md) of each
player's observation) are one outcome, their probabilities added. A search that shows the whole
deck is therefore one outcome: every order of the deck looks the same.

The probe classifies the deck change. The bottom of the deck is the front of the vector, so the
cards still there must be the old deck's bottom part in the same order, with only new cards put
under them (a hand put on the bottom); the depth then extends down to the deepest card that became
known. Anything else, a reshuffle or a card taken from the middle, is sampled.

| `ChanceOptions` | Default | Meaning |
|---|---|---|
| `max_outcomes` | 32 | The most combinations of top cards enumerated; above it, sampled |
| `samples` | 8 | Samples when sampling |
| `max_flips` | 8 | The longest flip sequence enumerated |

`ChanceStats` counts actions, actions with more than one flip sequence, enumerated and sampled
draws, and outcomes. `count_placements(copies, first, second)`, also used by the
[endgame lab](../../harness/endgame_lab.md), counts the ways to pick two disjoint groups of `first`
and `second` cards from cards with the given numbers of copies.

**Known limit:** when an action draws several cards one after another and their order matters
(draw one, and if it's a Pokémon draw another), which cards are drawn is enumerated, not their order.

## Tests

[`tests/test_chance.cpp`](../../../../tests/test_chance.cpp), tag `[endgame]`, on sample cards:

- an action without chance has one outcome, the action's own result;
- the opponent's draw at the start of its turn branches on each card, weighted by its copies;
- drawing two cards from a 5-card deck gives the same outcomes and probabilities as all 120 orders of
  the deck (brute force);
- a coin flip splits the outcomes in half (the attack then ends the turn, so the opponent's draw
  splits them again);
- flipping until tails stops at `max_flips`, with probabilities 1/2, 1/4, 1/8 and 1/8;
- a search is one outcome; shuffling the hand in before drawing is sampled, adding up to 1;
- on the real decks, every legal action's outcomes add up to 1 and keep the game's invariants.
