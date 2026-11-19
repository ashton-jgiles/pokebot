#include "agents/or_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <stdexcept>
#include <vector>

#include "agents/action_class.hpp"
#include "agents/state_hash.hpp"
#include "engine/engine.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        bool in_play(const PlayerState& player, CardIndex pokemon) {
            if (player.active && player.active->id() == pokemon) {
                return true;
            }
            return std::any_of(player.bench.begin(), player.bench.end(), [&](const PokemonInPlay& benched) { return benched.id() == pokemon; });
        }

        bool has_attack(const std::vector<Action>& actions) {
            return std::any_of(actions.begin(), actions.end(), [](const Action& action) { return action.kind == ActionKind::Attack; });
        }

        class GoalTest {
        public:
            GoalTest(const GameState& root, int player, const Goal& goal)
                : player_(player), goal_(goal), turn_(root.turn), prizes_at_start_(root.player(player).prizes.size()) {
                const PlayerState& theirs = root.player(opponent_of(player));
                target_ = theirs.active ? theirs.active->id() : kNoCard;
                knocked_out_at_start_ = root.history.this_turn().knocked_out[static_cast<std::size_t>(opponent_of(player))];
            }

            bool reached(const GameState& game) const {
                switch (goal_.kind) {
                    case GoalKind::Win:
                        return game.result == (player_ == 0 ? GameResult::Player0Wins : GameResult::Player1Wins);
                    case GoalKind::KnockOut: {
                        const TurnRecord& record = game.turn == turn_ ? game.history.turns[0] : game.history.turns[1];
                        const auto opponent = static_cast<std::size_t>(opponent_of(player_));
                        return target_ != kNoCard && !in_play(game.player(opponent_of(player_)), target_) && record.knocked_out[opponent] > knocked_out_at_start_;
                    }
                    case GoalKind::Prizes:
                        return prizes_at_start_ - game.player(player_).prizes.size() >= static_cast<std::size_t>(goal_.prizes);
                }
                return false;
            }

        private:
            int player_;
            Goal goal_;
            std::uint16_t turn_;
            std::size_t prizes_at_start_;
            CardIndex target_ = kNoCard;
            std::uint8_t knocked_out_at_start_ = 0;
        };

        struct Node {
            GameState state;
            int parent = -1;
            Action action;
            int depth = 0;
        };

        struct Leaf {
            int h = 0;
            int depth = 0;
            long long made = 0;
            int parent = -1;
            Action action;
        };

        struct Later {
            bool operator()(const Leaf& a, const Leaf& b) const {
                if (a.h != b.h) {
                    return a.h > b.h;
                }
                if (a.depth != b.depth) {
                    return a.depth < b.depth;
                }
                return a.made > b.made;
            }
        };

    } // namespace

    int knock_out_distance(const GameState& game, int player) {
        const PlayerState& theirs = game.player(opponent_of(player));
        if (!theirs.active) {
            return 0;
        }
        const PokemonInPlay& target = *theirs.active;
        int most = 0;
        const auto consider = [&](const PokemonInPlay& pokemon) {
            const std::size_t count = game.def(pokemon.top()).attacks.size();
            for (std::size_t i = 0; i < count; ++i) {
                const Attack& attack = rules::attack_text(game, AttackRef{pokemon.top(), static_cast<std::uint8_t>(i)});
                most = std::max(most, rules::attack_damage(game, pokemon, target, attack.damage));
            }
        };
        const PlayerState& mine = game.player(player);
        if (mine.active) {
            consider(*mine.active);
        }
        for (const PokemonInPlay& pokemon : mine.bench) {
            consider(pokemon);
        }
        return std::max(0, rules::remaining_hp(game, target) - most);
    }

    int action_order(const GameState& game, const Action& action, bool enables_attack) {
        if (enables_attack) {
            return 0;
        }
        switch (action.kind) {
            case ActionKind::Evolve:
                return 1;
            case ActionKind::AttachEnergy:
                return 2;
            case ActionKind::PlayTrainer: {
                const TrainerKind kind = game.def(action.card).trainer_kind;
                if (kind == TrainerKind::Item) {
                    return 3;
                }
                return kind == TrainerKind::Supporter ? 4 : 5;
            }
            case ActionKind::PlayBasic:
                return 6;
            case ActionKind::Retreat:
                return 7;
            default:
                return 5;
        }
    }

    OrTreeResult or_tree_search(const GameState& game, int player, const Goal& goal, const OrTreeOptions& options) {
        if (game.is_over() || game.decision().player != player) {
            throw std::invalid_argument("or_tree_search: the player must be deciding");
        }
        const GoalTest test(game, player, goal);
        OrTreeResult result;
        ClosedSet closed;
        std::vector<Node> nodes;
        std::priority_queue<Leaf, std::vector<Leaf>, Later> open;
        long long made = 0;
        open.push(Leaf{knock_out_distance(game, player), 0, made++, -1, Action{}});

        const auto path_to = [&](int index) {
            std::vector<Action> path;
            for (int i = index; nodes[static_cast<std::size_t>(i)].parent >= 0; i = nodes[static_cast<std::size_t>(i)].parent) {
                path.push_back(nodes[static_cast<std::size_t>(i)].action);
            }
            std::reverse(path.begin(), path.end());
            return path;
        };

        while (!open.empty()) {
            if (result.expanded >= options.budget) {
                result.out_of_budget = true;
                break;
            }
            const Leaf leaf = open.top();
            open.pop();
            GameState state = leaf.parent < 0 ? game : nodes[static_cast<std::size_t>(leaf.parent)].state;
            if (leaf.parent >= 0) {
                apply_legal(state, leaf.action);
            }
            if (options.closed_set && !closed.insert(state)) {
                ++result.closed;
                continue;
            }
            nodes.push_back(Node{state, leaf.parent, leaf.action, leaf.depth});
            const int index = static_cast<int>(nodes.size()) - 1;
            ++result.expanded;
            if (test.reached(state)) {
                result.found = true;
                result.plan = Plan{path_to(index), ActionClass::Deterministic};
                return result;
            }

            const std::vector<Action> actions = legal_actions(state);
            const bool can_attack = has_attack(actions);
            struct Child {
                int order;
                Action action;
                int h;
            };
            std::vector<Child> children;
            for (const Action& action : actions) {
                const TriedAction tried = try_action(state, player, action);
                if (tried.kind == ActionClass::Final && test.reached(tried.after)) {
                    result.found = true;
                    result.plan = Plan{path_to(index), ActionClass::Final};
                    result.plan.actions.push_back(action);
                    return result;
                }
                if (tried.kind == ActionClass::Deterministic) {
                    const bool enables = !can_attack && has_attack(legal_actions(tried.after));
                    children.push_back(Child{action_order(state, action, enables), action, knock_out_distance(tried.after, player)});
                }
            }
            std::stable_sort(children.begin(), children.end(), [](const Child& a, const Child& b) { return a.order < b.order; });
            for (const Child& child : children) {
                open.push(Leaf{child.h, leaf.depth + 1, made++, index, child.action});
                ++result.generated;
            }
        }
        return result;
    }

} // namespace pokebot
