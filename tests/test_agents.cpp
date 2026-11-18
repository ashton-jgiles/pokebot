#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random_agent.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::Observation;

namespace {

    const Action kEndTurn{ActionKind::EndTurn, pokebot::kNoCard, pokebot::kActiveSlot, 0};
    const Action kAttack{ActionKind::Attack, pokebot::kNoCard, pokebot::kActiveSlot, 0};
    const Action kRetreat{ActionKind::Retreat, pokebot::kNoCard, 1, 0};

} // namespace

TEST_CASE("the random agent picks every choice, and only from the list", "[agents]") {
    // The random agents don't look at what they're shown, so an empty observation is fine here.
    const Observation observation;
    const std::vector<Action> actions{kEndTurn, kAttack, kRetreat};
    pokebot::RandomAgent agent(1);

    int counts[3] = {0, 0, 0};
    for (int i = 0; i < 3000; ++i) {
        const Action chosen = agent.choose(observation, actions);
        if (chosen == kEndTurn) {
            ++counts[0];
        } else if (chosen == kAttack) {
            ++counts[1];
        } else if (chosen == kRetreat) {
            ++counts[2];
        } else {
            FAIL("chose an action that wasn't offered");
        }
    }
    for (const int count : counts) {
        CHECK(count > 800); // about 1000 each
    }
}

TEST_CASE("the biased agent attacks whenever it can, most of the time", "[agents]") {
    const Observation observation;
    const std::vector<Action> actions{kEndTurn, kRetreat, kAttack};
    pokebot::BiasedRandomAgent agent(2);

    int attacks = 0;
    for (int i = 0; i < 1000; ++i) {
        if (agent.choose(observation, actions) == kAttack) {
            ++attacks;
        }
    }
    CHECK(attacks > 950); // weight 100 against 1 + 1
}

TEST_CASE("the same seed makes the same choices", "[agents]") {
    const Observation observation;
    const std::vector<Action> actions{kEndTurn, kAttack, kRetreat};
    pokebot::RandomAgent a(7);
    pokebot::RandomAgent b(7);
    for (int i = 0; i < 100; ++i) {
        CHECK(a.choose(observation, actions) == b.choose(observation, actions));
    }
}
