#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "engine/game/game_state.hpp"

namespace pokebot {

    struct DeckCard {
        CardIndex card = kNoCard;
        std::uint8_t from_top = 0;

        bool operator==(const DeckCard&) const = default;
    };

    struct PokemonView {
        PokemonInPlay pokemon;
        std::int16_t max_hp = 0;

        int remaining_hp() const { return max_hp - 10 * static_cast<int>(pokemon.damage_counters); }
        bool operator==(const PokemonView&) const = default;
    };

    // One player's side of the table, as the viewer sees it.
    struct PlayerView {
        std::uint8_t hand_size = 0;
        std::uint8_t deck_size = 0;
        std::uint8_t prize_count = 0;
        std::uint8_t mulligans = 0;
        std::uint8_t face_down = 0;
        FixedVector<CardIndex, kDeckSize> hand;
        FixedVector<DeckCard, kDeckSize> deck;
        FixedVector<CardIndex, kDeckSize> discard;
        FixedVector<CardIndex, kDeckSize> lost_zone;
        std::optional<PokemonView> active;
        FixedVector<PokemonView, kMaxBenchSize> bench;
        FixedVector<CardIndex, 2> resolving;
        CardIndex stadium = kNoCard;

        bool operator==(const PlayerView&) const = default;
    };

    struct Observation {
        std::uint8_t viewer = 0;
        std::uint8_t current_player = 0;
        std::uint8_t first_player = 0;
        std::uint16_t turn = 0;
        TurnPhase phase = TurnPhase::Setup;
        TurnFlags flags;
        GameHistory history;
        FixedVector<LastingEffect, 8> lasting;
        Decision decision; 
        ProgramRef effect; 
        GameResult result = GameResult::Ongoing;
        std::array<PlayerView, kPlayers> players;
        FixedVector<CardIndex, kDeckSize> deck_or_prizes; 
        FixedVector<CardIndex, kDeckSize> opponent_seen;  
        std::array<const CardDef*, kMaxCards> defs{}; 

        const CardDef* def(CardIndex card) const { return defs[card]; }
        const PlayerView& player(int p) const { return players[static_cast<std::size_t>(p)]; }
        const PlayerView& me() const { return player(viewer); }
        const PlayerView& opponent() const { return player(opponent_of(viewer)); }
        bool is_over() const { return result != GameResult::Ongoing; }

        bool operator==(const Observation&) const = default;
    };

    static_assert(std::is_trivially_copyable_v<Observation>, "Observation must stay a plain value, like GameState");

    // What `viewer` can see of the game right now.
    Observation observe(const GameState& game, int viewer);

} // namespace pokebot
