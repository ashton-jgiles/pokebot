#include "agents/is_mcts/macro_actions.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "agents/is_mcts/info_set.hpp"
#include "agents/planner/and_tree.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    namespace {

        double seconds_since(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

        bool names_other_cards(const Plan& plan, int player) {
            return std::any_of(plan.actions.begin(), plan.actions.end(), [&](const Action& action) {
                return action.card != kNoCard && static_cast<int>(action.card / kDeckSize) != player;
            });
        }

        bool legal(const GameState& world, const Plan& plan) {
            GameState walk = world;
            for (const Action& action : plan.actions) {
                const std::vector<Action> actions = legal_actions(walk);
                if (std::find(actions.begin(), actions.end(), action) == actions.end()) {
                    return false;
                }
                apply_legal(walk, action);
            }
            return true;
        }

    } // namespace

    MacroActions::MacroActions(int searcher, std::uint64_t seed, const MacroOptions& options) : searcher_(searcher), seed_(seed), options_(options) {}

    std::vector<Plan> MacroActions::plans(const GameState& world, int actor, int budget, int keep) {
        PlanScorer scorer(actor, seed_, options_.samples, options_.weights);
        AndTreeOptions and_options;
        and_options.budget = budget;
        and_options.keep = keep;
        std::vector<Plan> found;
        for (const RankedPlan& ranked : and_tree_search(world, actor, scorer, and_options).plans) {
            found.push_back(ranked.plan);
        }
        return found;
    }

    const std::vector<Plan>& MacroActions::moves(const GameState& world) {
        scratch_.clear();
        if (world.is_over()) {
            return scratch_;
        }
        const Decision& decision = world.decision();
        const int actor = decision.player;
        const bool planning = world.turn > 0 && (decision.kind == DecisionKind::MainPhase || decision.kind == DecisionKind::ExtraAttack);
        if (planning && actor == searcher_) {
            const std::uint64_t key = hash_observation(observe(world, searcher_), searcher_);
            const auto it = cache_.find(key);
            if (it != cache_.end()) {
                ++stats_.cache_hits;
                const std::vector<Plan>& cached = it->second;
                const auto other = [&](const Plan& plan) {
                    return names_other_cards(plan, searcher_);
                };
                if (std::none_of(cached.begin(), cached.end(), other)) {
                    return cached;
                }
                for (const Plan& plan : cached) {
                    if (!other(plan) || legal(world, plan)) {
                        scratch_.push_back(plan);
                    }
                }
                if (!scratch_.empty()) {
                    return scratch_;
                }
            }
            const auto start = std::chrono::steady_clock::now();
            std::vector<Plan> found = plans(world, actor, options_.searcher_budget, options_.searcher_plans);
            ++stats_.searcher_searches;
            stats_.searcher_seconds += seconds_since(start);
            if (!found.empty()) {
                if (it != cache_.end()) {
                    scratch_ = std::move(found);
                    return scratch_;
                }
                return cache_.emplace(key, std::move(found)).first->second;
            }
        } else if (planning) {
            const auto start = std::chrono::steady_clock::now();
            scratch_ = plans(world, actor, options_.opponent_budget, options_.opponent_plans);
            ++stats_.opponent_searches;
            stats_.opponent_seconds += seconds_since(start);
            if (!scratch_.empty()) {
                return scratch_;
            }
        }
        ++stats_.single_actions;
        for (const Action& action : legal_actions(world)) {
            Plan single;
            single.actions.push_back(action);
            scratch_.push_back(single);
        }
        return scratch_;
    }

} // namespace pokebot
