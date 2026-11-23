#include "agents/endgame/endgame_agent.hpp"

#include <algorithm>

#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    EndgameAgent::EndgameAgent(std::uint64_t seed, const DeckModel& model, const EndgameAgentOptions& options)
        : model_(model), options_(options), rng_(seed ^ 0xC2B2AE3D27D4EB4FULL), is_mcts_(seed, model, options.is_mcts), solver_(&model, seed ^ 0x165667B19E3779F9ULL, options.solver) {}

    bool EndgameAgent::endgame(const Observation& observation, const std::vector<Action>& actions) const {
        const DecisionKind kind = observation.decision.kind;
        const int prizes = std::min(observation.player(0).prize_count, observation.player(1).prize_count);
        return observation.turn > 0 && actions.size() > 1 && prizes <= options_.prizes && (kind == DecisionKind::MainPhase || kind == DecisionKind::ExtraAttack);
    }

    bool EndgameAgent::following(const Observation& observation, const std::vector<Action>& actions) const {
        return next_ < plan_.size() && observation == expected_[next_] && std::find(actions.begin(), actions.end(), plan_[next_]) != actions.end();
    }

    bool EndgameAgent::follow(const Observation& observation, const std::vector<Action>& actions, const Plan& plan) {
        if (plan.actions.empty() || std::find(actions.begin(), actions.end(), plan.actions.front()) == actions.end()) {
            return false;
        }
        const World world(observation, rng_, &model_);
        GameState walk = world.game();
        const int me = observation.decision.player;
        for (const Action& action : plan.actions) {
            if (walk.is_over()) {
                break;
            }
            const std::vector<Action> legal = legal_actions(walk);
            if (std::find(legal.begin(), legal.end(), action) == legal.end()) {
                break;
            }
            plan_.push_back(action);
            expected_.push_back(observe(walk, me));
            apply_legal(walk, action);
        }
        if (plan_.empty()) {
            expected_.clear();
            return false;
        }
        expected_.front() = observation;
        return true;
    }

    Action EndgameAgent::choose(const Observation& observation, const std::vector<Action>& actions) {
        if (following(observation, actions)) {
            ++stats_.followed;
            return plan_[next_++];
        }
        plan_.clear();
        expected_.clear();
        next_ = 0;
        if (endgame(observation, actions) && !is_mcts_.following(observation, actions)) {
            ++stats_.endgame;
            analysis_ = solver_.solve(observation);
            if (!analysis_.plans.empty() && follow(observation, actions, analysis_.plans[analysis_.best].plan)) {
                ++stats_.played;
                ++stats_.followed;
                return plan_[next_++];
            }
            ++stats_.fallbacks;
        }
        return is_mcts_.choose(observation, actions);
    }

    std::vector<std::pair<std::string, double>> EndgameAgent::counters() const {
        const EndgameStats& s = solver_.stats();
        std::vector<std::pair<std::string, double>> out{
            {"endgame decisions", static_cast<double>(stats_.endgame)},
            {"solver plans played", static_cast<double>(stats_.played)},
            {"solver fallbacks", static_cast<double>(stats_.fallbacks)},
            {"solver plan actions", static_cast<double>(stats_.followed)},
            {"solves reaching depth 1", static_cast<double>(s.depths[1])},
            {"solves reaching depth 2", static_cast<double>(s.depths[2])},
            {"solver nodes", static_cast<double>(visited(s.search))},
            {"solver chance nodes", static_cast<double>(s.search.chance_nodes)},
            {"solver and-trees", static_cast<double>(s.search.and_trees)},
            {"solver alpha-beta cuts", static_cast<double>(s.search.alpha_beta_cuts)},
            {"solver Star1 cuts", static_cast<double>(s.search.star1_cuts)},
            {"solver plans dropped", static_cast<double>(s.dropped)},
            {"solver s", s.seconds},
        };
        for (const auto& counter : is_mcts_.counters()) {
            out.push_back(counter);
        }
        return out;
    }

} // namespace pokebot
