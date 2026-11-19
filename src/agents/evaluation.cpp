#include "agents/evaluation.hpp"

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
        const auto deck = [&](const PlayerState& p) {
            return counted(p.deck.size(), kDeckCounted);
        };

        Features phi{};
        phi[slot(Feature::Prizes)] = scaled(minus(theirs.prizes.size(), mine.prizes.size()), static_cast<double>(kPrizeCount));
        phi[slot(Feature::ActiveHp)] = hp_fraction(game, my_front) - hp_fraction(game, their_front);
        phi[slot(Feature::TurnsToKnockout)] = scaled(minus(turns(theirs, my_front), turns(mine, their_front)), static_cast<double>(kMaxTurns));
        phi[slot(Feature::ReadyAttackers)] = scaled(minus(ready_attackers(game, mine), ready_attackers(game, theirs)), 3.0);
        phi[slot(Feature::Energy)] = scaled(minus(energy_in_play(game, mine), energy_in_play(game, theirs)), 10.0);
        phi[slot(Feature::Bench)] = scaled(minus(mine.bench.size(), theirs.bench.size()), static_cast<double>(kBenchSize));
        phi[slot(Feature::Hand)] = scaled(minus(hand(mine), hand(theirs)), static_cast<double>(kHandCounted));
        phi[slot(Feature::Deck)] = scaled(minus(deck(mine), deck(theirs)), static_cast<double>(kDeckCounted));
        phi[slot(Feature::Conditions)] = scaled(minus(conditions(their_front), conditions(my_front)), 3.0);
        phi[slot(Feature::PrizesAtRisk)] = scaled(minus(prize_value(game, their_front), prize_value(game, my_front)), 3.0);
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
        const Features phi = features(game, player);
        double sum = 0.0;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            sum += weights[i] * phi[i];
        }
        return std::tanh(sum);
    }
} // namespace pokebot