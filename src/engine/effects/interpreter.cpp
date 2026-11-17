#include <algorithm>
#include <stdexcept>
#include <vector>

#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            using CardZone = FixedVector<CardIndex, kDeckSize>;

            const Program& program_of(const GameState& game, const ProgramRef& ref) {
                const CardDef& def = game.def(ref.card);
                switch (ref.kind) {
                    case ProgramKind::Attack:
                        return def.attacks[ref.index].program;
                }
                throw std::logic_error("program_of: unknown program kind");
            }

            const Step& current_step(const GameState& game) {
                const EffectRun& run = game.effects.back();
                return program_of(game, run.program)[run.pc];
            }

            const CardZone* zone_of(const GameState& game, const EffectRun& run, Zone zone) {
                const PlayerState& p = game.player(run.who.player);
                switch (zone) {
                    case Zone::Deck:
                        return &p.deck;
                    case Zone::Hand:
                        return &p.hand;
                    case Zone::Discard:
                        return &p.discard;
                    case Zone::AttachedEnergy: {
                        const PokemonInPlay* self = find_pokemon(game, run.who.self);
                        return self != nullptr ? &self->energy : nullptr;
                    }
                }
                return nullptr;
            }

            CardZone* zone_of(GameState& game, const EffectRun& run, Zone zone) {
                return const_cast<CardZone*>(zone_of(static_cast<const GameState&>(game), run, zone));
            }

            std::vector<CardIndex> choosable(const GameState& game, const EffectRun& run, const Step& step) {
                std::vector<CardIndex> cards;
                const CardZone* zone = zone_of(game, run, step.from);
                if (zone == nullptr) {
                    return cards;
                }
                for (const CardIndex card : *zone) {
                    if (!run.chosen.contains(card) && matches(step.filter, game.def(card))) {
                        cards.push_back(card);
                    }
                }
                return cards;
            }

            void ask(GameState& game, const EffectRun& run, DecisionKind kind, std::size_t max, std::size_t min) {
                game.pending.push_back(Decision{kind, run.who.player, static_cast<std::uint8_t>(max), static_cast<std::uint8_t>(min)});
            }

            void switch_with_chosen(GameState& game, const EffectRun& run) {
                PlayerState& p = game.player(run.who.player);
                if (run.chosen_pokemon == kNoCard || !p.active || p.active->id() != run.who.self) {
                    return;
                }
                for (PokemonInPlay& benched : p.bench) {
                    if (benched.id() == run.chosen_pokemon) {
                        std::swap(*p.active, benched);
                        raise(game, Event{EventKind::MovedToBench, run.who.player, run.who.self, kNoCard});
                        return;
                    }
                }
            }

            bool run_step(GameState& game, EffectRun& run, const Step& step) {
                PlayerState& p = game.player(run.who.player);
                switch (step.op) {
                    case Op::Damage:
                        deal_attack_damage(game, run.who, game.def(run.program.card).attacks[run.program.index]);
                        break;
                    case Op::Draw:
                        draw_cards(game, run.who.player, step.max);
                        break;
                    case Op::DiscardHand:
                        while (!p.hand.empty()) {
                            p.discard.push_back(p.hand.pop_back());
                        }
                        break;
                    case Op::ShuffleDeck:
                        game.rng.shuffle(p.deck);
                        break;
                    case Op::ChooseCards: {
                        const std::size_t max = std::min<std::size_t>(step.max, choosable(game, run, step).size());
                        if (max == 0) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChooseCards, max, std::min<std::size_t>(step.min, max));
                        return false;
                    }
                    case Op::MoveChosen: {
                        CardZone* from = zone_of(game, run, step.from);
                        CardZone* to = zone_of(game, run, step.to);
                        for (const CardIndex card : run.chosen) {
                            if (from != nullptr && to != nullptr && from->remove(card)) {
                                to->push_back(card);
                            }
                        }
                        run.chosen.clear();
                        break;
                    }
                    case Op::ChooseBenched:
                        run.chosen_pokemon = kNoCard;
                        if (p.bench.empty()) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChoosePokemon, 1, 1);
                        return false;
                    case Op::SwitchWithChosen:
                        switch_with_chosen(game, run);
                        break;
                    case Op::AskYesNo:
                        ask(game, run, DecisionKind::ChooseYesNo, 0, 0);
                        return false;
                }
                ++run.pc;
                return true;
            }

            void finish_run(GameState& game, const EffectRun& run) {
                switch (run.then) {
                    case AfterEffect::Nothing:
                        break;
                    case AfterEffect::FinishAttack:
                        finish_attack(game, run.who);
                        break;
                }
            }

            void run_effects(GameState& game) {
                while (!game.effects.empty() && !game.is_over()) {
                    EffectRun& run = game.effects.back();
                    const Program& program = program_of(game, run.program);
                    if (run.pc >= program.size()) {
                        const EffectRun done = run;
                        game.effects.pop_back();
                        finish_run(game, done);
                        continue;
                    }
                    if (!run_step(game, run, program[run.pc])) {
                        return;
                    }
                }
            }

            void resume(GameState& game) {
                game.pending.pop_back();
                ++game.effects.back().pc;
                run_effects(game);
            }

        } // namespace

        bool is_effect_decision(DecisionKind kind) {
            return kind == DecisionKind::ChooseCards || kind == DecisionKind::ChoosePokemon || kind == DecisionKind::ChooseYesNo;
        }

        void start_effect(GameState& game, const EffectContext& who, ProgramRef program, AfterEffect then) {
            EffectRun run;
            run.who = who;
            run.program = program;
            run.then = then;
            game.effects.push_back(run);
            run_effects(game);
        }

        void add_effect_choice_actions(const GameState& game, std::vector<Action>& out) {
            const Decision& decision = game.decision();
            const EffectRun& run = game.effects.back();
            switch (decision.kind) {
                case DecisionKind::ChooseCards: {
                    std::vector<const CardDef*> seen;
                    for (const CardIndex card : choosable(game, run, current_step(game))) {
                        const CardDef* def = &game.def(card);
                        if (std::find(seen.begin(), seen.end(), def) == seen.end()) {
                            seen.push_back(def);
                            out.push_back(Action{ActionKind::ChooseCard, card, kActiveSlot, 0});
                        }
                    }
                    if (run.chosen.size() >= decision.target) {
                        out.push_back(Action{ActionKind::ChooseDone, kNoCard, kActiveSlot, 0});
                    }
                    break;
                }
                case DecisionKind::ChoosePokemon: {
                    const PlayerState& p = game.player(decision.player);
                    for (std::size_t i = 0; i < p.bench.size(); ++i) {
                        out.push_back(Action{ActionKind::ChoosePokemon, kNoCard, bench_slot(i), 0});
                    }
                    break;
                }
                case DecisionKind::ChooseYesNo:
                    out.push_back(Action{ActionKind::AnswerYes, kNoCard, kActiveSlot, 0});
                    out.push_back(Action{ActionKind::AnswerNo, kNoCard, kActiveSlot, 0});
                    break;
                default:
                    throw std::logic_error("add_effect_choice_actions: not an effect decision");
            }
        }

        void apply_effect_choice(GameState& game, const Action& action) {
            EffectRun& run = game.effects.back();
            switch (action.kind) {
                case ActionKind::ChooseCard:
                    run.chosen.push_back(action.card);
                    if (run.chosen.size() >= game.decision().amount) {
                        resume(game);
                    }
                    break;
                case ActionKind::ChooseDone:
                case ActionKind::AnswerYes:
                    resume(game);
                    break;
                case ActionKind::ChoosePokemon:
                    run.chosen_pokemon = pokemon_at(game.player(run.who.player), action.target).id();
                    resume(game);
                    break;
                case ActionKind::AnswerNo:
                    game.pending.pop_back();
                    run.pc = current_step(game).jump;
                    run_effects(game);
                    break;
                default:
                    throw std::logic_error("apply_effect_choice: not an answer to an effect decision");
            }
        }

    } // namespace detail

} // namespace pokebot