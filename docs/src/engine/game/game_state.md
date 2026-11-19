# `src/engine/game/game_state` — the game in memory

**Files:** [`src/engine/game/game_state.hpp`](../../../../src/engine/game/game_state.hpp), [`src/engine/game/game_state.cpp`](../../../../src/engine/game/game_state.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The data structures of the formal model's world state. See the [engine overview](../README.md) for
how they're used.

| Formal model | C++ |
|---|---|
| card instances `Inst = {1..120}` | `CardIndex` 0–119 + `CardTable` |
| a Pokémon in play `Mon` | `PokemonInPlay` |
| a player's zones `Z_i` | `PlayerState` |
| once-per-turn flags | `TurnFlags` |
| decisions and the pending stack | `Decision`, `GameState::pending` |
| world state `w` | `GameState` |

## Card instances

```cpp
using CardIndex = std::uint8_t;   // 0-59: player 0's cards, 60-119: player 1's, in decklist order
constexpr CardIndex kNoCard = 255;
```

| Constant / function | Value |
|---|---|
| `kPlayers` | 2 |
| `kDeckSize` | 60 |
| `kMaxCards` | 120 |
| `kBenchSize` | 5 |
| `kPrizeCount` | 6 |
| `kMaxEvolutionStack` | 3 (Basic, Stage 1, Stage 2) |
| `owner_of(card)` | 0 for cards 0–59, 1 for 60–119 |
| `opponent_of(player)` | `1 - player` |

## `CardTable`

Maps every card instance to its `CardDef`. It's built once per game, and every copy of the game
points to it.

```cpp
CardTable(const CardDB& db, const Decklist& deck0, const Decklist& deck1);
explicit CardTable(const std::array<const CardDef*, kMaxCards>& defs);
const CardDef& def(CardIndex card) const;
CardTable shuffled(std::uint64_t seed) const;
```

Cards are numbered in decklist order. `shuffled(seed)` returns a copy with each player's cards in a
random order instead (player 0 keeps 0–59, player 1 60–119), so card numbers don't give a deck away.
See [knowledge](../knowledge/knowledge.md#card-numbers).

The constructor from an array of definitions is for a [`World`](../../agents/world.md), which fills
in the cards a player hasn't seen with a placeholder.

The constructor runs the [deck validator](../../cards/validator/deck_validator.md) on both decks and throws
`std::invalid_argument`, listing every problem, if either is illegal, so a game can never start
with an illegal deck.

**Lifetime:** it stores pointers into the `CardDB`, so the database must outlive the table, and
the table must outlive every `GameState` that uses it.

## `PokemonInPlay`

| Field | Meaning |
|---|---|
| `stack` | Evolution stack, bottom (Basic) to top (current card); `top()` returns the top |
| `energy` | Attached Energy cards |
| `tool` | Attached Tool, or `kNoCard` |
| `damage_counters` | Each is 10 damage |
| `turn_played` | Turn it came into play or last evolved (0 = setup); used by the evolution rules |
| `special` | Its [Special Conditions](../actions/conditions.md): `asleep`, `confused`, `paralyzed`, `burned`, `poison` (counters per Checkup, 0 = not Poisoned), `paralyzed_turn`; `any()` |

`id()` returns the **bottom** card. It never changes while the Pokémon stays in play, even when it
evolves, retreats or changes slot, so effects, events and history use it to name a Pokémon. Slots
shift when a Benched Pokémon leaves; ids don't. `detail::find_pokemon(game, id)` finds it again, or
returns `nullptr` once it has left play.

## `PlayerState`

| Field | Meaning |
|---|---|
| `deck` | `back()` is the **top** card, so drawing is a cheap `pop_back()` |
| `hand`, `prizes`, `discard`, `lost_zone` | The other zones |
| `active` | `std::optional<PokemonInPlay>`; empty only while choosing a new Active, or after the game is decided |
| `bench` | Up to `kMaxBenchSize` (8) Pokémon; how many are allowed is [`rules::bench_size`](../rules/rules.md) |
| `resolving` | Up to 2 Trainer cards being played, while their effect runs |
| `stadium` | This player's Stadium in play, or `kNoCard`. At most one player has one |
| `mulligans` | Mulligans taken during setup |

## `TurnFlags`

`energy_attached`, `supporter_played`, `stadium_played`, `stadium_used`, `retreated`: the
once-per-turn limits. `attacks_used` counts attacks this turn (Festival Lead allows two), and
`abilities_used` lists the [Abilities used](../actions/abilities.md) as `AbilityUse{pokemon, card, index}`.
They're reset at the start of every turn.

## `AttackRef` and `EffectContext`

| Type | Fields | Meaning |
|---|---|---|
| `AttackRef` | `text_from`, `index` | Which attack's text to use: the card it's printed on and its position on that card. Normally the attacker's own top card; a copied attack names another card |
| `EffectContext` | `player`, `self`, `source` | Who an effect runs for. "This Pokémon" means `self` (an `id()`), not the card the text is printed on; `source` is the card that started it |

A copied attack (N's Zoroark ex, Mew ex, Slowking) runs the other card's text with the copier as
`self`, so the copier's type, Energy and Weakness interactions apply.

## `EffectRun`

An effect part way through: who it's for (`EffectContext`), which compiled program (`ProgramRef`:
card, `ProgramKind` (`Attack` or `Trainer`), index), what to do when it finishes (`AfterEffect`), the next step (`pc`), and
the choices made so far (`chosen`, `chosen_pokemon`), and two working numbers: the attack's
`damage` so far and the latest `value` (heads, or cards moved), plus the chosen `targets` and
`chosen_attack`. `GameState::effects` stacks up to 4. See the
[interpreter](../effects/interpreter.md).

## `LastingEffect`

An effect that lasts for a turn: `kind`, the `player` it affects, the `pokemon` it's on (or
`kNoCard` for the whole player), an `attack` (for "can't use this attack"), `amount` and `only_ex`
(for a damage bonus), and the `turn` it applies during. `GameState::lasting` holds up to 8. See
[lasting effects](../actions/lasting.md).

## `GameHistory`

Cards look back at earlier turns ("if any of your Pokémon were Knocked Out during your opponent's
last turn"). `history.turns` keeps three `TurnRecord`s:

| Accessor | Turn |
|---|---|
| `this_turn()` | The current turn |
| `opponents_last_turn()` | The turn just before |
| `your_last_turn()` | The current player's previous turn |

A `TurnRecord` holds whose turn it was, how many of each player's Pokémon were Knocked Out
(`knocked_out`) and how many of those were by an attack (`knocked_out_by_attack`, not counting
Pokémon Checkup), and the attack used (`attacker` id and `AttackRef`; `kNoCard` if none).
`begin_turn` shifts them along.
More facts (cards played, once-per-game effects) join as the cards that need them arrive.

## `TurnPhase`

`Setup`, `Main`, `AfterMain`, `Checkup`. Where the game is within a turn, so the engine knows what
happens once every pending decision is answered. See [`continue_game`](../actions/turn.md#continue_game).

## `Decision`

| Field | Meaning |
|---|---|
| `kind` | One of the `DecisionKind`s listed in the [engine overview](../README.md#every-choice-is-a-decision) |
| `player` | Who decides |
| `amount` | Extra number the decision needs: bonus cards allowed, Energy still to discard, Benched Pokémon still to discard |
| `target` | A `Slot`, e.g. the Benched Pokémon a retreat will bring in, or for `ExtraAttack` the `id()` of the Pokémon allowed to attack again |

## `Knowledge`

Two [`CardSet`](#knowledge)s per player: `located` (where the card is) and `seen` (seen at some
point this game). A `CardSet` is 120 bits in two `std::uint64_t`s, with `insert`, `erase` and
`contains`; `std::bitset` isn't guaranteed trivially copyable. What goes in them is explained in
[knowledge](../knowledge/knowledge.md).

## `GameResult`

`Ongoing`, `Player0Wins`, `Player1Wins`, `Draw`. A draw is when both players win at once with the
same number of win conditions. The rulebook's tiebreaker game isn't modeled.

## `GameState`

| Field | Meaning |
|---|---|
| `cards` | Pointer to the shared `CardTable` |
| `players` | Both players' `PlayerState` |
| `current_player`, `first_player` | Whose turn it is; who went first |
| `turn` | 0 during setup; 1 is the first player's first turn, 2 the second player's |
| `phase` | `TurnPhase` |
| `flags` | `TurnFlags` for the current turn |
| `history` | `GameHistory`: this turn and the two before |
| `pending` | Stack of up to 8 open decisions; `back()` is decided first |
| `effects` | Stack of up to 4 running effects ([`EffectRun`](#effectrun)) |
| `lasting` | Up to 8 [lasting effects](#lastingeffect) |
| `knowledge` | What each player knows ([`Knowledge`](#knowledge)) |
| `rng` | The game's random number generator |
| `result` | `GameResult` |

Helpers: `player(p)`, `def(card)`, `decision()` (the top of `pending`), `is_over()`.

A `static_assert` requires `GameState` to be **trivially copyable**. Adding a `std::vector` or
`std::string` to it stops the build, because copying the state is how search explores the game.
A whole game is **2,688 bytes**: 1,920 at the start of Milestone 3, 2,624 once the Bench grew to 8
for Area Zero Underdepths, and 64 more for `knowledge`.

## Tests

[`tests/test_engine_state.cpp`](../../../../tests/test_engine_state.cpp), tag `[engine]`: copies are
independent and equal, card ownership, `CardTable` numbering, and illegal decks are refused.
