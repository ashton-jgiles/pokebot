#include "agents/is_mcts/tree.hpp"

#include <cmath>
#include <stdexcept>

namespace pokebot {

    double TreeNode::mean(int player) const {
        return visits == 0 ? 0.0 : reward[static_cast<std::size_t>(player)] / static_cast<double>(visits);
    }

    SearchTree::SearchTree(std::uint64_t root_key, int root_actor) {
        TreeNode root;
        root.key = root_key;
        root.actor = root_actor;
        nodes_.push_back(root);
    }

    std::size_t SearchTree::find_child(std::size_t parent, std::uint64_t key) const {
        for (const std::size_t child : nodes_[parent].children) {
            if (nodes_[child].key == key) {
                return child;
            }
        }
        return kNone;
    }

    std::size_t SearchTree::add_child(std::size_t parent, std::uint64_t key, int actor, const Plan& move) {
        if (find_child(parent, key) != kNone) {
            throw std::logic_error("SearchTree::add_child: the parent already has a child with that key");
        }
        TreeNode child;
        child.key = key;
        child.actor = actor;
        child.move = move;
        child.availability = 1;
        const std::size_t index = nodes_.size();
        nodes_.push_back(child);
        nodes_[parent].children.push_back(index);
        return index;
    }

    void SearchTree::mark_available(const std::vector<std::size_t>& children) {
        for (const std::size_t child : children) {
            ++nodes_[child].availability;
        }
    }

    std::size_t SearchTree::select(std::size_t parent, const std::vector<std::size_t>& available, double exploration) const {
        if (available.empty()) {
            throw std::invalid_argument("SearchTree::select: no children to choose from");
        }
        const int actor = nodes_[parent].actor;
        std::size_t best = available.front();
        double best_score = -std::numeric_limits<double>::infinity();
        for (const std::size_t child : available) {
            const TreeNode& c = nodes_[child];
            if (c.visits == 0) {
                return child;
            }
            const double score = c.mean(actor) + exploration * std::sqrt(std::log(static_cast<double>(c.availability)) / static_cast<double>(c.visits));
            if (score > best_score) {
                best = child;
                best_score = score;
            }
        }
        return best;
    }

    void SearchTree::backpropagate(const std::vector<std::size_t>& path, double value) {
        for (const std::size_t index : path) {
            TreeNode& n = nodes_[index];
            ++n.visits;
            n.reward[0] += value;
            n.reward[1] -= value;
        }
    }

    std::size_t SearchTree::most_visited(std::size_t parent) const {
        const int actor = nodes_[parent].actor;
        std::size_t best = kNone;
        for (const std::size_t child : nodes_[parent].children) {
            if (best == kNone) {
                best = child;
                continue;
            }
            const TreeNode& c = nodes_[child];
            const TreeNode& b = nodes_[best];
            if (c.visits > b.visits || (c.visits == b.visits && c.mean(actor) > b.mean(actor))) {
                best = child;
            }
        }
        return best;
    }

} // namespace pokebot