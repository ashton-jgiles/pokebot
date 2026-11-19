#include "agents/greedy_agent.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "agents/plan.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        constexpr double kTie = 1e-9;

        int setup_preferences(const Observation& observation, const Action& action) {
            switch (action.kind) {
                case ActionKind::ChooseTurnOrder:
                    return action.amount == 0 ? 1 : 0;
                case ActionKind::SetupPlaceActive:
                case ActionKind::SetupPlaceBench:
                    return observation.def(action.card)->hp;
                case ActionKind::DrawMulliganCards:
                    return action.amount;
                default:
                    return -1;
            }
        }

    } // namespace

    GreedyAgent::GreedyAgent(std::uint64_t seed, int worlds, const Weights& weights) : rng_(seed), worlds_(std::max(worlds, 1)), weights_(weights) {}

    std::vector<double> GreedyAgent::scores(const Observation& observation, const std::vector<Action>& actions) {
        std::vector<std::unique_ptr<World>> worlds;
        for (int i = 0; i < worlds_; ++i) {
            worlds.push_back(std::make_unique<World>(observation, rng_));
        }
        const int me = observation.decision.player;
        std::vector<double> total(actions.size(), 0.0);
        for (const auto& world : worlds) {
            for (std::size_t i = 0; i < actions.size(); ++i) {
                GameState next = world->game();
                apply_legal(next, actions[i]);
                finish_choices(next, me, weights_);
                total[i] += evaluate(next, me, weights_);
            }
        }
        for (double& score : total) {
            score /= static_cast<double>(worlds_);
        }
        return total;
    }

    Action GreedyAgent::choose(const Observation& observation, const std::vector<Action>& actions) {
        if (actions.size() == 1) {
            return actions.front();
        }
        if (observation.turn == 0) {
            const auto better = [&](const Action& a, const Action& b) {
                return setup_preferences(observation, a) < setup_preferences(observation, b);
            };
            return *std::max_element(actions.begin(), actions.end(), better);
        }
        const std::vector<double> score = scores(observation, actions);
        const double best = *std::max_element(score.begin(), score.end());
        std::vector<std::size_t> tied;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            if (score[i] >= best - kTie) {
                tied.push_back(i);
            }
        }
        return actions[tied[rng_.below(static_cast<std::uint32_t>(tied.size()))]];
    }

} // namespace pokebot