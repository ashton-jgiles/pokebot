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
| `move_attached_energy` | `count` (1), `filter`, `to` (`"hand"`), `from` (`"self"`), `up_to` (`false`) | Move exactly `count` Energy attached to this Pokémon (fewer if it has fewer), or up to `count` with `"up_to": true`. `"from": "your_pokemon"` takes it from any of your Pokémon | `ChooseCards`, `MoveChosen` |
| `set_damage` | `amount`, `per`, `for_each` | The attack's damage becomes `amount` + `per` × the [count](#counting). For "×" attacks | `SetDamage` |
| `add_damage` | `amount`, `per`, `for_each` | The attack's damage goes up by `amount` + `per` × the count. For "+" attacks and "this attack does N more damage" | `AddDamage` |
| `flip_coins` | `count` (1) | Flip coins; the number of heads can be counted (`"heads"`) or tested (`heads_at_least`) | `FlipCoins` |
| `flip_until_tails` | — | Flip until tails; the number of heads is kept the same way | `FlipUntilTails` |
| `hand_to_deck_bottom` | `player` | Shuffle that player's hand and put it on the bottom of their deck (Special Red Card) | `HandToDeckBottom` |
| `damage_to` | `targets` (1), `on` (`"any"`), `player` (`"opponent"`), `up_to`, `amount`, `per`, `for_each` | Choose that many different Pokémon (`"on": "bench"` for the Bench only) and do `amount` + `per` × the count to each. Weakness and Resistance apply only to an Active target; a Benched Tera Pokémon takes none. Attacks only | `ChooseTargets`, `DamageTargets` |
| `place_counters` | `count` (1), `on` (`"any"`), `player` (`"opponent"`) | Put damage counters on that player's Pokémon "in any way you like", one choice per counter. Counters aren't damage, so Tera doesn't stop them | `PlaceCounters` |
| `discard_all_attached_energy` | — | Discard every Energy attached to this Pokémon | `DiscardAllEnergy` |
| `return_self_to_hand` | — | Put this Pokémon and everything attached to it into your hand (Tuck Tail). If it was Active, you choose a new one after the attack | `ReturnSelfToHand` |
| `discard_target_energy` | `count` (1), `on`, `player` (`"opponent"`), `filter` | Choose 1 Pokémon that has Energy, then discard `count` of its Energy (Crushing Hammer) | `ChooseTargets`, `ChooseCards`, `MoveChosen` |
| `use_attack` | `filter` | Choose an attack of one of your matching Benched Pokémon and use it as this attack (Night Joker). Copy attacks can't be copied. Attacks only | `ChooseAttack`, `UseChosenAttack` |
| `search_to_top` | `count` (1), `filter` | Search your deck for up to `count` cards, shuffle, then put them on top; the last one chosen is the top card (Ciphermaniac's Codebreaking) | `ChooseCards`, `ShuffleDeck`, `MoveChosen` |
| `special_condition` | `condition`, `player` (`"opponent"`), `count` (1) | Give that player's Active Pokémon a [Special Condition](../../engine/actions/conditions.md): `"asleep"`, `"burned"`, `"confused"`, `"paralyzed"` or `"poisoned"`. **Defaults to the opponent**, since cards almost always say "your opponent's Active Pokémon is now …". `count` is the Poison damage counters per Checkup | `ApplySpecialCondition` |
| `switch_self` | — | Switch this Pokémon with 1 of your Benched Pokémon | `ChooseBenched`, `SwitchWithChosen` |
| `switch_active` | `player` (`"you"` or `"opponent"`), `filter` | Switch that player's Active with 1 of their Benched Pokémon that matches `filter`, chosen by you (Switch, Boss's Orders, Subjugating Chains) | `ChooseBenched`, `SwitchWithChosen` |
| `counters_to` | `count` (1), `targets` (1), `on`, `player` (`"opponent"`) | Put `count` damage counters on each chosen Pokémon (Mortal Shuriken) | `ChooseTargets`, `CountersOnTargets` |
| `move_counters` | `count` (1), `on`, `player` (`"opponent"`) | Choose one of your damaged Pokémon, then one of that player's, and move up to `count` damage counters (as many as it has, up to `count`) | `ChooseCounterSource`, `ChooseTargets`, `MoveCounters` |
| `look_at_top` | `count` (1), `take` (1), `to` (`"hand"`), `filter` | Look at the top `count` cards, take `take` of them, and put the rest on the bottom in the same order (Recon Directive) | `ChooseCards`, `MoveChosen`, `RestToBottom` |
| `attach_from_hand` | `count` (1), `filter` | Attach that many matching cards from your hand to this Pokémon (Teal Dance) | `ChooseCards`, `MoveChosen` |
| `shuffle_self_into_deck` | — | Shuffle this Pokémon and everything attached into your deck (Run Away Draw) | `ShuffleSelfIntoDeck` |
| `may` | `effects` (a list) | "You may …": ask first, and skip the list on no | `AskYesNo`, then the list |
| `ignore_defender_effects` | — | "This attack's damage isn't affected by any effects on your opponent's Active Pokémon" (Shred): the printed Weakness and Resistance are used and defender-side effects skipped. Must come before a placed `damage` step. Attacks only | `IgnoreDefenderEffects` |
| `lasting` | `effect`, `until`, `on` (`"self"`), `player`, `amount`, `only_ex` | A [lasting effect](../../engine/actions/lasting.md). `effect`: `"cant_attack"`, `"cant_use_attack"` (this attack), `"cant_retreat"`, `"no_items"`, `"damage_bonus"`. `until`: `"this_turn"`, `"opponents_next_turn"`, `"your_next_turn"`. `on`: `"self"` or `"defending"` for the first three (attacks only), `"player"` (with `player`) for the last two | `AddLasting` |
| `if` | `condition`, `then`, `else` (optional) | Do `then` if the [condition](#conditions) holds when the step runs, otherwise `else` | `JumpUnless`, `then`, `Jump`, `else` |

- Cards moved to a hand, deck or discard pile always go to **their owner's**, even when the effect is
  the opponent's (Crushing Hammer's Energy goes to its owner's discard pile).
- `to` can be `"hand"`, `"discard"`, `"deck"` or `"bench"`. Cards put on the Bench become new
  Pokémon in play, and no more are chosen than the Bench has room for.
- `player` can be `"you"` (the default), `"opponent"` or `"each"` (you first, then your opponent).
- `count` must be 0 to 60.
- Fewer cards are chosen when fewer match; with none, the choice is skipped.

**Damage comes first** unless the list places a `damage` block itself, since most attack text
describes what happens after the damage. A list that needs a different order (discard Energy, then
do damage) puts `damage` where it belongs. **An attack that uses `set_damage` or `add_damage` must
place `damage` itself**: the compiler refuses to guess whether a bonus comes before or after it.

The attack's damage is a working number on the running effect. It starts at the printed damage, and
`set_damage` / `add_damage` change it before the `damage` step uses it. Weakness and Resistance are
applied afterwards, in the [damage pipeline](../../engine/rules/rules.md#the-damage-pipeline).

### Counting

`for_each` counts something. Its `"of"` field says what, and `player` (`"you"`, `"opponent"`,
`"each"`) says whose:

| `"of"` | Counts |
|---|---|
| `"hand"` | Cards in hand |
| `"discard"` | Cards in the discard pile matching `filter` |
| `"benched"` | Benched Pokémon |
| `"prizes_taken"` | Prize cards taken (6 minus those left) |
| `"active_energy"` | Energy cards attached to the Active Pokémon |
| `"damage_counters"` | Damage counters on this Pokémon |
| `"target_damage_counters"` | Damage counters on the target being damaged (only in `damage_to`) |
| `"heads"`, `"moved"` | The heads from the last flip, or the cards moved by the last choice (both read the same working number) |

```json
{ "op": "set_damage", "per": 50, "for_each": { "of": "hand", "player": "opponent" } }
```

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
| `name_prefix` | A name starting with this ("N's") |
| `not_name` | Any name except this one ("except any Pecharunt ex") |
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
| `{"knocked_out_by_attack_last_turn": true}` | The same, but only Knock Outs by an attack (not at Pokémon Checkup) |
| `{"has_benched": true}` | You have a Benched Pokémon |
| `{"opponent_has_benched": true}` | Your opponent has a Benched Pokémon |
| `{"bench_not_full": true}` | Your Bench has room |
| `{"discard_has": filter}` | Your discard pile has a card matching the filter |
| `{"heads_at_least": N}`, `{"moved_at_least": N}`, `{"drew_at_least": N}` | The last flip gave at least N heads, the last choice moved at least N cards ("if you do"), or the last draw drew at least N |
| `{"opponent_prizes_at_most": N}` | Your opponent has at most N Prize cards left |
| `{"shares_type": true}` | One of your Pokémon in play shares a type with one of your opponent's |
| `{"self_energy_at_least": N}` | This Pokémon has at least N Energy attached (only inside an effect) |
| `{"hand_has": filter}` | A card in your hand (other than the one being played) matches |
| `{"bench_has": filter}` | One of your Benched Pokémon matches |
| `{"active_is": filter}` | Your Active Pokémon matches |
| `{"self_has_energy": filter}` | An Energy attached to this Pokémon matches |
| `{"has_damaged": true}` | One of your Pokémon in play has damage counters |

Anything the compiler doesn't recognise (an unknown op, filter field or destination, a bad count,
a value of the wrong JSON type) throws `CardDataError`.

## Always-on effects (`"passive"`)

A Tool, a Stadium or a passive Ability has a `"passive"` list instead of `effects`. Each entry is
compiled into a `Modifier`, which changes a [rule question](../../engine/rules/rules.md#always-on-effects)
for as long as the card is in play:

```json
{ "modify": "retreat_cost", "set": 0, "applies_to": "all", "filter": { "name_prefix": "N's" } }
```

| Field | Meaning |
|---|---|
| `modify` | `"max_hp"`, `"retreat_cost"`, `"weakness_type"` (needs `type`) or `"damage_dealt"` (attacks by the Pokémon do `add` more to the opponent's Active, before Weakness) |
| `add` / `set` | A change, or a new value. A `set` wins over `add`s ("has no Retreat Cost") |
| `type` | For `weakness_type`: the new Weakness type. A Pokémon with no printed Weakness gets one, ×2 |
| `applies_to` | `"holder"` (the default: the Pokémon the Tool is attached to, or the Pokémon with the Ability), `"yours"`, `"opponents"` or `"all"` (everyone's Pokémon in play) |
| `filter` | Which Pokémon it affects, matched against the Pokémon's top card |
| `if_poisoned` | Only while that Pokémon is Poisoned |

`compile_passive(json)` compiles a list; mistakes throw `CardDataError`.

## Steps (what the engine runs)

| `Op` | Uses | Does |
|---|---|---|
| `Damage` | — | The attack's working damage to the opponent's Active Pokémon |
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
| `ApplySpecialCondition` | `special`, `side`, `max` | Gives that side's Active the Special Condition (`max` = Poison counters) |
| `SetDamage`, `AddDamage` | `amount`, `per`, `count` | Set or raise the attack's working damage |
| `FlipCoins` | `max` | Flips `max` coins; the heads become the working value |
| `FlipUntilTails` | — | Flips until tails; the heads become the working value |
| `HandToDeckBottom` | `side` | As named; the number of cards moved becomes the working value |
| `ChooseTargets` | `side`, `from` (`Bench` or `InPlay`), `min`, `max`, `needs_energy` | The player chooses different Pokémon on that side |
| `DamageTargets` | `amount`, `per`, `count` | Damage to each target through the damage pipeline |
| `PlaceCounters` | `side`, `from`, `max` | One choice per counter |
| `DiscardAllEnergy`, `ReturnSelfToHand` | — | As named, for "this Pokémon" |
| `ChooseAttack` | `filter` | The player chooses a Benched Pokémon's attack |
| `UseChosenAttack` | — | Runs the chosen attack's program, then carries on |
| `AddLasting` | `lasting`, `side`, `amount` | Adds a lasting effect |
| `IgnoreDefenderEffects` | — | Sets the run's `ignore_defender` flag for its `Damage` step |
| `CountersOnTargets` | `max` | `max` damage counters on each target |
| `ShuffleSelfIntoDeck` | — | As named |
| `RestToBottom` | `amount` | The top `amount` cards that weren't taken go to the bottom |
| `ChooseCounterSource` | — | The player chooses one of their damaged Pokémon |
| `MoveCounters` | `max` | Up to `max` counters from that Pokémon to the first target |

`Draw` also records how many cards it drew, for `drew_at_least`.

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
