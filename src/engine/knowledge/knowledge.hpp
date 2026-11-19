#pragma once

#include "engine/game/game_state.hpp"

// What each player knows about where the cards are. The observation layer (step 4.2) shows a player
// only what this says they know.
namespace pokebot {

    // Whether `player` knows where `card` is right now: it's in the open, in their own hand, or in a
    // hidden zone they've been shown it going into (revealed by a search, looked at, or picked up from
    // the open) and it hasn't been shuffled away since.
    bool knows_where(const GameState& game, int player, CardIndex card);

    // Whether `player` has seen `card` at any time this game, wherever it is now.
    bool has_seen(const GameState& game, int player, CardIndex card);

    // Calls visit(card, owner, owner_only) for every card a player can see without being shown it:
    // each hand (owner_only: only its owner sees it), and every card in the open. Pokemon put down
    // during setup are face down until both players have set up, so only their owner sees them then.
    template <class Visit>
    void each_visible_card(const GameState& game, Visit visit) {
        const bool face_down = game.turn == 0;
        for (int p = 0; p < kPlayers; ++p) {
            const PlayerState& player = game.player(p);
            for (const CardIndex card : player.hand) {
                visit(card, p, true);
            }
            for (const auto* zone : {&player.discard, &player.lost_zone}) {
                for (const CardIndex card : *zone) {
                    visit(card, p, false);
                }
            }
            for (const CardIndex card : player.resolving) {
                visit(card, p, false);
            }
            if (player.stadium != kNoCard) {
                visit(player.stadium, p, false);
            }
            const auto in_play = [&](const PokemonInPlay& pokemon) {
                for (const CardIndex card : pokemon.stack) {
                    visit(card, p, face_down);
                }
                for (const CardIndex card : pokemon.energy) {
                    visit(card, p, face_down);
                }
                if (pokemon.tool != kNoCard) {
                    visit(pokemon.tool, p, face_down);
                }
            };
            if (player.active) {
                in_play(*player.active);
            }
            for (const PokemonInPlay& pokemon : player.bench) {
                in_play(pokemon);
            }
        }
    }

    namespace detail {

        // `player` sees `card` and knows where it is.
        void show(GameState& game, int player, CardIndex card);

        // Records everything each player can see right now. The engine calls it after every action.
        void note_visible(GameState& game);

        // Shuffles a player's deck. Nobody knows where any card in it is any more.
        void shuffle_deck(GameState& game, int player);

    } // namespace detail

} // namespace pokebot
