#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

#include "engine/game/game_state.hpp"

namespace pokebot {

    std::uint64_t hash_state(const GameState& game);

    class ClosedSet { 
        public:
            bool insert(const GameState& game) {
                return hashes_.insert(hash_state(game)).second;
            }
            bool contains(const GameState& game) const {
                return hashes_.count(hash_state(game)) != 0;
            }
            std::size_t size() const { return hashes_.size(); }
            void clear() { hashes_.clear(); }

        private:
            std::unordered_set<std::uint64_t> hashes_;
    };

} // namespace pokebot