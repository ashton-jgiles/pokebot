# `src/engine/actions/turn.cpp` — turn flow

**File:** [`src/engine/actions/turn.cpp`](../../../../src/engine/actions/turn.cpp) · **Namespace:** `pokebot::detail`

Starting turns, ending them, and everything that happens between turns.

## `begin_turn(game, player)`

1. `player` becomes the current player, `turn` goes up by 1, the once-per-turn flags reset, the
   [history](../game/game_state.md#gamehistory) moves along one turn, and the phase becomes `Main`.
2. **If the deck is empty, `player` loses** (rulebook p. 8). Drawing because a *card* says so never
   loses; only this start-of-turn draw does.
3. Draw 1 card and push the `MainPhase` decision.

The first player draws on their first turn too, as in the current rules.

## `end_turn(game)`

The `EndTurn` action. It removes the `MainPhase` decision, sets the phase to `AfterMain`, and
calls `continue_game`. Attacking ends the turn the same way (see [combat](combat.md)).

## `continue_game`

Runs the game forward while **no decision is pending**:

| Phase | What happens |
|---|---|
| `AfterMain` | Raise `TurnEnding`, move to `Checkup`, run Pokémon Checkup, then resolve any Knock Outs |
| `Checkup` | Start the opponent's turn |

If resolving Knock Outs pushes a "choose a new Active" decision, the loop stops. Once that's
answered, `apply_promote` calls `continue_game` again and the turn carries on. The same loop will
cover Knock Outs from Poison and Burn in Milestone 3.

## Pokémon Checkup

Runs between every turn (rulebook pp. 15–16). It raises the `Checkup` [event](../events/events.md) and is otherwise **empty for now**: Special Conditions arrive in
Milestone 3. `handle_knockouts` runs right after it either way.

## Tests

[`tests/test_engine_turns.cpp`](../../../../tests/test_engine_turns.cpp), tag `[turns]`: ending the
turn is always offered, the turn passes with a draw, turns alternate, flags reset, and deck-out
loses.
