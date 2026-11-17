#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/internal.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameResult;
using pokebot::GameState;
using pokebot::kActiveSlot;

namespace {

    // TST-1 Attackmon (Fire, 60 HP): Tackle [C] 30, Blast [Fire C] 60.
    // TST-2 Grassmon (Grass, 60 HP, Weakness Fire x2). TST-3 Steelmon (Metal, 100 HP, Resistance Fire -30).
    // TST-4 Bigmon ex (200 HP). TST-5 Mega Bigmon ex (300 HP). TST-6 Darkness Energy, TST-7 Fire Energy.
    CardDB sample_db() {
        using E = EnergyType;
        CardDef attacker = test::basic_pokemon("TST-1", "Attackmon", 60);
        attacker.types = {E::Fire};
        attacker.attacks = {test::attack("Tackle", {E::Colorless}, 30), test::attack("Blast", {E::Fire, E::Colorless}, 60)};

        CardDef grass = test::basic_pokemon("TST-2", "Grassmon", 60);
        grass.types = {E::Grass};
        grass.weakness = pokebot::Weakness{E::Fire, 2};

        CardDef steel = test::basic_pokemon("TST-3", "Steelmon", 100);
        steel.types = {E::Metal};
        steel.resistance = pokebot::Resistance{E::Fire, 30};
        steel.attacks = {test::attack("Iron Bash", {E::Metal}, 40)};

        CardDef big = test::basic_pokemon("TST-4", "Bigmon ex", 200);
        big.rules.ex = true;

        CardDef mega = test::basic_pokemon("TST-5", "Mega Bigmon ex", 300);
        mega.rules.ex = true;
        mega.rules.mega = true;

        CardDB db;
        for (const auto& card : {attacker, grass, steel, big, mega}) {
            db.add(card);
        }
        db.add(test::basic_energy("TST-6", "Darkness Energy", E::Darkness));
        db.add(test::basic_energy("TST-7", "Fire Energy", E::Fire));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {20, 6}, {20, 7}});

    const Action kTackle{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, 0};
    const Action kBlast{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, 1};

    // A game on turn 2, so the current player is allowed to attack. `me` attacks `them`.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 9);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        // Attackmon with `energy` attached against `defender`, which has `bench` Pokemon behind it.
        void arrange(const std::string& defender, std::initializer_list<const char*> energy, int bench) {
            test::set_active(game, me, "TST-1");
            for (const char* id : energy) {
                test::attach_from_deck(game, me, kActiveSlot, id);
            }
            test::set_active(game, them, defender);
            test::clear_bench(game, them);
            for (int i = 0; i < bench; ++i) {
                test::add_to_bench(game, them, "TST-3");
            }
        }
    };

    void check_all_cards(const GameState& game) {
        CHECK(test::cards_accounted_for(game.player(0)) == 60);
        CHECK(test::cards_accounted_for(game.player(1)) == 60);
    }

} // namespace

TEST_CASE("typed Energy must match; Colorless takes anything", "[engine][attacks]") {
    Fixture f;
    using E = EnergyType;
    test::set_active(f.game, f.me, "TST-1");
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-6"); // Darkness
    const auto& active = *f.game.player(f.me).active;

    CHECK(pokebot::detail::can_pay_cost(f.game, active, {E::Colorless}));
    CHECK_FALSE(pokebot::detail::can_pay_cost(f.game, active, {E::Fire}));
    CHECK_FALSE(pokebot::detail::can_pay_cost(f.game, active, {E::Fire, E::Colorless}));

    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-7"); // + Fire
    CHECK(pokebot::detail::can_pay_cost(f.game, active, {E::Fire, E::Colorless}));
    CHECK_FALSE(pokebot::detail::can_pay_cost(f.game, active, {E::Fire, E::Fire}));
    CHECK_FALSE(pokebot::detail::can_pay_cost(f.game, active, {E::Colorless, E::Colorless, E::Colorless}));
}

TEST_CASE("the first player can't attack on turn 1", "[engine][attacks]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 9);
    test::play_setup(game);
    for (int p = 0; p < 2; ++p) {
        test::set_active(game, p, "TST-1");
        test::attach_from_deck(game, p, kActiveSlot, "TST-6");
    }
    CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::Attack));

    test::end_turns(game, 1); // turn 2: the second player may attack
    CHECK(test::offers(pokebot::legal_actions(game), ActionKind::Attack));
}

TEST_CASE("an attack places damage counters and ends the turn", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-4", {"TST-6"}, 1); // Bigmon ex, 200 HP: no Knock Out
    const auto turn = f.game.turn;

    pokebot::apply(f.game, kTackle);
    CHECK(f.game.player(f.them).active->damage_counters == 3);
    CHECK(f.game.current_player == f.them);
    CHECK(f.game.turn == turn + 1);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    check_all_cards(f.game);
}

TEST_CASE("Weakness doubles damage; Resistance reduces it, but never below 0", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-2", {"TST-6"}, 1);
    const auto& attacker = *f.game.player(f.me).active;
    const auto& tackle = f.game.def(attacker.top()).attacks[0];
    const auto& blast = f.game.def(attacker.top()).attacks[1];

    CHECK(pokebot::detail::attack_damage(f.game, attacker, *f.game.player(f.them).active, tackle) == 60); // 30 x2

    test::set_active(f.game, f.them, "TST-3");
    const auto& steel = *f.game.player(f.them).active;
    CHECK(pokebot::detail::attack_damage(f.game, attacker, steel, tackle) == 0); // 30 - 30
    CHECK(pokebot::detail::attack_damage(f.game, attacker, steel, blast) == 30); // 60 - 30
}

TEST_CASE("a copied attack uses the attacker's type and needs none of the copied cost", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-2", {}, 1); // Fire Attackmon with no Energy against Grassmon (Weakness Fire)
    const auto& attacker = *f.game.player(f.me).active;
    pokebot::CardIndex steelmon = pokebot::kNoCard;
    for (const auto card : f.game.player(f.me).deck) {
        if (f.game.def(card).id == "TST-3") {
            steelmon = card;
        }
    }
    REQUIRE(steelmon != pokebot::kNoCard);

    const pokebot::EffectContext who{static_cast<std::uint8_t>(f.me), attacker.id(), attacker.top()};
    pokebot::detail::perform_attack(f.game, who, pokebot::AttackRef{steelmon, 0}); // Iron Bash [M] 40

    CHECK(f.game.player(f.them).active->damage_counters == 8); // 40 x2: Attackmon is Fire
}

TEST_CASE("a Pokemon keeps its id when it moves between the Active Spot and the Bench", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-2", {}, 0);
    test::clear_bench(f.game, f.me);
    test::add_to_bench(f.game, f.me, "TST-4");
    auto& p = f.game.player(f.me);
    const auto attackmon = p.active->id();
    const auto bigmon = p.bench[0].id();

    std::swap(*p.active, p.bench[0]);
    CHECK(pokebot::detail::find_pokemon(f.game, attackmon) == &p.bench[0]);
    CHECK(pokebot::detail::find_pokemon(f.game, bigmon) == &*p.active);
    CHECK(pokebot::detail::find_pokemon(f.game, p.deck[0]) == nullptr);
}

TEST_CASE("a Knock Out discards the Pokemon, gives a Prize, and asks for a new Active", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-2", {"TST-6"}, 1); // Grassmon takes 60 from Tackle: exactly its HP
    test::attach_from_deck(f.game, f.them, kActiveSlot, "TST-6");
    const auto discard_before = f.game.player(f.them).discard.size();
    const auto hand_before = f.game.player(f.me).hand.size();

    pokebot::apply(f.game, kTackle);
    CHECK_FALSE(f.game.player(f.them).active.has_value());
    CHECK(f.game.player(f.them).discard.size() == discard_before + 2); // Grassmon and its Energy
    CHECK(f.game.player(f.me).prizes.size() == 5);
    CHECK(f.game.player(f.me).hand.size() == hand_before + 1);
    REQUIRE(f.game.decision().kind == DecisionKind::PromoteActive);
    CHECK(f.game.decision().player == f.them);

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));
    CHECK(f.game.def(f.game.player(f.them).active->top()).id == "TST-3");
    CHECK(f.game.player(f.them).bench.empty());
    CHECK(f.game.current_player == f.them); // their turn begins
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    check_all_cards(f.game);
}

TEST_CASE("Pokemon ex give up 2 Prize cards and Mega ex 3", "[engine][attacks]") {
    {
        Fixture f;
        f.arrange("TST-4", {"TST-6"}, 1);
        f.game.player(f.them).active->damage_counters = 19; // 190 of 200 HP
        pokebot::apply(f.game, kTackle);
        CHECK(f.game.player(f.me).prizes.size() == 4);
    }
    {
        Fixture f;
        f.arrange("TST-5", {"TST-6"}, 1);
        f.game.player(f.them).active->damage_counters = 29; // 290 of 300 HP
        pokebot::apply(f.game, kTackle);
        CHECK(f.game.player(f.me).prizes.size() == 3);
    }
}

TEST_CASE("taking the last Prize card wins", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-2", {"TST-6"}, 1);
    auto& prizes = f.game.player(f.me).prizes;
    while (prizes.size() > 1) {
        f.game.player(f.me).discard.push_back(prizes.pop_back()); // keep every card accounted for
    }

    pokebot::apply(f.game, kTackle);
    CHECK(f.game.result == (f.me == 0 ? GameResult::Player0Wins : GameResult::Player1Wins));
    CHECK(pokebot::legal_actions(f.game).empty());
    check_all_cards(f.game);
}

TEST_CASE("Knocking Out the opponent's last Pokemon wins", "[engine][attacks]") {
    Fixture f;
    f.arrange("TST-2", {"TST-6"}, 0); // nothing on their Bench
    pokebot::apply(f.game, kTackle);
    CHECK(f.game.result == (f.me == 0 ? GameResult::Player0Wins : GameResult::Player1Wins));
    check_all_cards(f.game);
}

TEST_CASE("when both players win at once, more win conditions beats fewer, and a tie is a draw", "[engine][attacks]") {
    SECTION("tie: both lose their last Pokemon") {
        Fixture f;
        for (int p = 0; p < 2; ++p) {
            test::set_active(f.game, p, "TST-2");
            test::clear_bench(f.game, p);
            f.game.player(p).active->damage_counters = 6;
        }
        pokebot::detail::handle_knockouts(f.game);
        CHECK(f.game.result == GameResult::Draw);
    }
    SECTION("player 0 also takes their last Prize card") {
        Fixture f;
        for (int p = 0; p < 2; ++p) {
            test::set_active(f.game, p, "TST-2");
            test::clear_bench(f.game, p);
            f.game.player(p).active->damage_counters = 6;
        }
        auto& prizes = f.game.player(0).prizes;
        while (prizes.size() > 1) {
            f.game.player(0).discard.push_back(prizes.pop_back());
        }
        pokebot::detail::handle_knockouts(f.game);
        CHECK(f.game.result == GameResult::Player0Wins); // 2 win conditions against 1
    }
}