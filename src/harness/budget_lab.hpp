#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "cards/db/card_db.hpp"
#include "harness/arena.hpp"

namespace pokebot {

    struct BudgetLabOptions {
        int games = 16;
        std::uint64_t seed = 1;
        int every = 5;
        int reference = 300;
        std::vector<int> budgets{25, 50, 100, 150};
        int plans = 4;
        int samples = 1;
        std::function<void(long long, long long)> on_progress;
    };

    struct BudgetRow {
        int budget = 0;
        long long searches = 0;
        double seconds = 0.0;
        long long same_best = 0;
        double gap = 0.0;
        long long plans = 0;
    };

    struct BudgetLabResult {
        long long games = 0;
        long long positions = 0;
        BudgetRow reference;
        std::vector<BudgetRow> rows;
        double seconds = 0.0;
    };

    BudgetLabResult run_budget_lab(const CardDB& db, const std::vector<ArenaDeck>& decks, const BudgetLabOptions& options = {});

} // namespace pokebot
