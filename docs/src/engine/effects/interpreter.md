# `src/engine/effects/interpreter.cpp` — running effects

**File:** [`src/engine/effects/interpreter.cpp`](../../../../src/engine/effects/interpreter.cpp) · **Namespace:** `pokebot::detail`

Runs a compiled [effect program](../../cards/effects/program.md) step by step. When a step needs a
player's choice, it pushes a decision and stops; the answer resumes it.

## Where an effect is up to: `EffectRun`

`GameState::effects` is a stack of up to 4 `EffectRun`s (see [game_state](../game/game_state.md#effectrun)):

| Field | Meaning |
|---|---|
| `who` | The `EffectContext`: whose effect it is and which Pokémon is "this Pokémon" |
| `program` | A `ProgramRef` to the compiled program: card, kind (`Attack`, `Trainer` or `Ability`), index |
| `then` | What to do when it finishes: `FinishAttack` for an attack, `DiscardTrainer` for an Item or Supporter, `FinishAbility` for an [Ability](../actions/abilities.md) or Stadium action (resolve Knock Outs at once) |
| `pc` | The next step |
| `chosen`, `chosen_pokemon` | Choices made so far |
| `damage` | The attack's damage so far: the printed damage, changed by `set_damage` / `add_damage` |
| `targets` | Ids of the Pokémon chosen by the last `ChooseTargets` |
| `target_index` | Which target an `EachTarget` … `NextTarget` loop is on |
| `attached_to_active` | Whether this effect attached Energy to its player's Active Pokémon |
| `chosen_attack` | The attack a copy attack chose |
| `value` | The latest result: heads flipped, or cards moved by the last `MoveChosen` or `HandToDeckBottom`. Counts (`"heads"`, `"moved"`) and conditions (`heads_at_least`, `moved_at_least`) read it |

Because this lives in `GameState`, **a game copied in the middle of an effect carries on exactly
like the original.** Search can explore a half-finished Ultra Ball like any other position.

## Functions

| Function | Behavior |
|---|---|
| `start_effect(game, who, program, then)` | Pushes a run and runs it until it finishes or needs a choice |
| `queue_effect(game, who, program, then)` | The same when nothing is running. When an effect is running (a trigger in the middle of Poffin), it only pushes the run, and the running loop starts it right after the current step, so nothing runs twice |
| `is_effect_decision(kind)` | `ChooseCards`, `ChoosePokemon` or `ChooseYesNo` |
| `add_effect_choice_actions(game, out)` | The legal answers to the current effect decision |
| `apply_effect_choice(game, action)` | Records the answer and carries on |
| `condition_holds(game, player, condition, playing)` | Whether a [condition](../../cards/effects/program.md#conditions) holds for `player`. `playing` is the card being played, which `hand_besides` doesn't count |
| `requirements_met(game, player, card)` | Whether every condition in a card's `"requires"` holds |

When a run's last step is done, it's removed and its `then` runs.

## Decisions

| Decision | Actions | Notes |
|---|---|---|
| `ChooseCards` | `ChooseCard` (one per different matching card), `ChooseDone` | `decision.amount` is the most still allowed and `decision.target` the fewest. Both are capped by how many cards match; with none, the step is skipped. Reaching the most ends the choice on its own; `ChooseDone` is offered once the fewest are chosen |
| `ChoosePokemon` | `ChoosePokemon` with a slot (0 is the Active) | Depends on the step. **Switching**: a Benched Pokémon of the step's side. **Targets**: up to `decision.amount` different Pokémon, `ChooseDone` once `decision.target` are chosen. **Damage counters**: one per counter, each in a slot no lower than the last, so each spread of counters is offered once (6 counters on 5 Pokémon: 210 choices, not 15,625). Skipped when there's nothing to choose |
| `ChooseAttack` | `ChooseAttack` with the card, Bench slot and attack index | Each attack of each matching Benched Pokémon, identical ones merged; copy attacks left out |
| `ChooseYesNo` | `AnswerYes`, `AnswerNo` | No jumps past the `may` block |

The player answering is `who.player`. Choosing from the deck shows the chooser their whole deck;
hiding what a player shouldn't see is the observation layer's job (Milestone 4).

## Trainers are effects

An Item or Supporter is moved from the hand to `PlayerState::resolving` and its program starts with
`then = DiscardTrainer`. When the program finishes, the card goes to the discard pile. Until then it
isn't in the hand (Ultra Ball can't discard itself) or the discard pile (Night Stretcher can't take
it back). The `MainPhase` decision stays underneath, so the turn carries on afterwards. See
[main actions](../actions/main_actions.md#trainer-cards).

## Attacks are effects

Every attack runs through the interpreter. [`apply_attack`](../actions/combat.md) starts the
attack's program with `then = FinishAttack`, and its `Damage` step deals the damage. Knock Outs,
the extra-attack check and the end of the turn happen in `finish_attack`, **after** the whole
effect. Until then a Pokémon can sit at 0 HP while its owner's opponent is still choosing cards; the
[invariant checker](../invariants/invariants.md) allows that while an effect is running.

## Copy attacks

`UseChosenAttack` moves the outer run past itself, then pushes the copied attack's program as a new
run with the **same `who`**. The loop runs the copied attack next. When it finishes, the outer
attack carries on. The copied text sees the copier as "this Pokémon", so its type, Energy and
Weakness count, and the copied attack's own cost is never paid. The effect stack holds 4 runs, and
copy attacks can't copy other copy attacks.

## Tests

[`tests/test_engine_effects.cpp`](../../../../tests/test_engine_effects.cpp), tag `[effects]`:
drawing, searching card by card, filters, choosing nothing, yes and no, a forced Energy-to-hand
after the damage, switching, discarding the hand and drawing 6, a Knock Out waiting for the effect,
and a game copied mid-effect finishing the same way.
