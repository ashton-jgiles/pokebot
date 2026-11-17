# `src/engine/engine` — the public interface

**Files:** [`src/engine/engine.hpp`](../../../src/engine/engine.hpp), [`src/engine/engine.cpp`](../../../src/engine/engine.cpp), [`src/engine/internal.hpp`](../../../src/engine/internal.hpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

## Public functions (`engine.hpp`)

| Function | Behavior |
|---|---|
| `GameState new_game(const CardTable& cards, std::uint64_t seed)` | Shuffles, deals, resolves mulligans, flips the coin. Returns a game waiting for the coin-flip winner to choose turn order. See [setup](setup/setup.md) |
| `std::vector<Action> legal_actions(const GameState& game)` | Every choice for the pending decision. Empty once the game is over. Never empty otherwise |
| `void apply(GameState& game, const Action& action)` | Carries out one action, then runs the game forward to the next decision |

`apply` throws:
- `std::invalid_argument` if the action isn't in `legal_actions(game)`;
- `std::logic_error` if the game is already over.

A typical loop:

```cpp
GameState game = pokebot::new_game(table, seed);
while (!game.is_over()) {
    const auto actions = pokebot::legal_actions(game);
    pokebot::apply(game, pick_one(actions));
}
```

## Dispatch (`engine.cpp`)

Both functions look at the pending decision's kind and hand off:

| Decision | Legal actions from | Applied by |
|---|---|---|
| Setup decisions | `add_setup_actions` | `apply_setup_action` ([setup](setup/setup.md)) |
| `MainPhase` | `add_main_phase_actions` | `apply_main_phase_action` ([main actions](actions/main_actions.md)) |
| `RetreatDiscard` | `add_retreat_discard_actions` | `apply_retreat_discard` ([main actions](actions/main_actions.md)) |
| `PromoteActive` | `add_promote_actions` | `apply_promote` ([combat](actions/combat.md)) |

## Shared helpers (`internal.hpp`, namespace `pokebot::detail`)

Not part of the public interface. These are shared between the engine's `.cpp` files, and tests
use them to set up positions.

| Helper | Behavior |
|---|---|
| `is_basic_pokemon(game, card)` | A Pokémon card at the Basic stage |
| `new_pokemon(card, turn)` | A fresh `PokemonInPlay` with `turn_played = turn` |
| `pokemon_at(player, slot)` | The Pokémon in a slot (must be occupied) |
| `in_play_slots(player)` | Every occupied slot, Active first |
| `find_pokemon(game, id)` | The Pokémon in play with this [`id()`](game/game_state.md#pokemoninplay), or `nullptr` |
| `raise(game, event)` | Announces an [event](events/events.md) |
| `draw_cards(game, player, count)` | Draws up to `count`; returns how many. Running out here doesn't lose the game |
| `set_winner(game, player)` | Ends the game and clears pending decisions |
| `begin_turn`, `end_turn`, `continue_game` | Turn flow; see [turn](actions/turn.md) |
| `can_pay_cost`, `attack_damage`, `perform_attack`, `handle_knockouts`, `add_extra_attack_actions` | Combat; see [combat](actions/combat.md) |

`is_first_turn` moved to [`rules`](rules/rules.md), since cards check it too.
