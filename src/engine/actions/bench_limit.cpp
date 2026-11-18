#include "engine/internal.hpp"

// When a player has more Benched Pokemon than they're allowed (Area Zero Underdepths left play, or their
// last Tera Pokemon did), they discard Benched Pokemon of their choice until they're at the limit.
namespace pokebot {

    namespace detail {

        void enforce_bench_limits(GameState& game, int first) {
            // The decision on top is answered first, so the player who goes first is pushed last.
            for (const int player : {opponent_of(first), first}) {
                const PlayerState& p = game.player(player);
                const std::size_t limit = rules::bench_size(game, player);
                if (p.bench.size() > limit) {
                    const auto excess = static_cast<std::uint8_t>(p.bench.size() - limit);
                    game.pending.push_back(Decision{DecisionKind::DiscardBenched, static_cast<std::uint8_t>(player), excess, 0});
                }
            }
        }

        void add_discard_benched_actions(const GameState& game, std::vector<Action>& out) {
            const PlayerState& p = game.player(game.decision().player);
            for (std::size_t i = 0; i < p.bench.size(); ++i) {
                out.push_back(Action{ActionKind::DiscardBenched, kNoCard, bench_slot(i), 0});
            }
        }

        void apply_discard_benched(GameState& game, const Action& action) {
            Decision& decision = game.pending.back();
            PlayerState& p = game.player(decision.player);
            const std::size_t index = static_cast<std::size_t>(action.target) - 1;
            const PokemonInPlay pokemon = p.bench[index];
            p.bench.erase_at(index);
            end_effects_on(game, pokemon.id());
            for (const CardIndex card : pokemon.stack) {
                p.discard.push_back(card);
            }
            for (const CardIndex card : pokemon.energy) {
                p.discard.push_back(card);
            }
            if (pokemon.tool != kNoCard) {
                p.discard.push_back(pokemon.tool);
            }
            --decision.amount;
            if (decision.amount == 0) {
                game.pending.pop_back();
                continue_game(game);
            }
        }

    } // namespace detail

} // namespace pokebot
