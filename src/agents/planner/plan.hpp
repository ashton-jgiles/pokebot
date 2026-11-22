#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "agents/planner/action_class.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/world.hpp"
#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct Plan {
        std::vector<Action> actions;
        ActionClass last = ActionClass::Final;
    };

    GameState apply_plan(const GameState& game, const Plan& plan);
    bool follows_up(const GameState& game, int player);
    void finish_choices(GameState& game, int player, const Weights& weights);

    class PlanScorer {
    public:
        PlanScorer(int player, std::uint64_t seed, int samples = 3, const Weights& weights = default_weights());

        double value(const GameState& game, const Action& action, const TriedAction& tried);
        double value(const GameState& game, const Plan& plan);
        int player() const { return player_; }
        const Weights& weights() const { return weights_; }
        double seconds() const { return seconds_; }
        long long uncertain() const { return uncertain_; }

    private:
        int player_;
        int samples_;
        Weights weights_;
        std::uint64_t seed_;
        std::uint64_t worlds_for_ = 0;
        std::vector<std::unique_ptr<World>> worlds_;
        double seconds_ = 0.0;
        long long uncertain_ = 0;
    };

} // namespace pokebot