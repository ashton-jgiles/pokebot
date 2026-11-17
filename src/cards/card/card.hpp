#pragma once

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include "cards/card/card_types.hpp"
#include "cards/effects/program.hpp"

namespace pokebot {

    struct Weakness {
        EnergyType type = EnergyType::Colorless;
        int multiplier = 2;
    };

    struct Resistance {
        EnergyType type = EnergyType::Colorless;
        int amount = 30;
    };

    struct Ability {
        std::string name;
        std::string text;
        nlohmann::json effects = nlohmann::json::array();
        nlohmann::json passive = nlohmann::json::array();
        std::vector<Modifier> modifiers;

        // An Ability you choose to use: its effects compile to `program`, `requires` to `conditions`.
        nlohmann::json requirements = nlohmann::json::array();
        Program program;
        std::vector<Condition> conditions;
        bool once_per_turn = true; // "Once during your turn"
        bool one_per_name = false; // "You can't use more than 1 <name> Ability each turn"
        bool active_only = false;  // "if this Pokemon is in the Active Spot"
    };

    struct Attack {
        std::string name;
        std::vector<EnergyType> cost;
        int damage = 0;
        DamageMod damage_mod = DamageMod::None;
        std::string text;
        nlohmann::json effects = nlohmann::json::array();
        Program program;
    };

    struct CardRules {
        bool ex = false;
        bool mega = false;
        bool tera = false;
        bool ace_spec = false;
    };

    struct CardDef {
        std::string id;
        std::string tcgdex_id;
        std::string name;
        CardKind kind = CardKind::Pokemon;
        CardRules rules;

        // Pokemon
        Stage stage = Stage::None;
        std::string evolves_from;
        int hp = 0;
        std::vector<EnergyType> types;
        std::optional<Weakness> weakness;
        std::optional<Resistance> resistance;
        int retreat = 0;
        std::vector<Ability> abilities;
        std::vector<Attack> attacks;

        // Trainer and energy
        TrainerKind trainer_kind = TrainerKind::None;
        EnergyKind energy_kind = EnergyKind::None;
        std::vector<EnergyType> provides;
        std::string text;
        nlohmann::json effects = nlohmann::json::array();
        nlohmann::json requirements = nlohmann::json::array();
        Program program;
        std::vector<Condition> conditions;
        nlohmann::json passive = nlohmann::json::array();
        std::vector<Modifier> modifiers;

        int prize_value() const;
    };

    std::string_view to_string(EnergyType type);
    EnergyType energy_type_from_string(std::string_view name);
    CardKind card_kind_from_string(std::string_view name);

    void to_json(nlohmann::json& j, const CardDef& card);
    void from_json(const nlohmann::json& j, CardDef& card);

} // namespace pokebot