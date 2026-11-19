#include "agents/planner_agent.hpp"

#include <algorithm>

#include "agents/and_tree.hpp"
#include "agents/or_tree.hpp"
#include "agents/plan.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

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
        const OrTreeResult win = or_tree_search(state, me, Goal{GoalKind::Win, 1}, or_options);
        if (win.found) {
            plan = win.plan;
            ++stats_.wins;
        } else {
            PlanScorer scorer(me, rng_.next(), options_.samples, options_.weights);
            AndTreeOptions and_options;
            and_options.budget = options_.and_budget;
            const AndTreeResult best = and_tree_search(state, me, scorer, and_options);
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
        return greedy_.choose(observation, actions);
    }
    
} // namespace pokebot