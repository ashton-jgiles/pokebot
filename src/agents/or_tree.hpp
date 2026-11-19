#pragma once

#include <cstdint>

#include "agents/plan.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    enum class GoalKind : std::uint8_t {
        Win,
        KnockOut,
        Prizes,
    };

    struct Goal {
        GoalKind kind = GoalKind::Win;
        int prizes = 1;
    };

    struct OrTreeOptions {
        int budget = 2000;
        bool closed_set = true;
    };

    struct OrTreeResult {
        bool found = false;
        Plan plan;
        bool out_of_budget = false;
        int expanded = 0;
        int generated = 0;
        int closed = 0;
    };

    OrTreeResult or_tree_search(const GameState& game, int player, const Goal& goal, const OrTreeOptions& options = {});

    int knock_out_distance(const GameState& game, int player);

    int action_order(const GameState& game, const Action& action, bool enables_attack);

} // namespace pokebot
