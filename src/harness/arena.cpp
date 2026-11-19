#include "harness/arena.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace pokebot {

    namespace {

        struct GameSlot {
            std::size_t pairing = 0;
            std::uint64_t seed = 0;
            int a_seat = 0;
        };

        GameSlot slot_of(long long game, long long per_pairing, std::uint64_t first_seed) {
            const auto g = static_cast<std::uint64_t>(game);
            const auto per = static_cast<std::uint64_t>(per_pairing);
            const std::uint64_t within = g % per;
            return GameSlot{static_cast<std::size_t>(g / per), first_seed + within / 2, static_cast<int>(within % 2)};
        }

    } // namespace

    double Tally::score() const {
        const long long n = games();
        return n == 0 ? 0.0 : (static_cast<double>(wins) + 0.5 * static_cast<double>(draws)) / static_cast<double>(n);
    }

    void Tally::add(GameResult result, int a_seat) {
        if (result == GameResult::Draw || result == GameResult::Ongoing) {
            ++draws;
        } else if ((result == GameResult::Player0Wins) == (a_seat == 0)) {
            ++wins;
        } else {
            ++losses;
        }
    }

    Interval wilson_interval(double successes, long long trials, double z) {
        if (trials <= 0) {
            return Interval{};
        }
        const double n = static_cast<double>(trials);
        const double p = successes / n;
        const double z2 = z * z;
        const double centre = (p + z2 / (2.0 * n)) / (1.0 + z2 / n);
        const double half = z * std::sqrt(p * (1.0 - p) / n + z2 / (4.0 * n * n)) / (1.0 + z2 / n);
        return Interval{std::max(0.0, centre - half), std::min(1.0, centre + half)};
    }

    Interval score_interval(const Tally& tally) {
        return wilson_interval(static_cast<double>(tally.wins) + 0.5 * static_cast<double>(tally.draws), tally.games());
    }

    ArenaResult run_arena(const CardDB& db, const std::vector<ArenaDeck>& decks, const Contestant& a, const Contestant& b, const ArenaOptions& options) {
        const std::size_t n = decks.size();
        const long long per_pairing = std::max(2, options.games + options.games % 2);
        const long long total = static_cast<long long>(n * n) * per_pairing;

        std::vector<CardTable> tables;
        tables.reserve(2 * n *n);
        for (std::size_t pairing = 0; pairing < n * n; ++pairing) {
            const Decklist& deck_a = decks[pairing / n].list;
            const Decklist& deck_b = decks[pairing % n].list;
            tables.emplace_back(db, deck_a, deck_b);
            tables.emplace_back(db, deck_b, deck_a);
        }

        const auto replay_of = [&](const GameSlot& slot) {
            const ArenaDeck& deck_a = decks[slot.pairing / n];
            const ArenaDeck& deck_b = decks[slot.pairing % n];
            const ReplayPlayer player_a{deck_a.name, a.name, deck_a.list};
            const ReplayPlayer player_b{deck_b.name, b.name, deck_b.list};
            Replay replay;
            replay.seed = slot.seed;
            replay.players[0] = slot.a_seat == 0 ? player_a : player_b;
            replay.players[1] = slot.a_seat == 0 ? player_b : player_a;
            return replay;
        };

        const auto play = [&](long long game) {
            const GameSlot slot = slot_of(game, per_pairing, options.seed);
            const std::uint64_t agent_seed = slot.seed * 4 + static_cast<std::uint64_t>(slot.a_seat) * 2;
            const auto agent_a = a.make(agent_seed);
            const auto agent_b = b.make(agent_seed + 1);
            Agent& player0 = slot.a_seat == 0 ? *agent_a : *agent_b;
            Agent& player1 = slot.a_seat == 0 ? *agent_b : *agent_a;
            MatchOptions match;
            match.check_invariants = options.check_invariants;
            try {
                GameRecord record = play_game(tables[2 * slot.pairing + static_cast<std::size_t>(slot.a_seat)], slot.seed, player0, player1, match);
                if (options.replay_every <= 0 || game % options.replay_every != 0) {
                    record.moves = {};
                }
                return record;
            } catch (const InvariantError& e) {
                Replay replay = replay_of(slot);
                replay.actions = e.moves;
                replay.note = e.what();
                throw ArenaError(e.what(), std::move(replay));
            }
        };

        std::vector<GameRecord> records(static_cast<std::size_t>(total));
        std::atomic<long long> next{0};
        std::atomic<bool> stop{false};
        std::mutex failure_lock;
        std::exception_ptr failure;
        const auto work = [&] {
            while (!stop) {
                const long long game = next++;
                if (game >= total) {
                    return;
                }
                try {
                    records[static_cast<std::size_t>(game)] = play(game);
                } catch (...) {
                    const std::lock_guard<std::mutex> lock(failure_lock);
                    if (!failure) {
                        failure = std::current_exception();
                    }
                    stop = true;
                }
            }
        };

        const int hardware = static_cast<int>(std::thread::hardware_concurrency());
        const int wanted = options.threads > 0 ? options.threads : std::max(hardware, 1);
        const int threads = static_cast<int>(std::min<long long>(wanted, total));
        const auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> pool;
        for (int i = 0; i < threads; ++i) {
            pool.emplace_back(work);
        }
        for (std::thread& thread : pool) {
            thread.join();
        }
        if (failure) {
            std::rethrow_exception(failure);
        }

        ArenaResult result;
        result.threads = threads;
        result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        for (std::size_t pairing = 0; pairing < n * n; ++pairing) {
            result.pairings.push_back(PairingResult{pairing / n, pairing % n, Tally{}});
        }
        for (long long game = 0; game < total; game++) {
            const GameRecord& record = records[static_cast<std::size_t>(game)];
            const GameSlot slot = slot_of(game, per_pairing, options.seed);
            result.pairings[slot.pairing].tally.add(record.result, slot.a_seat);
            result.overall.add(record.result, slot.a_seat);
            (record.first_player == slot.a_seat ? result.a_first : result.b_first).add(record.result, slot.a_seat);
            ++result.ends[static_cast<std::size_t>(record.end)];
            ++result.games;
            result.turns += record.turns;
            result.actions += record.actions;
            if (options.replay_every > 0 && game % options.replay_every == 0) {
                Replay replay = replay_of(slot);
                replay.actions = record.moves;
                replay.result = record.result;
                result.replays.push_back(std::move(replay));
            }
        }
        return result;
    }

} // namespace pokbot