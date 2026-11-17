# `src/engine/setup/setup.cpp` — starting a game

**File:** [`src/engine/setup/setup.cpp`](../../../../src/engine/setup/setup.cpp) · **Namespace:** `pokebot`

`new_game` and the setup decisions, following the rulebook (pp. 8, 18).

## Flow

```
new_game(seed)                                       automatic
  ├─ build both decks in card-number order, shuffle, draw 7 each
  ├─ mulligans: anyone without a Basic reshuffles and redraws (counted per player)
  └─ coin flip
decisions, in order:
  1. ChooseTurnOrder   coin-flip winner: go first or second
  2. SetupActive       player 0 picks an Active Pokémon
  3. SetupBench        player 0 benches Basics one at a time, then "done"
  4. SetupActive       player 1
  5. SetupBench        player 1
  6. MulliganDraws     only if the opponent took extra mulligans
     └─ SetupBench     if a Basic was drawn, it may go on the Bench
finish                                               automatic
  ├─ 6 Prize cards from the top of each deck
  └─ turn 1 begins: the first player draws a card
```

The decisions are pushed onto `pending` in reverse, so the last one pushed is made first. When the
stack is empty, setup finishes.

## Mulligans

- A player whose opening hand has no Basic Pokémon shuffles it back and draws 7 again, as many
  times as needed. The deck validator guarantees at least one Basic, so this always ends.
- **The bonus is the difference.** A player may draw one extra card for each mulligan the opponent
  took *beyond their own*. When both players mulligan in the same round, both counts go up and the
  difference doesn't change.
- The bonus decision offers every amount from 0 to the bonus, capped at the cards left in the deck.

## Choices offered

- **SetupActive / SetupBench** offer each *distinct* Basic in hand once. SetupBench always offers
  "done", and offers Basics only while the Bench has room.
- **ChooseTurnOrder** offers go first and go second.

## Simplifications

- Setup is sequential, so player 1 decides after seeing player 0's choices. Hiding that is the
  observation layer's job (Milestone 4).
- After a bonus draw, any Basic in hand may be benched, not only the newly drawn ones.

## Tests

[`tests/test_engine_setup.cpp`](../../../../tests/test_engine_setup.cpp), tag `[setup]`:
- opening hands, determinism, choosing to go second;
- 6 Prize cards and turn 1 starting, choices merged and the Bench limit;
- bonus draws and equal mulligans;
- 1,000 random setups on the real decks;
- illegal actions rejected.
