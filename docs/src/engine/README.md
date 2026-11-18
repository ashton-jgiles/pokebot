# `src/engine` — the rules engine

**Library:** `pokebot_lib` · **Namespace:** `pokebot` (shared helpers in `pokebot::detail`)

The engine holds a game in memory, knows every legal choice, and applies choices. Everything that
plays the game (random agents, the simulator, and later the search-based AIs) uses only the
three functions in [`engine.hpp`](engine.md):

```cpp
GameState new_game(const CardTable& cards, std::uint64_t seed);
std::vector<Action> legal_actions(const GameState& game);
void apply(GameState& game, const Action& action);
```

These are the formal model's *Legal* and *δ* (transition) functions.

## What the engine covers (Milestone 3)

| Covered | Not yet (Milestone 3) |
|---|---|
| Setup: coin flip, opening hands, mulligans with bonus draws, Active/Bench, 6 Prize cards | Psyduck's Damp (no Ability in our decks needs it) |
| Trainers: Items, Supporters, Stadiums, Tools, their limits and `"requires"`; 16 with effects | |
| Turn loop: draw, main phase, end of turn, Pokémon Checkup step | |
| Benching, evolving, attaching Energy, retreating, and attaching Energy from the deck or discard pile | |
| [Triggered effects](actions/abilities.md#triggered-effects): "when you play this Pokémon", Risky Ruins | |
| [Abilities you choose to use](actions/abilities.md), and Stadium actions | |
| [Lasting effects](actions/lasting.md): can't attack, can't use an attack, can't retreat, Item lock, damage bonus | |
| Attacks: Energy costs, Weakness, Resistance, damage, and [effects](effects/interpreter.md): draw, search, switch, "you may", damage formulas, Bench damage, damage counters, copy attacks | |
| [Special Conditions](actions/conditions.md) and the full Pokémon Checkup | |
| Knock Outs, Prize cards (1 / 2 ex / 3 Mega ex), new Active | |
| Winning: Prize cards, no Pokémon left, deck-out, both at once | |
| Special Energy, Energy counted in units, exact cost matching, Retreat paid in Energy provided | |
| [Always-on effects](rules/rules.md#always-on-effects) from Tools, Stadiums and passive Abilities: HP, Retreat Cost, Weakness, damage bonuses | |
| Tera Bench protection | |
| [One-of-a-kind cards](effects/custom.md): Rare Candy, Transformation Tome, Area Zero Underdepths and the Bench limit | |

## Key ideas

### A game is a plain value

[`GameState`](game/game_state.md) contains no pointers to things that change and no heap
allocations. Zones are [`FixedVector`](shared/fixed_vector.md)s, and cards are small numbers
(`CardIndex` 0–119). Copying a game is one ~2.6 KB memory copy, which is what lets search explore
millions of positions. A `static_assert` stops the build if anyone breaks this.

The only pointer is `GameState::cards`, to the [`CardTable`](game/game_state.md#cardtable) that maps
card numbers to card definitions. It never changes during a game, so every copy shares it.
**Lifetime rule:** the `CardDB` must outlive the `CardTable`, which must outlive every game using it.

### Every choice is a decision

The engine runs automatically (shuffling, drawing, Knock Outs) until a player has to choose
something, then stops. Open decisions live on a stack, `GameState::pending`. The top (`back()`) is
the decision being made now.

| Decision | Who decides | Legal actions | Defined in |
|---|---|---|---|
| `ChooseTurnOrder` | Coin-flip winner | go first / go second | [setup](setup/setup.md) |
| `SetupActive` | Each player | a Basic Pokémon from hand | [setup](setup/setup.md) |
| `SetupBench` | Each player | a Basic to bench, or done | [setup](setup/setup.md) |
| `MulliganDraws` | Opponent of whoever mulliganed more | draw 0..N extra cards | [setup](setup/setup.md) |
| `MainPhase` | Current player | bench, evolve, attach, retreat, attack, end turn | [main actions](actions/main_actions.md) |
| `RetreatDiscard` | Current player | which Energy to discard for a Retreat Cost | [main actions](actions/main_actions.md) |
| `PromoteActive` | Player whose Active was Knocked Out | a Benched Pokémon | [combat](actions/combat.md) |
| `ExtraAttack` | Current player, after an attack when more are allowed | that Pokémon's attacks, or end the turn | [combat](actions/combat.md) |
| `ChooseCards` | Whoever the effect belongs to | a matching card, or done | [interpreter](effects/interpreter.md) |
| `ChoosePokemon` | Whoever the effect belongs to | a Benched Pokémon | [interpreter](effects/interpreter.md) |
| `ChooseYesNo` | Whoever the effect belongs to | yes / no | [interpreter](effects/interpreter.md) |
| `ChooseAttack` | Whoever the effect belongs to | a Benched Pokémon's attack to copy | [interpreter](effects/interpreter.md) |
| `CustomChoice` | Whoever the effect belongs to | a card and a Pokémon in play (Rare Candy, Transformation Tome) | [custom](effects/custom.md) |
| `DiscardBenched` | A player over their Bench limit | a Benched Pokémon to discard | [custom](effects/custom.md#the-bench-limit) |

A decision can belong to the player whose turn it *isn't*. After a Knock Out, the opponent
chooses their new Active.

### Rules are questions, not field reads

HP, Retreat Cost, attack costs, Energy provided, Weakness, Resistance, Prize cards, Bench size and
the turn limits are all asked through [`rules`](rules/rules.md) (`rules::max_hp(game, pokemon)` and
so on), never read straight off a card. Cards that bend a rule will change the answer there.

### Duplicate choices are merged

Three Dreepy in hand give **one** "put Dreepy on the Bench" action, not three, because the copies
are interchangeable. The same goes for attaching identical Energy and discarding identical Energy.
This keeps the number of choices search has to consider small.

### Where the turn is: `TurnPhase`

Some events need choices in the middle, for example "your Active was Knocked Out, pick a new one".
`GameState::phase` records where the turn is, so the engine knows how to carry on once those
choices are made:

```
Main ──(Attack or EndTurn)──► AfterMain ──► Checkup ──► begin_turn(opponent) ──► Main
                                 │              │
                     Knock Outs → promote   Knock Outs → promote
```

[`continue_game`](actions/turn.md#continue_game) runs this forward whenever no decision is pending.

### Slots

A `Slot` names a place in play: **0 is the Active Spot, 1–8 are Bench positions** (6–8 only with Area Zero Underdepths).
`detail::pokemon_at(player, slot)` and `detail::in_play_slots(player)` work with them.

### Randomness is seeded

Every random event (shuffles, coin flips, which Prize card is taken) uses the [`Rng`](shared/rng.md)
inside the game state. **The same seed and the same choices always give the same game**, so any
bug can be replayed exactly.

## Files

| File | Doc | Contents |
|---|---|---|
| `shared/fixed_vector.hpp` | [fixed_vector.md](shared/fixed_vector.md) | Fixed-capacity list with no heap use |
| `shared/rng.hpp` | [rng.md](shared/rng.md) | Seeded random number generator |
| `game/game_state.hpp/.cpp` | [game_state.md](game/game_state.md) | `CardTable`, `PokemonInPlay`, `PlayerState`, `GameState`, decisions |
| `actions/action.hpp` | [action.md](actions/action.md) | `Action`, `ActionKind`, slots |
| `engine.hpp/.cpp`, `internal.hpp` | [engine.md](engine.md) | Public interface, dispatch, shared helpers |
| `rules/rules.hpp/.cpp` | [rules.md](rules/rules.md) | Every rule a card can change, asked as a question |
| `events/events.hpp/.cpp` | [events.md](events/events.md) | Moments cards react to, and the event hook |
| `effects/interpreter.cpp` | [interpreter.md](effects/interpreter.md) | Runs effect programs, pausing for choices |
| `setup/setup.cpp` | [setup.md](setup/setup.md) | `new_game`, mulligans, setup decisions |
| `actions/turn.cpp` | [turn.md](actions/turn.md) | Starting and ending turns, `continue_game` |
| `actions/main_actions.cpp` | [main_actions.md](actions/main_actions.md) | Bench, evolve, attach, retreat |
| `actions/combat.cpp` | [combat.md](actions/combat.md) | Attacks, damage, Knock Outs, Prize cards, winning |
| `actions/conditions.cpp` | [conditions.md](actions/conditions.md) | Special Conditions and what Pokémon Checkup does with them |
| `actions/lasting.cpp` | [lasting.md](actions/lasting.md) | Effects that last past the card that made them |
| (in `actions/main_actions.cpp`) | [abilities.md](actions/abilities.md) | Abilities you choose to use, and Stadium actions |
| `effects/custom.cpp`, `actions/bench_limit.cpp` | [custom.md](effects/custom.md) | One-of-a-kind cards in C++, the Bench limit |
| `effects/triggers.cpp` | [abilities.md](actions/abilities.md#triggered-effects) | Triggered effects |
| `invariants/invariants.hpp/.cpp` | [invariants.md](invariants/invariants.md) | Consistency checker |
| `describe/describe.hpp/.cpp` | [describe.md](describe/describe.md) | Text descriptions of states and actions |

## Known simplifications

- **Setup is sequential, not face down.** Player 1 places Pokémon after player 0. Hiding what player 1
  shouldn't see is the observation layer's job (Milestone 4).
- **After a mulligan bonus draw, any Basic in hand may be benched**, not only the ones just drawn.
- **Damage modifiers are ignored:** "20×" and "30+" attacks deal their printed number.
- **Damage formulas count Energy cards**, not units ("for each Energy attached"). Only Neo Upper on a
  Stage 2 and Ignition on an Evolution Pokémon differ.
- **`apply` re-checks legality** by building the legal action list each time. That catches agent
  bugs at once, but costs speed; it can become debug-only later.
