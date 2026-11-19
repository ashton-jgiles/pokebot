#include "agents/plan.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "agents/state_hash.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    namespace {

        constexpr double kTie = 1e-9;
        constexpr int kMostFollowUps = 64;

    } // namespace

    GameState apply_plan(const GameState& game, const Plan& plan) {
        GameState after = game;
        for (const Action& action : plan.actions) {
            apply_legal(after, action);
        }
        return after;
    }

    bool follows_up(const GameState& game, int player) {
        if (game.is_over()) {
            return false;
        }
        const Decision& decision = game.decision();
        return decision.player == player && decision.kind != DecisionKind::MainPhase && decision.kind != DecisionKind::ExtraAttack;
    }

    void finish_choices(GameState& game, int player, const Weights& weights) {
        for (int step = 0; step < kMostFollowUps && follows_up(game, player); ++step) {
            const std::vector<Action> actions = legal_actions(game);
            std::size_t best = 0;
            double best_score = -2.0;
            for (std::size_t i = 0; i < actions.size(); ++i) {
                GameState next = game;
                apply_legal(next, actions[i]);
                const double score = evaluate(next, player, weights);
                if (score > best_score + kTie) {
                    best = i;
                    best_score = score;
                }
            }
            apply_legal(game, actions[best]);
        }
    }

    PlanScorer::PlanScorer(int player, std::uint64_t seed, int samples, const Weights& weights)
        : player_(player), samples_(std::max(samples, 1)), weights_(weights), rng_(seed) {}

    double PlanScorer::value(const GameState& game, const Action& action, const TriedAction& tried) {
        if (tried.kind != ActionClass::Uncertain) {
            GameState after = tried.after;
            finish_choices(after, player_, weights_);
            return evaluate(after, player_, weights_);
        }
        const std::uint64_t key = hash_state(game);
        if (worlds_.empty() || key != worlds_for_) {
            worlds_.clear();
            const Observation view = observe(game, player_);
            for (int i = 0; i < samples_; ++i) {
                worlds_.push_back(std::make_unique<World>(view, rng_));
            }
            worlds_for_ = key;
        }
        double total = 0.0;
        for (const auto& world : worlds_) {
            GameState after = world->game();
            apply_legal(after, action);
            finish_choices(after, player_, weights_);
            total += evaluate(after, player_, weights_);
        }
        return total / static_cast<double>(samples_);
    }

    double PlanScorer::value(const GameState& game, const Plan& plan) {
        if (plan.actions.empty()) {
            throw std::invalid_argument("PlanScorer::value: a plan has at least one action");
        }
        GameState before_last = game;
        for (std::size_t i = 0; i + 1 < plan.actions.size(); ++i) {
            apply_legal(before_last, plan.actions[i]);
        }
        const Action& last = plan.actions.back();
        return value(before_last, last, try_action(before_last, player_, last));
    }

} // namespace pokebot