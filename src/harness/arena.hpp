#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "agents/agent.hpp"
#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "harness/match.hpp"
#include "harness/replay.hpp"

namespace pokebot {

    struct Contestant {
        std::string name;
        std::function<std::unique_ptr<Agent>(std::uint64_t seed)> make;
    };

    struct ArenaDeck {
        std::string name;
        Decklist list;
    };

    struct ArenaOptions {
        int games = 1000;
        std::uint64_t seed = 1;
        int threads = 0;
        bool check_invariants = false;
        int replay_every = 0;
    };

    struct Tally {
        long long wins = 0;
        long long losses = 0;
        long long draws = 0;

        long long games() const { return wins + losses + draws; }
        double score() const;
        void add(GameResult result, int a_seat);

        bool operator==(const Tally&) const = default;
    };

    struct Interval {
        double low = 0.0;
        double high = 1.0;
    };

    Interval wilson_interval(double successes, long long trials, double z = 1.96);
    Interval score_interval(const Tally& tally);

    struct PairingResult {
        std::size_t deck_a = 0;
        std::size_t deck_b = 0;
        Tally tally;
    };

    struct ArenaResult {
        Tally overall;
        Tally a_first;
        Tally b_first;
        std::vector<PairingResult> pairings;

        std::array<long long, 5> ends{};
        long long games = 0;
        long long turns = 0;
        long long actions = 0;
        int threads = 0;
        double seconds = 0.0;
        std::vector<Replay> replays;
    };

    class ArenaError : public std::runtime_error {
        public:
            ArenaError(const std::string& message, Replay game) : std::runtime_error(message), replay(std::move(game)) {}
            Replay replay;
    };

    ArenaResult run_arena(const CardDB& db, const std::vector<ArenaDeck>& decks, const Contestant& a, const Contestant& b, const ArenaOptions& options = {});

} // namespace pokebot