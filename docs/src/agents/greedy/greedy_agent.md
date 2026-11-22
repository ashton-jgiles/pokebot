# `src/agents/greedy/greedy_agent` — one action ahead

**Files:** [`src/agents/greedy/greedy_agent.hpp`](../../../../src/agents/greedy/greedy_agent.hpp), [`src/agents/greedy/greedy_agent.cpp`](../../../../src/agents/greedy/greedy_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The second rung of the agent ladder (M5): Random → **Greedy** → turn planner (M6). It looks one
action ahead and takes the action whose result the [evaluation function](../evaluation/evaluation.md) scores best.
In CPSC 433 terms it's hill climbing, or greedy best-first search to depth 1, in the or-tree of the
deciding player's actions, with e as the heuristic.

```cpp
GreedyAgent greedy(seed);            // 3 worlds, default_weights()
GreedyAgent tuned(seed, 5, weights); // more worlds, other weights
```

## How it chooses

It only ever gets an [`Observation`](../../engine/observation/observation.md), so at each decision:

1. It builds `worlds` (3 by default) [worlds](../world.md) from the observation: games that look
   exactly like what it sees, with the cards it can't see dealt at random.
2. For each legal action and each world, it copies the world's game, applies the action with
   `apply_legal` (which runs on to the next decision, whoever's it is), **makes its own follow-up
   choices** (below), and scores the result with `evaluate` for itself: e, or the utility u if the
   game ended.
3. It takes the action with the best average score, breaking exact ties at random.

**Every action is tried in the same worlds,** with the same hidden cards and the same coin flips,
so differences between actions come from the actions, not from luck: common random numbers. An
action that uses no chance and reveals nothing gets the same score in every world. Draws, searches,
shuffles and flips differ, so they're averaged.

`scores(observation, actions)` returns each action's average score, as `choose` works it out.

**Follow-up choices (step 5.6c).** Many actions lead straight into another choice of the same
player's: the card to discard for Trade, the two cards for Ultra Ball and then the Pokémon to
search for, the Energy to discard when retreating. Scored at that point, nothing has happened yet:
using Trade with an empty deck looked as good as doing nothing, because the discard came in the next
choice. So after an action, while the next decision is still the agent's and isn't a new main-phase
decision (`MainPhase` or `ExtraAttack`), it makes that choice too, taking the option that scores
best right after it (the first of any tied), up to 64 choices. Only then is the action scored.
The rule lives in [plans](../planner/plan.md) (`finish_choices`), where the turn planner uses it too.

**A single legal action** is taken without building any world.

## Setup

No world can be built during setup (the opponent's Pokémon are face down), so it uses a fixed rule:

| Decision | Choice |
|---|---|
| Turn order | Go first |
| Active Pokémon, Benched Pokémon | The Basic with the most HP, and keep putting Basics on the Bench until none are left |
| Mulligan draws | Draw as many as allowed |

## What it does badly

It sees one action ahead (with its follow-up choices), so it can't plan a turn: it may attack
before attaching the Energy that would have made the attack a Knock Out, or switch in the Pokémon
that can't attack when the other could win the game. It ends its turn as soon as nothing it can do
scores higher. That's what the turn planner (M6) is for.

## Results

The Milestone 5 sign-off (2026-11-19), with the [match harness](../../harness/arena.md): 1,000 games
on each of the 16 deck pairings, every seed played once with Greedy in each seat.

| Against | Greedy's score (95% CI) | Lowest pairing (95% CI) | How Greedy lost |
|---|---|---|---|
| Random | **91.3%** (90.8% to 91.7%) | ns-zoroark against mega-greninja, 76.8% (74.1% to 79.3%) | 1,321 by its own deck-out, 60 by Prize cards, 14 by no Pokémon left |
| Biased random | **75.8%** (75.2% to 76.5%) | ns-zoroark against basic-box, 59.7% (56.6% to 62.7%) | 2,231 by Prize cards, 1,535 by its own deck-out, 102 by no Pokémon left |

How each change in step 5.6 moved it, on the same 16,000 games against Random:

| Greedy | Score (95% CI) | Lost by own deck-out | ns-zoroark's worst pairing |
|---|---|---|---|
| As first built (step 5.4) | 77.7% (77.1% to 78.4%) | 3,457 | 39.3% |
| Deck counted up to 30, hand up to 7 (5.6b) | 82.5% (81.9% to 83.1%) | 2,667 | 46.2% |
| Follow-up choices played out (5.6c) | 91.3% (90.8% to 91.7%) | 1,321 | 76.8% |

It's about 92 games/s against Random on one thread, 856 on 16 (see the [sim](../../../tools/sim/main.md)).

## Where it's used

`pokebot-sim --agent greedy` ([sim](../../../tools/sim/main.md)) and `pokebot play|watch --agent greedy`
([client](../../client/main.md)).

## Tests

[`tests/test_greedy.cpp`](../../../../tests/test_greedy.cpp), tag `[greedy]`:

- it takes a Knock Out when it can;
- it attaches Energy rather than ending the turn, and scores attaching higher;
- an Ability that discards and then draws, with an empty deck, scores lower than doing nothing
  (the discard, chosen after, counts) and isn't used;
- during setup it goes first, puts down the Basic with the most HP, and doesn't stop putting Basics
  on the Bench while any are left;
- the same seeds play the same game;
- whole games on the sample cards keep every invariant;
- on the real decks it beats the random agent in at least 3 of 4 games, from either seat.
