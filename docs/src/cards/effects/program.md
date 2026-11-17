# `src/cards/effects/program` — the effect language

**Files:** [`program.hpp`](../../../../src/cards/effects/program.hpp), [`program.cpp`](../../../../src/cards/effects/program.cpp) · **Library:** `pokebot_lib` · **Namespace:** `pokebot`

Card effects are written as data in [effects files](../../../data/cards/README.md#effectsidjson--hand-written-effects)
and **compiled** into a flat list of simple steps, a `Program`. The engine's
[interpreter](../../engine/effects/interpreter.md) runs the steps. Compiling happens once, when a
card is added to the `CardDB`, so a mistake in an effects file stops the load with the card and
attack named, instead of turning up in the middle of a game.

## Building blocks (what effects files say)

Each block is a JSON object with an `"op"`:

| `op` | Fields | Means | Compiles to |
|---|---|---|---|
| `damage` | — | The attack's damage, at this point in the list (attacks only) | `Damage` |
| `draw` | `count` (1), `player` | Draw cards | `Draw` |
| `discard_hand` | — | Discard your hand | `DiscardHand` |
| `shuffle_deck` | — | Shuffle your deck | `ShuffleDeck` |
| `shuffle_hand_into_deck` | `player` | Shuffle your hand into your deck | `ShuffleHandIntoDeck` |
| `search_deck` | `count` (1), `filter`, `to` (`"hand"`), `shuffle` (`true`) | Search your deck for **up to** `count` matching cards, then shuffle (unless `"shuffle": false`, for a card that searches several times and shuffles once) | `ChooseCards`, `MoveChosen`, `ShuffleDeck` |
| `discard_from_hand` | `count` (1), `filter` | Discard exactly `count` cards from your hand | `ChooseCards`, `MoveChosen` |
| `from_discard` | `count` (1), `filter`, `to` (`"hand"`), `up_to` (`false`) | Take exactly `count` matching cards from your discard pile, or up to `count` with `"up_to": true` | `ChooseCards`, `MoveChosen` |
| `move_attached_energy` | `count` (1), `filter`, `to` (`"hand"`) | Move exactly `count` Energy attached to this Pokémon (fewer if it has fewer) | `ChooseCards`, `MoveChosen` |
| `switch_self` | — | Switch this Pokémon with 1 of your Benched Pokémon | `ChooseBenched`, `SwitchWithChosen` |
| `switch_active` | `player` (`"you"` or `"opponent"`) | Switch that player's Active with 1 of their Benched Pokémon, chosen by you (Switch, Boss's Orders) | `ChooseBenched`, `SwitchWithChosen` |
| `may` | `effects` (a list) | "You may …": ask first, and skip the list on no | `AskYesNo`, then the list |
| `if` | `condition`, `then`, `else` (optional) | Do `then` if the [condition](#conditions) holds when the step runs, otherwise `else` | `JumpUnless`, `then`, `Jump`, `else` |

- `to` can be `"hand"`, `"discard"`, `"deck"` or `"bench"`. Cards put on the Bench become new
  Pokémon in play, and no more are chosen than the Bench has room for.
- `player` can be `"you"` (the default), `"opponent"` or `"each"` (you first, then your opponent).
- `count` must be 0 to 60.
- Fewer cards are chosen when fewer match; with none, the choice is skipped.

**Damage comes first** unless the list places a `damage` block itself, since most attack text
describes what happens after the damage. A list that needs a different order (discard Energy, then
do damage) puts `damage` where it belongs.

### Filters

| Field | Matches |
|---|---|
| `kind` | `"pokemon"`, `"trainer"` or `"energy"` |
| `trainer` | A Trainer of that kind: `"item"`, `"supporter"`, `"stadium"`, `"tool"` |
| `type` | A Pokémon of that type, or an Energy that provides it (`"darkness"`, …) |
| `stage` | `"basic"`, `"stage1"`, `"stage2"`, or `"evolution"` (Stage 1 or 2) |
| `basic` | `true`: a Basic Pokémon or a Basic Energy |
| `rule_box` | `true` / `false`: has a Rule Box or not (in Standard, that means Pokémon ex) |
| `max_hp` | A Pokémon with at most this much HP |
| `name` | That exact card name |
| `any_of` | A list of filters; at least one must match ("a Pokémon or a Basic Energy card") |

Every field given must match; `{}` (or no filter) matches any card.

## Conditions

Used by `if` and by a card's `"requires"` list (see [effects files](../../../data/cards/README.md#effectsidjson--hand-written-effects)).
Each is an object with one key, from the point of view of the player the card belongs to:

| Condition | True when |
|---|---|
| `{"hand_besides": N}` | At least N cards in hand besides the card being played |
| `{"prizes_exactly": N}` | Exactly N Prize cards left |
| `{"knocked_out_last_turn": true}` | Any of your Pokémon were Knocked Out during your opponent's last turn |
| `{"has_benched": true}` | You have a Benched Pokémon |
| `{"opponent_has_benched": true}` | Your opponent has a Benched Pokémon |
| `{"bench_not_full": true}` | Your Bench has room |
| `{"discard_has": filter}` | Your discard pile has a card matching the filter |

Anything the compiler doesn't recognise (an unknown op, filter field or destination, a bad count,
a value of the wrong JSON type) throws `CardDataError`.

## Steps (what the engine runs)

| `Op` | Uses | Does |
|---|---|---|
| `Damage` | — | The attack's damage to the opponent's Active Pokémon |
| `Draw` | `max` | Draws `max` cards |
| `DiscardHand`, `ShuffleDeck` | — | As named |
| `ShuffleHandIntoDeck` | `side` | As named, for each player on that side |
| `ChooseCards` | `from`, `to`, `filter`, `min`, `max` | The player chooses `min` to `max` matching cards |
| `MoveChosen` | `from`, `to` | Moves the chosen cards |
| `ChooseBenched` | `side` | The player chooses 1 of that side's Benched Pokémon |
| `SwitchWithChosen` | `side` | That side's Active switches with the chosen one |
| `AskYesNo` | `jump` | On no, carries on at step `jump` |
| `JumpUnless` | `condition`, `jump` | If the condition is false, carries on at step `jump` |
| `Jump` | `jump` | Carries on at step `jump` |

`Draw` uses `side` too. `Side` is `You`, `Opponent` or `Each`.

A `Step` holds all of those fields; each op uses the ones it needs. A program can have at most 255
steps.

## Functions

| Function | Behavior |
|---|---|
| `Program compile_attack(const json& effects)` | Compiles an attack's effects list, with the damage first unless placed |
| `Program compile_trainer(const json& effects)` | Compiles an Item's or Supporter's effects; `damage` isn't allowed |
| `std::vector<Condition> compile_conditions(const json& requirements)` | Compiles a `"requires"` list |
| `bool matches(const CardFilter&, const CardDef&)` | Whether a card passes a filter |
| `const char* to_string(Op)` | A step's name, for printing |

The enums cards use (`CardKind`, `EnergyType`, …) live in
[`card_types.hpp`](../card/card.md#enums), so this header and `card.hpp` don't include each other.

## Tests

[`tests/test_effect_program.cpp`](../../../../tests/test_effect_program.cpp), tag `[effects]`: an
attack with no effects, how `search_deck`, `may` and `if` expand, a placed `damage` step, Trainer
programs, conditions, every kind of mistake rejected, every filter field, and `CardDB::add` naming
the attack in its error.

**Naming note:** `requires` is a C++20 keyword, so the C++ name for a card's `"requires"` list is
`requirements`.
