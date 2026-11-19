# `src/agents/greedy_agent` — one action ahead

**Files:** [`src/agents/greedy_agent.hpp`](../../../src/agents/greedy_agent.hpp), [`src/agents/greedy_agent.cpp`](../../../src/agents/greedy_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The second rung of the agent ladder (M5): Random → **Greedy** → turn planner (M6). It looks one
action ahead and takes the action whose result the [evaluation function](evaluation.md) scores best.
In CPSC 433 terms it's hill climbing, or greedy best-first search to depth 1, in the or-tree of the
deciding player's actions, with e as the heuristic.

```cpp
GreedyAgent greedy(seed);            // 3 worlds, default_weights()
GreedyAgent tuned(seed, 5, weights); // more worlds, other weights
```

## How it chooses

It only ever gets an [`Observation`](../engine/observation/observation.md), so at each decision:

1. It builds `worlds` (3 by default) [worlds](world.md) from the observation: games that look
   exactly like what it sees, with the cards it can't see dealt at random.
2. For each legal action and each world, it copies the world's game, applies the action with
   `apply_legal` (which runs on to the next decision, whoever's it is) and scores the result with
   `evaluate` for itself: e, or the utility u if the game ended.
3. It takes the action with the best average score, breaking exact ties at random.

**Every action is tried in the same worlds,** with the same hidden cards and the same coin flips,
so differences between actions come from the actions, not from luck: common random numbers. An
action that uses no chance and reveals nothing gets the same score in every world. Draws, searches,
shuffles and flips differ, so they're averaged.

`scores(observation, actions)` returns each action's average score, as `choose` works it out.

**A single legal action** is taken without building any world.

## Setup

No world can be built during setup (the opponent's Pokémon are face down), so it uses a fixed rule:

| Decision | Choice |
|---|---|
| Turn order | Go first |
| Active Pokémon, Benched Pokémon | The Basic with the most HP, and keep putting Basics on the Bench until none are left |
| Mulligan draws | Draw as many as allowed |

## What it does badly

It sees one action ahead, so it can't plan a turn: it may attack before attaching the Energy that
would have made the attack a Knock Out, and Trainers whose payoff comes after a choice (Ultra Ball
discards first) look worse than they are. It ends its turn as soon as nothing it can do scores
higher. That's what the turn planner (M6) is for.

## Where it's used

`pokebot-sim --agent greedy` ([sim](../../tools/sim/main.md)) and `pokebot play|watch --agent greedy`
([client](../client/main.md)).

## Tests

[`tests/test_greedy.cpp`](../../../tests/test_greedy.cpp), tag `[greedy]`:

- it takes a Knock Out when it can;
- it attaches Energy rather than ending the turn, and scores attaching higher;
- during setup it goes first, puts down the Basic with the most HP, and doesn't stop putting Basics
  on the Bench while any are left;
- the same seeds play the same game;
- whole games on the sample cards keep every invariant;
- on the real decks it beats the random agent in at least 3 of 4 games, from either seat.
