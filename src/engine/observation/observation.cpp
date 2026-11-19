#include "engine/observation/observation.hpp"

#include <algorithm>

#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        std::uint8_t count(std::size_t size) {
            return static_cast<std::uint8_t>(size);
        }

        CardIndex card_at(std::size_t index) {
            return static_cast<CardIndex>(index);
        }

        PlayerView view_of(const GameState& game, int viewer, int owner) {
            const PlayerState& player = game.player(owner);
            const auto knows = [&](CardIndex card) {
                return knows_where(game, viewer, card);
            };
            PlayerView out;
            out.hand_size = count(player.hand.size());
            out.deck_size = count(player.deck.size());
            out.prize_count = count(player.prizes.size());
            out.mulligans = player.mulligans;
            for (const CardIndex card : player.hand) {
                if (knows(card)) {
                    out.hand.push_back(card);
                }
            }
            if (owner != viewer) {
                std::sort(out.hand.begin(), out.hand.end());
            }
            for (std::size_t i = player.deck.size(); i-- > 0;) {
                if (knows(player.deck[i])) {
                    out.deck.push_back(DeckCard{player.deck[i], count(player.deck.size() - 1 - i)});
                }
            }
            out.discard = player.discard;
            out.lost_zone = player.lost_zone;
            out.resolving = player.resolving;
            out.stadium = player.stadium;
            const auto add = [&](const PokemonInPlay& pokemon, auto put) {
                if (knows(pokemon.id())) {
                    put(PokemonView{pokemon, static_cast<std::int16_t>(rules::max_hp(game, pokemon))});
                } else {
                    ++out.face_down;
                }
            };
            if (player.active) {
                add(*player.active, [&](const PokemonView& view) { out.active = view; });
            }
            for (const PokemonInPlay& pokemon : player.bench) {
                add(pokemon, [&](const PokemonView& view) { out.bench.push_back(view); });
            }
            return out;
        }

    } // namespace

    Observation observe(const GameState& game, int viewer) {
        Observation view;
        view.viewer = static_cast<std::uint8_t>(viewer);
        view.current_player = game.current_player;
        view.first_player = game.first_player;
        view.turn = game.turn;
        view.phase = game.phase;
        view.flags = game.flags;
        view.history = game.history;
        view.lasting = game.lasting;
        if (!game.is_over()) {
            view.decision = game.decision();
            view.choice_owner = static_cast<std::uint8_t>(choice_owner(game));
        }
        if (!game.effects.empty()) {
            view.effect = game.effects.back().program;
        }
        view.result = game.result;
        for (int p = 0; p < kPlayers; ++p) {
            view.players[static_cast<std::size_t>(p)] = view_of(game, viewer, p);
        }

        const PlayerState& mine = game.player(viewer);
        CardSet unknown;
        const auto add_unknown = [&](const auto& zone) {
            for (const CardIndex card : zone) {
                if (!knows_where(game, viewer, card)) {
                    unknown.insert(card);
                }
            }
        };
        add_unknown(mine.deck);
        add_unknown(mine.prizes);

        const std::size_t own_first = static_cast<std::size_t>(viewer) * kDeckSize;
        const std::size_t opponent_first = static_cast<std::size_t>(opponent_of(viewer)) * kDeckSize;
        for (std::size_t i = 0; i < kDeckSize; ++i) {
            const CardIndex own = card_at(own_first + i);
            view.defs[own] = &game.def(own);
            if (unknown.contains(own)) {
                view.deck_or_prizes.push_back(own); // in card order, so the deck's order can't show
            }
            const CardIndex theirs = card_at(opponent_first + i);
            if (has_seen(game, viewer, theirs)) {
                view.opponent_seen.push_back(theirs);
                view.defs[theirs] = &game.def(theirs);
            }
        }
        return view;
    }

} // namespace pokebot
