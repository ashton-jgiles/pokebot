#pragma once

#include <cstdint>

#include "engine/game/game_state.hpp"

namespace pokebot {

    enum class EventKind : std::uint8_t {
        PutOnBench, 
        Evolved,      
        EnergyAttached, 
        Retreated,      
        MovedToBench, 
        DamagedByAttack, 
        KnockedOut, 
        AttackDone,
        TurnEnding,
        Checkup,
        TrainerPlayed,
    };

    struct Event {
        EventKind kind = EventKind::TurnEnding;
        std::uint8_t player = 0;    
        CardIndex pokemon = kNoCard; 
        CardIndex card = kNoCard;  
    };

    const char* to_string(EventKind kind);

    using EventHook = void (*)(const GameState& game, const Event& event);
    void set_event_hook(EventHook hook);

} // namespace pokebot
