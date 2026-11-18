# One-of-a-kind cards and the Bench limit

**Code:** [`src/engine/effects/custom.cpp`](../../../../src/engine/effects/custom.cpp), [`src/engine/actions/bench_limit.cpp`](../../../../src/engine/actions/bench_limit.cpp) · **Namespace:** `pokebot::detail`

A few cards change the rules in ways the [effect language](../../cards/effects/program.md) can't
say without a building block only they would use. They're written in C++ instead, and their effects
file names them:

```json
{ "requires": [{ "custom": "rare_candy" }], "effects": [{ "op": "custom", "name": "rare_candy" }] }
```

A `{"custom": name}` requirement compiles to `ConditionKind::Custom` and an `{"op": "custom"}`
step to `Op::Custom`, both holding a `CustomEffect`. An unknown name is a `CardDataError`.

## Custom effects

| `CustomEffect` | Card | Can be played when | What it does |
|---|---|---|---|
| `rare_candy` | Rare Candy (MEG 125) | Not on either player's first turn; one of your unevolved Basic Pokémon, not put into play this turn, has a Stage 2 in your hand whose Stage 1 evolves from it | Puts the Stage 2 on the Basic, skipping the Stage 1. It counts as evolving: Special Conditions and effects on it end, and `Evolved` is raised |
| `transformation_tome` | Transformation Tome (CRI 83) | You have a second Tome in hand, a Basic Pokémon in your discard pile, and an unevolved Basic in play | Discards the second Tome with it ("play 2 at once"), then swaps the Basic in play for the one from the discard pile. Attached cards, damage counters, Special Conditions, turns in play and lasting effects stay |

The Stage 1 Rare Candy skips is looked up among every card in the game, since only its name and
"evolves from" are needed and it doesn't have to be in play.

Both are one choice of a **(card, Pokémon in play)** pair, made with the `CustomChoice` decision.
Its actions are `ChooseCard` with the card and the target slot. Pairs with the same card name and
slot are merged.

| Function | Behavior |
|---|---|
| `custom_condition(game, player, effect)` | Whether the card can be played (`ConditionKind::Custom`) |
| `run_custom(game, run, step)` | Runs the step: pushes `CustomChoice`, or finishes the step if there's nothing to choose |
| `add_custom_actions(game, run, step, out)` | The legal pairs |
| `apply_custom(game, run, step, action)` | Carries out the chosen pair |

When a Trainer finishes, the engine checks for Knock Outs. Transformation Tome keeps the damage, so
swapping in a Basic with less HP can Knock it Out on the spot, and the opponent takes the Prize
cards.

## The Bench limit

`rules::bench_size` is 5. Area Zero Underdepths (SCR 131) raises it to 8 for a player with a Tera
Pokémon in play, through a `bench_size` [passive](../../cards/effects/program.md#always-on-effects-passive)
with `"if_tera_in_play": true`. The Bench itself holds up to `kMaxBenchSize` (8), and slots go up
to 8.

When the limit falls, a player can be over it: Area Zero was replaced by another Stadium, or their
last Tera Pokémon left play. `enforce_bench_limits(game, first)` then pushes a `DiscardBenched`
decision for each player over the limit, `first` choosing first. Its `amount` is how many to
discard. Each `DiscardBenched` action (with a Bench slot) puts that Pokémon and its cards in the
discard pile; this isn't a Knock Out, so no Prize cards are taken. It's checked:

- after a Stadium is played, for the old Stadium's owner first,
- after a Trainer finishes,
- after Knock Outs, once any new Active has been chosen.

## Tests

[`tests/test_engine_custom.cpp`](../../../../tests/test_engine_custom.cpp), tag `[custom]`: Rare Candy
evolving a Basic straight to Stage 2; not on the first turns or on a Pokémon played this turn;
Transformation Tome needing two, keeping Energy and damage; Tome Knocking Out the new Basic; Area
Zero giving 8 spaces with a Tera Pokémon; a forced discard down to 5 when Area Zero leaves; the load
checks.
