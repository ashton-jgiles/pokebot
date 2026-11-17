#include "engine/invariants/invariants.hpp"

#include <array>
#include <string>

#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        class Checker {
        public:
            explicit Checker(const GameState& game) : game_(game) {}

            std::vector<std::string> run() {
                for (int p = 0; p < kPlayers; ++p) {
                    check_player(p);
                }
                for (std::size_t card = 0; card < kMaxCards; ++card) {
                    if (copies_[card] != 1) {
                        fail("card " + std::to_string(card) + " appears " + std::to_string(copies_[card]) + " times");
                    }
                }
                check_decisions();
                return std::move(problems_);
            }

        private:
            void fail(std::string problem) { problems_.push_back(std::move(problem)); }

            // Records that `card` was found in a zone belonging to `zone_owner`.
            bool note(CardIndex card, int zone_owner, const std::string& where) {
                if (card >= kMaxCards) {
                    fail(where + " holds an invalid card number " + std::to_string(card));
                    return false;
                }
                ++copies_[card];
                if (owner_of(card) != zone_owner) {
                    fail("card " + std::to_string(card) + " is in " + where + " but belongs to player " + std::to_string(owner_of(card)));
                }
                return true;
            }

            template <class Zone>
            void note_zone(const Zone& zone, int owner, const std::string& where) {
                for (const CardIndex card : zone) {
                    note(card, owner, where);
                }
            }

            void check_pokemon(const PokemonInPlay& pokemon, int owner, const std::string& where) {
                if (pokemon.stack.empty()) {
                    fail(where + " has no cards");
                    return;
                }
                bool cards_valid = true;
                for (const CardIndex card : pokemon.stack) {
                    cards_valid = note(card, owner, where) && cards_valid;
                }
                for (const CardIndex card : pokemon.energy) {
                    if (note(card, owner, where + "'s Energy") && game_.def(card).kind != CardKind::Energy) {
                        fail(where + " has a non-Energy card attached as Energy: " + game_.def(card).name);
                    }
                }
                if (pokemon.tool != kNoCard) {
                    note(pokemon.tool, owner, where + "'s Tool");
                }
                if (!cards_valid) {
                    return;
                }

                const CardDef& base = game_.def(pokemon.stack[0]);
                if (base.kind != CardKind::Pokemon || base.stage != Stage::Basic) {
                    fail(where + " doesn't start with a Basic Pokemon: " + base.name);
                }
                for (std::size_t i = 1; i < pokemon.stack.size(); ++i) {
                    const CardDef& below = game_.def(pokemon.stack[i - 1]);
                    const CardDef& above = game_.def(pokemon.stack[i]);
                    if (above.evolves_from != below.name) {
                        fail(where + ": " + above.name + " doesn't evolve from " + below.name);
                    }
                }
                const CardDef& top = game_.def(pokemon.top());
                if (rules::is_knocked_out(game_, pokemon)) {
                    fail(where + " (" + top.name + ") has " + std::to_string(pokemon.damage_counters * 10) + " damage on " +
                         std::to_string(rules::max_hp(game_, pokemon)) + " HP and should have been Knocked Out");
                }
            }

            void check_player(int p) {
                const PlayerState& player = game_.player(p);
                const std::string who = "player " + std::to_string(p) + "'s ";
                note_zone(player.deck, p, who + "deck");
                note_zone(player.hand, p, who + "hand");
                note_zone(player.prizes, p, who + "Prize cards");
                note_zone(player.discard, p, who + "discard pile");
                note_zone(player.lost_zone, p, who + "Lost Zone");
                if (player.active) {
                    check_pokemon(*player.active, p, who + "Active Pokemon");
                }
                for (std::size_t i = 0; i < player.bench.size(); ++i) {
                    check_pokemon(player.bench[i], p, who + "Bench " + std::to_string(i + 1));
                }
            }

            bool promotion_pending(int player) const {
                for (const Decision& decision : game_.pending) {
                    if (decision.kind == DecisionKind::PromoteActive && decision.player == player) {
                        return true;
                    }
                }
                return false;
            }

            void check_decisions() {
                if (game_.is_over()) {
                    if (!game_.pending.empty()) {
                        fail("the game is over but a decision is still pending");
                    }
                    return;
                }
                if (game_.pending.empty()) {
                    fail("the game isn't over but no decision is pending");
                    return;
                }
                for (const Decision& decision : game_.pending) {
                    if (decision.player >= kPlayers) {
                        fail("a pending decision belongs to player " + std::to_string(decision.player));
                    }
                }
                const Decision& decision = game_.decision();
                if ((decision.kind == DecisionKind::MainPhase || decision.kind == DecisionKind::ExtraAttack) &&
                    (decision.player != game_.current_player || game_.phase != TurnPhase::Main)) {
                    fail("a main-phase decision for a player whose turn it isn't, or outside the main phase");
                }
                if (game_.turn > 0) {
                    for (int p = 0; p < kPlayers; ++p) {
                        if (!game_.player(p).active && !promotion_pending(p)) {
                            fail("player " + std::to_string(p) + " has no Active Pokemon and isn't choosing one");
                        }
                    }
                }
            }

            const GameState& game_;
            std::array<int, kMaxCards> copies_{};
            std::vector<std::string> problems_;
        };

    } // namespace

    std::vector<std::string> check_invariants(const GameState& game) {
        return Checker(game).run();
    }

} // namespace pokebot
