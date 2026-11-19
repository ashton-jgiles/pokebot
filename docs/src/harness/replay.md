# `src/harness/replay` — saving and playing back games

**Files:** [`replay.hpp`](../../../src/harness/replay.hpp), [`replay.cpp`](../../../src/harness/replay.cpp) · **Library:** `pokebot_lib` · **Namespace:** `pokebot`

A game is decided by its two decks, its seed and the actions taken: the seed fixes every shuffle and
coin flip, and the card numbering. A `Replay` stores exactly those, so the game can be played back
action for action, as either player saw it or with everything shown.

```cpp
struct ReplayPlayer {
    std::string deck_name; // where the deck came from, such as "dragapult.txt"
    std::string agent;     // who played it, such as "biased"
    Decklist deck;
};

struct Replay {
    std::uint64_t seed = 0;
    std::array<ReplayPlayer, kPlayers> players;
    std::vector<Action> actions;
    GameResult result = GameResult::Ongoing; // Ongoing if it was saved before the end
    std::string note;                        // why it was saved, such as the invariant that broke
};
```

## Functions

| Function | Does |
|---|---|
| `to_json(replay)`, `replay_from_json(json)` | Convert to and from JSON (`nlohmann::ordered_json`, so the keys stay in order) |
| `save_replay(replay, path)`, `load_replay(path)` | Write and read a file. Saving creates the folder if needed |
| `replay_table(db, replay)` | The card table the game was played on, numbered as [`play_game`](match.md) numbers it (`shuffled(~seed)`) |
| `play_back(table, replay, each)` | Plays every action again and returns the final state. `each(before, action, after)` is called for every action |
| `print_replay(out, table, replay, viewer, at_turn)` | Writes the board at the start of each turn and every action, as `viewer` saw them, or everything for `kEveryone`. `at_turn`, if given, is called before each turn, such as to wait for Enter |

Anything wrong throws `ReplayError` (a `std::runtime_error`):
- a file that isn't a replay or is from another version;
- a bad action or deck line;
- decks that can't be played;
- an action that isn't legal when it comes up, or a game that ends differently from the saved
  result. Either means the cards or the engine have changed since the replay was saved, and the
  message says which action failed.

## The file

```json
{
  "format": "pokebot replay",
  "version": 1,
  "seed": 7,
  "players": [
    {
      "deck_name": "basic-box.txt",
      "agent": "biased",
      "deck": ["4 Mega Kangaskhan ex MEG 104", "3 Meowth ex POR 62", "..."]
    },
    { "deck_name": "dragapult.txt", "agent": "biased", "deck": ["..."] }
  ],
  "result": "player 1 wins",
  "note": "",
  "actions": ["ChooseTurnOrder - 0 1", "SetupPlaceActive 54 0 0", "AttachEnergy 97 0 0", "..."]
}
```

- **Decks are written out in full**, as decklist lines, so a replay doesn't depend on the files in
  `data/decks` staying the same.
- **Each action is one line:** its kind, its card number (`-` for none), its target and its amount.
  Kinds are saved by name, so reordering `ActionKind` doesn't break old replays.
- **Card numbers** are the shuffled ones the game used. `replay_table` makes the same numbering again
  from the seed.

## Playing back as one player

`print_replay` with a viewer shows each turn's board as [`describe(observe(game, viewer))`](../engine/observation/observation.md),
and each action with [`describe(before, action, after, viewer)`](../engine/describe/describe.md#an-action-as-one-player-saw-it):
a card the viewer didn't know the place of before or after the action is "an unseen card", or
"a face-down Pokemon" during setup. So a replay shows a player only what they could see at the table.

## Where replays come from

- [`play_game`](match.md) records every action in `GameRecord::moves`. If an invariant breaks it
  throws `InvariantError`, which carries the seed and the actions so far.
- [`pokebot-sim`](../../tools/sim/main.md) saves a game with `--print --save-replay FILE`, saves one to
  `replays/invariant-seed-S.json` whenever an invariant breaks, and prints one with `--replay FILE
  [--as 0|1|all]`. The `replays/` folder is in `.gitignore`.
- [`pokebot`](../client/main.md) saves a game you play or watch with `--save-replay`, and plays one back
  with `pokebot replay FILE`.

## Tests

[`tests/test_replay.cpp`](../../../tests/test_replay.cpp), tag `[replay]`:

- a replay survives saving and loading, as JSON and as a file;
- playing back reaches exactly the same `GameState` (30 games), and games from `play_game` replay to
  the same ending (30 more);
- 50 real-deck games survive saving and replay to the same ending and turn, and 20 sample games
  replay to exactly the final `GameState` that `play_game` hands back;
- a replay with an illegal action says which action, and a wrong result or extra action is caught;
- files that aren't replays, from another version, or with bad actions, cards or players are rejected;
- the opponent doesn't see what a search that doesn't reveal found, but does when it reveals;
- across 30 random games, an action is described to each player with "an unseen card" or "a
  face-down Pokemon" exactly when they don't know the card;
- `print_replay` with everything shown names every card, and each player's view hides the opponent's
  setup.
