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
| `draw` | `count` (1), `player`, or `until` | Draw cards; with `until`, draw until the hand has that many (none if it has them already) | `Draw`, or `DrawUntil` |
| `discard_tools` | `player` (`"opponent"`) | Discard the Pokémon Tool attached to that player's Active Pokémon ("before doing damage" places it before a `damage` step). The number discarded is the working value | `DiscardTools` |
| `discard_random` | `count` (1) or `for_each`, `player` (`"opponent"`) | Discard that many random cards from that player's hand | `DiscardRandom` |
| `discard_deck_top` | `count` (1) or `for_each`, `player` (`"you"`) | Discard that many cards from the top of that player's deck | `DiscardDeckTop` |
| `discard_hand` | — | Discard your hand | `DiscardHand` |
| `shuffle_deck` | — | Shuffle your deck | `ShuffleDeck` |
| `shuffle_hand_into_deck` | `player` | Shuffle your hand into your deck | `ShuffleHandIntoDeck` |
| `search_deck` | `count` (1), `filter`, `to` (`"hand"`), `shuffle` (`true`), `reveal` (`false`) | Search your deck for **up to** `count` matching cards, then shuffle (unless `"shuffle": false`, for a card that searches several times and shuffles once). With `"reveal": true` ("reveal them") the opponent sees what was found | `ChooseCards`, `MoveChosen`, `ShuffleDeck` |
| `discard_from_hand` | `count` (1), `filter` | Discard exactly `count` cards from your hand | `ChooseCards`, `MoveChosen` |
| `from_discard` | `count` (1), `filter`, `to` (`"hand"`), `up_to` (`false`) | Take exactly `count` matching cards from your discard pile, or up to `count` with `"up_to": true` | `ChooseCards`, `MoveChosen` |
| `attach_to` | `from` (`"discard"`), `targets` (1), `up_to_targets`, `on` (`"any"`), `target`, `count` (1), `up_to`, `filter` | Choose that many of your Pokémon (matching the `target` filter), then for **each** one choose `count` matching cards from `from` (`"discard"`, `"deck"` or `"hand"`) and attach them. From the deck, it shuffles afterwards (N's PP Up, Rosa's Encouragement, Glass Trumpet, Janine's Secret Art) | `ChooseTargets`, `EachTarget`, `ChooseCards`, `MoveChosen`, `NextTarget` |
| `move_energy` | `filter` | Choose one of your Pokémon with Energy, one of its matching Energy, then another of your Pokémon to move it to (Energy Switch) | `ChooseTargets`, `ChooseCards`, `ChooseDestination`, `MoveChosen` |
| `move_attached_energy` | `count` (1), `filter`, `to` (`"hand"`), `from` (`"self"`), `up_to` (`false`) | Move exactly `count` Energy attached to this Pokémon (fewer if it has fewer), or up to `count` with `"up_to": true`. `"from": "your_pokemon"` takes it from any of your Pokémon | `ChooseCards`, `MoveChosen` |
| `set_damage` | `amount`, `per`, `for_each` | The attack's damage becomes `amount` + `per` × the [count](#counting). For "×" attacks | `SetDamage` |
| `add_damage` | `amount`, `per`, `for_each` | The attack's damage goes up by `amount` + `per` × the count. For "+" attacks and "this attack does N more damage" | `AddDamage` |
| `flip_coins` | `count` (1) | Flip coins; the number of heads can be counted (`"heads"`) or tested (`heads_at_least`) | `FlipCoins` |
| `flip_until_tails` | — | Flip until tails; the number of heads is kept the same way | `FlipUntilTails` |
| `hand_to_deck_bottom` | `player` | Shuffle that player's hand and put it on the bottom of their deck (Special Red Card) | `HandToDeckBottom` |
| `damage_to` | `targets` (1), `on` (`"any"`), `player` (`"opponent"`), `up_to`, `each`, `amount`, `per`, `for_each` | Choose that many different Pokémon (`"on": "bench"` for the Bench only), or with `"each": true` take every one with no choice, and do `amount` + `per` × the count to each. Weakness and Resistance apply only to an Active target; a Benched Tera Pokémon takes none. Attacks only | `ChooseTargets`, `DamageTargets` |
| `place_counters` | `count` (1), `on` (`"any"`), `player` (`"opponent"`) | Put damage counters on that player's Pokémon "in any way you like", one choice per counter. Counters aren't damage, so Tera doesn't stop them | `PlaceCounters` |
| `discard_all_attached_energy` | — | Discard every Energy attached to this Pokémon | `DiscardAllEnergy` |
| `return_self_to_hand` | — | Put this Pokémon and everything attached to it into your hand (Tuck Tail). If it was Active, you choose a new one after the attack | `ReturnSelfToHand` |
| `discard_target_energy` | `count` (1), `on`, `player` (`"opponent"`), `filter` | Choose 1 Pokémon that has Energy, then discard `count` of its Energy (Crushing Hammer). With `"on": "active"` the Active Pokémon is taken, with no choice | `ChooseTargets`, `ChooseCards`, `MoveChosen` |
| `use_attack` | `filter` | Choose an attack of one of your matching Benched Pokémon and use it as this attack (Night Joker). Copy attacks can't be copied. Attacks only | `ChooseAttack`, `UseChosenAttack` |
| `search_to_top` | `count` (1), `filter`, `reveal` (`false`) | Search your deck for up to `count` cards, shuffle, then put them on top; the last one chosen is the top card (Ciphermaniac's Codebreaking) | `ChooseCards`, `ShuffleDeck`, `MoveChosen` |
| `special_condition` | `condition`, `player` (`"opponent"`), `count` (1) | Give that player's Active Pokémon a [Special Condition](../../engine/actions/conditions.md): `"asleep"`, `"burned"`, `"confused"`, `"paralyzed"` or `"poisoned"`. **Defaults to the opponent**, since cards almost always say "your opponent's Active Pokémon is now …". `count` is the Poison damage counters per Checkup | `ApplySpecialCondition` |
| `switch_self` | — | Switch this Pokémon with 1 of your Benched Pokémon | `ChooseBenched`, `SwitchWithChosen` |
| `switch_active` | `player` (`"you"` or `"opponent"`), `filter`, `chooser` (`"you"`) | Switch that player's Active with 1 of their Benched Pokémon that matches `filter`, chosen by you (Switch, Boss's Orders, Subjugating Chains), or by the opponent with `"chooser": "opponent"` ("switch out your opponent's Active Pokémon") | `ChooseBenched`, `SwitchWithChosen` |
| `counters_to` | `count` (1), `targets` (1), `on`, `player` (`"opponent"`), `each` | Put `count` damage counters on each chosen Pokémon (Mortal Shuriken), or on every one with `"each": true` | `ChooseTargets`, `CountersOnTargets` |
| `move_counters` | `count` (1), `on`, `player` (`"opponent"`) | Choose one of your damaged Pokémon, then one of that player's, and move up to `count` damage counters (as many as it has, up to `count`) | `ChooseCounterSource`, `ChooseTargets`, `MoveCounters` |
| `look_at_top` | `count` (1), `take` (1), `to` (`"hand"`), `filter` | Look at the top `count` cards, take `take` of them, and put the rest on the bottom in the same order (Recon Directive) | `ChooseCards`, `MoveChosen`, `RestToBottom` |
| `attach_from_hand` | `count` (1), `filter` | Attach that many matching cards from your hand to this Pokémon (Teal Dance) | `ChooseCards`, `MoveChosen` |
| `shuffle_self_into_deck` | — | Shuffle this Pokémon and everything attached into your deck (Run Away Draw) | `ShuffleSelfIntoDeck` |
| `counters_on_self` | `count` (1) | Put damage counters on "this Pokémon" (Risky Ruins, where "this Pokémon" is the one put on the Bench) | `CountersOnSelf` |
| `discard_stadium` | — | Discard the Stadium in play, to its owner's discard pile (Snow Sink) | `DiscardStadium` |
| `switch_to_active` | — | This Benched Pokémon switches with your Active Pokémon (Rapid Vernier) | `SwitchToActive` |
| `may` | `effects` (a list) | "You may …": ask first, and skip the list on no | `AskYesNo`, then the list |
| `ignore_weakness`, `ignore_resistance` | — | "This attack's damage isn't affected by Weakness" (or Resistance): that stage of the [damage pipeline](../../engine/rules/rules.md#the-damage-pipeline) is skipped, for the `damage` step and `damage_to`. Must come before a placed `damage` step. Attacks only | `IgnoreWeakness`, `IgnoreResistance` |
| `damage_self` | `amount` | "This Pokémon also does `amount` damage to itself": `amount` ÷ 10 damage counters on the attacker, with no Weakness or Resistance. A multiple of 10, at most 600. Attacks only | `CountersOnSelf` |
| `heal_self` | `amount` or `all` | Heal `amount` damage (a multiple of 10, at most 990), or all of it, from this Pokémon | `HealSelf` |
| `heal_to` | `amount` or `all`, `targets` (1), `up_to`, `each`, `on`, `player` (`"you"`) | Choose that many of your **damaged** Pokémon, or every one with `"each": true`, and heal each. With no damaged Pokémon, nothing happens | `ChooseTargets`, `HealTargets` |
| `ignore_defender_effects` | — | "This attack's damage isn't affected by any effects on your opponent's Active Pokémon" (Shred): the printed Weakness and Resistance are used and defender-side effects skipped. Must come before a placed `damage` step. Attacks only | `IgnoreDefenderEffects` |
| `lasting` | `effect`, `until`, `on` (`"self"`), `player`, `amount`, `only_ex`, `prevent_effects`, `from_basic` | A [lasting effect](../../engine/actions/lasting.md). `effect`: `"cant_attack"`, `"cant_use_attack"` (this attack), `"cant_retreat"`, `"no_items"`, `"damage_bonus"`, `"takes_less"`, `"prevent_damage"`, `"deals_less"`, `"takes_more"`, `"attack_flip"`. `until`: `"this_turn"`, `"opponents_next_turn"`, `"your_next_turn"`. `on`: `"self"` or `"defending"` for the first three (attacks only); `"player"` (with `player`) for `no_items`; `"player"` or `"self"` for `damage_bonus`; `"self"` for `takes_less` and `prevent_damage`; `"defending"` for `deals_less`, `takes_more` and `attack_flip`. `takes_less`, `deals_less`, `takes_more` and `damage_bonus` need a positive `amount`; for `prevent_damage`, an `amount` prevents only damage up to it, `from_basic` only damage from Basic Pokémon, and `prevent_effects` the attacks' effects too | `AddLasting` |
| `if` | `condition`, `then`, `else` (optional) | Do `then` if the [condition](#conditions) holds when the step runs, otherwise `else` | `JumpUnless`, `then`, `Jump`, `else` |

- Cards moved to a hand, deck or discard pile always go to **their owner's**, even when the effect is
  the opponent's (Crushing Hammer's Energy goes to its owner's discard pile).
- `to` can be `"hand"`, `"discard"`, `"deck"`, `"bench"` or `"self"` (attached to this Pokémon). Cards put on the Bench become new
  Pokémon in play, and no more are chosen than the Bench has room for.
- `player` can be `"you"` (the default), `"opponent"` or `"each"` (you first, then your opponent).
- `count` must be 0 to 60.
- `on` can be `"any"` (the default: the Active Pokémon and the Bench), `"bench"` or `"active"`. `"active"` and `"each": true` take
  every matching Pokémon without a choice, so they can't have `targets` or `up_to`.
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
| `"active_damage_counters"` | Damage counters on that player's Active Pokémon |
| `"in_play"` | That player's Pokémon in play matching `filter` |
| `"tools_in_play"` | Pokémon Tools attached to that player's Pokémon |
| `"energy_in_play"` | Energy cards attached to that player's Pokémon, matching `filter` |
| `"self_energy"` | Energy cards attached to this Pokémon, matching `filter` |
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
| `not_type` | Not of this type ("non-{D}") |
| `tera` | `true` / `false`: a Tera Pokémon or not |
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
| `{"self_is": filter}` | "This Pokémon" matches |
| `{"stadium_in_play": true}` | A Stadium is in play |
| `{"more_prizes_than_opponent": true}` | You have more Prize cards left than your opponent |
| `{"in_play_has": filter}` | One of your Pokémon in play matches |
| `{"your_energy_has": filter}` | An Energy attached to one of your Pokémon matches |
| `{"attached_to_active": true}` | This effect attached Energy to your Active Pokémon (only inside an effect) |
| `{"opponent_active_is": filter}` | Your opponent's Active Pokémon matches (`{"rule_box": true}` for a Pokémon ex) |
| `{"opponent_active_damaged": true}` | Your opponent's Active Pokémon has damage counters |
| `{"opponent_active_has": condition}` | Your opponent's Active Pokémon has that Special Condition (`"poisoned"`, …) |
| `{"self_damaged": true}` | This Pokémon has damage counters (only inside an effect) |
| `{"self_has_tool": true}` | This Pokémon has a Pokémon Tool attached (only inside an effect) |
| `{"bench_damaged": true}` | One of your Benched Pokémon has damage counters |
| `{"custom": name}` | A [one-of-a-kind card](../../engine/effects/custom.md)'s own check (`rare_candy`, `transformation_tome`) |

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
| `modify` | `"max_hp"`, `"retreat_cost"`, `"weakness_type"` (needs `type`) `"damage_dealt"` (attacks by the Pokémon do `add` more to the opponent's Active, before Weakness), `"bench_size"` (needs `set`: the owner of each Pokémon it applies to gets that many Bench spaces), `"damage_taken"` (the Pokémon takes `add` more from attacks, after Weakness and Resistance; negative for less), `"prevent_damage"` (no damage from the opponent's attacks, from attackers matching `from`) or `"prevent_effects"` (no effects of the opponent's attacks) |
| `add` / `set` | A change, or a new value. A `set` wins over `add`s ("has no Retreat Cost") |
| `type` | For `weakness_type`: the new Weakness type. A Pokémon with no printed Weakness gets one, ×2 |
| `applies_to` | `"holder"` (the default: the Pokémon the Tool is attached to, or the Pokémon with the Ability), `"yours"`, `"opponents"` or `"all"` (everyone's Pokémon in play) |
| `filter` | Which Pokémon it affects, matched against the Pokémon's top card |
| `if_poisoned` | Only while that Pokémon is Poisoned |
| `if_tera_in_play` | For `bench_size`: only for a player with a Tera Pokémon in play (Area Zero Underdepths) |
| `from` | For `prevent_damage`: only attacks by Pokémon matching this filter (`{"rule_box": true}`: Pokémon ex) |
| `if_active`, `if_benched` | Only while the Pokémon with the Ability or Tool is Active, or on the Bench |

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
| `ChooseTargets` | `side`, `from` (`Bench`, `InPlay` or `Active`), `min`, `max`, `needs_energy`, `needs_damage`, `each` | The player chooses different Pokémon on that side (only ones with Energy, or with damage counters, if asked); with `each`, every one is taken with no choice |
| `DamageTargets` | `amount`, `per`, `count` | Damage to each target through the damage pipeline |
| `PlaceCounters` | `side`, `from`, `max` | One choice per counter |
| `DiscardAllEnergy`, `ReturnSelfToHand` | — | As named, for "this Pokémon" |
| `ChooseAttack` | `filter` | The player chooses a Benched Pokémon's attack |
| `UseChosenAttack` | — | Runs the chosen attack's program, then carries on |
| `AddLasting` | `lasting`, `side`, `amount` | Adds a lasting effect |
| `IgnoreDefenderEffects` | — | Sets the run's `ignore_defender` flag for its `Damage` step |
| `IgnoreWeakness`, `IgnoreResistance` | — | Set the run's `ignore_weakness` or `ignore_resistance` flag, for `Damage` and `DamageTargets` |
| `HealSelf` | `amount` | Removes `amount` ÷ 10 damage counters from "this Pokémon", as many as it has |
| `HealTargets` | `amount` | The same, from each target |
| `CountersOnTargets` | `max` | `max` damage counters on each target |
| `ShuffleSelfIntoDeck` | — | As named |
| `RestToBottom` | `amount` | The top `amount` cards that weren't taken go to the bottom |
| `ChooseCounterSource` | — | The player chooses one of their damaged Pokémon |
| `MoveCounters` | `max` | Up to `max` counters from that Pokémon to the first target |
| `CountersOnSelf` | `max` | `max` counters on "this Pokémon" |
| `DiscardStadium` | — | As named |
| `SwitchToActive` | — | As named |
| `EachTarget` | `jump` | The start of a loop over the targets: when every target is done, carry on at `jump` |
| `NextTarget` | `jump` | The end of the loop: the next target, back to `jump` |
| `MoveChosen` with `reveal` | — | Also shows the moved cards to the opponent ([knowledge](../../engine/knowledge/knowledge.md)) |
| `ChooseDestination` | — | The player chooses another of their Pokémon (not a target) |
| `Custom` | `custom` | Hands over to a [one-of-a-kind card's C++](../../engine/effects/custom.md) |

`ChooseTargets` only offers Pokémon that match its `filter`. In a loop, the `TargetEnergy` zone is
the Energy of the current target. `ChosenEnergy` is the Energy of the Pokémon `ChooseDestination` chose.

### Special Energy

A Special Energy's effects file sets `provides` (TCGdex leaves it empty) and, if what it provides
depends on the Pokémon it's attached to, `provides_when`:

```json
{ "provides": ["colorless"],
  "provides_when": [{ "holder": { "stage": "stage2" }, "provides": ["any", "any"] }] }
```

The first entry whose `holder` filter matches the Pokémon wins. Each `provides` item is one unit of
Energy: a type, or `"any"` (it can count as every type). `compile_provides_when` compiles the list.
`"discard_at_end_of_turn": true` discards the card at the end of its owner's turn (Ignition
Energy).

### Triggers

An Ability or a Stadium can say `"trigger"` to run its `effects` on its own instead of being chosen:

| `"trigger"` | When |
|---|---|
| `"played_to_bench"` | An Ability: when you play this Pokémon from your hand onto your Bench |
| `"put_on_bench"` | A Stadium: whenever a player puts a Pokémon onto their Bench during their turn, by any means |
| `"evolved"` | An Ability: when you play this Pokémon from your hand to evolve one of your Pokémon (Rare Candy too) |
| `"damaged_by_attack"` | An Ability: when this Pokémon, in the Active Spot, is damaged by the opponent's attack, even if it's then Knocked Out. "The Attacking Pokémon" is the opponent's Active |

See [triggered effects](../../engine/actions/abilities.md#triggered-effects). `trigger_from_string`
reads the name; an unknown one is a `CardDataError`.

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

`Step`, `CardFilter`, `Condition`, `Count`, `LastingSpec`, `Modifier` and `ConditionalProvides` compare with `==`, field by field, so two programs are equal
exactly when they run the same steps. The [translator's check](../../../tools/import/translate.md#checking-it) uses it.

The enums cards use (`CardKind`, `EnergyType`, …) live in
[`card_types.hpp`](../card/card.md#enums), so this header and `card.hpp` don't include each other.

## Tests

[`tests/test_effect_program.cpp`](../../../../tests/test_effect_program.cpp), tag `[effects]`: an
attack with no effects, how `search_deck`, `may` and `if` expand, a placed `damage` step, Trainer
programs, conditions, every kind of mistake rejected, every filter field, and `CardDB::add` naming
the attack in its error.

**Naming note:** `requires` is a C++20 keyword, so the C++ name for a card's `"requires"` list is
`requirements`.
