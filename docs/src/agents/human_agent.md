# `src/agents/human_agent` — a person at the keyboard

**Files:** [`human_agent.hpp`](../../../src/agents/human_agent.hpp), [`human_agent.cpp`](../../../src/agents/human_agent.cpp) · **Library:** `pokebot_lib` · **Namespace:** `pokebot`

```cpp
HumanAgent(std::istream& in, std::ostream& out);
```

An [agent](agent.md) whose choices are typed. Like any agent it gets only an
[`Observation`](../engine/observation/observation.md), so a person playing through it sees exactly
what an AI would. [`pokebot play`](../client/main.md) uses it with the console; the tests use string
streams.

At each decision it writes:
- **the board**, [`describe(observation)`](../engine/describe/describe.md#an-observation), unless this is
  a choice partway through an effect (cards for a search, a Pokémon for Boss's Orders, Energy to
  discard when retreating) in the same turn, where the board hasn't changed;
- **what's being decided**, [`describe_decision`](../engine/describe/describe.md#a-decision), such as
  `choose cards for an effect (Ultra Ball)`;
- **the choices**, numbered from 1, each as
  [`describe(observation, action)`](../engine/describe/describe.md#an-action-from-an-observation).

Then it reads a line:

| Typed | Does |
|---|---|
| A number from 1 to the number of choices | Takes that choice |
| `b` | Shows the board and the choices again |
| `q`, or the end of the input | Throws `QuitGame`, which stops `play_game` |
| Anything else | Says what it expects and asks again |

A decision with only one choice is taken without asking, and shown as `(the only choice)`.

## Tests

[`tests/test_client.cpp`](../../../tests/test_client.cpp), tag `[client]`: the typed number is taken,
anything else is asked again, `b` shows the board again, `q` and the end of the input quit, a single
choice isn't asked, a choice partway through an effect doesn't repeat the board, and a whole game
played through `play_game` finishes.
