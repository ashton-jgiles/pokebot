#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "agents/planner/plan.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    struct TreeNode {
        std::uint64_t key = 0;
        int actor = 0;
        Plan move;
        long long visits = 0;
        long long availability = 0;
        std::array<double, kPlayers> reward{};
        std::vector<std::size_t> children;

        double mean(int player) const;
    };

    class SearchTree {
    public:
        static constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

        SearchTree(std::uint64_t root_key, int root_actor);

        std::size_t root() const { return 0; }
        const TreeNode& node(std::size_t index) const { return nodes_[index]; }
        std::size_t size() const { return nodes_.size(); }

        std::size_t find_child(std::size_t parent, std::uint64_t key) const;
        std::size_t add_child(std::size_t parent, std::uint64_t key, int actor, const Plan& move);
        void mark_available(const std::vector<std::size_t>& children);
        std::size_t select(std::size_t parent, const std::vector<std::size_t>& available, double exploration) const;
        void backpropagate(const std::vector<std::size_t>& path, double value);
        std::size_t most_visited(std::size_t parent) const;

    private:
        std::vector<TreeNode> nodes_;
    };

} // namespace pokebot