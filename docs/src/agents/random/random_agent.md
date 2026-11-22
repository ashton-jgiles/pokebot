# `src/agents/random/random_agent` — random agents

**Files:** [`src/agents/random/random_agent.hpp`](../../../../src/agents/random/random_agent.hpp), [`src/agents/random/random_agent.cpp`](../../../../src/agents/random/random_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Two baseline [agents](../agent.md). Both take a seed, so the same seed always makes the same choices.

## `RandomAgent` (`"random"`)

Picks uniformly among the legal actions. It's the **baseline every other agent has to beat**.

It plays badly: it ends turns early, spreads Energy across the Bench, and retreats Energy away. In
real-deck games it attacks about 3 times per game, and nearly every game ends by deck-out.

## `BiasedRandomAgent` (`"biased"`)

Still random, but weighted toward actions that move the game forward:

| Action | Weight |
|---|---|
| `Attack` | 100 |
| `Evolve` | 10 |
| `AttachEnergy` to the Active | 10 |
| `PlayBasic` | 5 |
| `AttachEnergy` to the Bench | 3 |
| Anything else (including `EndTurn`, `Retreat`) | 1 |

It exists to **stress-test the combat rules**. With it, about 58% of games end by Knock Outs
(Prize cards or no Pokémon left), compared with about 2.5% for `RandomAgent`, so Knock Outs,
Prize cards and promotions get exercised hundreds of thousands of times. It isn't meant to play
well.

## Tests

[`tests/test_agents.cpp`](../../../../tests/test_agents.cpp), tag `[agents]`: the random agent picks
every choice and only offered ones; the biased agent attacks whenever it can, over 95% of the time;
the same seed repeats choices. They ignore the observation, so the tests hand them an empty one.
