#include "engine/describe/describe.hpp"

#include <sstream>

#include "engine/knowledge/knowledge.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot {

    namespace {

        template <class Cards, class Name>
        std::string join_names(const Cards& cards, Name name) {
            if (cards.empty()) {
                return "(none)";
            }
            std::string text;
            for (const auto& card : cards) {
                if (!text.empty()) {
                    text += ", ";
                }
                text += name(card);
            }
            return text;
        }

        template <class Cards>
        std::string card_list(const GameState& game, const Cards& cards) {
            return join_names(cards, [&](CardIndex card) { return game.def(card).name; });
        }

        std::string name_of(const Observation& view, CardIndex card) {
            const CardDef* def = view.def(card);
            return def != nullptr ? def->name : "an unseen card";
        }

        template <class Cards>
        std::string card_list(const Observation& view, const Cards& cards) {
            return join_names(cards, [&](CardIndex card) { return name_of(view, card); });
        }

        std::string conditions(const SpecialConditions& special) {
            const std::pair<bool, const char*> names[] = {{special.asleep, "Asleep"}, {special.burned, "Burned"}, {special.confused, "Confused"}, {special.paralyzed, "Paralyzed"}, {special.poison > 0, "Poisoned"}};
            std::string text;
            for (const auto& [has, name] : names) {
                if (has) {
                    text += std::string(" (") + name + ")";
                }
            }
            return text;
        }

        std::string pokemon_summary(const GameState& game, const PokemonInPlay& pokemon) {
            const CardDef& def = game.def(pokemon.top());
            std::string text = def.name + " " + std::to_string(rules::remaining_hp(game, pokemon)) + "/" + std::to_string(rules::max_hp(game, pokemon)) + " HP";
            if (!pokemon.energy.empty()) {
                text += " [" + card_list(game, pokemon.energy) + "]";
            }
            return text + conditions(pokemon.special);
        }

        std::string pokemon_summary(const Observation& view, const PokemonView& seen) {
            const PokemonInPlay& pokemon = seen.pokemon;
            std::string text = name_of(view, pokemon.top()) + " " + std::to_string(seen.remaining_hp()) + "/" + std::to_string(seen.max_hp) + " HP";
            if (!pokemon.energy.empty()) {
                text += " [" + card_list(view, pokemon.energy) + "]";
            }
            if (pokemon.tool != kNoCard) {
                text += " {" + name_of(view, pokemon.tool) + "}";
            }
            return text + conditions(pokemon.special);
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
                case DecisionKind::ChooseCards:
                    return "choose cards for an effect";
                case DecisionKind::ChoosePokemon:
                    return "choose a Pokemon for an effect";
                case DecisionKind::ChooseYesNo:
                    return "decide whether to use an effect";
                case DecisionKind::ChooseAttack:
                    return "choose an attack to use";
                case DecisionKind::CustomChoice:
                    return "choose a card and a Pokemon for an effect";
                case DecisionKind::DiscardBenched:
                    return "discard Benched Pokemon down to the limit";
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

    namespace {

        template <class Name>
        std::string action_text(const GameState& game, const Action& action, Name name) {
            const int player = game.is_over() ? game.current_player : game.decision().player;
            const auto card = [&]() {
                return name(action.card);
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
                case ActionKind::PlayTrainer:
                    return "play " + card();
                case ActionKind::UseAbility:
                    return "use " + game.def(action.card).abilities[action.amount].name + " (" + slot_name(game, player, action.target) + ")";
                case ActionKind::UseStadium:
                    return "use " + card();
                case ActionKind::AttachTool:
                    return "attach " + card() + " to " + slot_name(game, player, action.target);
                case ActionKind::EndTurn:
                    return "end the turn";
                case ActionKind::DiscardEnergy:
                    return "discard " + card();
                case ActionKind::Promote:
                    return "promote " + slot_name(game, player, action.target);
                case ActionKind::ChooseCard:
                    if (action.target != kActiveSlot || (!game.is_over() && game.decision().kind == DecisionKind::CustomChoice)) {
                        return "choose " + card() + " for " + slot_name(game, player, action.target);
                    }
                    return "choose " + card();
                case ActionKind::ChooseDone:
                    return "stop choosing";
                case ActionKind::ChoosePokemon:
                    return "choose " + slot_name(game, player, action.target);
                case ActionKind::AnswerYes:
                    return "yes";
                case ActionKind::AnswerNo:
                    return "no";
                case ActionKind::ChooseAttack:
                    return "use " + game.def(action.card).attacks[action.amount].name + " from " + card();
                case ActionKind::DiscardBenched:
                    return "discard " + slot_name(game, player, action.target);
            }
            return "unknown action";
        }

    } // namespace

    std::string describe(const GameState& game, const Action& action) {
        return action_text(game, action, [&](CardIndex card) { return game.def(card).name; });
    }

    std::string describe(const GameState& before, const Action& action, const GameState& after, int viewer) {
        const bool setup = action.kind == ActionKind::SetupPlaceActive || action.kind == ActionKind::SetupPlaceBench;
        return action_text(before, action, [&](CardIndex card) {
            if (knows_where(before, viewer, card) || knows_where(after, viewer, card)) {
                return before.def(card).name;
            }
            return std::string(setup ? "a face-down Pokemon" : "an unseen card");
        });
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
        if (const CardIndex stadium = rules::stadium_in_play(game); stadium != kNoCard) {
            out << "Stadium: " << game.def(stadium).name << '\n';
        }
        return out.str();
    }

    std::string describe(const Observation& view) {
        std::ostringstream out;
        out << "Turn " << view.turn << ", player " << static_cast<int>(view.current_player) << "'s turn";
        if (view.is_over()) {
            out << " - game over: " << result_name(view.result);
        } else {
            out << " - waiting on player " << static_cast<int>(view.decision.player) << ": " << decision_name(view.decision.kind);
        }
        out << '\n';

        CardIndex stadium = kNoCard;
        for (const int p : {static_cast<int>(view.viewer), opponent_of(view.viewer)}) {
            const PlayerView& player = view.player(p);
            const bool mine = p == view.viewer;
            out << (mine ? "You" : "Opponent") << " (player " << p << (view.turn > 0 && p == view.first_player ? ", went first" : "")
                << "): " << static_cast<int>(player.prize_count) << " Prize cards, " << static_cast<int>(player.deck_size) << " in deck, "
                << static_cast<int>(player.hand_size) << " in hand, " << player.discard.size() << " in discard\n";
            out << "  Active: " << (player.active ? pokemon_summary(view, *player.active) : "(none)") << '\n';
            out << "  Bench:  ";
            if (player.bench.empty()) {
                out << "(empty)";
            }
            for (std::size_t i = 0; i < player.bench.size(); ++i) {
                out << (i > 0 ? "; " : "") << pokemon_summary(view, player.bench[i]);
            }
            out << '\n';
            if (player.face_down > 0) {
                out << "  Face down: " << static_cast<int>(player.face_down) << " Pokemon\n";
            }
            if (mine) {
                out << "  Hand:   " << card_list(view, player.hand) << '\n';
            } else {
                const std::size_t unknown = player.hand_size - player.hand.size();
                out << "  Hand:   " << static_cast<int>(player.hand_size) << (player.hand_size == 1 ? " card" : " cards");
                if (!player.hand.empty()) {
                    out << ": " << card_list(view, player.hand) << " and " << unknown << " unknown";
                }
                out << '\n';
            }
            if (!player.deck.empty()) {
                out << "  Known in deck: " << join_names(player.deck, [&](const DeckCard& card) {
                    return name_of(view, card.card) + " (" + (card.from_top == 0 ? std::string("top") : std::to_string(card.from_top + 1) + " from the top") + ")";
                }) << '\n';
            }
            if (player.stadium != kNoCard) {
                stadium = player.stadium;
            }
        }
        if (stadium != kNoCard) {
            out << "Stadium: " << name_of(view, stadium) << '\n';
        }
        return out.str();
    }

} // namespace pokebot
