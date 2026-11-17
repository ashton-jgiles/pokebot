#include "agents/random_agent.hpp"

namespace pokebot {

    namespace {

        // Relative chance of picking each kind of action.
        std::uint32_t weight(const Action& action) {
            switch (action.kind) {
                case ActionKind::Attack:
                    return 100;
                case ActionKind::Evolve:
                    return 10;
                case ActionKind::AttachEnergy:
                    return action.target == kActiveSlot ? 10 : 3;
                case ActionKind::PlayBasic:
                    return 5;
                default:
                    return 1;
            }
        }

    } // namespace

    Action RandomAgent::choose(const GameState& /*game*/, const std::vector<Action>& actions) {
        return actions[rng_.below(static_cast<std::uint32_t>(actions.size()))];
    }

    Action BiasedRandomAgent::choose(const GameState& /*game*/, const std::vector<Action>& actions) {
        std::uint32_t total = 0;
        for (const Action& action : actions) {
            total += weight(action);
        }
        std::uint32_t pick = rng_.below(total);
        for (const Action& action : actions) {
            if (pick < weight(action)) {
                return action;
            }
            pick -= weight(action);
        }
        return actions.back();
    }

} // namespace pokebot
