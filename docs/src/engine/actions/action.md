# `src/engine/actions/action.hpp` — choices a player can make

**File:** [`src/engine/actions/action.hpp`](../../../../src/engine/actions/action.hpp) (header-only) · **Namespace:** `pokebot`

An `Action` is **one choice**. [`legal_actions`](../engine.md) returns a list of them, and
[`apply`](../engine.md) carries one out. It's 4 bytes, so search can store millions.

```cpp
struct Action {
    ActionKind kind = ActionKind::EndTurn;
    CardIndex card = kNoCard;     // the card being played, attached, discarded...
    Slot target = kActiveSlot;    // where it goes or what it acts on
    std::uint8_t amount = 0;      // a number: attack index, cards to draw, turn order
};
```

## Slots

```cpp
using Slot = std::uint8_t;
constexpr Slot kActiveSlot = 0;
constexpr Slot bench_slot(std::size_t bench_index);  // Bench position 0 -> slot 1, ... 7 -> slot 8
```

## Action kinds

| Kind | Uses | Answers |
|---|---|---|
| `ChooseTurnOrder` | `amount`: 0 = go first, 1 = go second | `ChooseTurnOrder` |
| `SetupPlaceActive` | `card`: a Basic from hand | `SetupActive` |
| `SetupPlaceBench` | `card`: a Basic from hand | `SetupBench` |
| `SetupDone` | — | `SetupBench` |
| `DrawMulliganCards` | `amount`: extra cards to draw (0 allowed) | `MulliganDraws` |
| `PlayBasic` | `card`: a Basic from hand, onto the Bench | `MainPhase` |
| `Evolve` | `card`: an Evolution from hand; `target`: the Pokémon to evolve | `MainPhase` |
| `AttachEnergy` | `card`: an Energy from hand; `target`: the Pokémon | `MainPhase` |
| `Retreat` | `target`: the Benched Pokémon that becomes Active | `MainPhase` |
| `Attack` | `amount`: attack index (0 = first) | `MainPhase` |
| `EndTurn` | — | `MainPhase` |
| `DiscardEnergy` | `card`: an Energy on the retreating Pokémon | `RetreatDiscard` |
| `Promote` | `target`: the Benched Pokémon that becomes Active | `PromoteActive` |
| `DiscardBenched` | `target`: a Benched Pokémon to discard | `DiscardBenched` |

Actions compare with `==`. `apply` uses this to check that the chosen action is in the legal list.

To turn an action into words, use [`describe(game, action)`](../describe/describe.md).
