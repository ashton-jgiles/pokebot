# `src/agents/evaluation` — the evaluation function e

**Files:** [`src/agents/evaluation.hpp`](../../../src/agents/evaluation.hpp), [`src/agents/evaluation.cpp`](../../../src/agents/evaluation.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The proposal's heuristic estimate of how good a state is for player i:

  e_i(w) = tanh( Σ_j θ_j · φ_j(w, i) ) ∈ (−1, 1)

and, once the game is over, the utility u_i(w) ∈ {−1, 0, +1}. The greedy agent (M5) scores the state
after each action with it; Models 1–3 use it at their leaves; M10 learns θ.

```cpp
double evaluate(const GameState& game, int player, const Weights& weights = default_weights());
Features features(const GameState& game, int player); // phi(w, i), for M10's training data
double utility(const GameState& game, int player);    // u_i(w); throws if the game isn't over
```

## Features

Each feature is **player i's side minus the opponent's**, divided by a scale and clamped to [−1, 1].
So a positive value favours i, the opponent's features are exactly the negatives of i's, and
e_opponent(w) = −e_i(w): the evaluation is zero-sum, as Model 3's minimax needs.

| Feature | Proposal | Measures (i's minus the opponent's, unless it says otherwise) | Scale | θ |
|---|---|---|---|---|
| `Prizes` | φ1 | Prize cards the opponent has left minus i's | 6 | 2.0 |
| `ActiveHp` | φ2 | The Active's remaining HP as a fraction of its max HP | 1 | 0.3 |
| `TurnsToKnockout` | φ3 | Turns the opponent needs to Knock Out i's Active minus turns i needs to Knock Out theirs, each counted up to 5 | 5 | 0.8 |
| `ReadyAttackers` | φ4 | Pokémon in play with an attack they can use and whose cost the attached Energy pays | 3 | 0.4 |
| `Energy` | φ5 | Energy units attached to Pokémon in play | 10 | 0.3 |
| `Bench` | φ6 | Benched Pokémon | 5 | 0.3 |
| `Hand` | φ6 | Cards in hand, counting at most 7: more cards than you can use aren't worth more | 7 | 0.2 |
| `Deck` | φ6 | Cards in the deck, counting at most 30, so every card drawn has a price long before the deck runs out | 30 | 0.5 |
| `Conditions` | φ7 | Special Conditions on the opponent's Active minus on i's | 3 | 0.2 |
| `PrizesAtRisk` | φ8 | Prize cards the opponent's Active is worth minus i's | 3 | 0.2 |

**Turns to Knock Out** (φ3) is relaxed, as in the proposal: the attacking side's strongest attack
among all its Pokémon in play, with Weakness, Resistance and other effects through
[`rules::attack_damage`](../engine/rules/rules.md), as if it could be used every turn whatever its
cost. An attack that does no damage, or no attack at all, counts as 5 turns. Damage formulas (such as
"30×") count only their printed damage.

**A player choosing a new Active** (after a Knock Out) is scored as if their Benched Pokémon with the
most HP left were Active, so the moment between a Knock Out and the promotion isn't scored as having
no Active at all.

The weights are set by hand, with Prize cards far the heaviest; M10 fits them by logistic regression
on self-play.

**Hand against deck (step 5.6b).** At first the hand counted in full (scale 10) and the deck only
below 10 cards. Drawing then always looked good, so Greedy drew its deck away: in the first full
run against Random, 3,457 of its 3,566 losses were its own deck running out. Now a card in the deck
is worth about 0.017 (θ 0.5 over 30) and a card in hand about 0.029 (θ 0.2 over 7) up to 7 cards,
so an Ability like Trade (discard 1, draw 2) is about even with a small hand and a loss with a full
one.

## What it reads

It takes a `GameState`, because the rules it needs (HP after Tools, damage after Weakness, Energy
cost matching) all take one, but **it reads only what both players can see**: the Pokémon in play,
the open zones, lasting effects, and the sizes of the hands, decks and Prize cards. So it gives the
same value for a [`World`](world.md) as for the real game it was built from, which the tests check
at every action of real-deck games. The greedy agent can evaluate the states it tries in a world
directly, without observing them first.

Whether an attack's cost is paid comes from [`can_pay_attack`](../engine/engine.md), the engine's
own cost matching.

## Tests

[`tests/test_evaluation.cpp`](../../../tests/test_evaluation.cpp), tag `[evaluation]`:

- a finished game scores 1, −1 or 0, and `utility` throws if it isn't finished;
- an even position scores about 0, with every feature but hand and deck size 0, and zero weights give 0;
- taking a Prize card, damaging the opponent's Active (with the expected HP and turns-to-Knock-Out
  values), attaching Energy (one ready attacker) each score higher, and Poison scores lower;
- a player choosing a new Active scored by their healthiest Benched Pokémon;
- cards in hand counting up to 7 and cards in the deck up to 30;
- in random games, the two players' features are exact opposites, every feature is in [−1, 1], and e
  is in (−1, 1) with e_1 = −e_0;
- on the real decks, a world built for either player gives exactly the same features and evaluation,
  for both players, as the game.
