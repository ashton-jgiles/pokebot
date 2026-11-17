# `src/engine/actions/main_actions.cpp` — what you can do on your turn

**File:** [`src/engine/actions/main_actions.cpp`](../../../../src/engine/actions/main_actions.cpp) · **Namespace:** `pokebot::detail`

The main-phase choices, plus the retreat-discard decision.

## Legal actions in the main phase

`add_main_phase_actions` offers, in this order:

| Action | Allowed when | Rulebook |
|---|---|---|
| `PlayBasic` | Bench has fewer than 5 Pokémon | p. 10 |
| `Evolve` | The card evolves from the target's current name; **not** the player's first turn; the target didn't come into play or evolve this turn; its stack isn't full | p. 11 |
| `AttachEnergy` | No Energy attached yet this turn | p. 11 |
| `Retreat` | Not retreated yet this turn; a Benched Pokémon exists; the Active has at least as many Energy as its Retreat Cost | p. 12 |
| `PlayTrainer` | See [Trainer cards](#trainer-cards) | pp. 11–12 |
| `AttachTool` | The target has no Tool | p. 12 |
| `Attack` | See [combat](combat.md) | p. 13 |
| `EndTurn` | Always (it's always the last action in the list) | |

Each (card, target) pair is offered once. Two Darkness Energy in hand give one "attach Darkness
to the Active", not two.

Evolution checks names exactly: "Evolves from Testmon" needs a Pokémon whose top card is
named **Testmon**. Different printings with the same name work; regional forms and Trainer's
Pokémon have different names, so they don't.

## Trainer cards

| Kind | Allowed when (each a [rules](../rules/rules.md) question) | Played how |
|---|---|---|
| Item | Any number per turn; its `"requires"` hold; its effects are written | Waits in `resolving` while its [effect](../effects/interpreter.md#trainers-are-effects) runs, then goes to the discard pile |
| Supporter | One per turn, **not on the first player's first turn**; its `"requires"` hold; its effects are written | Same as an Item; sets the once-per-turn flag |
| Stadium | One per turn; not the same name as the Stadium in play | Goes into play in its owner's `PlayerState::stadium`; the old Stadium goes to **its** owner's discard pile |
| Tool | One per Pokémon | Attached to the target Pokémon (`PokemonInPlay::tool`) |

An Item or Supporter whose card has text but no effects yet **can't be played**, so an unfinished
card never does the wrong thing. Stadiums and Tools can be played already; what they do arrives in
step 3.7. Every Trainer played raises `TrainerPlayed`.

## Applying them

| Action | Effect |
|---|---|
| `PlayBasic` | Hand → new Benched Pokémon, played this turn |
| `Evolve` | Card goes on top of the stack; Energy and damage stay; `turn_played` becomes this turn, so it can't evolve again until next turn |
| `AttachEnergy` | Hand → the target's Energy; sets the once-per-turn flag |
| `Retreat` | Sets the flag, pays the cost, swaps (see below) |
| `PlayTrainer` | See [Trainer cards](#trainer-cards) |
| `AttachTool` | Hand → the target's Tool |
| `Attack` | Handed to [combat](combat.md) |
| `EndTurn` | Handed to [`end_turn`](turn.md) |

Benching, evolving, attaching and retreating each raise an [event](../events/events.md).

Clearing Special Conditions on evolving and retreating arrives with Special Conditions in Milestone 3.

## Retreating

```
Retreat (pick who comes in)
  ├─ cost 0, every attached Energy has to go, or they're all the same card
  │     → discard automatically, swap at once
  └─ otherwise → RetreatDiscard decision: discard one Energy at a time until the cost is paid
                 → then swap
```

The `RetreatDiscard` decision stores the cost still owed in `amount` and the incoming Benched
Pokémon in `target`. Its legal actions are one `DiscardEnergy` per *distinct* Energy attached. The
swap keeps both Pokémon's cards and damage, and the old Active takes the incoming one's Bench
position.

## Tests

[`tests/test_engine_actions.cpp`](../../../../tests/test_engine_actions.cpp), tag `[actions]`:
- benching to the limit, one Energy per turn;
- the evolution timing rules;
- retreating with and without a real choice, free retreat, and when retreat isn't allowed;
- 200 random games on the real decks, reporting how they ended and how often attacks happened.
