#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            std::uint16_t turn_for(const GameState& game, Until until) {
                switch (until) {
                    case Until::ThisTurn:
                        return game.turn;
                    case Until::OpponentsNextTurn:
                        return static_cast<std::uint16_t>(game.turn + 1);
                    case Until::YourNextTurn:
                        return static_cast<std::uint16_t>(game.turn + 2);
                }
                return game.turn;
            }

        } // namespace

        void add_lasting(GameState& game, const EffectRun& run, const Step& step) {
            LastingEffect effect;
            effect.kind = step.lasting.kind;
            effect.turn = turn_for(game, step.lasting.until);
            effect.amount = step.amount;
            effect.only_ex = step.lasting.only_ex;
            effect.prevent_effects = step.lasting.prevent_effects;
            effect.from_basic = step.lasting.from_basic;
            effect.attacker_no_rule_box = step.lasting.attacker_no_rule_box;
            effect.attacker_type = step.lasting.attacker_type;
            switch (step.lasting.on) {
                case LastingOn::Self:
                    effect.player = run.who.player;
                    effect.pokemon = run.who.self;
                    break;
                case LastingOn::Defending: {
                    effect.player = static_cast<std::uint8_t>(opponent_of(run.who.player));
                    const PlayerState& opponent = game.player(effect.player);
                    if (!opponent.active || (run.program.kind == ProgramKind::Attack && rules::effects_prevented(game, *opponent.active))) {
                        return;
                    }
                    effect.pokemon = opponent.active->id();
                    break;
                }
                case LastingOn::Player:
                    effect.player = static_cast<std::uint8_t>(step.side == Side::Opponent ? opponent_of(run.who.player) : run.who.player);
                    break;
            }
            if (effect.kind == LastingKind::CantUseAttack) {
                effect.attack = AttackRef{run.program.card, run.program.index};
            }
            if (effect.pokemon == kNoCard && step.lasting.on != LastingOn::Player) {
                return;
            }
            game.lasting.push_back(effect);
        }

        void end_effects_on(GameState& game, CardIndex pokemon) {
            for (std::size_t i = game.lasting.size(); i-- > 0;) {
                if (game.lasting[i].pokemon == pokemon) {
                    game.lasting.erase_at(i);
                }
            }
        }

        void expire_lasting(GameState& game) {
            for (std::size_t i = game.lasting.size(); i-- > 0;) {
                if (game.lasting[i].turn < game.turn) {
                    game.lasting.erase_at(i);
                }
            }
        }

    } // namespace detail

} // namespace pokebot
