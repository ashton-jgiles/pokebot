#include "agents/planner/planner_agent.hpp"

#include <algorithm>
#include <chrono>

#include "agents/planner/and_tree.hpp"
#include "agents/planner/or_tree.hpp"
#include "agents/planner/plan.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        double seconds_since(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

    } // namespace

    PlannerAgent::PlannerAgent(std::uint64_t seed, const PlannerOptions& options) : options_(options), rng_(seed), greedy_(seed ^ 0x5DEECE66DULL, 3, options.weights) {}

    bool PlannerAgent::following(const Observation& observation, const std::vector<Action>& actions) const {
        return next_ < plan_.size() && observation == expected_[next_] && std::find(actions.begin(), actions.end(), plan_[next_]) != actions.end();
    }

    bool PlannerAgent::plan_turn(const Observation& observation) {
        const World world(observation, rng_);
        const GameState& state = world.game();
        const int me = observation.decision.player;

        Plan plan;
        OrTreeOptions or_options;
        or_options.budget = options_.or_budget;
        const auto or_start = std::chrono::steady_clock::now();
        const OrTreeResult win = or_tree_search(state, me, Goal{GoalKind::Win, 1}, or_options);
        stats_.or_seconds += seconds_since(or_start);
        stats_.or_nodes += win.expanded;
        if (win.found) {
            plan = win.plan;
            ++stats_.wins;
        } else {
            PlanScorer scorer(me, rng_.next(), options_.samples, options_.weights);
            AndTreeOptions and_options;
            and_options.budget = options_.and_budget;
            const auto and_start = std::chrono::steady_clock::now();
            const AndTreeResult best = and_tree_search(state, me, scorer, and_options);
            stats_.and_seconds += seconds_since(and_start);
            stats_.scoring_seconds += scorer.seconds();
            stats_.and_nodes += best.expanded;
            stats_.scored += best.scored;
            stats_.uncertain += scorer.uncertain();
            if (best.plans.empty()) {
                return false;
            }
            plan = best.plans.front().plan;
        }
        ++stats_.plans;

        plan_ = plan.actions;
        expected_.clear();
        next_ = 0;
        GameState walk = state;
        for (const Action& action : plan_) {
            expected_.push_back(observe(walk, me));
            apply_legal(walk, action);
        }
        return true;
    }

    Action PlannerAgent::choose(const Observation& observation, const std::vector<Action>& actions) {
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
        const DecisionKind kind = observation.decision.kind;
        if (observation.turn > 0 && (kind == DecisionKind::MainPhase || kind == DecisionKind::ExtraAttack) && plan_turn(observation)) {
            ++stats_.followed;
            return plan_[next_++];
        }
        ++stats_.greedy;
        const auto greedy_start = std::chrono::steady_clock::now();
        const Action action = greedy_.choose(observation, actions);
        stats_.greedy_seconds += seconds_since(greedy_start);
        return action;
    }

    std::vector<std::pair<std::string, double>> PlannerAgent::counters() const {
        return {
            {"plans", static_cast<double>(stats_.plans)},
            {"wins found", static_cast<double>(stats_.wins)},
            {"plan actions", static_cast<double>(stats_.followed)},
            {"greedy decisions", static_cast<double>(stats_.greedy)},
            {"or-tree nodes", static_cast<double>(stats_.or_nodes)},
            {"and-tree nodes", static_cast<double>(stats_.and_nodes)},
            {"plan endings scored", static_cast<double>(stats_.scored)},
            {"of them uncertain", static_cast<double>(stats_.uncertain)},
            {"or-tree s", stats_.or_seconds},
            {"and-tree s", stats_.and_seconds},
            {"of it scoring s", stats_.scoring_seconds},
            {"greedy s", stats_.greedy_seconds},
        };
    }

} // namespace pokebot