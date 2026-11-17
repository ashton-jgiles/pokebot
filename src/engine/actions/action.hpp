#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/game/game_state.hpp"

namespace pokebot {

    using Slot = std::uint8_t;
    constexpr Slot kActiveSlot = 0;

    constexpr Slot bench_slot(std::size_t bench_index) {
        return static_cast<Slot>(bench_index + 1);
    }

    enum class ActionKind : std::uint8_t {
        // setup
        ChooseTurnOrder,
        SetupPlaceActive,
        SetupPlaceBench,
        SetupDone,
        DrawMulliganCards,

        // main phase
        PlayBasic,
        Evolve,
        AttachEnergy,
        Retreat,
        PlayTrainer,
        AttachTool,
        Attack,
        EndTurn,

        // decision answers
        DiscardEnergy,
        Promote,
        ChooseCard,
        ChooseDone,
        ChoosePokemon,
        AnswerYes,
        AnswerNo,
    };

    struct Action {
        ActionKind kind = ActionKind::EndTurn;
        CardIndex card = kNoCard;
        Slot target = kActiveSlot;
        std::uint8_t amount = 0;

        bool operator==(const Action&) const = default;
    };

} // namespace pokebot
