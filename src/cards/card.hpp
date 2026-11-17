#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace pokebot {

    enum class EnergyType : std::uint8_t {
        Grass,
        Fire,
        Water,
        Lightning,
        Psychic,
        Fighting,
        Darkness,
        Metal,
        Fairy,
        Dragon,
        Colorless
    };

    enum class CardKind : std::uint8_t {
        Pokemon,
        Trainer,
        Energy
    };
    enum class Stage : std::uint8_t {
        None,
        Basic,
        Stage1,
        Stage2
    };
    enum class TrainerKind : std::uint8_t {
        None,
        Item,
        Supporter,
        Stadium,
        Tool
    };
    enum class EnergyKind : std::uint8_t {
        None,
        Basic,
        Special
    };
    enum class DamageMod : std::uint8_t {
        None,
        Plus,
        Minus,
        Times
    };

    class CardDataError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

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
    };

    struct Attack {
        std::string name;
        std::vector<EnergyType> cost;
        int damage = 0;
        DamageMod damage_mod = DamageMod::None;
        std::string text;
        nlohmann::json effects = nlohmann::json::array();
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

        int prize_value() const;
    };

    std::string_view to_string(EnergyType type);
    EnergyType energy_type_from_string(std::string_view name);

    void to_json(nlohmann::json& j, const CardDef& card);
    void from_json(const nlohmann::json& j, CardDef& card);

} // namespace pokebot