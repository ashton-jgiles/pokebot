#include "agents/is_mcts/is_mcts_agent.hpp"

#include <algorithm>
#include <chrono>

#include "agents/planner/or_tree.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    namespace {

        double seconds_since(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

    } // namespace

    IsMctsAgent::IsMctsAgent(std::uint64_t seed, const DeckModel& model, const IsMctsAgentOptions& options)
        : model_(model), options_(options), rng_(seed), search_(model, seed ^ 0x9E3779B97F4A7C15ULL, options.search), greedy_(seed ^ 0x5DEECE66DULL, 3, options.weights) {}

    bool IsMctsAgent::following(const Observation& observation, const std::vector<Action>& actions) const {
        return next_ < plan_.size() && observation == expected_[next_] && std::find(actions.begin(), actions.end(), plan_[next_]) != actions.end();
    }

    Action IsMctsAgent::follow(const GameState& world, int me, const Plan& plan) {
        ++stats_.plans;
        plan_ = plan.actions;
        expected_.clear();
        next_ = 0;
        GameState walk = world;
        std::size_t playable = 0;
        for (const Action& action : plan_) {
            if (walk.is_over()) {
                break;
            }
            const std::vector<Action> actions = legal_actions(walk);
            if (std::find(actions.begin(), actions.end(), action) == actions.end()) {
                break;
            }
            expected_.push_back(observe(walk, me));
            apply_legal(walk, action);
            ++playable;
        }
        plan_.resize(std::max<std::size_t>(playable, 1));
        ++stats_.followed;
        return plan_[next_++];
    }

    Action IsMctsAgent::choose(const Observation& observation, const std::vector<Action>& actions) {
        if (following(observation, actions)) {
            ++stats_.followed;
            return plan_[next_++];
        }
        plan_.clear();
        expected_.clear();
        next_ = 0;
        if (actions.size() == 1) {
            return actions.front();
        }
        if (observation.turn == 0) {
            ++stats_.greedy;
            return greedy_.choose(observation, actions);
        }
        const World world(observation, rng_, &model_);
        const int me = observation.decision.player;
        const DecisionKind kind = observation.decision.kind;
        if (kind == DecisionKind::MainPhase || kind == DecisionKind::ExtraAttack) {
            OrTreeOptions or_options;
            or_options.budget = options_.or_budget;
            const auto start = std::chrono::steady_clock::now();
            const OrTreeResult win = or_tree_search(world.game(), me, Goal{GoalKind::Win, 1}, or_options);
            stats_.or_seconds += seconds_since(start);
            stats_.or_nodes += win.expanded;
            if (win.found) {
                ++stats_.wins;
                analysis_ = {RootChoice{win.plan, 0, 1.0}};
                return follow(world.game(), me, win.plan);
            }
        }
        analysis_ = search_.search(observation);
        if (analysis_.empty()) {
            ++stats_.greedy;
            return greedy_.choose(observation, actions);
        }
        return follow(world.game(), me, analysis_.front().plan);
    }

    std::vector<std::pair<std::string, double>> IsMctsAgent::counters() const {
        const IsMctsStats& s = search_.stats();
        return {
            {"plans", static_cast<double>(stats_.plans)},
            {"wins found", static_cast<double>(stats_.wins)},
            {"plan actions", static_cast<double>(stats_.followed)},
            {"greedy decisions", static_cast<double>(stats_.greedy)},
            {"searches", static_cast<double>(s.searches)},
            {"iterations", static_cast<double>(s.iterations)},
            {"tree nodes", static_cast<double>(s.nodes)},
            {"playout actions", static_cast<double>(s.playout_actions)},
            {"moves unplayable in a world", static_cast<double>(s.unplayable)},
            {"outcome nodes", static_cast<double>(s.outcomes)},
            {"matched iterations skipped", static_cast<double>(s.skipped)},
            {"searcher and-trees", static_cast<double>(s.macros.searcher_searches)},
            {"cache hits", static_cast<double>(s.macros.cache_hits)},
            {"opponent and-trees", static_cast<double>(s.macros.opponent_searches)},
            {"single-action nodes", static_cast<double>(s.macros.single_actions)},
            {"or-tree nodes", static_cast<double>(stats_.or_nodes)},
            {"or-tree s", stats_.or_seconds},
            {"search s", s.seconds},
            {"of it playouts s", s.playout_seconds},
            {"of it searcher and-trees s", s.macros.searcher_seconds},
            {"of it opponent and-trees s", s.macros.opponent_seconds},
        };
    }

} // namespace pokebot
