#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "agents/evaluation/weights.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/set_weights.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"

using pokebot::default_weights;
using pokebot::kFeatureCount;
using pokebot::load_weights;
using pokebot::save_weights;
using pokebot::set_weights;
using pokebot::Weights;

namespace {

    struct TempFile {
        std::filesystem::path path;

        explicit TempFile(const std::string& name) : path(std::filesystem::temp_directory_path() / ("pokebot-" + name + ".txt")) {}
        ~TempFile() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
        TempFile(const TempFile&) = delete;
        TempFile& operator=(const TempFile&) = delete;

        void write(const std::string& text) const {
            std::ofstream out(path, std::ios::binary);
            out << text;
        }
    };

    Weights some_weights() {
        Weights weights{};
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            weights[i] = 1.0 / static_cast<double>(i + 3) - 0.2;
        }
        return weights;
    }

    std::filesystem::path hand_file() {
        return std::filesystem::path(POKEBOT_SOURCE_DIR) / "data/weights/hand.txt";
    }

    std::string hand_text() {
        std::ifstream in(hand_file());
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

} // namespace

TEST_CASE("weights survive saving and loading exactly", "[weights]") {
    const TempFile file("weights-saved");
    save_weights(file.path, some_weights());
    CHECK(load_weights(file.path) == some_weights());
}

TEST_CASE("the hand weights file holds the default weights", "[weights]") {
    CHECK(load_weights(hand_file()) == default_weights());
}

TEST_CASE("a weights file can have Windows line endings, blank lines and any order", "[weights]") {
    const TempFile file("weights-order");
    const std::string text = hand_text();
    const std::string first = text.substr(0, text.find('\n'));
    file.write(text.substr(first.size() + 1) + "\r\n\r\n" + first + "\r\n");
    CHECK(load_weights(file.path) == default_weights());
}

TEST_CASE("a weights file that's wrong is rejected", "[weights]") {
    const TempFile file("weights-wrong");
    const std::string good = hand_text();
    REQUIRE(!good.empty());
    const std::string without_first = good.substr(good.find('\n') + 1);
    const auto rejected = [&](const std::string& text) {
        INFO(text);
        file.write(text);
        CHECK_THROWS(load_weights(file.path));
    };
    rejected("");
    rejected(without_first);
    rejected(good + "\nprizes 1\n");
    rejected(good + "\nluck 1\n");
    rejected("prizes\n" + without_first);
    rejected("prizes two\n" + without_first);
    rejected("prizes 2 3\n" + without_first);
    rejected("prizes nan\n" + without_first);
    CHECK_THROWS(load_weights(file.path.parent_path() / "pokebot-no-such-weights.txt"));
}

TEST_CASE("set_weights sets every copy an agent's options hold", "[weights]") {
    const Weights weights = some_weights();
    pokebot::PlannerOptions planner;
    set_weights(planner, weights);
    CHECK(planner.weights == weights);

    pokebot::FlatMcOptions flat_mc;
    set_weights(flat_mc, weights);
    CHECK(flat_mc.weights == weights);
    CHECK(flat_mc.playout.weights == weights);

    pokebot::IsMctsAgentOptions is_mcts;
    set_weights(is_mcts, weights);
    CHECK(is_mcts.weights == weights);
    CHECK(is_mcts.search.playout.weights == weights);
    CHECK(is_mcts.search.macros.weights == weights);

    pokebot::EndgameAgentOptions endgame;
    set_weights(endgame, weights);
    CHECK(endgame.is_mcts.weights == weights);
    CHECK(endgame.is_mcts.search.playout.weights == weights);
    CHECK(endgame.is_mcts.search.macros.weights == weights);
    CHECK(endgame.solver.search.weights == weights);
}

TEST_CASE("on the real decks, the saved hand weights play the default's games, and other weights don't", "[weights]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const Weights hand = load_weights(hand_file());
    const pokebot::CardTable table(real->db, real->decks[0], real->decks[1]);
    const auto play = [&](std::uint64_t seed, const Weights& weights) {
        pokebot::GreedyAgent first(seed * 2, 1, weights);
        pokebot::GreedyAgent second(seed * 2 + 1, 1, weights);
        return pokebot::play_game(table, seed, first, second).moves;
    };
    int different = 0;
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        const auto moves = play(seed, default_weights());
        CHECK(play(seed, hand) == moves);
        different += play(seed, some_weights()) != moves ? 1 : 0;
    }
    CHECK(different > 0);
}
