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
        TargetEnergy, // Energy attached to the target being worked on (the first, or the current one in a loop)
        ChosenEnergy, // Energy attached to the chosen Pokemon (chosen_pokemon)
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
        IgnoreDefenderEffects,
        CountersOnTargets,
        ShuffleSelfIntoDeck,
        RestToBottom,
        ChooseCounterSource,
        MoveCounters,
        CountersOnSelf,
        DiscardStadium,
        SwitchToActive,
        EachTarget,        // the start of a loop over the targets: done, carry on at `jump`
        NextTarget,        // the end of the loop: go back to `jump` for the next target
        ChooseDestination, // the player chooses another of their Pokemon (chosen_pokemon)
        Custom,            // a one-of-a-kind card's own C++ (engine/effects/custom.cpp)
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
        std::string not_name;
        std::optional<EnergyType> not_type;
        std::optional<bool> tera;
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
        HandHas,                // a card in your hand (other than the one being played) matches `filter`
        BenchHas,               // one of your Benched Pokemon matches `filter`
        ActiveIs,               // your Active Pokemon matches `filter`
        SelfHasEnergy,          // an Energy attached to this Pokemon matches `filter`
        YouHaveDamaged,         // one of your Pokemon in play has damage counters
        SelfIs,                 // "this Pokemon" matches `filter`
        StadiumInPlay,          // a Stadium is in play
        MorePrizesThanOpponent, // you have more Prize cards left than your opponent
        InPlayHas,              // one of your Pokemon in play matches `filter`
        YourEnergyHas,          // an Energy attached to one of your Pokemon matches `filter`
        AttachedToActive,       // this effect attached Energy to your Active Pokemon
        Custom,                 // a one-of-a-kind card's own requirement (`custom`)
    };

    // The one-of-a-kind cards with their own C++.
    enum class CustomEffect : std::uint8_t {
        RareCandy,
        TransformationTome,
    };

    // When a triggered effect happens on its own, without being chosen.
    enum class Trigger : std::uint8_t {
        None,
        PlayedToBench, // an Ability: "when you play this Pokemon from your hand onto your Bench"
        PutOnBench,    // a Stadium: "whenever any player puts a Pokemon onto their Bench during their turn"
    };

    struct Condition {
        ConditionKind kind = ConditionKind::HandBesides;
        int value = 0;
        CardFilter filter;
        CustomEffect custom = CustomEffect::RareCandy;
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
        CustomEffect custom = CustomEffect::RareCandy;
        std::uint8_t jump = 0;
        bool reveal = false; // MoveChosen: the opponent sees the cards ("reveal them")
    };

    using Program = std::vector<Step>;

    // An always-on change to a rule question, from a Tool, a Stadium or a passive Ability.
    enum class Modifies : std::uint8_t {
        MaxHp,
        RetreatCost,
        WeaknessType,
        DamageDealt, // attacks by the Pokemon do `add` more to the opponent's Active (before Weakness)
        BenchSize,   // a player's Bench can hold `set` Pokemon
    };

    enum class AppliesTo : std::uint8_t {
        Holder,    // the Pokemon the Tool is attached to, or the Pokemon with the Ability
        Yours,     // every Pokemon in play of the card's owner
        Opponents, // every Pokemon in play of the owner's opponent
        All,       // every Pokemon in play
    };

    struct Modifier {
        Modifies what = Modifies::MaxHp;
        AppliesTo to = AppliesTo::Holder;
        int add = 0;
        std::optional<int> set;
        std::optional<EnergyType> type;
        CardFilter filter;
        bool only_if_poisoned = false;
        bool only_with_tera = false; // only for a player with a Tera Pokemon in play
    };

    bool matches(const CardFilter& filter, const CardDef& card);
    Program compile_attack(const nlohmann::json& effects);
    Program compile_trainer(const nlohmann::json& effects);
    std::vector<Condition> compile_conditions(const nlohmann::json& requirements);
    std::vector<Modifier> compile_passive(const nlohmann::json& passive);

    // A Special Energy that provides something else when attached to a Pokemon that matches `holder`.
    struct ConditionalProvides {
        CardFilter holder;
        std::vector<EnergyMask> provides;
    };

    std::vector<ConditionalProvides> compile_provides_when(const nlohmann::json& provides_when);
    Trigger trigger_from_string(const std::string& name);
    const char* to_string(Trigger trigger);
    const char* to_string(Op op);

} // namespace pokebot
