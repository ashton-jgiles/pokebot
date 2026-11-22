#include "agents/flat_mc/flat_mc_agent.hpp"

#include <algorithm>
#include <chrono>

#include "agents/planner/or_tree.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        double seconds_since(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

    } // namespace

    FlatMcAgent::FlatMcAgent(std::uint64_t seed, const DeckModel& model, const FlatMcOptions& options) : model_(model), options_(options), rng_(seed), greedy_(seed ^ 0x5DEECE66DULL, 3, options.weights) {}

    bool FlatMcAgent::following(const Observation& observation, const std::vector<Action>& actions) const {
        return next_ < plan_.size() && observation == expected_[next_] && std::find(actions.begin(), actions.end(), plan_[next_]) != actions.end();
    }

    std::vector<ScoredPlan> FlatMcAgent::score(const Observation& observation, int me, const std::vector<RankedPlan>& plans) {
        std::vector<ScoredPlan> scored;
        for (const RankedPlan& ranked : plans) {
            scored.push_back(ScoredPlan{ranked.plan, ranked.value, ranked.value, 0});
        }
        if (scored.size() < 2) {
            return scored;
        }
        const auto start = std::chrono::steady_clock::now();
        ++stats_.searched;
        const int each = std::max(1, options_.playouts / static_cast<int>(scored.size()));
        for (ScoredPlan& candidate : scored) {
            candidate.mean = 0.0;
        }
        for (int i = 0; i < each; ++i) {
            Rng deal(rng_.next());
            const World sample(observation, deal, &model_);
            const std::uint64_t seed = rng_.next();
            for (ScoredPlan& candidate : scored) {
                const PlayoutResult result = play_out(apply_plan(sample.game(), candidate.plan), me, seed, options_.playout);
                candidate.mean += result.value;
                ++candidate.playouts;
                ++stats_.playouts;
                stats_.playout_actions += result.actions;
            }
        }
        for (ScoredPlan& candidate : scored) {
            candidate.mean /= static_cast<double>(candidate.playouts);
        }
        std::stable_sort(scored.begin(), scored.end(), [](const ScoredPlan& a, const ScoredPlan& b) { return a.mean > b.mean; });
        stats_.playout_seconds += seconds_since(start);
        return scored;
    }

    bool FlatMcAgent::plan_turn(const Observation& observation) {
        const World world(observation, rng_, &model_);
        const GameState& state = world.game();
        const int me = observation.decision.player;

        OrTreeOptions or_options;
        or_options.budget = options_.or_budget;
        const auto or_start = std::chrono::steady_clock::now();
        const OrTreeResult win = or_tree_search(state, me, Goal{GoalKind::Win, 1}, or_options);
        stats_.or_seconds += seconds_since(or_start);
        stats_.or_nodes += win.expanded;
        if (win.found) {
            ++stats_.wins;
            analysis_ = {ScoredPlan{win.plan, 1.0, 1.0, 0}};
        } else {
            PlanScorer scorer(me, rng_.next(), options_.samples, options_.weights);
            AndTreeOptions and_options;
            and_options.budget = options_.and_budget;
            and_options.keep = options_.plans;
            const auto and_start = std::chrono::steady_clock::now();
            const AndTreeResult best = and_tree_search(state, me, scorer, and_options);
            stats_.and_seconds += seconds_since(and_start);
            stats_.and_nodes += best.expanded;
            if (best.plans.empty()) {
                analysis_.clear();
                return false;
            }
            analysis_ = score(observation, me, best.plans);
        }
        ++stats_.plans;

        plan_ = analysis_.front().plan.actions;
        expected_.clear();
        next_ = 0;
        GameState walk = state;
        for (const Action& action : plan_) {
            expected_.push_back(observe(walk, me));
            apply_legal(walk, action);
        }
        return true;
    }

    Action FlatMcAgent::choose(const Observation& observation, const std::vector<Action>& actions) {
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

    std::vector<std::pair<std::string, double>> FlatMcAgent::counters() const {
        return {
            {"plans", static_cast<double>(stats_.plans)},
            {"wins found", static_cast<double>(stats_.wins)},
            {"decisions played out", static_cast<double>(stats_.searched)},
            {"playouts", static_cast<double>(stats_.playouts)},
            {"playout actions", static_cast<double>(stats_.playout_actions)},
            {"plan actions", static_cast<double>(stats_.followed)},
            {"greedy decisions", static_cast<double>(stats_.greedy)},
            {"or-tree nodes", static_cast<double>(stats_.or_nodes)},
            {"and-tree nodes", static_cast<double>(stats_.and_nodes)},
            {"or-tree s", stats_.or_seconds},
            {"and-tree s", stats_.and_seconds},
            {"playout s", stats_.playout_seconds},
            {"greedy s", stats_.greedy_seconds},
        };
    }

} // namespace pokebot