#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random_agent.hpp"
#include "cards/db/card_db.hpp"
#include "engine/describe/describe.hpp"
#include "engine/engine.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"

using pokebot::CardTable;
using pokebot::GameEnd;
using test::load_real_decks;
using test::RealDecks;

namespace {

    template <class AgentType>
    std::array<int, 5> play_many(const RealDecks& real, int games) {
        std::array<int, 5> ends{};
        const std::size_t n = real.decks.size();
        for (int i = 0; i < games; ++i) {
            const auto index = static_cast<std::size_t>(i);
            const CardTable table(real.db, real.decks[index % n], real.decks[(index / n) % n]);
            const auto seed = static_cast<std::uint64_t>(i);
            AgentType agent0(seed * 2);
            AgentType agent1(seed * 2 + 1);
            // play_game throws, with the seed and the broken rules, if any invariant breaks.
            const auto record = pokebot::play_game(table, seed, agent0, agent1);
            ++ends[static_cast<std::size_t>(record.end)];
        }
        return ends;
    }

    std::string summary(const std::array<int, 5>& ends) {
        std::string text;
        for (std::size_t end = 0; end < ends.size(); ++end) {
            text += std::string(pokebot::to_string(static_cast<GameEnd>(end))) + ": " + std::to_string(ends[end]) + "  ";
        }
        return text;
    }

} // namespace

// The full 100,000-game run is `pokebot-sim --games 100000`; this smaller version runs with every ctest.
TEST_CASE("1,000 biased random games on the real decks never break an invariant", "[stress]") {
    const auto real = load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const auto ends = play_many<pokebot::BiasedRandomAgent>(*real, 1000);
    INFO(summary(ends));
    CHECK(ends[static_cast<std::size_t>(GameEnd::ActionLimit)] == 0);
    // The biased agent exists to exercise combat, so plenty of games should end by Knock Outs. Random
    // players also burn through their decks with draw and search cards, so deck-outs are common too.
    const int by_knock_outs = ends[static_cast<std::size_t>(GameEnd::PrizeCards)] + ends[static_cast<std::size_t>(GameEnd::NoPokemon)];
    CHECK(by_knock_outs > 300);
}

TEST_CASE("1,000 uniform random games on the real decks never break an invariant", "[stress]") {
    const auto real = load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const auto ends = play_many<pokebot::RandomAgent>(*real, 1000);
    INFO(summary(ends));
    CHECK(ends[static_cast<std::size_t>(GameEnd::ActionLimit)] == 0);
}

TEST_CASE("every state and every legal action can be described", "[stress][describe]") {
    const auto real = load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardTable table(real->db, real->decks[0], real->decks[1 % real->decks.size()]);
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        auto game = pokebot::new_game(table, seed);
        pokebot::BiasedRandomAgent agent(seed);
        while (!game.is_over()) {
            const std::string state = pokebot::describe(game);
            REQUIRE(state.find("Turn ") == 0);
            REQUIRE(state.find("Player 0") != std::string::npos);
            const auto actions = pokebot::legal_actions(game);
            for (const auto& action : actions) {
                const std::string text = pokebot::describe(game, action);
                REQUIRE_FALSE(text.empty());
                REQUIRE(text.find("unknown") == std::string::npos);
            }
            pokebot::apply(game, agent.choose(game, actions));
        }
        CHECK(pokebot::describe(game).find("game over") != std::string::npos);
    }
}
