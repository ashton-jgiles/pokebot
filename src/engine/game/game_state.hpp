#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "engine/shared/fixed_vector.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    using CardIndex = std::uint8_t;
    constexpr CardIndex kNoCard = 255;

    constexpr int kPlayers = 2;
    constexpr std::size_t kDeckSize = 60;
    constexpr std::size_t kMaxCards = kPlayers * kDeckSize;
    constexpr std::size_t kBenchSize = 5;
    constexpr std::size_t kPrizeCount = 6;
    constexpr std::size_t kMaxEvolutionStack = 3;

    constexpr int owner_of(CardIndex card) {
        return card < kDeckSize ? 0 : 1;
    }

    constexpr int opponent_of(int player) {
        return 1 - player;
    }

    class CardTable {
    public:
        CardTable(const CardDB& db, const Decklist& deck0, const Decklist& deck1);

        const CardDef& def(CardIndex card) const { return *defs_[card]; }

    private:
        std::array<const CardDef*, kMaxCards> defs_{};
    };

    struct PokemonInPlay {
        FixedVector<CardIndex, kMaxEvolutionStack> stack;
        FixedVector<CardIndex, kDeckSize> energy;
        CardIndex tool = kNoCard;
        std::uint16_t damage_counters = 0;
        std::uint16_t turn_played = 0;

        CardIndex id() const { return stack.front(); }
        CardIndex top() const { return stack.back(); }
        bool operator==(const PokemonInPlay&) const = default;
    };

    struct PlayerState {
        FixedVector<CardIndex, kDeckSize> deck;
        FixedVector<CardIndex, kDeckSize> hand;
        FixedVector<CardIndex, kPrizeCount> prizes;
        FixedVector<CardIndex, kDeckSize> discard;
        FixedVector<CardIndex, kDeckSize> lost_zone;
        std::optional<PokemonInPlay> active;
        FixedVector<PokemonInPlay, kBenchSize> bench;
        std::uint8_t mulligans = 0;

        bool operator==(const PlayerState&) const = default;
    };

    struct TurnFlags {
        bool energy_attached = false;
        bool supporter_played = false;
        bool stadium_played = false;
        bool retreated = false;
        std::uint8_t attacks_used = 0;

        bool operator==(const TurnFlags&) const = default;
    };

    enum class TurnPhase : std::uint8_t {
        Setup,
        Main,
        AfterMain,
        Checkup,
    };

    enum class DecisionKind : std::uint8_t {
        ChooseTurnOrder,
        SetupActive,
        SetupBench,
        MulliganDraws,
        MainPhase,
        RetreatDiscard,
        PromoteActive,
        ExtraAttack,
    };

    struct Decision {
        DecisionKind kind = DecisionKind::MainPhase;
        std::uint8_t player = 0;
        std::uint8_t amount = 0;
        std::uint8_t target = 0;

        bool operator==(const Decision&) const = default;
    };

    struct AttackRef {
        CardIndex text_from = kNoCard;
        std::uint8_t index = 0;

        bool operator==(const AttackRef&) const = default;
    };

    struct EffectContext {
        std::uint8_t player = 0;
        CardIndex self = kNoCard;
        CardIndex source = kNoCard;

        bool operator==(const EffectContext&) const = default;
    };

    struct TurnRecord {
        std::uint8_t player = 0;
        std::array<std::uint8_t, kPlayers> knocked_out{};
        CardIndex attacker = kNoCard;
        AttackRef attack;

        bool operator==(const TurnRecord&) const = default;
    };

    struct GameHistory {
        std::array<TurnRecord, 3> turns{};

        TurnRecord& this_turn() { return turns[0]; }
        const TurnRecord& this_turn() const { return turns[0]; }
        const TurnRecord& opponents_last_turn() const { return turns[1]; }
        const TurnRecord& your_last_turn() const { return turns[2]; }

        void start_turn(std::uint8_t player) {
            turns[2] = turns[1];
            turns[1] = turns[0];
            turns[0] = TurnRecord{};
            turns[0].player = player;
        }

        bool operator==(const GameHistory&) const = default;
    };

    enum class GameResult : std::uint8_t {
        Ongoing,
        Player0Wins,
        Player1Wins,
        Draw
    };

    struct GameState {
        const CardTable* cards = nullptr;
        std::array<PlayerState, kPlayers> players;
        std::uint8_t current_player = 0;
        std::uint8_t first_player = 0;
        std::uint16_t turn = 0;
        TurnPhase phase = TurnPhase::Setup;
        TurnFlags flags;
        GameHistory history;
        FixedVector<Decision, 8> pending;
        Rng rng;
        GameResult result = GameResult::Ongoing;

        PlayerState& player(int p) { return players[static_cast<std::size_t>(p)]; }
        const PlayerState& player(int p) const { return players[static_cast<std::size_t>(p)]; }
        const CardDef& def(CardIndex card) const { return cards->def(card); }
        const Decision& decision() const { return pending.back(); }
        bool is_over() const { return result != GameResult::Ongoing; }

        bool operator==(const GameState&) const = default;
    };

    static_assert(std::is_trivially_copyable_v<GameState>, "GameState must stay a plain value: copying it is how search explores the game");

} // namespace pokebot
