#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/agent.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "agents/flat_mc/playout.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/random/random_agent.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"

using pokebot::Action;
using pokebot::ArchetypeLibrary;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::DeckModel;
using pokebot::GameState;
using pokebot::Observation;
using pokebot::observe;
using pokebot::play_out;
using pokebot::PlayoutOptions;
using pokebot::PlayoutPolicy;
using pokebot::PlayoutResult;
using pokebot::Rng;
using pokebot::World;

namespace {

    CardDB sample_db() {
        CardDB db;
        for (int number = 1; number <= 5; ++number) {
            CardDef mon = test::basic_pokemon("TST-" + std::to_string(number), "Attackmon " + std::to_string(number), 70);
            mon.attacks.push_back(test::attack("Tackle", {pokebot::EnergyType::Colorless}, 30));
            db.add(mon);
        }
        db.add(test::basic_energy("TST-6", "Grass Energy", pokebot::EnergyType::Grass));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {40, 6}});

    // A sample game played at random to a main-phase decision of `player`, at least `turns` turns in.
    GameState at_main_phase(const CardTable& table, std::uint64_t seed, int player, int turns) {
        GameState game = pokebot::new_game(table, seed);
        Rng chooser(seed);
        while (!game.is_over()) {
            const auto& decision = game.decision();
            if (game.turn >= turns && decision.kind == DecisionKind::MainPhase && decision.player == player) {
                break;
            }
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
        }
        return game;
    }

    PlayoutOptions options(int depth, PlayoutPolicy policy) {
        PlayoutOptions o;
        o.depth = depth;
        o.policy = policy;
        return o;
    }

    class Recorder final : public pokebot::Agent {
    public:
        explicit Recorder(std::uint64_t seed) : inner_(seed) {}
        std::string name() const override { return "recorder"; }
        Action choose(const Observation& observation, const std::vector<Action>& actions) override {
            seen.push_back(observation);
            return inner_.choose(observation, actions);
        }
        std::vector<Observation> seen;

    private:
        pokebot::RandomAgent inner_;
    };

    template <class ZoneA, class ZoneB>
    bool swap_hidden(const GameState& game, int viewer, ZoneA& a, ZoneB& b) {
        for (auto& x : a) {
            for (auto& y : b) {
                if (!pokebot::knows_where(game, viewer, x) && !pokebot::knows_where(game, viewer, y) && game.def(x).id != game.def(y).id) {
                    std::swap(x, y);
                    return true;
                }
            }
        }
        return false;
    }

    GameState without_table(GameState game) {
        game.cards = nullptr;
        return game;
    }

} // namespace

TEST_CASE("a playout of depth 0 scores the state as it is", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const GameState game = at_main_phase(table, 1, 0, 3);
    REQUIRE(!game.is_over());
    const PlayoutResult result = play_out(game, 0, 5, options(0, PlayoutPolicy::Greedy));
    CHECK(result.actions == 0);
    CHECK(result.turns == 0);
    CHECK(result.end == game);
    CHECK(result.value == pokebot::evaluate(game, 0));
}

TEST_CASE("a playout stops at the start of the player's r-th next main phase", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    int checked = 0;
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        INFO("seed " << seed);
        const GameState mine = at_main_phase(table, seed, 0, 3);
        const GameState theirs = at_main_phase(table, seed, 1, 3);
        for (int depth = 1; depth <= 3; ++depth) {
            INFO("depth " << depth);
            if (!mine.is_over()) {
                const PlayoutResult result = play_out(mine, 0, seed, options(depth, PlayoutPolicy::Biased));
                if (!result.over) {
                    CHECK(result.turns == 2 * depth);
                    CHECK(result.end.turn == mine.turn + 2 * depth);
                    CHECK(result.end.decision().kind == DecisionKind::MainPhase);
                    CHECK(result.end.decision().player == 0);
                    ++checked;
                }
            }
            if (!theirs.is_over()) {
                const PlayoutResult result = play_out(theirs, 0, seed, options(depth, PlayoutPolicy::Biased));
                if (!result.over) {
                    CHECK(result.turns == 2 * depth - 1);
                    CHECK(result.end.decision().kind == DecisionKind::MainPhase);
                    CHECK(result.end.decision().player == 0);
                    ++checked;
                }
            }
        }
    }
    CHECK(checked > 30);
}

TEST_CASE("a playout to the end of the game scores it with u", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    int finished = 0;
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        const GameState game = at_main_phase(table, seed, 0, 3);
        if (game.is_over()) {
            continue;
        }
        PlayoutOptions o = options(1000, PlayoutPolicy::Biased);
        o.max_actions = 100000;
        const PlayoutResult result = play_out(game, 0, seed, o);
        REQUIRE(result.over);
        CHECK(result.value == pokebot::utility(result.end, 0));
        CHECK((result.value == 1.0 || result.value == -1.0 || result.value == 0.0));
        ++finished;
    }
    CHECK(finished > 5);
}

TEST_CASE("the same seed plays the same playout, and other seeds play others", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const GameState game = at_main_phase(table, 2, 0, 3);
    REQUIRE(!game.is_over());
    for (const PlayoutPolicy policy : {PlayoutPolicy::Greedy, PlayoutPolicy::Biased}) {
        INFO(pokebot::to_string(policy));
        const PlayoutResult a = play_out(game, 0, 7, options(2, policy));
        const PlayoutResult b = play_out(game, 0, 7, options(2, policy));
        CHECK(a.end == b.end);
        CHECK(a.value == b.value);
        std::set<std::pair<int, double>> outcomes;
        for (std::uint64_t seed = 0; seed < 20; ++seed) {
            const PlayoutResult other = play_out(game, 0, seed, options(2, policy));
            outcomes.insert({other.actions, other.value});
        }
        CHECK(outcomes.size() > 1);
    }
}

TEST_CASE("a playout can be played with other weights than it's scored with", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const GameState game = at_main_phase(table, 2, 0, 3);
    REQUIRE(!game.is_over());
    pokebot::Weights policy = pokebot::default_weights();
    for (double& weight : policy) {
        weight = -weight;
    }
    int different = 0;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        PlayoutOptions o = options(2, PlayoutPolicy::Greedy);
        const PlayoutResult plain = play_out(game, 0, seed, o);
        o.policy_weights = o.weights;
        const PlayoutResult same = play_out(game, 0, seed, o);
        CHECK(same.end == plain.end);
        CHECK(same.value == plain.value);

        o.policy_weights = policy;
        const PlayoutResult other = play_out(game, 0, seed, o);
        Rng rng(seed);
        pokebot::GreedyAgent mine(rng.next(), o.greedy_worlds, policy);
        pokebot::GreedyAgent theirs(rng.next(), o.greedy_worlds, policy);
        const PlayoutResult expected = play_out(game, 0, mine, theirs, o);
        CHECK(other.end == expected.end);
        CHECK(other.actions == expected.actions);
        CHECK(other.value == pokebot::evaluate(other.end, 0, o.weights));
        different += other.end == plain.end ? 0 : 1;
    }
    CHECK(different > 0);
}

TEST_CASE("a playout stops after its most actions", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const GameState game = at_main_phase(table, 3, 0, 3);
    REQUIRE(!game.is_over());
    PlayoutOptions o = options(5, PlayoutPolicy::Biased);
    o.max_actions = 3;
    const PlayoutResult result = play_out(game, 0, 1, o);
    CHECK(result.actions == 3);
    CHECK(result.value == pokebot::evaluate(result.end, 0));
    o.depth = -1;
    CHECK_THROWS_AS(play_out(game, 0, 1, o), std::invalid_argument);
}

TEST_CASE("each player in a playout decides from its own observation", "[playout]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const GameState game = at_main_phase(table, 4, 0, 3);
    REQUIRE(!game.is_over());
    Rng rng(4);
    const World world(observe(game, 0), rng);
    Recorder mine(1);
    Recorder theirs(2);
    play_out(world.game(), 0, mine, theirs, options(2, PlayoutPolicy::Biased));
    REQUIRE(!mine.seen.empty());
    REQUIRE(!theirs.seen.empty());
    for (const Observation& seen : mine.seen) {
        CHECK(seen.viewer == 0);
        CHECK(seen.decision.player == 0);
        CHECK(seen.me().hand.size() == static_cast<std::size_t>(seen.me().hand_size));
    }
    for (const Observation& seen : theirs.seen) {
        CHECK(seen.viewer == 1);
        CHECK(seen.decision.player == 1);
        CHECK(seen.player(0).hand.empty());
    }
}

TEST_CASE("playout policies by name", "[playout]") {
    CHECK(pokebot::playout_policy("greedy") == PlayoutPolicy::Greedy);
    CHECK(pokebot::playout_policy("biased") == PlayoutPolicy::Biased);
    CHECK(std::string(pokebot::to_string(PlayoutPolicy::Biased)) == "biased");
    CHECK_THROWS_AS(pokebot::playout_policy("random"), std::invalid_argument);
}

TEST_CASE("on the real decks, games that differ only in hidden cards give the same playouts", "[playout]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    const std::size_t n = real->decks.size();
    Rng chooser(8);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        const int stop = static_cast<int>(chooser.below(90)) + 30;
        for (int step = 0; (step < stop || game.turn == 0) && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, agent.choose(observe(game, game.decision().player), actions));
        }
        if (game.is_over()) {
            continue;
        }
        for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
            INFO("seed " << seed << ", viewer " << viewer);
            GameState changed = game;
            auto& theirs = changed.player(pokebot::opponent_of(viewer));
            auto& mine = changed.player(viewer);
            const bool hand = swap_hidden(game, viewer, theirs.hand, theirs.deck);
            const bool prizes = swap_hidden(game, viewer, mine.prizes, mine.deck);
            if (!hand && !prizes) {
                continue;
            }
            REQUIRE(observe(changed, viewer) == observe(game, viewer));
            for (const PlayoutPolicy policy : {PlayoutPolicy::Greedy, PlayoutPolicy::Biased}) {
                INFO(pokebot::to_string(policy));
                Rng a(seed);
                Rng b(seed);
                const World real_world(observe(game, viewer), a, &model);
                const World changed_world(observe(changed, viewer), b, &model);
                const PlayoutResult x = play_out(real_world.game(), viewer, seed, options(1, policy));
                const PlayoutResult y = play_out(changed_world.game(), viewer, seed, options(1, policy));
                CHECK(x.value == y.value);
                CHECK(x.actions == y.actions);
                CHECK(without_table(x.end) == without_table(y.end));
            }
            ++compared;
        }
    }
    CHECK(compared > 10);
}
