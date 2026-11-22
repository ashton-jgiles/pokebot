#include "agents/is_mcts/is_mcts.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>

#include "agents/is_mcts/info_set.hpp"
#include "agents/is_mcts/tree.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        double seconds_since(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

        struct Successor {
            std::uint64_t key = 0;
            std::size_t move = 0;
            GameState after;
        };

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
        {
            Rng deal(rng_.next());
            const World world(observation, deal, &model_);
            const std::vector<Plan>& moves = macros.moves(world.game());
            if (moves.size() <= 1) {
                for (const Plan& plan : moves) {
                    choices.push_back(RootChoice{plan, 0, 0.0});
                }
                ++stats_.searches;
                add(stats_.macros, macros.stats());
                stats_.seconds += seconds_since(start);
                return choices;
            }
        }

        std::vector<Successor> steps;
        for (int i = 0; i < options_.iterations; ++i) {
            Rng deal(rng_.next());
            const World world(observation, deal, &model_);
            GameState state = world.game();
            std::vector<std::size_t> path{tree.root()};
            std::size_t node = tree.root();
            for (int depth = 0; depth < options_.max_depth && !state.is_over(); ++depth) {
                const std::vector<Plan>& moves = macros.moves(state);
                steps.clear();
                for (std::size_t m = 0; m < moves.size(); ++m) {
                    GameState after = state;
                    if (!apply_plan_checked(after, moves[m])) {
                        ++stats_.unplayable;
                        continue;
                    }
                    const std::uint64_t key = hash_observation(observe(after, me));
                    const bool seen = std::any_of(steps.begin(), steps.end(), [&](const Successor& s) { return s.key == key; });
                    if (!seen) {
                        steps.push_back(Successor{key, m, after});
                    }
                }
                if (steps.empty()) {
                    break;
                }
                std::vector<std::size_t> existing;
                const Successor* untried = nullptr;
                for (const Successor& step : steps) {
                    const std::size_t child = tree.find_child(node, step.key);
                    if (child == SearchTree::kNone) {
                        if (untried == nullptr) {
                            untried = &step;
                        }
                    } else {
                        existing.push_back(child);
                    }
                }
                tree.mark_available(existing);
                if (untried != nullptr) {
                    const int actor = untried->after.is_over() ? me : untried->after.decision().player;
                    node = tree.add_child(node, untried->key, actor, moves[untried->move]);
                    path.push_back(node);
                    state = untried->after;
                    break;
                }
                node = tree.select(node, existing, options_.exploration);
                path.push_back(node);
                for (const Successor& step : steps) {
                    if (step.key == tree.node(node).key) {
                        state = step.after;
                        break;
                    }
                }
            }
            const auto playout_start = std::chrono::steady_clock::now();
            const PlayoutResult result = play_out(state, me, rng_.next(), options_.playout);
            stats_.playout_seconds += seconds_since(playout_start);
            stats_.playout_actions += result.actions;
            tree.backpropagate(path, me == 0 ? result.value : -result.value);
            ++stats_.iterations;
        }

        for (const std::size_t child : tree.node(tree.root()).children) {
            const TreeNode& c = tree.node(child);
            choices.push_back(RootChoice{c.move, c.visits, c.mean(me)});
        }
        std::stable_sort(choices.begin(), choices.end(), [](const RootChoice& a, const RootChoice& b) { return a.visits > b.visits || (a.visits == b.visits && a.mean > b.mean); });
        ++stats_.searches;
        stats_.nodes += static_cast<long long>(tree.size());
        add(stats_.macros, macros.stats());
        stats_.seconds += seconds_since(start);
        return choices;
    }

} // namespace pokebot