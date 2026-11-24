# `src/harness/self_play` — games saved for learning, and their positions

**Files:** [`src/harness/self_play.hpp`](../../../src/harness/self_play.hpp), [`src/harness/self_play.cpp`](../../../src/harness/self_play.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

M10 fits the [evaluation](../agents/evaluation/evaluation.md)'s weights to the results of self-play
games. The games are saved whole, as [replays](replay.md), and the positions and their features are
worked out by playing them back, so a new feature needs no new games, and playing back checks each
game against its saved result.

```cpp
save_games("replays/self-play/one.jsonl", result.replays);        // one replay a line
const std::vector<Replay> games = load_games("replays/self-play/one.jsonl");
for (const Position& p : positions(db, games[0])) {
    p.features;   // phi, for the player to move
    p.result;     // 1, 0.5 or 0, for the player to move
}
```

`save_games` refuses a file that exists, so a long run can't be overwritten. `load_games` names the
line of a game that can't be read.

## Positions

`positions(db, replay, game)` gives, for each turn:

- **its start:** the first main-phase decision, after the turn's draw;
- **one later main-phase decision** of the same turn (`start` false), if there is one, picked evenly
  at random from a generator seeded by the game's seed, so it's the same each time. e also scores
  states mid-turn, such as after a draw.

The state after a turn ends is the next turn's start, so it isn't recorded again. Each `Position` has
the game's index, turn, the player to move, whether they went first, both prize counts, the features
and the game's result, all from the view of the player to move; the opponent's features are exactly
the negatives. `action` is the index in the replay's actions of the decision it was taken at. A game
without a result gives none.

**Split by seed, never by game or position.** A game's positions share one result. And the arena plays
each deal twice: pairing (i, j) in one seat and (j, i) in the other have the same seed and the same
deck in each seat, so N games hold N/2 different deals. The [evaluation lab](evaluation_lab.md) does this.

## In the sim

`--agents A,B --save-games FILE` saves every game of a match, with each side's weights in its note.
`--check-games FILE` plays every game back and prints the seeds, the number of different deals, the
score of the player who went first, and the positions by kind and by the smaller prize count. The
arena keeps every game in memory and saves at the end, so a long run is best made in parts.

## Tests

[`tests/test_self_play.cpp`](../../../tests/test_self_play.cpp), tag `[self_play]`:

- games survive saving and loading, and a file isn't overwritten; a bad line and a missing file are
  rejected;
- each turn gives its first main phase and at most one later, with that moment's features;
- a position's result is the game's for the player to move, and an unfinished game gives none;
- the later decision is picked evenly, and the same each time;
- on the real decks, features in range and positions for both players.
