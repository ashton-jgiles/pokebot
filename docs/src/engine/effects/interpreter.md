# `src/engine/effects/interpreter.cpp` — running effects

**File:** [`src/engine/effects/interpreter.cpp`](../../../../src/engine/effects/interpreter.cpp) · **Namespace:** `pokebot::detail`

Runs a compiled [effect program](../../cards/effects/program.md) step by step. When a step needs a
player's choice, it pushes a decision and stops; the answer resumes it.

## Where an effect is up to: `EffectRun`

`GameState::effects` is a stack of up to 4 `EffectRun`s (see [game_state](../game/game_state.md#effectrun)):

| Field | Meaning |
|---|---|
| `who` | The `EffectContext`: whose effect it is and which Pokémon is "this Pokémon" |
| `program` | A `ProgramRef` to the compiled program: card, kind (attack for now), index |
| `then` | What to do when it finishes (`FinishAttack` for an attack) |
| `pc` | The next step |
| `chosen`, `chosen_pokemon` | Choices made so far |

Because this lives in `GameState`, **a game copied in the middle of an effect carries on exactly
like the original.** Search can explore a half-finished Ultra Ball like any other position.

## Functions

| Function | Behavior |
|---|---|
| `start_effect(game, who, program, then)` | Pushes a run and runs it until it finishes or needs a choice |
| `is_effect_decision(kind)` | `ChooseCards`, `ChoosePokemon` or `ChooseYesNo` |
| `add_effect_choice_actions(game, out)` | The legal answers to the current effect decision |
| `apply_effect_choice(game, action)` | Records the answer and carries on |

When a run's last step is done, it's removed and its `then` runs.

## Decisions

| Decision | Actions | Notes |
|---|---|---|
| `ChooseCards` | `ChooseCard` (one per different matching card), `ChooseDone` | `decision.amount` is the most still allowed and `decision.target` the fewest. Both are capped by how many cards match; with none, the step is skipped. Reaching the most ends the choice on its own; `ChooseDone` is offered once the fewest are chosen |
| `ChoosePokemon` | `ChoosePokemon` with a Bench slot | Skipped if the Bench is empty |
| `ChooseYesNo` | `AnswerYes`, `AnswerNo` | No jumps past the `may` block |

The player answering is `who.player`. Choosing from the deck shows the chooser their whole deck;
hiding what a player shouldn't see is the observation layer's job (Milestone 4).

## Attacks are effects

Every attack runs through the interpreter. [`apply_attack`](../actions/combat.md) starts the
attack's program with `then = FinishAttack`, and its `Damage` step deals the damage. Knock Outs,
the extra-attack check and the end of the turn happen in `finish_attack`, **after** the whole
effect. Until then a Pokémon can sit at 0 HP while its owner's opponent is still choosing cards; the
[invariant checker](../invariants/invariants.md) allows that while an effect is running.

## Not yet

Starting an effect from inside another (a copy attack's chosen attack) needs the outer run to
step past itself first. That arrives with copy attacks in step 3.5.

## Tests

[`tests/test_engine_effects.cpp`](../../../../tests/test_engine_effects.cpp), tag `[effects]`:
drawing, searching card by card, filters, choosing nothing, yes and no, a forced Energy-to-hand
after the damage, switching, discarding the hand and drawing 6, a Knock Out waiting for the effect,
and a game copied mid-effect finishing the same way.
