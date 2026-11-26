# `src/engine/actions/lasting.cpp` — lasting effects

**File:** [`src/engine/actions/lasting.cpp`](../../../../src/engine/actions/lasting.cpp) · **Namespace:** `pokebot::detail`

Effects that outlast the attack or card that made them: "during your next turn, this Pokémon can't
attack", "during your opponent's next turn, the Defending Pokémon can't retreat", "during this turn,
your attacks do 40 more damage".

## Where they're stored

`GameState::lasting` holds up to 8 `LastingEffect`s (see [game_state](../game/game_state.md#lastingeffect)):

| Field | Meaning |
|---|---|
| `kind` | `CantAttack`, `CantUseAttack`, `CantRetreat`, `NoItems`, `DamageBonus`, `TakesLess`, `PreventDamage`, `DealsLess`, `TakesMore` or `AttackFlip` |
| `player` | The player it affects |
| `pokemon` | The Pokémon it's on (an `id()`), or `kNoCard` for the whole player |
| `attack` | For `CantUseAttack`: the attack that can't be used |
| `amount`, `only_ex` | For `DamageBonus`: how much more, and whether only against Pokémon ex. For `TakesLess`, `DealsLess` and `TakesMore`: how much. For `PreventDamage`: the most damage it prevents, or 0 for all |
| `prevent_effects`, `from_basic` | For `PreventDamage`: it also prevents the effects of attacks; it prevents only damage from Basic Pokémon |
| `turn` | The only turn it applies during |

## Timing

`until` in the effects file decides `turn`:

| `until` | Applies during |
|---|---|
| `this_turn` | This turn |
| `opponents_next_turn` | This turn + 1 |
| `your_next_turn` | This turn + 2 |

Turns alternate, so "your next turn" is two turns ahead. An effect does nothing until its turn
comes, and `begin_turn` removes it once that turn has passed (`expire_lasting`).

**Effects on a Pokémon end early** when it moves to the Bench, evolves or leaves play (Knocked Out,
or Tuck Tail): `end_effects_on` is called in the same places that clear
[Special Conditions](conditions.md).

## What reads them

Each is a [rules](../rules/rules.md) question, so the rest of the engine just asks:

| Kind | Question |
|---|---|
| `CantAttack` | `rules::can_attack` |
| `CantUseAttack` | `rules::can_use_attack(game, pokemon, attack)`, checked per attack |
| `CantRetreat` | `rules::can_retreat` |
| `NoItems` | `rules::can_play_item` |
| `DamageBonus` | The attacker-side stage of the damage pipeline, when the defender is Active; on a player, or on one Pokémon ("attacks used by this Pokémon") |
| `DealsLess` | The attacker-side stage, for attacks by that Pokémon |
| `TakesLess`, `TakesMore` | The defender-side stage, for damage to that Pokémon |
| `PreventDamage` | The defender-side stage (damage from the opponent's attacks becomes 0), and `rules::effects_prevented` with `prevent_effects` |
| `AttackFlip` | `rules::attack_needs_flip`: before that Pokémon's attack, a coin; on tails the attack doesn't happen |

A lasting effect put on the Defending Pokémon is itself an effect of an attack, so it isn't added when that Pokémon's
effects are prevented.

## Functions

| Function | Behavior |
|---|---|
| `add_lasting(game, run, step)` | Makes the effect from the step (the `AddLasting` op) |
| `end_effects_on(game, pokemon)` | Removes the effects on that Pokémon |
| `expire_lasting(game)` | Removes effects whose turn has passed |

## In effects files

```json
{ "op": "lasting", "effect": "cant_retreat", "on": "defending", "until": "opponents_next_turn" }
```

See [the effect language](../../cards/effects/program.md).

## Tests

[`tests/test_engine_lasting.cpp`](../../../../tests/test_engine_lasting.cpp), tag `[lasting]`.
