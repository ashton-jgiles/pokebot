#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "cards/db/card_db.hpp"
#include "harness/arena.hpp"

namespace pokebot {

    struct PlannerLabOptions {
        int games = 16;
        std::uint64_t seed = 1;
        int every = 5;
        int or_budget = 200;
        int limit = 5000;
        int samples = 1;
        std::function<void(long long, long long)> on_progress;
    };

    struct PlannerLabResult {
        long long games = 0;
        long long positions = 0;
        long long finished = 0;
        long long wins = 0;
        long long wins_found = 0;
        long long false_wins = 0;
        long long win_nodes = 0;
        long long win_nodes_max = 0;
        long long or_closed = 0;
        long long or_open = 0;
        long long and_both = 0;
        long long and_closed = 0;
        long long and_bound = 0;
        long long and_neither = 0;
        long long pruned = 0;
        long long bound_changed = 0;
        double seconds = 0.0;
    };

    PlannerLabResult run_planner_lab(const CardDB& db, const std::vector<ArenaDeck>& decks, const PlannerLabOptions& options = {});

} // namespace pokebot
