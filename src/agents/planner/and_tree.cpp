#include "agents/planner/and_tree.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "agents/planner/action_class.hpp"
#include "agents/planner/or_tree.hpp"
#include "agents/planner/state_hash.hpp"
#include "engine/engine.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        constexpr int kMaxTurns = 5;
        constexpr int kLastCards = 8;
        constexpr double kMostStages = 6.0;

        template <class Visit>
        void each_in_play(const PlayerState& player, Visit visit) {
            if (player.active) {
                visit(*player.active);
            }
            for (const PokemonInPlay& pokemon : player.bench) {
                visit(pokemon);
            }
        }

        double hp_fraction(const GameState& game, const PokemonInPlay& pokemon) {
            const int max = rules::max_hp(game, pokemon);
            return max > 0 ? static_cast<double>(std::max(rules::remaining_hp(game, pokemon), 0)) / max : 0.0;
        }

        int conditions(const PokemonInPlay& pokemon) {
            const SpecialConditions& s = pokemon.special;
            return int{s.asleep} + int{s.paralyzed} + int{s.burned} + int{s.confused} + int{s.poison > 0};
        }

        int most_damage(const GameState& game, const PlayerState& attacker, const PokemonInPlay& target, bool with_hand) {
            int most = 0;
            each_in_play(attacker, [&](const PokemonInPlay& pokemon) {
                const std::size_t count = game.def(pokemon.top()).attacks.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const Attack& attack = rules::attack_text(game, AttackRef{pokemon.top(), static_cast<std::uint8_t>(i)});
                    most = std::max(most, rules::attack_damage(game, pokemon, target, attack.damage));
                }
            });
            if (with_hand) {
                for (const CardIndex card : attacker.hand) {
                    const CardDef& def = game.def(card);
                    if (def.kind == CardKind::Pokemon) {
                        for (const Attack& attack : def.attacks) {
                            most = std::max(most, 2 * attack.damage);
                        }
                    }
                }
            }
            return most;
        }

        int stages_of(const CardDef& def) {
            if (def.kind != CardKind::Pokemon) {
                return 0;
            }
            if (def.stage == Stage::Stage2) {
                return 2;
            }
            return def.stage == Stage::Stage1 ? 1 : 0;
        }

        bool bounded(Feature feature) {
            return feature < static_cast<Feature>(kFirstCandidate) || feature == Feature::Evolved || feature == Feature::DeckOut;
        }

        int turns_to_knock_out(int hp, int damage) {
            if (damage <= 0) {
                return kMaxTurns;
            }
            return std::min((std::max(hp, 1) + damage - 1) / damage, kMaxTurns);
        }

        std::uint64_t action_key(const Action& action) {
            const std::uint64_t packed = (std::uint64_t{static_cast<std::uint8_t>(action.kind)} << 24) | (std::uint64_t{action.card} << 16) |
                                         (std::uint64_t{action.target} << 8) | std::uint64_t{action.amount};
            return (packed + 1) * 0x9E3779B97F4A7C15ULL;
        }

        class AndTree {
        public:
            AndTree(int player, PlanScorer& scorer, const AndTreeOptions& options) : player_(player), scorer_(scorer), options_(options) {}

            AndTreeResult run(const GameState& root) {
                std::vector<Action> path;
                visit(root, path);
                return result_;
            }

        private:
            double incumbent() const {
                const std::size_t keep = static_cast<std::size_t>(options_.keep);
                if (options_.keep <= 0 || result_.plans.size() < keep) {
                    return -std::numeric_limits<double>::infinity();
                }
                return result_.plans.back().value;
            }

            void record(const std::vector<Action>& path, const Action& last, ActionClass kind, double value, std::uint64_t key) {
                if (ends_.count(key) != 0) {
                    return;
                }
                const std::size_t keep = static_cast<std::size_t>(options_.keep);
                if (options_.keep > 0 && result_.plans.size() >= keep && value <= result_.plans.back().value) {
                    return;
                }
                ends_.insert(key);
                RankedPlan ranked{Plan{path, kind}, value};
                ranked.plan.actions.push_back(last);
                const auto at = std::upper_bound(result_.plans.begin(), result_.plans.end(), value,
                                                 [](double v, const RankedPlan& other) { return v > other.value; });
                result_.plans.insert(at, std::move(ranked));
                if (options_.keep > 0 && result_.plans.size() > keep) {
                    result_.plans.pop_back();
                }
            }

            void visit(const GameState& state, std::vector<Action>& path) {
                if (stopped_) {
                    return;
                }
                if (result_.expanded >= options_.budget) {
                    result_.out_of_budget = true;
                    stopped_ = true;
                    return;
                }
                if (options_.closed_set && !closed_.insert(state)) {
                    ++result_.closed;
                    return;
                }
                ++result_.expanded;

                const std::vector<Action> actions = legal_actions(state);
                const bool can_attack = std::any_of(actions.begin(), actions.end(), [](const Action& a) { return a.kind == ActionKind::Attack; });
                struct Child {
                    Action action;
                    GameState after;
                    double e;
                    int order;
                };
                std::vector<Child> children;
                for (const Action& action : actions) {
                    TriedAction tried = try_action(state, player_, action);
                    if (tried.kind == ActionClass::Deterministic) {
                        const std::vector<Action> next = legal_actions(tried.after);
                        const bool enables = !can_attack && std::any_of(next.begin(), next.end(), [](const Action& a) { return a.kind == ActionKind::Attack; });
                        const double e = evaluate(tried.after, player_, scorer_.weights());
                        children.push_back(Child{action, std::move(tried.after), e, action_order(state, action, enables)});
                        ++result_.generated;
                    } else {
                        const double value = scorer_.value(state, action, tried);
                        ++result_.scored;
                        const std::uint64_t key = tried.kind == ActionClass::Final ? hash_state(tried.after) : hash_state(state) ^ action_key(action);
                        record(path, action, tried.kind, value, key);
                    }
                }
                std::stable_sort(children.begin(), children.end(), [](const Child& a, const Child& b) {
                    if (a.e != b.e) {
                        return a.e > b.e;
                    }
                    return a.order < b.order;
                });
                for (const Child& child : children) {
                    if (options_.bound && options_.keep > 0 && pokebot::upper_bound(child.after, player_, scorer_.weights()) <= incumbent()) {
                        ++result_.pruned;
                        continue;
                    }
                    path.push_back(child.action);
                    visit(child.after, path);
                    path.pop_back();
                    if (stopped_) {
                        return;
                    }
                }
            }

            int player_;
            PlanScorer& scorer_;
            AndTreeOptions options_;
            AndTreeResult result_;
            ClosedSet closed_;
            std::unordered_set<std::uint64_t> ends_;
            bool stopped_ = false;
        };

    } // namespace

    double upper_bound(const GameState& game, int player, const Weights& weights) {
        if (game.is_over()) {
            return utility(game, player);
        }
        if (std::any_of(weights.begin(), weights.end(), [](double w) { return w < 0.0; })) {
            return 1.0;
        }
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            if (weights[i] != 0.0 && !bounded(static_cast<Feature>(i))) {
                return 1.0;
            }
        }
        Features phi = features(game, player, weights);
        const auto raise = [&](Feature feature, double value) {
            double& at = phi[static_cast<std::size_t>(feature)];
            at = std::max(at, std::min(value, 1.0));
        };
        const auto at = [&](Feature feature) {
            return phi[static_cast<std::size_t>(feature)];
        };
        const PlayerState& mine = game.player(player);
        const PlayerState& theirs = game.player(opponent_of(player));

        int prize_gain = 0;
        int their_most_prizes = 0;
        int stage_gain = 0;
        int their_fewest_turns = kMaxTurns;
        double their_lowest_hp = 1.0;
        each_in_play(theirs, [&](const PokemonInPlay& target) {
            const int damage = most_damage(game, mine, target, true);
            const int hp = rules::remaining_hp(game, target);
            const int max = rules::max_hp(game, target);
            if (damage >= hp) {
                prize_gain = std::max(prize_gain, rules::prize_value(game, target));
                stage_gain = std::max(stage_gain, stages_of(game.def(target.top())));
            }
            their_most_prizes = std::max(their_most_prizes, rules::prize_value(game, target));
            their_fewest_turns = std::min(their_fewest_turns, turns_to_knock_out(hp, damage));
            their_lowest_hp = std::min(their_lowest_hp, max > 0 ? static_cast<double>(std::max(hp - damage, 0)) / max : 0.0);
        });
        if (prize_gain > 0 && static_cast<std::size_t>(prize_gain) >= mine.prizes.size()) {
            return 1.0;
        }

        double my_best_hp = 0.0;
        int my_most_turns = 0;
        int my_fewest_prizes = 3;
        each_in_play(mine, [&](const PokemonInPlay& pokemon) {
            my_best_hp = std::max(my_best_hp, hp_fraction(game, pokemon));
            my_most_turns = std::max(my_most_turns, turns_to_knock_out(rules::remaining_hp(game, pokemon), most_damage(game, theirs, pokemon, false)));
            my_fewest_prizes = std::min(my_fewest_prizes, rules::prize_value(game, pokemon));
        });

        for (const CardIndex card : mine.hand) {
            stage_gain += stages_of(game.def(card));
        }
        const auto last_cards = [](int cards) {
            return std::clamp(cards, 0, kLastCards);
        };
        const int my_last = last_cards(static_cast<int>(mine.deck.size() + mine.hand.size()));
        const int their_last = last_cards(static_cast<int>(theirs.deck.size()) - 1);

        const double bench_space = static_cast<double>(rules::bench_size(game, player)) - static_cast<double>(mine.bench.size());
        const double their_hand = static_cast<double>(std::min<std::size_t>(theirs.hand.size(), 7));
        const int their_conditions = theirs.active ? conditions(*theirs.active) : 0;

        raise(Feature::Prizes, at(Feature::Prizes) + prize_gain / 6.0);
        raise(Feature::ActiveHp, my_best_hp - their_lowest_hp);
        raise(Feature::TurnsToKnockout, static_cast<double>(my_most_turns - their_fewest_turns) / kMaxTurns);
        raise(Feature::ReadyAttackers, at(Feature::ReadyAttackers) + 1.0 / 3.0);
        raise(Feature::Energy, at(Feature::Energy) + 0.2);
        raise(Feature::Bench, at(Feature::Bench) + std::max(bench_space, 0.0) / 5.0);
        raise(Feature::Hand, (7.0 - their_hand) / 7.0);
        raise(Feature::Deck, at(Feature::Deck) + static_cast<double>(mine.hand.size()) / 30.0);
        raise(Feature::Conditions, std::min(their_conditions + 2, 3) / 3.0);
        raise(Feature::PrizesAtRisk, static_cast<double>(their_most_prizes - my_fewest_prizes) / 3.0);
        raise(Feature::Evolved, at(Feature::Evolved) + stage_gain / kMostStages);
        raise(Feature::DeckOut, static_cast<double>(my_last - their_last) / kLastCards);

        double sum = 0.0;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            sum += weights[i] * phi[i];
        }
        return std::tanh(sum);
    }

    AndTreeResult and_tree_search(const GameState& game, int player, PlanScorer& scorer, const AndTreeOptions& options) {
        if (game.is_over() || game.decision().player != player || scorer.player() != player) {
            throw std::invalid_argument("and_tree_search: the player must be deciding, and the scorer theirs");
        }
        return AndTree(player, scorer, options).run(game);
    }

} // namespace pokebot
