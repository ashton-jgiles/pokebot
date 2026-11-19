#include "agents/action_class.hpp"

#include <cstddef>

#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        bool still_main_phase(const GameState& before, const GameState& after, int player) {
            return !after.is_over() && after.turn == before.turn && after.phase == TurnPhase::Main && after.decision().player == player;
        }

    } // namespace

    const char* to_string(ActionClass kind) {
        switch (kind) {
            case ActionClass::Deterministic:
                return "deterministic";
            case ActionClass::Final:
                return "final";
            case ActionClass::Uncertain:
                return "uncertain";
        }
        return "?";
    }

    TriedAction try_action(const GameState& game, int player, const Action& action) {
        TriedAction tried{ActionClass::Uncertain, game};
        apply_legal(tried.after, action);
        const bool chance = !(tried.after.rng == game.rng);
        const auto p = static_cast<std::size_t>(player);
        const Knowledge& before = game.knowledge;
        const Knowledge& after = tried.after.knowledge;
        const auto& prizes = game.player(player).prizes;
        bool revealed = false;
        bool prize_taken = false;
        for (std::size_t i = 0; i < kMaxCards && !revealed; ++i) {
            const auto card = static_cast<CardIndex>(i);
            const bool located = !before.located[p].contains(card) && after.located[p].contains(card);
            const bool seen = !before.seen[p].contains(card) && after.seen[p].contains(card);
            if (located || seen) {
                (prizes.contains(card) ? prize_taken : revealed) = true;
            }
        }

        if (!chance && !revealed && !prize_taken && still_main_phase(game, tried.after, player)) {
            tried.kind = ActionClass::Deterministic;
        } else if (!chance && !revealed) {
            tried.kind = ActionClass::Final;
        }
        return tried;
    }

} // namespace pokebot