# `src/engine/actions/conditions.cpp` — Special Conditions

**File:** [`src/engine/actions/conditions.cpp`](../../../../src/engine/actions/conditions.cpp) · **Namespace:** `pokebot::detail`

The five Special Conditions (rulebook pp. 15–16) and what Pokémon Checkup does with them.

## The rules

| Condition | Effect | At Pokémon Checkup |
|---|---|---|
| **Asleep** | Can't attack or retreat | Flip a coin: heads, it recovers |
| **Paralyzed** | Can't attack or retreat | Recovers at the Checkup after its owner's next turn |
| **Confused** | Can retreat. When it attacks, flip a coin: on tails the attack does nothing and it gets 3 damage counters | — |
| **Burned** | — | 2 damage counters, then flip a coin: heads, it recovers |
| **Poisoned** | — | 1 damage counter, or as many as the card that Poisoned it says |

- **Asleep, Confused and Paralyzed replace each other.** A new one removes the old one. Burned and
  Poisoned are separate and can be added on top.
- **Only the Active Pokémon** can have a Special Condition (asked through
  `rules::can_get_special_condition`).
- **Moving to the Bench or evolving removes them all.** That covers retreating, Switch, and Boss's
  Orders on the Pokémon that leaves the Active Spot.

## Where they're stored

`PokemonInPlay::special` is a `SpecialConditions` (see [game_state](../game/game_state.md#pokemoninplay)):
`asleep`, `confused`, `paralyzed`, `burned`, `poison` (damage counters per Checkup, 0 when not
Poisoned) and `paralyzed_turn`, plus `any()`.

**Paralysis timing.** The rulebook removes Paralyzed if the Pokémon "has been Paralyzed since the
beginning of your last turn". The engine records the turn it was Paralyzed and removes it at a
Checkup that ends **its owner's** turn, on a later turn. Paralyzed by the opponent, it misses one
turn; paralyzed by its own attack, it misses its owner's next turn.

## Functions

| Function | Behavior |
|---|---|
| `apply_special_condition(game, pokemon, condition, poison_counters)` | Gives the condition, if `rules::can_get_special_condition` allows it |
| `clear_special_conditions(pokemon)` | Removes them all |
| `special_conditions_checkup(game)` | Both Active Pokémon, the current player's first, in rulebook order: Poisoned, Burned, Asleep, Paralyzed |
| `confusion_stops_attack(game, attacker)` | For a Confused attacker, flips; on tails puts 3 damage counters on it and returns `true` |

[Pokémon Checkup](turn.md#pokémon-checkup) calls `special_conditions_checkup`, then Knock Outs are
resolved as usual. On tails for confusion, [`apply_attack`](combat.md) skips the attack's effect
and goes straight to `finish_attack`. The attack still counts as used, and the turn ends.

Checkup damage counters aren't damage from an attack, so they don't raise `DamagedByAttack`.

## In effects files

```json
{ "op": "special_condition", "condition": "asleep" }
```

See [the effect language](../../cards/effects/program.md). It targets the opponent's Active unless
`"player": "you"` is given; `"count"` sets the Poison counters.

## Tests

[`tests/test_engine_conditions.cpp`](../../../../tests/test_engine_conditions.cpp), tag `[conditions]`.
The coin-flip tests run several seeds and require both outcomes to appear.
