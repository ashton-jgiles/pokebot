# `src/engine/events` — moments cards react to

**Files:** [`events.hpp`](../../../../src/engine/events/events.hpp), [`events.cpp`](../../../../src/engine/events/events.cpp) · **Namespace:** `pokebot` (`raise` is in `pokebot::detail`)

Triggered effects ("when this Pokémon is damaged by an attack", "whenever you attach an Energy from
your hand", Risky Ruins, Punk Helmet…) need the engine to announce the moment. The engine calls
`detail::raise(game, event)` at each one. The [Standard scan](../../../research/standard-scan.md#events-when-cards-react)
lists the events Standard cards use.

## Events raised today

| `EventKind` | Raised by | `pokemon` | `card` |
|---|---|---|---|
| `PutOnBench` | Playing a Basic from hand in the main phase (not during setup) | The new Pokémon | — |
| `Evolved` | Evolving from hand | The Pokémon | The evolution card |
| `EnergyAttached` | Attaching Energy from hand | The Pokémon | The Energy |
| `Retreated`, then `MovedToBench` | Finishing a retreat | The Pokémon that left the Active Spot | — |
| `DamagedByAttack` | An attack that did more than 0 damage | The defender | The attacker's id |
| `AttackDone` | After the attack, before Knock Outs are handled | The attacker | — |
| `KnockedOut` | Each Knock Out, **before** its cards leave play | The Pokémon | — |
| `TurnEnding` | Once the main phase is over, before Pokémon Checkup | — | — |
| `Checkup` | Pokémon Checkup | — | — |
| `TrainerPlayed` | Playing a Trainer from hand, before its effect | The Pokémon a Tool went on | The Trainer |
| `PlayedToBench` | Playing a Basic from hand onto the Bench, right after `PutOnBench`. Not raised for Pokémon put on the Bench any other way | The new Pokémon | — |

`player` is whose Pokémon or card the event happened to. Pokémon are named by their
[`id()`](../game/game_state.md#pokemoninplay), so an event still points at the right Pokémon after it moves.

Still to come, with the cards that need them: cards discarded from the deck (Ferrothorn), an
Ability or Tera Pokémon leaving play (Area Zero).

## What happens on an event

`raise` tells the **event hook**, if one is set, then calls `run_triggers`, which starts any
[triggered effects](../actions/abilities.md#triggered-effects) for the event. Today those are
`PlayedToBench` (Abilities) and `PutOnBench` (Stadiums).

## The event hook

```cpp
using EventHook = void (*)(const GameState& game, const Event& event);
void set_event_hook(EventHook hook);
```

Lets a test or tool watch every event (the tests use it to check the order). It's a plain function
pointer stored outside `GameState`, one per thread, so copying a game for search never copies it.
Pass `nullptr` to turn it off.

`to_string(EventKind)` names an event for printing.

## Tests

[`tests/test_engine_events.cpp`](../../../../tests/test_engine_events.cpp), tag `[events]`: the
events of an attack that Knocks Out, in order; benching, evolving and attaching; retreating; game
history; and the extra-attack decision.
