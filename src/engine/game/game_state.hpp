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

    struct SpecialConditions {
        bool asleep = false;
        bool paralyzed = false;
        bool burned = false;
        bool confused = false;
        std::uint8_t poison = 0;
        std::uint16_t paralyzed_turn = 0;

        bool any() const { return asleep || paralyzed || burned || confused || poison > 0; }

        bool operator==(const SpecialConditions&) const = default;
    };

    struct PokemonInPlay {
        FixedVector<CardIndex, kMaxEvolutionStack> stack;
        FixedVector<CardIndex, kDeckSize> energy;
        CardIndex tool = kNoCard;
        std::uint16_t damage_counters = 0;
        std::uint16_t turn_played = 0;
        SpecialConditions special;

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
        FixedVector<CardIndex, 2> resolving;
        CardIndex stadium = kNoCard;
        std::uint8_t mulligans = 0;

        bool operator==(const PlayerState&) const = default;
    };

    // An Ability used this turn: by which Pokemon, from which card, and which Ability on it.
    struct AbilityUse {
        CardIndex pokemon = kNoCard;
        CardIndex card = kNoCard;
        std::uint8_t index = 0;

        bool operator==(const AbilityUse&) const = default;
    };

    struct TurnFlags {
        bool energy_attached = false;
        bool supporter_played = false;
        bool stadium_played = false;
        bool stadium_used = false;
        bool retreated = false;
        std::uint8_t attacks_used = 0;
        FixedVector<AbilityUse, 12> abilities_used;

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
        ChooseCards,
        ChoosePokemon,
        ChooseYesNo,
        ChooseAttack,
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

    enum class ProgramKind : std::uint8_t {
        Attack,
        Trainer,
        Ability,
    };

    struct ProgramRef {
        CardIndex card = kNoCard;
        ProgramKind kind = ProgramKind::Attack;
        std::uint8_t index = 0;

        bool operator==(const ProgramRef&) const = default;
    };

    enum class AfterEffect : std::uint8_t {
        Nothing,
        FinishAttack,
        DiscardTrainer,
        FinishAbility, // resolve any Knock Outs, and choose a new Active if the Active Spot is empty
    };

    struct EffectRun {
        EffectContext who;
        ProgramRef program;
        AfterEffect then = AfterEffect::Nothing;
        std::uint8_t pc = 0;
        FixedVector<CardIndex, kDeckSize> chosen;
        CardIndex chosen_pokemon = kNoCard;
        std::int16_t damage = 0;           // the attack's damage so far
        std::int16_t value = 0;            // heads flipped, or cards just moved
        FixedVector<CardIndex, 6> targets; // ids of the Pokemon chosen as targets
        AttackRef chosen_attack;
        std::uint8_t target_index = 0;   // which target a loop over the targets is on
        bool attached_to_active = false; // this effect attached Energy to its player's Active Pokemon
        bool ignore_defender = false;    // the attack isn't affected by effects on the Defending Pokemon

        bool operator==(const EffectRun&) const = default;
    };

    // An effect that lasts for a turn. It applies only while `turn` is the current turn, and is removed after.
    struct LastingEffect {
        LastingKind kind = LastingKind::CantAttack;
        std::uint8_t player = 0;     // the player it affects
        CardIndex pokemon = kNoCard; // the Pokemon it's on, or kNoCard for the whole player
        AttackRef attack;            // CantUseAttack: which attack
        std::int16_t amount = 0;     // DamageBonus: how much
        bool only_ex = false;        // DamageBonus: only against Pokemon ex
        std::uint16_t turn = 0;

        bool operator==(const LastingEffect&) const = default;
    };

    struct TurnRecord {
        std::uint8_t player = 0;
        std::array<std::uint8_t, kPlayers> knocked_out{};
        std::array<std::uint8_t, kPlayers> knocked_out_by_attack{};
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
        FixedVector<EffectRun, 4> effects;
        FixedVector<LastingEffect, 8> lasting;
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
