#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random_agent.hpp"
#include "cards/db/card_db.hpp"
#include "engine/engine.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardKind;
using pokebot::CardTable;
using pokebot::GameState;

// Milestone 3's sign-off: every card in our decks has its effects written, and random games reach every
// one of them. Mechanics no deck card uses (Burned, Paralyzed, extra attacks, ...) have test-only cards in
// the other engine tests; docs/tests/README.md lists them.
namespace {

    struct RealDecks {
        CardDB db;
        std::vector<pokebot::Decklist> decks;
    };

    std::unique_ptr<RealDecks> load_real_decks() {
        const std::filesystem::path root = POKEBOT_SOURCE_DIR;
        if (!std::filesystem::exists(root / "data/cards/generated")) {
            return nullptr;
        }
        auto real = std::make_unique<RealDecks>();
        real->db = CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
        for (const auto& file : std::filesystem::directory_iterator(root / "data/decks")) {
            if (file.path().extension() == ".txt") {
                real->decks.push_back(pokebot::load_decklist(file.path()));
            }
        }
        return real;
    }

    std::string card_key(const CardDef& c) {
        return c.id + " " + c.name;
    }

    std::string attack_key(const CardDef& c, std::size_t attack) {
        return card_key(c) + ": attack " + c.attacks[attack].name;
    }

    std::string ability_key(const CardDef& c, std::size_t ability) {
        return card_key(c) + ": ability " + c.abilities[ability].name;
    }

    // Things our decks can never do, and why. Each one's effect is still tested with a made-up card.
    const std::map<std::string, std::string> kCantHappen = {
        {"SSP-56 Chien-Pao: attack Icicle Loop",
         "costs {W}{W}{C}, but basic-box has 1 Water Energy; tested by \"an Energy attached to the attacker can be put into the hand\""},
    };

    bool is_special_energy(const CardDef& c) {
        return c.kind == CardKind::Energy && c.energy_kind == pokebot::EnergyKind::Special;
    }

    // Everything a player chooses to do with a card. Triggered Abilities and always-on effects aren't
    // chosen, so they're left to their own tests.
    void add_expected(const CardDef& c, std::set<std::string>& out) {
        if (c.kind == CardKind::Pokemon) {
            for (std::size_t i = 0; i < c.attacks.size(); ++i) {
                out.insert(attack_key(c, i));
            }
            for (std::size_t i = 0; i < c.abilities.size(); ++i) {
                if (c.abilities[i].trigger == pokebot::Trigger::None && !c.abilities[i].program.empty()) {
                    out.insert(ability_key(c, i));
                }
            }
        } else if (c.kind == CardKind::Trainer) {
            out.insert(card_key(c));
            if (c.trainer_kind == pokebot::TrainerKind::Stadium && c.trigger == pokebot::Trigger::None && !c.program.empty()) {
                out.insert(card_key(c) + ": action");
            }
        } else if (is_special_energy(c)) {
            out.insert(card_key(c));
        }
    }

    void record(const GameState& game, const Action& action, std::set<std::string>& used) {
        switch (action.kind) {
            case ActionKind::PlayTrainer:
            case ActionKind::AttachTool:
                used.insert(card_key(game.def(action.card)));
                break;
            case ActionKind::AttachEnergy:
                if (is_special_energy(game.def(action.card))) {
                    used.insert(card_key(game.def(action.card)));
                }
                break;
            case ActionKind::UseAbility:
                used.insert(ability_key(game.def(action.card), action.amount));
                break;
            case ActionKind::UseStadium:
                used.insert(card_key(game.def(action.card)) + ": action");
                break;
            case ActionKind::Attack:
                used.insert(attack_key(game.def(game.player(game.current_player).active->top()), action.amount));
                break;
            case ActionKind::ChooseAttack: // a copy attack running a Benched Pokemon's attack
                used.insert(attack_key(game.def(action.card), action.amount));
                break;
            default:
                break;
        }
    }

} // namespace

TEST_CASE("every card in our decks has its effects written", "[coverage]") {
    const auto real = load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    std::string list;
    for (const auto& missing : real->db.missing_effects()) {
        list += "\n  " + missing;
    }
    INFO("still missing:" << list);
    CHECK(list.empty());
}

TEST_CASE("random games use every attack, Ability, Trainer and Special Energy in our decks", "[coverage]") {
    const auto real = load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    std::set<std::string> expected;
    std::set<std::string> used;
    const std::size_t n = real->decks.size();
    for (std::size_t i = 0; i < 2000; ++i) {
        const CardTable table(real->db, real->decks[i % n], real->decks[(i / n) % n]);
        GameState game = pokebot::new_game(table, i);
        if (i < n * n) { // the first n * n games play every pairing
            for (std::size_t card = 0; card < pokebot::kMaxCards; ++card) {
                add_expected(game.def(static_cast<pokebot::CardIndex>(card)), expected);
            }
        }
        pokebot::BiasedRandomAgent agent(i);
        for (int step = 0; step < 20000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const Action action = agent.choose(game, actions);
            record(game, action, used);
            pokebot::apply(game, action);
        }
    }

    std::string never;
    for (const auto& key : expected) {
        if (!used.contains(key) && !kCantHappen.contains(key)) {
            never += "\n  " + key;
        }
    }
    INFO(expected.size() << " things to use; never used in 2,000 games:" << never);
    CHECK(never.empty());
}
