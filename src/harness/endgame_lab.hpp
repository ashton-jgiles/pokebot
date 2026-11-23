#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "agents/flat_mc/deck_model.hpp"
#include "cards/db/card_db.hpp"
#include "harness/arena.hpp"

namespace pokebot {

    struct EndgameLabOptions {
        int games = 16;
        std::uint64_t seed = 1;
        int every = 1;
        int tree_prizes = 2;
        int and_budget = 300;
        int plans = 16;
        int opponent_budget = 25;
        int opponent_plans = 4;
        int samples = 1;
        double list_threshold = 0.001;
        const DeckModel* model = nullptr;
        std::function<void(long long, long long)> on_progress;
    };

    struct EndgameTree {
        long long plans = 0;
        long long unplayable = 0;
        long long random = 0;
        long long mid_turn = 0;
        std::array<long long, 3> mid_turn_draws{};
        long long chance = 0;
        long long opponent_searches = 0;
        long long opponent_plans = 0;
        long long leaves = 0;
        long long greedy_steps = 0;
        double value = 0.0;
        double seconds = 0.0;
    };

    struct EndgamePosition {
        int min_prizes = 0;
        double own_worlds = 0.0;
        double opponent_worlds = 0.0;
        int hidden_hand = 0;
        int own_deck_cards = 0;
        int opponent_deck_cards = 0;
        int lists = 0;
        bool measured = false;
        EndgameTree tree;
    };

    struct EndgameLabResult {
        long long games = 0;
        std::array<long long, 7> lowest_prizes{};
        std::vector<EndgamePosition> positions;
        double seconds = 0.0;
    };

    struct EndgameSummary {
        long long positions = 0;
        double median_own = 0.0;
        double median_opponent = 0.0;
        double median_total = 0.0;
        std::array<double, 4> total_at_most{};
        double median_hidden_hand = 0.0;
        double hidden_hand_at_most_2 = 0.0;
        double median_own_deck_cards = 0.0;
        double median_opponent_deck_cards = 0.0;
        double median_lists = 0.0;
        long long trees = 0;
        EndgameTree total;
        double median_seconds = 0.0;
        double p90_seconds = 0.0;
    };

    double count_placements(const std::vector<int>& copies, int first, int second = 0);
    EndgameLabResult run_endgame_lab(const CardDB& db, const std::vector<ArenaDeck>& decks, const EndgameLabOptions& options = {});
    EndgameSummary summarize(const EndgameLabResult& result, int min_prizes);

} // namespace pokebot
