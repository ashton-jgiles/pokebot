# `src/engine/actions/combat.cpp` — attacks, Knock Outs and winning

**File:** [`src/engine/actions/combat.cpp`](../../../../src/engine/actions/combat.cpp) · **Namespace:** `pokebot::detail`

## Which attacks are legal

`add_attack_actions` offers each attack of the Active Pokémon whose cost (`rules::attack_cost`) can
be paid, as long as `rules::can_attack` allows it. The rulebook answer is no on **turn 1**: the player who goes first can't attack on their first turn (rulebook p. 13).

### `can_pay_cost(game, pokemon, cost)`

Attached Energy is counted in **units**: `rules::energy_provided` gives each card's units, as
`EnergyMask`s (the types a unit can count as). A Basic Energy is one unit of its type; Neo Upper
Energy on a Stage 2 is two units of any type; Ignition Energy on an Evolution is three {C} units.

- Each typed symbol needs its own unit that can be that type.
- Each Colorless symbol takes any unit left over.

Typed symbols are matched by trying every assignment. Costs are only a few symbols, so this is
cheap. It's exact even when some Energy can be any type, where a greedy match could fail: with any,
any and {R}, the cost {R}{W}{P} needs the {R} Energy to pay {R}.

## Damage: `attack_damage(game, attacker, defender, attack)`

A thin wrapper over [`rules::attack_damage`](../rules/rules.md#the-damage-pipeline), which runs the stages below.

Rulebook p. 20:

```
printed damage  → × Weakness multiplier   (if the defender is weak to one of the attacker's types)
                → − Resistance amount     (if the defender resists one of the attacker's types)
                → never below 0           → 1 damage counter per 10
```

An attack with no printed damage does none. For "20×" and "30+" attacks only the printed number
counts until their effects are written (Milestone 3).

## An attack: `apply_attack`

1. Removes the `MainPhase` (or `ExtraAttack`) decision and counts the attack in `flags.attacks_used`.
2. Records the attacker and attack in [history](../game/game_state.md#gamehistory).
3. If the attacker is **Confused**, flips a coin. On tails the attack does nothing: the attacker gets
   3 damage counters and it goes straight to `finish_attack` (see [conditions](conditions.md)).
   Otherwise, starts the attack's [effect program](../effects/interpreter.md) with `then = FinishAttack`. Its
   `Damage` step calls `deal_attack_damage`. The program may stop for choices.
4. **`finish_attack`**, once the whole effect is done: raises `AttackDone`. If the same Pokémon is still Active and `rules::attacks_per_turn` allows another attack, pushes an
   `ExtraAttack` decision. Otherwise **the turn ends**: the phase becomes `AfterMain`.
5. Resolves Knock Outs and runs [`continue_game`](turn.md#continue_game).

The `ExtraAttack` decision is pushed **before** Knock Outs, so any "choose a new Active" decisions
sit on top of it: the opponent promotes first, then the attacker attacks again or ends the turn
(Festival Lead: "after your opponent chooses a new Active Pokémon"). It offers the attacks of the
Pokémon whose id is in `decision.target` only, plus `EndTurn`.

## Using an attack's text: `perform_attack(game, who, attack)`

Runs the effect program of the attack `attack` names, with `who.self` as the attacker. It doesn't
check or pay a cost. Copy attacks work the same way inside the interpreter (see
[copy attacks](../effects/interpreter.md#copy-attacks)).

`deal_attack_damage(game, who, base, options)` is the `Damage` step: `base` damage (the attack's working
damage) to the opponent's Active, through the damage pipeline with the run's `DamageOptions`, with `who.self` as the attacker,
raising `DamagedByAttack` when it's above 0.

## Knock Outs: `handle_knockouts(game, after_attack)`

`after_attack` is `true` when `finish_attack` calls it: those Knock Outs also count in history as
Knocked Out by an attack. Each Knock Out ends the [lasting effects](lasting.md) on that Pokémon.

Runs after every attack and after every Pokémon Checkup (rulebook p. 14):

1. Every Pokémon `rules::is_knocked_out` says is (damage ≥ its HP) is **Knocked Out**. The `KnockedOut`
   event is raised and counted in history first; then it and every attached card go to its owner's
   discard pile.
2. The opponent takes `rules::prize_value` Prize cards: **1**, **2** for Pokémon ex, **3** for Mega Evolution Pokémon ex,
   or as many as remain, from the end of the pile. They're face down and were dealt from a shuffled
   deck, so taking a particular one is the same as taking one at random. Until step 6.1 they were
   picked at random, which made every Knock Out look like chance to the turn planner
   ([action classes](../../agents/planner/action_class.md)).
3. **Check for a winner** (below). If the game is over, stop.
4. Each player whose Active Spot is empty gets a `PromoteActive` decision: after a Knock Out, or after
   an effect like Tuck Tail took the Active out of play. The opponent of the current player chooses first.
5. [Bench limits](../effects/custom.md#the-bench-limit) are checked: Knocking Out the last Tera
   Pokémon can take Area Zero Underdepths' 8 spaces back to 5.

Bench Pokémon are checked too, since attacks can damage them and put damage counters on them.

## Winning

After Knock Outs, each player's **win conditions** are counted (rulebook p. 8):
- they've taken their last Prize card;
- their opponent has no Pokémon in play.

| Situation | Result |
|---|---|
| Only one player meets any | That player wins |
| Both do, one meets more | The one with more wins (rulebook p. 21) |
| Both meet the same number | **Draw** (the rulebook's tiebreaker game isn't modeled) |

Deck-out, the third way to win, is checked when a turn begins (see [turn](turn.md)).

## Choosing a new Active

`add_promote_actions` offers each Benched Pokémon. `apply_promote` moves it to the Active Spot and
runs `continue_game`, which finishes the turn or waits for the other player's promotion.

## Tests

[`tests/test_engine_attacks.cpp`](../../../../tests/test_engine_attacks.cpp), tag `[attacks]`:
- cost matching, no attack on turn 1, damage ending the turn;
- Weakness and Resistance;
- Knock Out and promotion, ex and Mega ex Prize counts;
- winning by Prize cards and by no Pokémon left, and both kinds of simultaneous win.
