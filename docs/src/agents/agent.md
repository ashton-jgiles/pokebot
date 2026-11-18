# `src/agents/agent.hpp` — the agent interface

**File:** [`src/agents/agent.hpp`](../../../src/agents/agent.hpp) (header-only) · **Namespace:** `pokebot`

Every player the engine can host, from the random baselines to the search-based AIs of
Milestones 5–9, implements this interface:

```cpp
class Agent {
public:
    virtual ~Agent() = default;
    virtual std::string name() const = 0;
    virtual Action choose(const Observation& observation, const std::vector<Action>& actions) = 0;
};
```

- `choose` gets the legal actions for `observation.decision` and returns one of them. The list is never empty.
- An agent is asked about **every** decision that belongs to its player, including setup, choosing a
  new Active, and which Energy to discard when retreating, not just the main phase.
- `name()` is a short label for reports, such as `"random"`.

## Agents see only their own view

An agent is never handed the `GameState` or the `CardTable`. It gets an
[`Observation`](../engine/observation/observation.md): what its player could see at the table. The
opponent's hand is a count plus any cards they were shown, your own deck and Prize cards are one
unordered list, and an opponent card you haven't seen has no definition. So an agent can't cheat,
even by accident.

The harness builds the observation with [`ask`](../harness/match.md#ask), for the player whose
decision it is. Anything that runs agents should go through `ask` too.

The actions the agent is offered name only cards its player knows the place of. A test plays 100
real-deck games and checks every one. That test is what found that a deck search didn't record that
you'd looked through the deck (see [knowledge](../engine/knowledge/knowledge.md#what-a-player-knows)).

## Implementations

| Agent | Doc |
|---|---|
| `RandomAgent`, `BiasedRandomAgent` | [random_agent.md](random_agent.md) |

Games between agents are played by [`play_game`](../harness/match.md).
