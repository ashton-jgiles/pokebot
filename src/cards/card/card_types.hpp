#pragma once

#include <cstdint>
#include <stdexcept>

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

    enum class SpecialCondition : std::uint8_t {
        Asleep,
        Burned,
        Confused,
        Paralyzed,
        Poisoned,
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

} // namespace pokebot
