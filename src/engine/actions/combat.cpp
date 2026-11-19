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
                    p.hand.push_back(p.prizes.pop_back());
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
                    game.effects.clear();
                }
            }

            void knockout(GameState& game, int owner, const PokemonInPlay& pokemon, bool by_attack) {
                raise(game, Event{EventKind::KnockedOut, static_cast<std::uint8_t>(owner), pokemon.id(), kNoCard});
                ++game.history.this_turn().knocked_out[static_cast<std::size_t>(owner)];
                if (by_attack) {
                    ++game.history.this_turn().knocked_out_by_attack[static_cast<std::size_t>(owner)];
                }
                end_effects_on(game, pokemon.id());
                const int prizes = rules::prize_value(game, pokemon);
                discard_pokemon(game, pokemon);
                take_prizes(game, opponent_of(owner), prizes);
            }

        } // namespace

        bool can_pay_cost(const GameState& game, const PokemonInPlay& pokemon, const rules::EnergyList& cost) {
            FixedVector<EnergyMask, kDeckSize> units;
            for (const CardIndex card : pokemon.energy) {
                for (const EnergyMask unit : rules::energy_provided(game, pokemon, card)) {
                    units.push_back(unit);
                }
            }
            FixedVector<EnergyType, 8> typed;
            std::size_t colorless = 0;
            for (const EnergyType symbol : cost) {
                if (symbol == EnergyType::Colorless) {
                    ++colorless;
                } else {
                    typed.push_back(symbol);
                }
            }
            if (units.size() < typed.size() + colorless) {
                return false;
            }
            // Each typed symbol needs its own unit that can be that type; whatever is left pays for Colorless.
            // Costs are at most a few symbols, so trying every assignment is cheap.
            std::array<bool, kDeckSize> used{};
            const auto assign = [&](const auto& self, std::size_t i) -> bool {
                if (i == typed.size()) {
                    return true;
                }
                for (std::size_t u = 0; u < units.size(); ++u) {
                    if (!used[u] && (units[u] & energy_mask(typed[i])) != 0) {
                        used[u] = true;
                        if (self(self, i + 1)) {
                            return true;
                        }
                        used[u] = false;
                    }
                }
                return false;
            };
            return assign(assign, 0);
        }

        int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, const Attack& attack) {
            return rules::attack_damage(game, attacker, defender, attack.damage);
        }

        void add_attack_actions(const GameState& game, std::vector<Action>& out) {
            if (!rules::can_attack(game, game.current_player)) {
                return;
            }
            const PlayerState& p = game.player(game.current_player);
            const auto& attacks = game.def(p.active->top()).attacks;
            for (std::size_t i = 0; i < attacks.size(); ++i) {
                if (rules::can_use_attack(game, *p.active, i) && can_pay_cost(game, *p.active, rules::attack_cost(game, *p.active, i))) {
                    out.push_back(Action{ActionKind::Attack, kNoCard, kActiveSlot, static_cast<std::uint8_t>(i)});
                }
            }
        }

        void apply_attack(GameState& game, const Action& action) {
            const auto player = static_cast<std::uint8_t>(game.current_player);
            PokemonInPlay& attacker = *game.player(player).active;
            const EffectContext who{player, attacker.id(), attacker.top()};
            const AttackRef attack{attacker.top(), action.amount};

            game.pending.pop_back();
            ++game.flags.attacks_used;
            game.history.this_turn().attacker = who.self;
            game.history.this_turn().attack = attack;
            if (confusion_stops_attack(game, attacker)) {
                finish_attack(game, who);
                return;
            }
            start_effect(game, who, ProgramRef{attack.text_from, ProgramKind::Attack, attack.index}, AfterEffect::FinishAttack);
        }

        void finish_attack(GameState& game, const EffectContext& who) {
            const std::uint8_t player = who.player;
            raise(game, Event{EventKind::AttackDone, player, who.self, kNoCard});
            const PokemonInPlay* still_active = game.player(player).active ? &*game.player(player).active : nullptr;
            if (still_active != nullptr && still_active->id() == who.self &&
                game.flags.attacks_used < rules::attacks_per_turn(game, *still_active)) {
                game.pending.push_back(Decision{DecisionKind::ExtraAttack, player, 0, who.self});
            } else {
                game.phase = TurnPhase::AfterMain;
            }
            handle_knockouts(game, true);
            continue_game(game);
        }

        void perform_attack(GameState& game, const EffectContext& who, AttackRef attack) {
            start_effect(game, who, ProgramRef{attack.text_from, ProgramKind::Attack, attack.index}, AfterEffect::Nothing);
        }

        void deal_attack_damage(GameState& game, const EffectContext& who, int base, bool ignore_defender) {
            const PokemonInPlay* attacker = find_pokemon(game, who.self);
            PlayerState& opponent = game.player(opponent_of(who.player));
            if (attacker == nullptr || !opponent.active) {
                return;
            }
            PokemonInPlay& defender = *opponent.active;
            const int damage = rules::attack_damage(game, *attacker, defender, base, ignore_defender);
            defender.damage_counters = static_cast<std::uint16_t>(defender.damage_counters + damage / 10);
            if (damage > 0) {
                raise(game, Event{EventKind::DamagedByAttack, static_cast<std::uint8_t>(opponent_of(who.player)), defender.id(), who.self});
            }
        }

        void handle_knockouts(GameState& game, bool after_attack) {
            std::array<bool, kPlayers> needs_new_active{};
            for (int p = 0; p < kPlayers; ++p) {
                PlayerState& player = game.player(p);
                if (player.active && rules::is_knocked_out(game, *player.active)) {
                    knockout(game, p, *player.active, after_attack);
                    player.active.reset();
                    needs_new_active[static_cast<std::size_t>(p)] = true;
                }
                for (std::size_t i = player.bench.size(); i-- > 0;) {
                    if (rules::is_knocked_out(game, player.bench[i])) {
                        knockout(game, p, player.bench[i], after_attack);
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
                if (needs_new_active[static_cast<std::size_t>(p)] || !game.player(p).active) {
                    game.pending.push_back(Decision{DecisionKind::PromoteActive, static_cast<std::uint8_t>(p), 0, 0});
                }
            }
            enforce_bench_limits(game, current); // a Tera Pokemon was Knocked Out, so Area Zero's 8 may be back to 5
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
