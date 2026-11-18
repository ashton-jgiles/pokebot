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

    // One unit of Energy, as the set of types it can count as: a Basic {W} Energy is just {W}; Neo Upper
    // Energy on a Stage 2 Pokemon is two units that can each be any type.
    using EnergyMask = std::uint16_t;
    constexpr EnergyMask kAnyEnergy = 0x07FF;

    constexpr EnergyMask energy_mask(EnergyType type) {
        return static_cast<EnergyMask>(1u << static_cast<unsigned>(type));
    }

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
