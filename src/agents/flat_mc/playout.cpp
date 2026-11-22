#include "agents/flat_mc/playout.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "agents/greedy/greedy_agent.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    const char* to_string(PlayoutPolicy policy) {
        switch (policy) {
            case PlayoutPolicy::Greedy:
                return "greedy";
            case PlayoutPolicy::Biased:
                return "biased";
        }
        return "?";
    }

    PlayoutPolicy playout_policy(std::string_view name) {
        if (name == "greedy") {
            return PlayoutPolicy::Greedy;
        }
        if (name == "biased") {
            return PlayoutPolicy::Biased;
        }
        throw std::invalid_argument("unknown playout policy '" + std::string(name) + "' (greedy or biased)");
    }

    PlayoutResult play_out(const GameState& game, int player, Agent& mine, Agent& theirs, const PlayoutOptions& options) {
        if (options.depth < 0) {
            throw std::invalid_argument("play_out: the depth can't be negative");
        }
        PlayoutResult result;
        result.end = game;
        GameState& state = result.end;
        const int start = state.turn;
        int counted = start;
        int reached = 0;
        while (!state.is_over() && reached < options.depth && result.actions < options.max_actions) {
            const Decision& decision = state.decision();
            const int who = decision.player;
            if (decision.kind == DecisionKind::MainPhase && who == player && state.turn != counted) {
                counted = state.turn;
                if (++reached == options.depth) {
                    break;
                }
            }
            const std::vector<Action> actions = legal_actions(state);
            Agent& agent = who == player ? mine : theirs;
            apply_legal(state, agent.choose(observe(state, who), actions));
            ++result.actions;
        }
        result.turns = state.turn - start;
        result.over = state.is_over();
        result.value = evaluate(state, player, options.weights);
        return result;
    }

    PlayoutResult play_out(const GameState& game, int player, std::uint64_t seed, const PlayoutOptions& options) {
        Rng rng(seed);
        const std::uint64_t mine_seed = rng.next();
        const std::uint64_t theirs_seed = rng.next();
        if (options.policy == PlayoutPolicy::Biased) {
            BiasedRandomAgent mine(mine_seed);
            BiasedRandomAgent theirs(theirs_seed);
            return play_out(game, player, mine, theirs, options);
        }
        GreedyAgent mine(mine_seed, options.greedy_worlds, options.weights);
        GreedyAgent theirs(theirs_seed, options.greedy_worlds, options.weights);
        return play_out(game, player, mine, theirs, options);
    }

} // namespace pokebot