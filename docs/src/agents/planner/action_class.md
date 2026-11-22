# `src/agents/planner/action_class` — Det, Fin and the rest of Bnd

**Files:** [`src/agents/planner/action_class.hpp`](../../../../src/agents/planner/action_class.hpp), [`src/agents/planner/action_class.cpp`](../../../../src/agents/planner/action_class.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The turn planner (Model 1, M6) searches the part of a turn whose outcome is certain. It needs to know,
for each action, whether it can search on past it. The proposal's classes:

| Class | Proposal | Meaning | Examples |
|---|---|---|---|
| `Deterministic` | Det | Uses no chance, reveals nothing, and the player is still in their main phase | Attach Energy, evolve, bench a Basic, retreat, Boss's Orders, Subjugating Chains |
| `Final` | Fin | Ends the plan, but uses no chance and reveals nothing | An attack without coin flips (a Knock Out too), ending the turn |
| `Uncertain` | the rest of Bnd | Chance or hidden cards decide what happens | Drawing, searching the deck, coin flips, shuffles, ending the turn into a coin-flip Checkup |

A plan is Det actions followed by one `Final` or `Uncertain` action. The planner searches on past Det
actions only.

```cpp
const TriedAction tried = try_action(game, player, action);
tried.kind;  // its class
tried.after; // the game after it, so the planner needn't apply it again
```

## The rule (decided 2026-11-19)

`try_action` applies the action once to a copy and looks at three things:

1. **Chance:** whether the game's random number generator moved. Coin flips, shuffles and random
   discards all draw from it.
2. **Revealing:** whether the player now knows the place of, or has seen, a card they didn't before
   ([knowledge](../../engine/knowledge/knowledge.md)). A draw puts an unknown card in the hand; a deck
   search shows the player their whole deck; looking at the top of the deck shows those cards.
   Attaching, benching or evolving move cards the player already knew.
3. **Still in the main phase:** the game isn't over, it's the same turn and phase, and the decision
   is still the player's. A choice the action leads to (which Energy to discard for a retreat, which
   Pokémon Boss's Orders brings up) counts, so it's searched through too.

All three hold: `Deterministic`. No chance and nothing revealed, but the main phase is over (an
attack, ending the turn, a decision handed to the opponent): `Final`. Otherwise `Uncertain`.

**Prize cards taken for a Knock Out** are an exception for `Final`: the card is revealed, but taking
it is certain and which card it is changes neither the goal (a Knock Out, a win, a number of Prize
cards) nor the plan's value, which counts only how many cards are in each hand. A Knock Out that
leaves the player in their main phase (an Ability's damage counters) is `Final` too, not
`Deterministic`: the planner stops there, because later actions could use the new card.

The engine takes Prize cards from the end of the pile, not at random (see
[combat](../../engine/actions/combat.md)), so a Knock Out uses no chance.

It looks only at the deciding player's knowledge, so it works the same on a [`World`](../world.md) as on
the real game.

**A known gap:** an effect whose result depends on a hidden card without revealing it ("if your
opponent has a Supporter in their hand") would be called `Deterministic`. No card in our four decks
has one.

## Why not try the action in two worlds

The proposal defines Det by "uses no chance and reveals no hidden information". Applying the action in
two worlds with different hidden cards and comparing what the player sees tests that directly, but
costs two applies, and is fooled when both worlds happen to hide the same card in the same place.
The rule above needs one apply and can't be fooled that way. The tests still do the two-world
comparison, as a check on the rule.

## Tests

[`tests/test_action_class.cpp`](../../../../tests/test_action_class.cpp), tag `[planner]`:

- attaching Energy, benching, evolving and retreating are deterministic;
- ending the turn and attacking are final, and so is a Knock Out, which takes a Prize card without
  using chance;
- a draw, a deck search, a coin-flip attack and ending the turn while Asleep are uncertain;
- on the real decks, at many decisions of random games: each action gets the same class in two
  worlds, a deterministic action leaves the player seeing exactly the same in both, and a final one
  gives the same result and evaluation in both.
