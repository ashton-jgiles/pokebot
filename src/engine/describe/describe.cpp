#include "engine/describe/describe.hpp"

#include <sstream>

#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        template <class Cards>
        std::string card_list(const GameState& game, const Cards& cards) {
            if (cards.empty()) {
                return "(none)";
            }
            std::string text;
            for (const CardIndex card : cards) {
                if (!text.empty()) {
                    text += ", ";
                }
                text += game.def(card).name;
            }
            return text;
        }

        std::string pokemon_summary(const GameState& game, const PokemonInPlay& pokemon) {
            const CardDef& def = game.def(pokemon.top());
            std::string text = def.name + " " + std::to_string(rules::remaining_hp(game, pokemon)) + "/" + std::to_string(rules::max_hp(game, pokemon)) + " HP";
            if (!pokemon.energy.empty()) {
                text += " [" + card_list(game, pokemon.energy) + "]";
            }
            return text;
        }

        std::string slot_name(const GameState& game, int player, Slot slot) {
            const PlayerState& p = game.player(player);
            if (slot == kActiveSlot) {
                return p.active ? "Active " + game.def(p.active->top()).name : "the Active Spot";
            }
            const std::size_t index = static_cast<std::size_t>(slot) - 1;
            const std::string name = "Bench " + std::to_string(slot);
            return index < p.bench.size() ? name + " " + game.def(p.bench[index].top()).name : name;
        }

        const char* decision_name(DecisionKind kind) {
            switch (kind) {
                case DecisionKind::ChooseTurnOrder:
                    return "choose who goes first";
                case DecisionKind::SetupActive:
                    return "choose an Active Pokemon";
                case DecisionKind::SetupBench:
                    return "put Basic Pokemon on the Bench";
                case DecisionKind::MulliganDraws:
                    return "draw extra cards for mulligans";
                case DecisionKind::MainPhase:
                    return "main phase";
                case DecisionKind::RetreatDiscard:
                    return "discard Energy to retreat";
                case DecisionKind::PromoteActive:
                    return "choose a new Active Pokemon";
                case DecisionKind::ExtraAttack:
                    return "attack again or end the turn";
            }
            return "unknown decision";
        }

        const char* result_name(GameResult result) {
            switch (result) {
                case GameResult::Ongoing:
                    return "in progress";
                case GameResult::Player0Wins:
                    return "player 0 wins";
                case GameResult::Player1Wins:
                    return "player 1 wins";
                case GameResult::Draw:
                    return "draw";
            }
            return "unknown result";
        }

    } // namespace

    std::string describe(const GameState& game, const Action& action) {
        const int player = game.is_over() ? game.current_player : game.decision().player;
        const auto card = [&game, &action]() {
            return game.def(action.card).name;
        };

        switch (action.kind) {
            case ActionKind::ChooseTurnOrder:
                return action.amount == 0 ? "go first" : "go second";
            case ActionKind::SetupPlaceActive:
                return "make " + card() + " the Active Pokemon";
            case ActionKind::SetupPlaceBench:
            case ActionKind::PlayBasic:
                return "put " + card() + " on the Bench";
            case ActionKind::SetupDone:
                return "finish setting up";
            case ActionKind::DrawMulliganCards:
                return "draw " + std::to_string(action.amount) + " extra card(s)";
            case ActionKind::Evolve:
                return "evolve " + slot_name(game, player, action.target) + " into " + card();
            case ActionKind::AttachEnergy:
                return "attach " + card() + " to " + slot_name(game, player, action.target);
            case ActionKind::Retreat:
                return "retreat, bringing in " + slot_name(game, player, action.target);
            case ActionKind::Attack: {
                const PlayerState& p = game.player(player);
                if (!p.active) {
                    return "attack";
                }
                const auto& attacks = game.def(p.active->top()).attacks;
                if (action.amount >= attacks.size()) {
                    return "attack " + std::to_string(action.amount);
                }
                const Attack& attack = attacks[action.amount];
                return "attack with " + attack.name + " (" + std::to_string(attack.damage) + ")";
            }
            case ActionKind::EndTurn:
                return "end the turn";
            case ActionKind::DiscardEnergy:
                return "discard " + card();
            case ActionKind::Promote:
                return "promote " + slot_name(game, player, action.target);
        }
        return "unknown action";
    }

    std::string describe(const GameState& game) {
        std::ostringstream out;
        out << "Turn " << game.turn << ", player " << static_cast<int>(game.current_player) << "'s turn";
        if (game.is_over()) {
            out << " - game over: " << result_name(game.result);
        } else {
            out << " - waiting on player " << static_cast<int>(game.decision().player) << ": "
                << decision_name(game.decision().kind);
        }
        out << '\n';

        for (int p = 0; p < kPlayers; ++p) {
            const PlayerState& player = game.player(p);
            out << "Player " << p << (game.turn > 0 && p == game.first_player ? " (went first)" : "") << ": " << player.prizes.size()
                << " Prize cards, " << player.deck.size() << " in deck, " << player.hand.size() << " in hand, "
                << player.discard.size() << " in discard\n";
            out << "  Active: " << (player.active ? pokemon_summary(game, *player.active) : "(none)") << '\n';
            out << "  Bench:  ";
            if (player.bench.empty()) {
                out << "(empty)";
            }
            for (std::size_t i = 0; i < player.bench.size(); ++i) {
                out << (i > 0 ? "; " : "") << pokemon_summary(game, player.bench[i]);
            }
            out << '\n';
            out << "  Hand:   " << card_list(game, player.hand) << '\n';
        }
        return out.str();
    }

} // namespace pokebot
