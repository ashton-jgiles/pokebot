#pragma once

#include <cstdint>

#include "engine/game/game_state.hpp"

namespace pokebot {

    // Moments cards can react to. The engine raises each one where it happens.
    enum class EventKind : std::uint8_t {
        PutOnBench,      // a Basic Pokemon was played from hand onto the Bench
        Evolved,         // a Pokemon evolved; card is the evolution card
        EnergyAttached,  // Energy was attached from hand; card is the Energy
        Retreated,       // the Active Pokemon retreated
        MovedToBench,    // an Active Pokemon moved to the Bench
        DamagedByAttack, // card is the attacker's id
        KnockedOut,      // raised before the Pokemon and its cards leave play
        AttackDone,      // after the attack and its effects, before Knock Outs are handled
        TurnEnding,
        Checkup,
        TrainerPlayed,
    };

    struct Event {
        EventKind kind = EventKind::TurnEnding;
        std::uint8_t player = 0;     // whose Pokemon or card it happened to
        CardIndex pokemon = kNoCard; // id() of the Pokemon involved
        CardIndex card = kNoCard;    // the other card involved, if any
    };

    const char* to_string(EventKind kind);

    // Lets a tool or test watch every event. It lives outside GameState, so search copies don't carry it.
    using EventHook = void (*)(const GameState& game, const Event& event);
    void set_event_hook(EventHook hook);

} // namespace pokebot
