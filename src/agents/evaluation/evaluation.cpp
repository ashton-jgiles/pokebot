#include "agents/evaluation/evaluation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "engine/engine.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        constexpr int kMaxTurns = 5;
        constexpr std::size_t kHandCounted = 7;
        constexpr std::size_t kDeckCounted = 30;
        constexpr std::size_t kLastCards = 8;
        constexpr double kMostDamage = 300.0;
        constexpr double kMostStages = 6.0;
        constexpr double kMostHp = 1000.0;

        constexpr std::size_t slot(Feature feature) {
            return static_cast<std::size_t>(feature);
        }

        double scaled(double difference, double scale) {
            return std::clamp(difference / scale, -1.0, 1.0);
        }

        double minus(std::size_t a, std::size_t b) {
            return static_cast<double>(a) - static_cast<double>(b);
        }

        template <class Visit>
        void each_in_play(const PlayerState& player, Visit visit) {
            if (player.active) {
                visit(*player.active);
            }
            for (const PokemonInPlay& pokemon : player.bench) {
                visit(pokemon);
            }
        }

        const PokemonInPlay* front(const GameState& game, const PlayerState& player) {
            if (player.active) {
                return &*player.active;
            }
            const PokemonInPlay* best = nullptr;
            for (const PokemonInPlay& pokemon : player.bench) {
                if (best == nullptr || rules::remaining_hp(game, pokemon) > rules::remaining_hp(game, *best)) {
                    best = &pokemon;
                }
            }
            return best;
        }

        double hp_fraction(const GameState& game, const PokemonInPlay* pokemon) {
            if (pokemon == nullptr) {
                return 0.0;
            }
            const int max = rules::max_hp(game, *pokemon);
            return max > 0 ? static_cast<double>(std::max(rules::remaining_hp(game, *pokemon), 0)) / max : 0.0;
        }

        int turns_to_knockout(const GameState& game, const PlayerState& attacker, const PokemonInPlay* defender) {
            if (defender == nullptr) {
                return kMaxTurns;
            }
            int best = 0;
            each_in_play(attacker, [&](const PokemonInPlay& pokemon) {
                const std::size_t count = game.def(pokemon.top()).attacks.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const Attack& attack = rules::attack_text(game, AttackRef{pokemon.top(), static_cast<std::uint8_t>(i)});
                    best = std::max(best, rules::attack_damage(game, pokemon, *defender, attack.damage));
                }
            });
            if (best <= 0) {
                return kMaxTurns;
            }
            const int hp = std::max(rules::remaining_hp(game, *defender), 1);
            return std::min((hp + best - 1) / best, kMaxTurns);
        }

        std::size_t ready_attackers(const GameState& game, const PlayerState& player) {
            std::size_t ready = 0;
            each_in_play(player, [&](const PokemonInPlay& pokemon) {
                const std::size_t count = game.def(pokemon.top()).attacks.size();
                bool can = false;
                for (std::size_t i = 0; i < count && !can; ++i) {
                    can = rules::can_use_attack(game, pokemon, i) && can_pay_attack(game, pokemon, i);
                }
                ready += can ? 1 : 0;
            });
            return ready;
        }

        std::size_t energy_in_play(const GameState& game, const PlayerState& player) {
            std::size_t units = 0;
            each_in_play(player, [&](const PokemonInPlay& pokemon) {
                units += rules::energy_units(game, pokemon);
            });
            return units;
        }

        std::size_t conditions(const PokemonInPlay* pokemon) {
            if (pokemon == nullptr) {
                return 0;
            }
            const SpecialConditions& s = pokemon->special;
            const bool poisoned = s.poison > 0;
            return std::size_t{s.asleep} + std::size_t{s.paralyzed} + std::size_t{s.burned} + std::size_t{s.confused} + std::size_t{poisoned};
        }
        
        std::size_t prize_value(const GameState& game, const PokemonInPlay* pokemon) {
            return pokemon == nullptr ? 0 : static_cast<std::size_t>(rules::prize_value(game, *pokemon));
        }

        template <class Visit>
        void each_payable_attack(const GameState& game, const PlayerState& player, Visit visit) {
            each_in_play(player, [&](const PokemonInPlay& pokemon) {
                const std::size_t count = game.def(pokemon.top()).attacks.size();
                for (std::size_t i = 0; i < count; ++i) {
                    if (rules::can_use_attack(game, pokemon, i) && can_pay_attack(game, pokemon, i)) {
                        visit(pokemon, rules::attack_text(game, AttackRef{pokemon.top(), static_cast<std::uint8_t>(i)}).damage);
                    }
                }
            });
        }

        double knockout_threat(const GameState& game, const PlayerState& attacker, const PokemonInPlay* defender) {
            if (defender == nullptr) {
                return 0.0;
            }
            int best = 0;
            each_payable_attack(game, attacker, [&](const PokemonInPlay& pokemon, int damage) {
                best = std::max(best, rules::attack_damage(game, pokemon, *defender, damage));
            });
            if (best < std::max(rules::remaining_hp(game, *defender), 1)) {
                return 0.0;
            }
            const double left = static_cast<double>(std::max<std::size_t>(attacker.prizes.size(), 1));
            return std::min(static_cast<double>(prize_value(game, defender)) / left, 1.0);
        }

        double payable_damage(const GameState& game, const PlayerState& player) {
            int best = 0;
            each_payable_attack(game, player, [&](const PokemonInPlay&, int damage) { best = std::max(best, damage); });
            return std::min(static_cast<double>(best) / kMostDamage, 1.0);
        }

        std::size_t stages(const GameState& game, const PlayerState& player) {
            std::size_t count = 0;
            each_in_play(player, [&](const PokemonInPlay& pokemon) {
                const Stage stage = game.def(pokemon.top()).stage;
                if (stage == Stage::Stage1) {
                    count += 1;
                } else if (stage == Stage::Stage2) {
                    count += 2;
                }
            });
            return count;
        }

        std::size_t board_hp(const GameState& game, const PlayerState& player) {
            std::size_t hp = 0;
            each_in_play(player, [&](const PokemonInPlay& pokemon) {
                hp += static_cast<std::size_t>(std::max(rules::remaining_hp(game, pokemon), 0));
            });
            return hp;
        }

        double late_prizes(const PlayerState& player) {
            const double taken = static_cast<double>(kPrizeCount) - static_cast<double>(player.prizes.size());
            const double share = taken / static_cast<double>(kPrizeCount);
            return share * share;
        }

    } // namespace

    const char* to_string(Feature feature) {
        switch (feature) {
            case Feature::Prizes:
                return "prizes";
            case Feature::ActiveHp:
                return "active_hp";
            case Feature::TurnsToKnockout:
                return "turns_to_knock_out";
            case Feature::ReadyAttackers:
                return "ready_attackers";
            case Feature::Energy:
                return "energy";
            case Feature::Bench:
                return "bench";
            case Feature::Hand:
                return "hand";
            case Feature::Deck:
                return "deck";
            case Feature::Conditions:
                return "conditions";
            case Feature::PrizesAtRisk:
                return "prizes_at_risk";
            case Feature::LatePrizes:
                return "late_prizes";
            case Feature::KnockoutThreat:
                return "knockout_threat";
            case Feature::PayableDamage:
                return "payable_damage";
            case Feature::Evolved:
                return "evolved";
            case Feature::BoardHp:
                return "board_hp";
            case Feature::DeckOut:
                return "deck_out";
        }
        return "?";
    }

    Weights default_weights() {
        Weights theta{};
        theta[slot(Feature::Prizes)] = 2.0;
        theta[slot(Feature::ActiveHp)] = 0.3;
        theta[slot(Feature::TurnsToKnockout)] = 0.8;
        theta[slot(Feature::ReadyAttackers)] = 0.4;
        theta[slot(Feature::Energy)] = 0.3;
        theta[slot(Feature::Bench)] = 0.3;
        theta[slot(Feature::Hand)] = 0.2;
        theta[slot(Feature::Deck)] = 0.5;
        theta[slot(Feature::Conditions)] = 0.2;
        theta[slot(Feature::PrizesAtRisk)] = 0.2;
        return theta;
    }

    Features features(const GameState& game, int player) {
        Weights all{};
        all.fill(1.0);
        return features(game, player, all);
    }

    Features features(const GameState& game, int player, const Weights& wanted) {
        const PlayerState& mine = game.player(player);
        const PlayerState& theirs = game.player(opponent_of(player));
        const PokemonInPlay* my_front = front(game, mine);
        const PokemonInPlay* their_front = front(game, theirs);
        const auto turns = [&](const PlayerState& attacker, const PokemonInPlay* defender) {
            return static_cast<std::size_t>(turns_to_knockout(game, attacker, defender));
        };
        const auto counted = [](std::size_t size, std::size_t most) {
            return std::min(size, most);
        };
        const auto hand = [&](const PlayerState& p) {
            return counted(p.hand.size(), kHandCounted);
        };
        const auto deck = [&](const PlayerState& p, std::size_t most) {
            return counted(p.deck.size(), most);
        };

        Features phi{};
        const auto set = [&](Feature feature, const auto& value) {
            if (wanted[slot(feature)] != 0.0) {
                phi[slot(feature)] = value();
            }
        };
        set(Feature::Prizes, [&] { return scaled(minus(theirs.prizes.size(), mine.prizes.size()), static_cast<double>(kPrizeCount)); });
        set(Feature::ActiveHp, [&] { return hp_fraction(game, my_front) - hp_fraction(game, their_front); });
        set(Feature::TurnsToKnockout, [&] { return scaled(minus(turns(theirs, my_front), turns(mine, their_front)), static_cast<double>(kMaxTurns)); });
        set(Feature::ReadyAttackers, [&] { return scaled(minus(ready_attackers(game, mine), ready_attackers(game, theirs)), 3.0); });
        set(Feature::Energy, [&] { return scaled(minus(energy_in_play(game, mine), energy_in_play(game, theirs)), 10.0); });
        set(Feature::Bench, [&] { return scaled(minus(mine.bench.size(), theirs.bench.size()), static_cast<double>(kBenchSize)); });
        set(Feature::Hand, [&] { return scaled(minus(hand(mine), hand(theirs)), static_cast<double>(kHandCounted)); });
        set(Feature::Deck, [&] { return scaled(minus(deck(mine, kDeckCounted), deck(theirs, kDeckCounted)), static_cast<double>(kDeckCounted)); });
        set(Feature::Conditions, [&] { return scaled(minus(conditions(their_front), conditions(my_front)), 3.0); });
        set(Feature::PrizesAtRisk, [&] { return scaled(minus(prize_value(game, their_front), prize_value(game, my_front)), 3.0); });
        set(Feature::LatePrizes, [&] { return late_prizes(mine) - late_prizes(theirs); });
        set(Feature::KnockoutThreat, [&] { return knockout_threat(game, mine, their_front) - knockout_threat(game, theirs, my_front); });
        set(Feature::PayableDamage, [&] { return payable_damage(game, mine) - payable_damage(game, theirs); });
        set(Feature::Evolved, [&] { return scaled(minus(stages(game, mine), stages(game, theirs)), kMostStages); });
        set(Feature::BoardHp, [&] { return scaled(minus(board_hp(game, mine), board_hp(game, theirs)), kMostHp); });
        set(Feature::DeckOut, [&] { return scaled(minus(deck(mine, kLastCards), deck(theirs, kLastCards)), static_cast<double>(kLastCards)); });
        return phi;
    }

    double utility(const GameState& game, int player) {
        switch (game.result) {
            case GameResult::Player0Wins:
                return player == 0 ? 1.0 : -1.0;
            case GameResult::Player1Wins:
                return player == 1 ? 1.0 : -1.0;
            case GameResult::Draw:
                return 0.0;
            case GameResult::Ongoing:
                break;
        }
        throw std::logic_error("utility: the game isn't over");
    }

    double evaluate(const GameState& game, int player, const Weights& weights) {
        if (game.is_over()) {
            return utility(game, player);
        }
        const Features phi = features(game, player, weights);
        double sum = 0.0;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            sum += weights[i] * phi[i];
        }
        return std::tanh(sum);
    }
} // namespace pokebot