# `src/agents/agent.hpp` — the agent interface

**File:** [`src/agents/agent.hpp`](../../../src/agents/agent.hpp) (header-only) · **Namespace:** `pokebot`

Every player the engine can host, from the random baselines to the search-based AIs of
Milestones 5–9, implements this interface:

```cpp
class Agent {
public:
    virtual ~Agent() = default;
    virtual std::string name() const = 0;
    virtual Action choose(const GameState& game, const std::vector<Action>& actions) = 0;
};
```

- `choose` gets the legal actions for `game.decision()` and returns one of them. The list is never empty.
- An agent is asked about **every** decision that belongs to its player, including setup, choosing a
  new Active, and which Energy to discard when retreating, not just the main phase.
- `name()` is a short label for reports, such as `"random"`.

## Agents see everything (for now)

Until the observation layer in Milestone 4, agents are handed the whole `GameState`, including
the opponent's hand and both decks' order. The random agents ignore it. A search agent built before
Milestone 4 could cheat without meaning to, which is why the observation layer comes before the
first real AI.

## Implementations

| Agent | Doc |
|---|---|
| `RandomAgent`, `BiasedRandomAgent` | [random_agent.md](random_agent.md) |

Games between agents are played by [`play_game`](../harness/match.md).
