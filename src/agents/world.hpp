#pragma once

#include "engine/game/game_state.hpp"
#include "engine/observation/observation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    class DeckModel;
    struct ArchetypeList;

    const CardDef& unknown_card();

    class World {
    public:
        World(const Observation& observation, Rng& rng, const DeckModel* model = nullptr);
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        const GameState& game() const { return game_; }
        const ArchetypeList* list() const { return list_; }

    private:
        const ArchetypeList* list_ = nullptr;
        CardTable cards_;
        GameState game_;
    };

} // namespace pokebot