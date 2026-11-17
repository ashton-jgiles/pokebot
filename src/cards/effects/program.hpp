#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/card/card_types.hpp"

namespace pokebot {

    struct CardDef;

    enum class Zone : std::uint8_t {
        Deck,
        Hand,
        Discard,
        AttachedEnergy,
        Bench,
        YourAttachedEnergy,
        DeckTop,
        InPlay,       // the Active Pokemon and the Bench
        TargetEnergy, // Energy attached to the first chosen target
    };

    enum class Side : std::uint8_t {
        You,
        Opponent,
        Each,
    };

    enum class Op : std::uint8_t {
        Damage,
        Draw,
        DiscardHand,
        ShuffleDeck,
        ShuffleHandIntoDeck,
        ChooseCards,
        MoveChosen,
        ChooseBenched,
        SwitchWithChosen,
        AskYesNo,
        JumpUnless,
        Jump,
        ApplySpecialCondition,
        SetDamage,
        AddDamage,
        FlipCoins,
        FlipUntilTails,
        HandToDeckBottom,
        ChooseTargets,
        DamageTargets,
        PlaceCounters,
        DiscardAllEnergy,
        ReturnSelfToHand,
        ChooseAttack,
        UseChosenAttack,
        AddLasting,
    };

    enum class StageFilter : std::uint8_t {
        Any,
        Basic,
        Stage1,
        Stage2,
        Evolution,
    };

    struct CardFilter {
        std::optional<CardKind> kind;
        std::optional<TrainerKind> trainer;
        std::optional<EnergyType> type;
        StageFilter stage = StageFilter::Any;
        bool basic = false;
        std::optional<bool> rule_box;
        int max_hp = 0;
        std::string name;
        std::string name_prefix;
        std::vector<CardFilter> any_of;
    };

    enum class ConditionKind : std::uint8_t {
        HandBesides,
        PrizesExactly,
        KnockedOutLastTurn,
        HasBenched,
        OpponentHasBenched,
        BenchNotFull,
        DiscardHas,
        ValueAtLeast,
        OpponentPrizesAtMost,
        SharesType,
        SelfEnergyAtLeast,
        KnockedOutByAttackLastTurn,
    };

    struct Condition {
        ConditionKind kind = ConditionKind::HandBesides;
        int value = 0;
        CardFilter filter;
    };

    enum class CountKind : std::uint8_t {
        None,
        Value,
        Hand,
        Discard,
        Benched,
        PrizesTaken,
        ActiveEnergy,
        DamageCountersOnSelf,
        TargetDamageCounters,
    };

    struct Count {
        CountKind kind = CountKind::None;
        Side side = Side::You;
        CardFilter filter;
    };

    enum class LastingKind : std::uint8_t {
        CantAttack,
        CantUseAttack,
        CantRetreat,
        NoItems,
        DamageBonus,
    };

    enum class Until : std::uint8_t {
        ThisTurn,
        OpponentsNextTurn,
        YourNextTurn,
    };

    enum class LastingOn : std::uint8_t {
        Self,
        Defending,
        Player,
    };

    struct LastingSpec {
        LastingKind kind = LastingKind::CantAttack;
        Until until = Until::ThisTurn;
        LastingOn on = LastingOn::Self;
        bool only_ex = false;
    };

    struct Step {
        Op op = Op::Damage;
        std::uint8_t min = 0;
        std::uint8_t max = 0;
        Zone from = Zone::Deck;
        Zone to = Zone::Hand;
        Side side = Side::You;
        CardFilter filter;
        Condition condition;
        SpecialCondition special = SpecialCondition::Poisoned;
        std::int16_t amount = 0;
        std::int16_t per = 0;
        Count count;
        bool needs_energy = false;
        LastingSpec lasting;
        std::uint8_t jump = 0;
    };

    using Program = std::vector<Step>;

    bool matches(const CardFilter& filter, const CardDef& card);
    Program compile_attack(const nlohmann::json& effects);
    Program compile_trainer(const nlohmann::json& effects);
    std::vector<Condition> compile_conditions(const nlohmann::json& requirements);
    const char* to_string(Op op);

} // namespace pokebot
