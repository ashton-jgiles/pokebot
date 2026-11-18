#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "engine/game/game_state.hpp"

// What one player can see of a game: everything an agent is allowed to know.
namespace pokebot {

    // A card in a deck whose place the viewer knows. `from_top` 0 is the top card.
    struct DeckCard {
        CardIndex card = kNoCard;
        std::uint8_t from_top = 0;

        bool operator==(const DeckCard&) const = default;
    };

    // A Pokemon in play, with its HP worked out by the engine (Tools and Stadiums can change it).
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
        std::uint8_t face_down = 0;             // Pokemon put down during setup that the viewer can't see yet
        FixedVector<CardIndex, kDeckSize> hand; // the cards in hand the viewer knows: all of their own
        FixedVector<DeckCard, kDeckSize> deck;  // the deck cards whose place the viewer knows, top first
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
        Decision decision; // what's being decided, unless the game is over
        ProgramRef effect; // the attack, Trainer or Ability resolving (card is kNoCard if none)
        GameResult result = GameResult::Ongoing;
        std::array<PlayerView, kPlayers> players;
        FixedVector<CardIndex, kDeckSize> deck_or_prizes; // the viewer's cards whose place they don't know, sorted
        FixedVector<CardIndex, kDeckSize> opponent_seen;  // every opponent card the viewer has seen this game, sorted
        std::array<const CardDef*, kMaxCards> defs{};     // nullptr for opponent cards the viewer hasn't seen

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
