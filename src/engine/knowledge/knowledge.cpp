#include "engine/knowledge/knowledge.hpp"

namespace pokebot {

    bool knows_where(const GameState& game, int player, CardIndex card) {
        return game.knowledge.located[static_cast<std::size_t>(player)].contains(card);
    }

    bool has_seen(const GameState& game, int player, CardIndex card) {
        return game.knowledge.seen[static_cast<std::size_t>(player)].contains(card);
    }

    namespace detail {

        void show(GameState& game, int player, CardIndex card) {
            game.knowledge.located[static_cast<std::size_t>(player)].insert(card);
            game.knowledge.seen[static_cast<std::size_t>(player)].insert(card);
        }

        void note_visible(GameState& game) {
            each_visible_card(game, [&](CardIndex card, int owner, bool owner_only) {
                show(game, owner, card);
                if (!owner_only) {
                    show(game, opponent_of(owner), card);
                }
            });
        }

        void shuffle_deck(GameState& game, int player) {
            PlayerState& p = game.player(player);
            game.rng.shuffle(p.deck);
            for (const CardIndex card : p.deck) {
                for (CardSet& located : game.knowledge.located) {
                    located.erase(card);
                }
            }
        }

    } // namespace detail

} // namespace pokebot
