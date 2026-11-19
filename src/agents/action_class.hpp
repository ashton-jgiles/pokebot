#pragma once

#include <cstdint>

#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    enum class ActionClass : std::uint8_t {
        Deterministic,
        Final,
        Uncertain,
    };

    const char* to_string(ActionClass kind);

    struct TriedAction {
        ActionClass kind = ActionClass::Uncertain;
        GameState after;
    };

    TriedAction try_action(const GameState& game, int player, const Action& action);
    
} // namespace pokebot