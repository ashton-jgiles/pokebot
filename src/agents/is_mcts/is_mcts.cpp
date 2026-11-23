#include "agents/is_mcts/is_mcts.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>

#include "agents/is_mcts/info_set.hpp"
#include "agents/is_mcts/tree.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        double seconds_since(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

        std::uint64_t mix(std::uint64_t z) {
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }

        std::uint64_t move_key(const Plan& plan) {
            std::uint64_t h = 0x6D6F7665ULL;
            for (const Action& a : plan.actions) {
                h = mix(h ^ (static_cast<std::uint64_t>(a.kind) | static_cast<std::uint64_t>(a.card) << 8 | static_cast<std::uint64_t>(a.target) << 16 |
                             static_cast<std::uint64_t>(a.amount) << 24));
            }
            return h;
        }

        struct Successor {
            std::uint64_t edge = 0;
            std::uint64_t outcome = 0;
            std::size_t move = 0;
            GameState after;
        };

        int actor_after(const GameState& after, int me) {
            return after.is_over() ? me : after.decision().player;
        }

        void add(MacroStats& total, const MacroStats& more) {
            total.searcher_searches += more.searcher_searches;
            total.cache_hits += more.cache_hits;
            total.opponent_searches += more.opponent_searches;
            total.single_actions += more.single_actions;
            total.searcher_seconds += more.searcher_seconds;
            total.opponent_seconds += more.opponent_seconds;
        }

    } // namespace

    IsMcts::IsMcts(const DeckModel& model, std::uint64_t seed, const IsMctsOptions& options) : model_(model), options_(options), rng_(seed) {}

    std::vector<RootChoice> IsMcts::search(const Observation& observation) {
        const auto start = std::chrono::steady_clock::now();
        const int me = observation.decision.player;
        SearchTree tree(hash_observation(observation), me);
        MacroActions macros(me, rng_.next(), options_.macros);
        std::vector<RootChoice> choices;
        std::vector<Plan> root_moves;
        {
            Rng deal(rng_.next());
            const World world(observation, deal, &model_);
            root_moves = macros.moves(world.game());
            if (root_moves.size() <= 1) {
                for (const Plan& plan : root_moves) {
                    choices.push_back(RootChoice{plan, 0, 0.0});
                }
                ++stats_.searches;
                add(stats_.macros, macros.stats());
                stats_.seconds += seconds_since(start);
                return choices;
            }
        }

        const std::uint64_t world_base = rng_.next();
        const auto world_seed = [&](long long k) {
            return mix(world_base ^ (static_cast<std::uint64_t>(k) * 0x9E3779B97F4A7C15ULL + 1));
        };
        const auto playout_seed = [&](long long k) {
            return mix(world_base ^ (static_cast<std::uint64_t>(k) * 0xD1B54A32D192ED03ULL + 2));
        };

        std::vector<Successor> steps;
        for (int i = 0; i < options_.iterations; ++i) {
            std::vector<std::size_t> path{tree.root()};
            std::size_t node = tree.root();
            Rng deal(0);
            std::optional<World> world;
            GameState state;
            std::uint64_t playout = 0;
            bool expanded = false;

            if (options_.matched_worlds) {
                std::size_t move = root_moves.size();
                for (std::size_t m = 0; m < root_moves.size(); ++m) {
                    if (tree.find_child(tree.root(), move_key(root_moves[m])) == SearchTree::kNone) {
                        move = m;
                        break;
                    }
                }
                std::size_t chosen = SearchTree::kNone;
                if (move == root_moves.size()) {
                    const std::vector<std::size_t> all = tree.node(tree.root()).children;
                    tree.mark_available(all);
                    chosen = tree.select(tree.root(), all, options_.exploration);
                    const std::uint64_t key = tree.node(chosen).key;
                    move = static_cast<std::size_t>(
                        std::find_if(root_moves.begin(), root_moves.end(), [&](const Plan& p) { return move_key(p) == key; }) - root_moves.begin());
                    if (move == root_moves.size()) {
                        ++stats_.skipped;
                        continue;
                    }
                }
                const long long k = chosen == SearchTree::kNone ? 0 : tree.node(chosen).visits;
                deal = Rng(world_seed(k));
                world.emplace(observation, deal, &model_);
                state = world->game();
                if (!apply_plan_checked(state, root_moves[move])) {
                    ++stats_.skipped;
                    continue;
                }
                const std::uint64_t outcome = hash_observation(observe(state, me));
                if (chosen == SearchTree::kNone) {
                    chosen = tree.add_child(tree.root(), move_key(root_moves[move]), me, root_moves[move]);
                }
                path.push_back(chosen);
                node = tree.find_child(chosen, outcome);
                if (node == SearchTree::kNone) {
                    node = tree.add_child(chosen, outcome, actor_after(state, me), root_moves[move]);
                    ++stats_.outcomes;
                    expanded = true;
                }
                path.push_back(node);
                playout = playout_seed(k);
            } else {
                deal = Rng(rng_.next());
                world.emplace(observation, deal, &model_);
                state = world->game();
                playout = rng_.next();
            }

            for (int depth = 0; !expanded && depth < options_.max_depth && !state.is_over(); ++depth) {
                const bool by_move = state.decision().player == me;
                const std::vector<Plan>& moves = macros.moves(state);
                steps.clear();
                for (std::size_t m = 0; m < moves.size(); ++m) {
                    GameState after = state;
                    if (!apply_plan_checked(after, moves[m])) {
                        ++stats_.unplayable;
                        continue;
                    }
                    const std::uint64_t outcome = hash_observation(observe(after, me));
                    const std::uint64_t edge = by_move ? move_key(moves[m]) : outcome;
                    if (std::none_of(steps.begin(), steps.end(), [&](const Successor& s) { return s.edge == edge; })) {
                        steps.push_back(Successor{edge, outcome, m, after});
                    }
                }
                if (steps.empty()) {
                    break;
                }
                std::vector<std::size_t> existing;
                const Successor* untried = nullptr;
                for (const Successor& step : steps) {
                    const std::size_t child = tree.find_child(node, step.edge);
                    if (child == SearchTree::kNone) {
                        if (untried == nullptr) {
                            untried = &step;
                        }
                    } else {
                        existing.push_back(child);
                    }
                }
                tree.mark_available(existing);
                const Successor* step = untried;
                std::size_t edge_node = SearchTree::kNone;
                if (step != nullptr) {
                    edge_node = tree.add_child(node, step->edge, by_move ? me : actor_after(step->after, me), moves[step->move]);
                    expanded = true;
                } else {
                    edge_node = tree.select(node, existing, options_.exploration);
                    const std::uint64_t key = tree.node(edge_node).key;
                    for (const Successor& s : steps) {
                        if (s.edge == key) {
                            step = &s;
                            break;
                        }
                    }
                }
                path.push_back(edge_node);
                node = edge_node;
                if (by_move) {
                    std::size_t outcome_node = tree.find_child(edge_node, step->outcome);
                    if (outcome_node == SearchTree::kNone) {
                        outcome_node = tree.add_child(edge_node, step->outcome, actor_after(step->after, me), moves[step->move]);
                        ++stats_.outcomes;
                        expanded = true;
                    }
                    path.push_back(outcome_node);
                    node = outcome_node;
                }
                state = step->after;
            }

            const auto playout_start = std::chrono::steady_clock::now();
            const PlayoutResult result = play_out(state, me, playout, options_.playout);
            stats_.playout_seconds += seconds_since(playout_start);
            stats_.playout_actions += result.actions;
            tree.backpropagate(path, me == 0 ? result.value : -result.value);
            ++stats_.iterations;
        }

        for (const std::size_t child : tree.node(tree.root()).children) {
            const TreeNode& c = tree.node(child);
            choices.push_back(RootChoice{c.move, c.visits, c.mean(me)});
        }
        std::stable_sort(choices.begin(), choices.end(), [](const RootChoice& a, const RootChoice& b) {
            return a.visits > b.visits || (a.visits == b.visits && a.mean > b.mean);
        });
        if (options_.final_by_mean && !choices.empty()) {
            const long long enough = std::max(1, options_.final_min_visits);
            std::size_t best = 0;
            for (std::size_t c = 1; c < choices.size(); ++c) {
                if (choices[c].visits >= enough && (choices[best].visits < enough || choices[c].mean > choices[best].mean)) {
                    best = c;
                }
            }
            std::rotate(choices.begin(), choices.begin() + static_cast<std::ptrdiff_t>(best), choices.begin() + static_cast<std::ptrdiff_t>(best) + 1);
        }
        ++stats_.searches;
        stats_.nodes += static_cast<long long>(tree.size());
        add(stats_.macros, macros.stats());
        stats_.seconds += seconds_since(start);
        return choices;
    }

} // namespace pokebot
