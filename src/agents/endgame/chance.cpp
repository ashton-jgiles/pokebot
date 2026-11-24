#include "agents/endgame/chance.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>

#include "agents/is_mcts/info_set.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    namespace {

        struct Script {
            std::uint32_t heads = 0;
            int length = 0;
        };

        struct Arrangement {
            std::vector<CardIndex> top;
            double probability = 1.0;
        };

        bool located(const GameState& game, CardIndex card) {
            return knows_where(game, 0, card) || knows_where(game, 1, card);
        }

        GameState scripted(const GameState& game, const Action& action, const Script& script) {
            GameState after = game;
            after.rng.script_flips(script.heads, script.length);
            apply_legal(after, action);
            return after;
        }

        int touched(const GameState& before, const GameState& after, int player) {
            const auto& was = before.player(player).deck;
            const auto& now = after.player(player).deck;
            std::size_t kept = std::min(was.size(), now.size());
            while (kept > 0 && !std::equal(was.begin(), was.begin() + static_cast<std::ptrdiff_t>(kept), now.end() - static_cast<std::ptrdiff_t>(kept))) {
                --kept;
            }
            const auto stays = [&](CardIndex card) {
                return std::find(now.begin(), now.end(), card) != now.end();
            };
            if (std::any_of(now.begin(), now.end() - static_cast<std::ptrdiff_t>(kept), [&](CardIndex card) { return std::find(was.begin(), was.end(), card) != was.end(); }) ||
                std::any_of(was.begin() + static_cast<std::ptrdiff_t>(kept), was.end(), stays)) {
                return -1;
            }
            int depth = static_cast<int>(was.size() - kept);
            for (std::size_t i = 0; i < kept; ++i) {
                if (!located(before, was[i]) && located(after, was[i])) {
                    depth = std::max(depth, static_cast<int>(was.size() - i));
                }
            }
            return depth;
        }

        double choose(int n, int k) {
            if (k < 0 || k > n) {
                return 0.0;
            }
            double ways = 1.0;
            for (int i = 1; i <= k; ++i) {
                ways = ways * static_cast<double>(n - k + i) / static_cast<double>(i);
            }
            return ways;
        }

        void enumerate(const std::vector<std::vector<CardIndex>>& groups, const std::vector<int>& after, std::size_t g, int left, std::vector<CardIndex>& picked,
                       double ways, double total, std::vector<Arrangement>& out) {
            if (g == groups.size()) {
                if (left == 0) {
                    out.push_back(Arrangement{picked, ways / total});
                }
                return;
            }
            const int n = static_cast<int>(groups[g].size());
            const int least = std::max(0, left - after[g + 1]);
            for (int x = least; x <= std::min(n, left); ++x) {
                for (int i = 0; i < x; ++i) {
                    picked.push_back(groups[g][static_cast<std::size_t>(i)]);
                }
                enumerate(groups, after, g + 1, left - x, picked, ways * choose(n, x), total, out);
                picked.resize(picked.size() - static_cast<std::size_t>(x));
            }
        }

        std::vector<std::size_t> free_slots(const GameState& game, int player, int depth) {
            const auto& deck = game.player(player).deck;
            std::vector<std::size_t> slots;
            for (int d = 0; d < depth; ++d) {
                const std::size_t i = deck.size() - 1 - static_cast<std::size_t>(d);
                if (!located(game, deck[i])) {
                    slots.push_back(i);
                }
            }
            return slots;
        }

        std::vector<CardIndex> unlocated(const GameState& game, int player) {
            std::vector<CardIndex> pool;
            for (const CardIndex card : game.player(player).deck) {
                if (!located(game, card)) {
                    pool.push_back(card);
                }
            }
            return pool;
        }

        void shuffle(std::vector<CardIndex>& cards, Rng& rng) {
            for (std::size_t i = cards.size(); i > 1; --i) {
                std::swap(cards[i - 1], cards[rng.below(static_cast<std::uint32_t>(i))]);
            }
        }

        std::vector<Arrangement> arrangements(const GameState& game, int player, int depth, const ChanceOptions& options, Rng& rng, bool& sampled) {
            const std::size_t slots = free_slots(game, player, depth).size();
            if (slots == 0) {
                return {Arrangement{}};
            }
            const std::vector<CardIndex> pool = unlocated(game, player);
            std::vector<const CardDef*> defs;
            std::vector<std::vector<CardIndex>> groups;
            for (const CardIndex card : pool) {
                const auto it = std::find(defs.begin(), defs.end(), &game.def(card));
                if (it == defs.end()) {
                    defs.push_back(&game.def(card));
                    groups.push_back({card});
                } else {
                    groups[static_cast<std::size_t>(it - defs.begin())].push_back(card);
                }
            }
            std::vector<int> copies;
            for (const auto& group : groups) {
                copies.push_back(static_cast<int>(group.size()));
            }
            const int u = static_cast<int>(slots);
            std::vector<Arrangement> out;
            if (count_placements(copies, u) <= static_cast<double>(options.max_outcomes)) {
                std::vector<int> after(groups.size() + 1, 0);
                for (std::size_t g = groups.size(); g-- > 0;) {
                    after[g] = after[g + 1] + copies[g];
                }
                std::vector<CardIndex> picked;
                enumerate(groups, after, 0, u, picked, 1.0, choose(static_cast<int>(pool.size()), u), out);
                return out;
            }
            sampled = true;
            for (int s = 0; s < options.samples; ++s) {
                std::vector<CardIndex> order = pool;
                shuffle(order, rng);
                order.resize(slots);
                out.push_back(Arrangement{order, 1.0 / static_cast<double>(options.samples)});
            }
            return out;
        }

        void place(GameState& game, int player, int depth, const std::vector<CardIndex>& top) {
            const std::vector<std::size_t> slots = free_slots(game, player, depth);
            auto& deck = game.player(player).deck;
            for (std::size_t i = 0; i < top.size(); ++i) {
                const auto it = std::find(deck.begin(), deck.end(), top[i]);
                std::iter_swap(it, deck.begin() + static_cast<std::ptrdiff_t>(slots[i]));
            }
        }

        void scramble(GameState& game, Rng& rng) {
            for (int p = 0; p < kPlayers; ++p) {
                auto& deck = game.player(p).deck;
                std::vector<std::size_t> slots;
                std::vector<CardIndex> cards;
                for (std::size_t i = 0; i < deck.size(); ++i) {
                    if (!located(game, deck[i])) {
                        slots.push_back(i);
                        cards.push_back(deck[i]);
                    }
                }
                shuffle(cards, rng);
                for (std::size_t i = 0; i < slots.size(); ++i) {
                    deck[slots[i]] = cards[i];
                }
            }
            game.rng = Rng(rng.next());
        }

        class Collector {
        public:
            explicit Collector(std::vector<Outcome>& out) : out_(out) {}

            void add(GameState state, double probability) {
                state.rng.end_script();
                const std::pair<std::uint64_t, std::uint64_t> key{hash_observation(observe(state, 0)), hash_observation(observe(state, 1))};
                const auto [it, fresh] = index_.try_emplace(key, out_.size());
                if (fresh) {
                    out_.push_back(Outcome{std::move(state), probability});
                } else {
                    out_[it->second].probability += probability;
                }
            }

        private:
            std::vector<Outcome>& out_;
            std::map<std::pair<std::uint64_t, std::uint64_t>, std::size_t> index_;
        };

    } // namespace

    double count_placements(const std::vector<int>& copies, int first, int second) {
        if (first < 0 || second < 0) {
            return 0.0;
        }
        const auto rows = static_cast<std::size_t>(first) + 1;
        const auto cols = static_cast<std::size_t>(second) + 1;
        std::vector<double> ways(rows * cols, 0.0);
        ways[0] = 1.0;
        for (const int n : copies) {
            std::vector<double> next(ways.size(), 0.0);
            for (std::size_t a = 0; a < rows; ++a) {
                for (std::size_t b = 0; b < cols; ++b) {
                    const double here = ways[a * cols + b];
                    if (here == 0.0) {
                        continue;
                    }
                    for (std::size_t x = 0; x <= static_cast<std::size_t>(n) && a + x < rows; ++x) {
                        for (std::size_t y = 0; x + y <= static_cast<std::size_t>(n) && b + y < cols; ++y) {
                            next[(a + x) * cols + b + y] += here;
                        }
                    }
                }
            }
            ways = std::move(next);
        }
        return ways.back();
    }

    std::vector<Outcome> chance_outcomes(const GameState& game, const Action& action, Rng& rng, const ChanceOptions& options, ChanceStats* stats) {
        ChanceStats local;
        ChanceStats& counts = stats != nullptr ? *stats : local;
        ++counts.actions;
        std::vector<std::pair<Script, GameState>> leaves;
        std::vector<Script> pending{Script{}};
        while (!pending.empty()) {
            const Script script = pending.back();
            pending.pop_back();
            GameState probe = scripted(game, action, script);
            if (probe.rng.scripted_flips() > script.length && script.length < options.max_flips) {
                pending.push_back(Script{script.heads | (1U << script.length), script.length + 1});
                pending.push_back(Script{script.heads, script.length + 1});
            } else {
                leaves.emplace_back(script, std::move(probe));
            }
        }
        if (leaves.size() > 1) {
            ++counts.flipped;
        }
        std::vector<Outcome> out;
        Collector collect(out);
        for (auto& [script, probe] : leaves) {
            const double flips = std::ldexp(1.0, -script.length);
            const int first = touched(game, probe, 0);
            const int second = touched(game, probe, 1);
            if (first < 0 || second < 0) {
                ++counts.sampled;
                for (int s = 0; s < options.samples; ++s) {
                    GameState state = game;
                    scramble(state, rng);
                    collect.add(scripted(state, action, script), flips / static_cast<double>(options.samples));
                }
                continue;
            }
            if (first == 0 && second == 0) {
                collect.add(std::move(probe), flips);
                continue;
            }
            bool sampled = false;
            const std::vector<Arrangement> mine = arrangements(game, 0, first, options, rng, sampled);
            const std::vector<Arrangement> theirs = arrangements(game, 1, second, options, rng, sampled);
            ++(sampled ? counts.sampled : counts.enumerated);
            for (const Arrangement& a : mine) {
                for (const Arrangement& b : theirs) {
                    GameState state = game;
                    place(state, 0, first, a.top);
                    place(state, 1, second, b.top);
                    collect.add(scripted(state, action, script), flips * a.probability * b.probability);
                }
            }
        }
        counts.outcomes += static_cast<long long>(out.size());
        return out;
    }

} // namespace pokebot
