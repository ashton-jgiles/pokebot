#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "engine/internal.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            constexpr std::size_t kEnergyTypeCount = 11;

            bool has_pokemon_in_play(const PlayerState& player) {
                return player.active.has_value() || !player.bench.empty();
            }

            void discard_pokemon(GameState& game, const PokemonInPlay& pokemon) {
                auto discard = [&game](CardIndex card) {
                    game.player(owner_of(card)).discard.push_back(card);
                };
                for (const CardIndex card : pokemon.stack) {
                    discard(card);
                }
                for (const CardIndex card : pokemon.energy) {
                    discard(card);
                }
                if (pokemon.tool != kNoCard) {
                    discard(pokemon.tool);
                }
            }

            void take_prizes(GameState& game, int player, int count) {
                PlayerState& p = game.player(player);
                for (int i = 0; i < count && !p.prizes.empty(); ++i) {
                    const std::size_t pick = game.rng.below(static_cast<std::uint32_t>(p.prizes.size()));
                    p.hand.push_back(p.prizes[pick]);
                    p.prizes.erase_at(pick);
                }
            }

            int wins_for(const GameState& game, int player) {
                int wins = 0;
                if (game.player(player).prizes.empty()) {
                    ++wins;
                }
                if (!has_pokemon_in_play(game.player(opponent_of(player)))) {
                    ++wins;
                }
                return wins;
            }

            void check_for_winner(GameState& game) {
                const int wins0 = wins_for(game, 0);
                const int wins1 = wins_for(game, 1);
                if (wins0 == 0 && wins1 == 0) {
                    return;
                }

                if (wins0 > wins1) {
                    set_winner(game, 0);
                } else if (wins1 > wins0) {
                    set_winner(game, 1);
                } else {
                    game.result = GameResult::Draw;
                    game.pending.clear();
                }
            }

            void knockout(GameState& game, int owner, const PokemonInPlay& pokemon) {
                raise(game, Event{EventKind::KnockedOut, static_cast<std::uint8_t>(owner), pokemon.id(), kNoCard});
                ++game.history.this_turn().knocked_out[static_cast<std::size_t>(owner)];
                const int prizes = rules::prize_value(game, pokemon);
                discard_pokemon(game, pokemon);
                take_prizes(game, opponent_of(owner), prizes);
            }

        } // namespace

        bool can_pay_cost(const GameState& game, const PokemonInPlay& pokemon, const rules::EnergyList& cost) {
            std::array<int, kEnergyTypeCount> available{};
            int total = 0;
            for (const CardIndex card : pokemon.energy) {
                for (const EnergyType type : rules::energy_provided(game, pokemon, card)) {
                    ++available[static_cast<std::size_t>(type)];
                    ++total;
                }
            }

            int colorless = 0;
            for (const EnergyType symbol : cost) {
                if (symbol == EnergyType::Colorless) {
                    ++colorless;
                    continue;
                }
                int& left = available[static_cast<std::size_t>(symbol)];
                if (left == 0) {
                    return false;
                }
                --left;
                --total;
            }
            return total >= colorless;
        }

        int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, const Attack& attack) {
            return rules::attack_damage(game, attacker, defender, attack);
        }

        void add_attack_actions(const GameState& game, std::vector<Action>& out) {
            if (!rules::can_attack(game, game.current_player)) {
                return;
            }
            const PlayerState& p = game.player(game.current_player);
            const auto& attacks = game.def(p.active->top()).attacks;
            for (std::size_t i = 0; i < attacks.size(); ++i) {
                if (can_pay_cost(game, *p.active, rules::attack_cost(game, *p.active, i))) {
                    out.push_back(Action{ActionKind::Attack, kNoCard, kActiveSlot, static_cast<std::uint8_t>(i)});
                }
            }
        }

        void apply_attack(GameState& game, const Action& action) {
            const auto player = static_cast<std::uint8_t>(game.current_player);
            const PokemonInPlay& attacker = *game.player(player).active;
            const EffectContext who{player, attacker.id(), attacker.top()};
            const AttackRef attack{attacker.top(), action.amount};

            game.pending.pop_back();
            ++game.flags.attacks_used;
            game.history.this_turn().attacker = who.self;
            game.history.this_turn().attack = attack;
            perform_attack(game, who, attack);
            raise(game, Event{EventKind::AttackDone, player, who.self, kNoCard});

            const PokemonInPlay* still_active = game.player(player).active ? &*game.player(player).active : nullptr;
            if (still_active != nullptr && still_active->id() == who.self &&
                game.flags.attacks_used < rules::attacks_per_turn(game, *still_active)) {
                game.pending.push_back(Decision{DecisionKind::ExtraAttack, player, 0, who.self});
            } else {
                game.phase = TurnPhase::AfterMain;
            }
            handle_knockouts(game);
            continue_game(game);
        }

        void perform_attack(GameState& game, const EffectContext& who, AttackRef attack) {
            const PokemonInPlay* attacker = find_pokemon(game, who.self);
            PlayerState& opponent = game.player(opponent_of(who.player));
            if (attacker == nullptr || !opponent.active) {
                return;
            }
            const Attack& text = rules::attack_text(game, attack);
            PokemonInPlay& defender = *opponent.active;
            const int damage = rules::attack_damage(game, *attacker, defender, text);
            defender.damage_counters = static_cast<std::uint16_t>(defender.damage_counters + damage / 10);
            if (damage > 0) {
                raise(game, Event{EventKind::DamagedByAttack, static_cast<std::uint8_t>(opponent_of(who.player)), defender.id(), who.self});
            }
        }

        void handle_knockouts(GameState& game) {
            std::array<bool, kPlayers> needs_new_active{};
            for (int p = 0; p < kPlayers; ++p) {
                PlayerState& player = game.player(p);
                if (player.active && rules::is_knocked_out(game, *player.active)) {
                    knockout(game, p, *player.active);
                    player.active.reset();
                    needs_new_active[static_cast<std::size_t>(p)] = true;
                }
                for (std::size_t i = player.bench.size(); i-- > 0;) {
                    if (rules::is_knocked_out(game, player.bench[i])) {
                        knockout(game, p, player.bench[i]);
                        player.bench.erase_at(i);
                    }
                }
            }

            check_for_winner(game);
            if (game.is_over()) {
                return;
            }

            const int current = game.current_player;
            for (const int p : {current, opponent_of(current)}) {
                if (needs_new_active[static_cast<std::size_t>(p)]) {
                    game.pending.push_back(Decision{DecisionKind::PromoteActive, static_cast<std::uint8_t>(p), 0, 0});
                }
            }
        }

        void add_promote_actions(const GameState& game, std::vector<Action>& out) {
            const PlayerState& p = game.player(game.decision().player);
            for (std::size_t i = 0; i < p.bench.size(); ++i) {
                out.push_back(Action{ActionKind::Promote, kNoCard, bench_slot(i), 0});
            }
        }

        void add_extra_attack_actions(const GameState& game, std::vector<Action>& out) {
            const PlayerState& p = game.player(game.current_player);
            if (p.active && p.active->id() == game.decision().target) {
                add_attack_actions(game, out);
            }
            out.push_back(Action{ActionKind::EndTurn, kNoCard, kActiveSlot, 0});
        }

        void apply_promote(GameState& game, const Action& action) {
            const int player = game.decision().player;
            game.pending.pop_back();
            PlayerState& p = game.player(player);
            const std::size_t index = static_cast<std::size_t>(action.target) - 1;
            p.active = p.bench[index];
            p.bench.erase_at(index);
            continue_game(game);
        }

    } // namespace detail

} // namespace pokebot
