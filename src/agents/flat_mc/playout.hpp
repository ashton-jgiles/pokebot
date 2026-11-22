#pragma once

#include <cstdint>
#include <string_view>

#include "agents/agent.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    enum class PlayoutPolicy : std::uint8_t {
        Greedy,
        Biased,
    };

    const char* to_string(PlayoutPolicy policy);
    PlayoutPolicy playout_policy(std::string_view name);

    struct PlayoutOptions {
        int depth = 1;
        PlayoutPolicy policy = PlayoutPolicy::Greedy;
        int greedy_worlds = 1;
        int max_actions = 2000;
        Weights weights = default_weights();
    };

    struct PlayoutResult {
        double value = 0.0;
        int actions = 0;
        int turns = 0;
        bool over = false;
        GameState end;
    };

    PlayoutResult play_out(const GameState& game, int player, Agent& mine, Agent& theirs, const PlayoutOptions& options);
    PlayoutResult play_out(const GameState& game, int player, std::uint64_t seed, const PlayoutOptions& options = {});

} // namespace pokebot