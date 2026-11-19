#pragma once

#include "engine/game/game_state.hpp"
#include "engine/observation/observation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    const CardDef& unknown_card();
    
    class World {
        public:
            World(const Observation& observation, Rng& rng);
            World(const World&) = delete;
            World& operator=(const World&) = delete;
            const GameState& game() const { return game_; }
        
        private:
            CardTable cards_;
            GameState game_;
    };
    
} // namespace pokebot