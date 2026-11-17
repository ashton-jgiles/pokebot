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
                    case ProgramKind::Trainer:
                        return def.program;
                }
                throw std::logic_error("program_of: unknown program kind");
            }

            const Step& current_step(const GameState& game) {
                const EffectRun& run = game.effects.back();
                return program_of(game, run.program)[run.pc];
            }

            int player_on(Side side, int you) {
                return side == Side::Opponent ? opponent_of(you) : you;
            }

            FixedVector<int, kPlayers> players_on(Side side, int you) {
                FixedVector<int, kPlayers> players;
                if (side != Side::Opponent) {
                    players.push_back(you);
                }
                if (side != Side::You) {
                    players.push_back(opponent_of(you));
                }
                return players;
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
                    case Zone::Bench:
                        return nullptr;
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

            // How many cards this step may choose: no more than match, and no more than fit on the Bench.
            std::size_t choice_limit(const GameState& game, const EffectRun& run, const Step& step) {
                std::size_t limit = std::min<std::size_t>(step.max, choosable(game, run, step).size());
                if (step.to == Zone::Bench) {
                    const int player = run.who.player;
                    const std::size_t bench = game.player(player).bench.size();
                    const std::size_t size = rules::bench_size(game, player);
                    limit = std::min(limit, size > bench ? size - bench : 0);
                }
                return limit;
            }

            void ask(GameState& game, const EffectRun& run, DecisionKind kind, std::size_t max, std::size_t min) {
                game.pending.push_back(Decision{kind, run.who.player, static_cast<std::uint8_t>(max), static_cast<std::uint8_t>(min)});
            }

            void move_chosen(GameState& game, EffectRun& run, const Step& step) {
                CardZone* from = zone_of(game, run, step.from);
                for (const CardIndex card : run.chosen) {
                    if (from == nullptr || !from->remove(card)) {
                        continue;
                    }
                    if (step.to == Zone::Bench) {
                        game.player(run.who.player).bench.push_back(new_pokemon(card, game.turn));
                        raise(game, Event{EventKind::PutOnBench, run.who.player, card, kNoCard});
                    } else if (CardZone* to = zone_of(game, run, step.to)) {
                        to->push_back(card);
                    }
                }
                run.chosen.clear();
            }

            void shuffle_hand_into_deck(GameState& game, int player) {
                PlayerState& p = game.player(player);
                while (!p.hand.empty()) {
                    p.deck.push_back(p.hand.pop_back());
                }
                game.rng.shuffle(p.deck);
            }

            void switch_with_chosen(GameState& game, const EffectRun& run, const Step& step) {
                const int player = player_on(step.side, run.who.player);
                PlayerState& p = game.player(player);
                if (run.chosen_pokemon == kNoCard || !p.active) {
                    return;
                }
                for (PokemonInPlay& benched : p.bench) {
                    if (benched.id() == run.chosen_pokemon) {
                        const CardIndex leaving = p.active->id();
                        std::swap(*p.active, benched);
                        raise(game, Event{EventKind::MovedToBench, static_cast<std::uint8_t>(player), leaving, kNoCard});
                        return;
                    }
                }
            }

            // Runs one step. Returns false if the step is waiting for a player's choice.
            bool run_step(GameState& game, EffectRun& run, const Step& step) {
                PlayerState& p = game.player(run.who.player);
                switch (step.op) {
                    case Op::Damage:
                        deal_attack_damage(game, run.who, game.def(run.program.card).attacks[run.program.index]);
                        break;
                    case Op::Draw:
                        for (const int player : players_on(step.side, run.who.player)) {
                            draw_cards(game, player, step.max);
                        }
                        break;
                    case Op::DiscardHand:
                        while (!p.hand.empty()) {
                            p.discard.push_back(p.hand.pop_back());
                        }
                        break;
                    case Op::ShuffleDeck:
                        game.rng.shuffle(p.deck);
                        break;
                    case Op::ShuffleHandIntoDeck:
                        for (const int player : players_on(step.side, run.who.player)) {
                            shuffle_hand_into_deck(game, player);
                        }
                        break;
                    case Op::ChooseCards: {
                        const std::size_t max = choice_limit(game, run, step);
                        if (max == 0) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChooseCards, max, std::min<std::size_t>(step.min, max));
                        return false;
                    }
                    case Op::MoveChosen:
                        move_chosen(game, run, step);
                        break;
                    case Op::ChooseBenched:
                        run.chosen_pokemon = kNoCard;
                        if (game.player(player_on(step.side, run.who.player)).bench.empty()) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChoosePokemon, 1, 1);
                        return false;
                    case Op::SwitchWithChosen:
                        switch_with_chosen(game, run, step);
                        break;
                    case Op::AskYesNo:
                        ask(game, run, DecisionKind::ChooseYesNo, 0, 0);
                        return false;
                    case Op::JumpUnless:
                        if (!condition_holds(game, run.who.player, step.condition, run.who.source)) {
                            run.pc = step.jump;
                            return true;
                        }
                        break;
                    case Op::Jump:
                        run.pc = step.jump;
                        return true;
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
                    case AfterEffect::DiscardTrainer: {
                        PlayerState& p = game.player(run.who.player);
                        if (p.resolving.remove(run.who.source)) {
                            p.discard.push_back(run.who.source);
                        }
                        break;
                    }
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

            // The current step's choice is complete: carry on with the next step.
            void resume(GameState& game) {
                game.pending.pop_back();
                ++game.effects.back().pc;
                run_effects(game);
            }

            bool discard_has(const GameState& game, int player, const CardFilter& filter) {
                for (const CardIndex card : game.player(player).discard) {
                    if (matches(filter, game.def(card))) {
                        return true;
                    }
                }
                return false;
            }

        } // namespace

        bool is_effect_decision(DecisionKind kind) {
            return kind == DecisionKind::ChooseCards || kind == DecisionKind::ChoosePokemon || kind == DecisionKind::ChooseYesNo;
        }

        bool condition_holds(const GameState& game, int player, const Condition& condition, CardIndex playing) {
            const PlayerState& p = game.player(player);
            const PlayerState& opponent = game.player(opponent_of(player));
            switch (condition.kind) {
                case ConditionKind::HandBesides: {
                    const std::size_t others = p.hand.size() - (p.hand.contains(playing) ? 1 : 0);
                    return static_cast<int>(others) >= condition.value;
                }
                case ConditionKind::PrizesExactly:
                    return static_cast<int>(p.prizes.size()) == condition.value;
                case ConditionKind::KnockedOutLastTurn:
                    return game.history.opponents_last_turn().knocked_out[static_cast<std::size_t>(player)] > 0;
                case ConditionKind::HasBenched:
                    return !p.bench.empty();
                case ConditionKind::OpponentHasBenched:
                    return !opponent.bench.empty();
                case ConditionKind::BenchNotFull:
                    return p.bench.size() < rules::bench_size(game, player);
                case ConditionKind::DiscardHas:
                    return discard_has(game, player, condition.filter);
            }
            return false;
        }

        bool requirements_met(const GameState& game, int player, CardIndex card) {
            for (const Condition& condition : game.def(card).conditions) {
                if (!condition_holds(game, player, condition, card)) {
                    return false;
                }
            }
            return true;
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
                    const PlayerState& p = game.player(player_on(current_step(game).side, run.who.player));
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
                case ActionKind::ChoosePokemon: {
                    const int player = player_on(current_step(game).side, run.who.player);
                    run.chosen_pokemon = pokemon_at(game.player(player), action.target).id();
                    resume(game);
                    break;
                }
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