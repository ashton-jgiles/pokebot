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

## Tests

[`tests/test_engine_abilities.cpp`](../../../../tests/test_engine_abilities.cpp), tag `[abilities]`.
