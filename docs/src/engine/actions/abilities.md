# Abilities you choose to use

**Code:** [`src/engine/actions/main_actions.cpp`](../../../../src/engine/actions/main_actions.cpp) (`ability_ready`, `add_ability_actions`, `use_ability`) · **Namespace:** `pokebot::detail`

"Once during your turn, you may use this Ability." In the main phase the engine offers a
`UseAbility` action for every Ability that's ready, and a `UseStadium` action for a Stadium with an
action of its own (Surfing Beach).

## When an Ability is ready

An Ability is offered when all of these hold:

1. It has effects (a passive or not-yet-written Ability has none).
2. `rules::has_abilities(game, pokemon)` says the Pokémon has its Abilities. The rulebook answer is
   always yes; Ability locks will change it.
3. If it's `active_only`, the Pokémon is in the Active Spot.
4. If it's `once_per_turn` (the default), this Pokémon hasn't used this Ability this turn.
5. If it's `one_per_name`, no Pokémon of yours has used an Ability with that name this turn ("You
   can't use more than 1 Run Errand Ability each turn").
6. Its `"requires"` [conditions](../../cards/effects/program.md#conditions) hold, with the Pokémon
   as "this Pokémon" (so `self_has_energy` works).

Uses are recorded in `TurnFlags::abilities_used` as `AbilityUse{pokemon, card, index}` (the
Pokémon's id, the card the Ability is printed on, and which Ability). The list resets every turn.
Only Abilities limited to once per turn or one per name are recorded: one usable "as often as you
like" (`"once_per_turn": false`) leaves nothing to check, and recording it would fill the list.
Evolving gives the Pokémon a new card, so an evolved Pokémon's Ability is fresh.

## Using one

`UseAbility` carries the top card, the slot and the Ability's index. It records the use, then runs
the Ability's program (`ProgramKind::Ability`) with the Pokémon as "this Pokémon" and
`then = FinishAbility`. The `MainPhase` decision stays underneath, so the turn carries on.

**`FinishAbility`** resolves Knock Outs straight away: damage counters from Mortal Shuriken Knock Out
before anything else happens. A player whose Active Spot is now empty (Dudunsparce shuffled itself
away) chooses a new Active.

## Stadium actions

A Stadium whose card has `effects` offers `UseStadium` to the current player, once per turn
(`TurnFlags::stadium_used`), when its `"requires"` hold. Either player's Stadium works. It runs the
Stadium's program with `then = FinishAbility`.

## In effects files

```json
"abilities": {
  "Mortal Shuriken": {
    "active_only": true,
    "requires": [{ "hand_has": { "kind": "energy", "basic": true, "type": "water" } }],
    "effects": [
      { "op": "discard_from_hand", "filter": { "kind": "energy", "basic": true, "type": "water" } },
      { "op": "counters_to", "count": 6 }
    ]
  }
}
```

A cost ("you must discard a card from your hand in order to use this Ability") is the first effect,
plus a requirement that it can be paid.

## Triggered effects

**Code:** [`src/engine/effects/triggers.cpp`](../../../../src/engine/effects/triggers.cpp)

Some effects happen on their own when something happens. Every [event](../events/events.md) goes
through `run_triggers`:

| Event | What runs |
|---|---|
| `PlayedToBench` (raised only when a Basic is played from hand) | That Pokémon's Abilities with `"trigger": "played_to_bench"`, when `ability_allowed` says yes (Ability lock, once per turn, one per name, `"requires"`). The use is recorded like a chosen one |
| `PutOnBench` (any source, during that player's turn) | The Stadium in play, if it has `"trigger": "put_on_bench"`, with the new Pokémon as "this Pokémon" |
| `Evolved` (from the hand, or by Rare Candy) | The new card's Abilities with `"trigger": "evolved"`, as for `PlayedToBench` |
| `EnergyAttached` (from the hand) | The Energy's own effects, if it has `"trigger": "attached_from_hand"` and its `requires` hold with the Pokémon it went on as "this Pokémon" |
| `DamagedByAttack` | The damaged Pokémon's Abilities with `"trigger": "damaged_by_attack"`, if it's the Active Pokémon and the attack is the opponent's. They run straight after that damage, inside the attack, and leave Knock Outs to the attack's end. The use isn't recorded |

"You may use this Ability" is written as a `may` block in the effects. Triggers start through
[`queue_effect`](../effects/interpreter.md), so one firing in the middle of another effect waits for
the current step to finish. They end with `FinishAbility`.

`ability_allowed(game, player, pokemon, index)` holds the checks shared by chosen and triggered
Abilities. Triggered Abilities are never offered as `UseAbility`, and a triggered Stadium is never
offered as `UseStadium`.

**Known gap:** if a triggered effect Knocks Out a Pokémon in the middle of another card's effect,
the "choose a new Active" decision comes after that card finishes. Risky Ruins can't Knock Out any
Pokémon in our decks.

## Tests

[`tests/test_engine_abilities.cpp`](../../../../tests/test_engine_abilities.cpp), tag `[abilities]`, and
[`tests/test_engine_triggers.cpp`](../../../../tests/test_engine_triggers.cpp), tag `[triggers]`.
