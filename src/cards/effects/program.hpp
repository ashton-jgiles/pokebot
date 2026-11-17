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
    };

    enum class Op : std::uint8_t {
        Damage,
        Draw,
        DiscardHand,
        ShuffleDeck,
        ChooseCards,
        MoveChosen,
        ChooseBenched,
        SwitchWithChosen,
        AskYesNo,
    };

    struct CardFilter {
        std::optional<CardKind> kind;
        std::optional<EnergyType> type;
        bool basic = false;
        std::string name;
    };

    struct Step {
        Op op = Op::Damage;
        std::uint8_t min = 0;
        std::uint8_t max = 0;
        Zone from = Zone::Deck;
        Zone to = Zone::Hand;
        CardFilter filter;
        std::uint8_t jump = 0;
    };

    using Program = std::vector<Step>;

    bool matches(const CardFilter& filter, const CardDef& card);
    Program compile_attack(const nlohmann::json& effects);
    const char* to_string(Op op);

} // namespace pokebot
