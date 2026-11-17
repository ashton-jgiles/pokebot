#include "engine/events/events.hpp"

#include "engine/internal.hpp"

namespace pokebot {

    namespace {

        thread_local EventHook event_hook = nullptr;

    } // namespace

    const char* to_string(EventKind kind) {
        switch (kind) {
            case EventKind::PutOnBench:
                return "put on Bench";
            case EventKind::Evolved:
                return "evolved";
            case EventKind::EnergyAttached:
                return "Energy attached";
            case EventKind::Retreated:
                return "retreated";
            case EventKind::MovedToBench:
                return "moved to Bench";
            case EventKind::DamagedByAttack:
                return "damaged by an attack";
            case EventKind::KnockedOut:
                return "Knocked Out";
            case EventKind::AttackDone:
                return "attack done";
            case EventKind::TurnEnding:
                return "turn ending";
            case EventKind::Checkup:
                return "Pokemon Checkup";
            case EventKind::TrainerPlayed:
                return "trainer played";
        }
        return "unknown event";
    }

    void set_event_hook(EventHook hook) {
        event_hook = hook;
    }

    namespace detail {

        // Abilities, Tools, Stadiums and Special Energy that react to events get queued here in step 3.7.
        void raise(GameState& game, const Event& event) {
            if (event_hook != nullptr) {
                event_hook(game, event);
            }
        }

    } // namespace detail

} // namespace pokebot
