# `src/engine/rules` — the questions cards can change

**Files:** [`rules.hpp`](../../../../src/engine/rules/rules.hpp), [`rules.cpp`](../../../../src/engine/rules/rules.cpp) · **Namespace:** `pokebot::rules`

The engine never reads a rule straight off a card definition (`def.hp`, `def.retreat`,
`attack.cost`…). It **asks a question** here instead. Today each answer is the rulebook default.
In Milestone 3, Stadiums, Tools, Special Energy, passive Abilities and lasting effects will change
the answers here, without the rest of the engine knowing. That was the lesson from Forest of
Vitality: a rule a card can bend must not be hard-coded where it's used.

The [Standard scan](../../../research/standard-scan.md) lists the 25 rules cards in Standard
bend; this file is where they live.

## Questions

| Question | Rulebook answer | Cards that will change it (examples) |
|---|---|---|
| `max_hp(game, pokemon)` | Printed HP of the top card, plus [always-on](#always-on-effects) `max_hp` changes | Hero's Cape, Lively Stadium |
| `remaining_hp`, `is_knocked_out` | Max HP − 10 per damage counter; Knocked Out at 0 or less | Survival Brace |
| `types(game, pokemon)` | Printed types | Carbink, Scovillain ex |
| `weakness`, `resistance` | Printed; `weakness_type` changes the Weakness type | Lillie's Clefairy ex, Illumise |
| `retreat_cost(game, pokemon)` | Printed Retreat Cost, plus `retreat_cost` changes; a `set` wins; never below 0 | Air Balloon, N's Castle, Latias ex |
| `attack_text(game, attack_ref)` | The attack printed on `attack_ref.text_from` | (copy attacks pass another card) |
| `attack_cost(game, pokemon, attack)` | Printed cost | Counter Gain, Kyurem |
| `energy_provided(game, pokemon, energy)` | The card's `provides` list | Prism Energy, Meganium's Wild Growth |
| `is_active(game, pokemon)` | Whether it's in its owner's Active Spot | |
| `can_get_special_condition(game, pokemon, condition)` | Only the Active Pokémon | Festival Grounds, Hoothoot, Slowpoke |
| `poison_counters(game, pokemon)` | What Poisoned it said (usually 1) | Perilous Jungle |
| `burn_counters(game, pokemon)` | 2 | |
| `prize_value(game, pokemon)` | 1, 2 for ex, 3 for Mega ex | Lillie's Pearl, Legacy Energy |
| `bench_size(game, player)` | 5 | Area Zero Underdepths |
| `is_first_turn(game)` | Turns 1 and 2 | (used by other questions) |
| `can_attach_energy(game, player)` | Not yet this turn | Emboar, Iono's Bellibolt ex |
| `can_retreat(game, player)` | Not yet this turn, with a Benched Pokémon to switch in, the Active isn't Asleep or Paralyzed, and no [lasting](../actions/lasting.md) "can't retreat" | Clutch, Sob |
| `can_attack(game, player)` | Not on turn 1, only with an Active, not while it's Asleep or Paralyzed, and no lasting "can't attack" | Eon Blade, Prism Edge |
| `has_abilities(game, pokemon)` | Yes | Team Rocket's Watchtower, Flutter Mane (Ability locks) |
| `can_use_attack(game, pokemon, attack)` | Yes, unless a lasting "can't use this attack" is on it | Impact Blow |
| `can_play_item(game, player)` | Yes, unless a lasting Item lock is on the player | Budew, Tyranitar (Item locks) |
| `can_play_supporter(game, player)` | Not yet this turn, and not on turn 1 | |
| `can_play_stadium(game, player, card)` | Not yet this turn, and not the same name as the one in play | Copperajah |
| `can_attach_tool(game, pokemon)` | It has no Tool | Rotom ex's Multi Adapter |
| `stadium_in_play(game)` | Whichever player's Stadium is in play, or `kNoCard` | |
| `attacks_per_turn(game, pokemon)` | 1 | Dipplin, Goldeen, Seaking (Festival Lead with Festival Grounds) |
| `can_evolve(game, pokemon)` | Not on either player's first turn, not on the turn it was played, stack not full | Forest of Vitality |
| `evolves_into(game, pokemon, card)` | The card's "evolves from" names the top card | Rare Candy, Eevee ex |
| `damage_prevented(game, attacker, defender)` | A Benched Tera Pokémon takes no damage from attacks | Mist Energy, Battle Cage |
| `attack_damage(game, attacker, defender, base)` | See below. `base` is the attack's damage after its own formula | Maximum Belt, Full Metal Lab |

`EnergyList` is a `FixedVector<EnergyType, 8>`: answers come back by value with no heap use,
because a card effect may build a different list than the printed one.

## The damage pipeline

`attack_damage` runs the stages in rulebook order (p. 20). Each is a separate function so effects
can hook the right one:

```
base (printed damage, after the attack's formula) → attacker-side effects → × Weakness → − Resistance → defender-side effects → never below 0
```

Prevented damage is 0. **Weakness and Resistance apply only when the defender is Active**; damage to
a Benched Pokémon skips them, as the rulebook says.

The attacker-side stage adds lasting damage bonuses (Black Belt's Training) and always-on
`damage_dealt` bonuses (Binding Mochi) when the defender is Active. The defender-side stage does
nothing yet. An attack with no damage does none.

`attack_damage(…, ignore_defender_effects)` is for Shred: it uses the defender's **printed**
Weakness and Resistance and skips the defender-side stage.

## Always-on effects

Tools, Stadiums and passive Abilities carry [`Modifier`s](../../cards/effects/program.md#always-on-effects-passive).
Two private helpers in `rules.cpp` connect them to the questions:

- `each_modifier(game, visit)` visits every modifier in play: the Stadium's, then each Tool's and
  each Ability's for every Pokémon in play, with its owner and the Pokémon holding it.
- `applies(game, modifier, owner, holder, target)` decides whether a modifier reaches a Pokémon,
  from `applies_to`, the filter, and `if_poisoned`.

Nothing is cached: each question looks through what's in play when it's asked, so a Tool or Stadium
works the moment it's played and stops the moment it leaves.

## Speed

These are ordinary function calls in their own file. Without inlining across files they cost
about 15% of simulator speed, so the release preset turns on link-time optimization
(`CMAKE_INTERPROCEDURAL_OPTIMIZATION`), which brings it back to the old speed.

## Tests

[`tests/test_rules.cpp`](../../../../tests/test_rules.cpp), tag `[rules]`: HP and Knock Outs, the
printed type, costs, Energy and Prize cards, turn limits, and evolution timing. Every older engine
test passes unchanged, and seeded simulator runs give identical results before and after this
layer was added.
