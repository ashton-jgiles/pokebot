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
                    case ProgramKind::Ability:
                        return def.abilities[ref.index].program;
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
                    case Zone::DeckTop:
                        return &p.deck;
                    case Zone::YourAttachedEnergy:
                        return nullptr; // spread over several Pokemon; see choosable and take_card
                    case Zone::InPlay:
                        return nullptr;
                    case Zone::TargetEnergy: {
                        const PokemonInPlay* target = run.target_index < run.targets.size() ? find_pokemon(game, run.targets[run.target_index]) : nullptr;
                        return target != nullptr ? &target->energy : nullptr;
                    }
                    case Zone::ChosenEnergy: {
                        const PokemonInPlay* chosen = run.chosen_pokemon == kNoCard ? nullptr : find_pokemon(game, run.chosen_pokemon);
                        return chosen != nullptr ? &chosen->energy : nullptr;
                    }
                }
                return nullptr;
            }

            CardZone* zone_of(GameState& game, const EffectRun& run, Zone zone) {
                return const_cast<CardZone*>(zone_of(static_cast<const GameState&>(game), run, zone));
            }

            std::vector<CardIndex> choosable(const GameState& game, const EffectRun& run, const Step& step) {
                std::vector<CardIndex> cards;
                if (step.from == Zone::DeckTop) {
                    const auto& deck = game.player(run.who.player).deck;
                    const std::size_t look = std::min<std::size_t>(static_cast<std::size_t>(step.amount), deck.size());
                    for (std::size_t i = deck.size() - look; i < deck.size(); ++i) {
                        if (!run.chosen.contains(deck[i]) && matches(step.filter, game.def(deck[i]))) {
                            cards.push_back(deck[i]);
                        }
                    }
                    return cards;
                }
                if (step.from == Zone::YourAttachedEnergy) {
                    const PlayerState& p = game.player(run.who.player);
                    for (const Slot slot : in_play_slots(p)) {
                        for (const CardIndex card : pokemon_at(p, slot).energy) {
                            if (!run.chosen.contains(card) && matches(step.filter, game.def(card))) {
                                cards.push_back(card);
                            }
                        }
                    }
                    return cards;
                }
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

            int count_value(const GameState& game, const EffectRun& run, const Count& count, const PokemonInPlay* target = nullptr);

            bool remove_self_from_play(GameState& game, const EffectRun& run, PokemonInPlay& out) {
                PlayerState& p = game.player(owner_of(run.who.self));
                if (p.active && p.active->id() == run.who.self) {
                    out = *p.active;
                    p.active.reset();
                    end_effects_on(game, out.id());
                    return true;
                }
                for (std::size_t i = 0; i < p.bench.size(); ++i) {
                    if (p.bench[i].id() == run.who.self) {
                        out = p.bench[i];
                        p.bench.erase_at(i);
                        end_effects_on(game, out.id());
                        return true;
                    }
                }
                return false;
            }

            void shuffle_self_into_deck(GameState& game, const EffectRun& run) {
                PokemonInPlay pokemon;
                if (!remove_self_from_play(game, run, pokemon)) {
                    return;
                }
                PlayerState& p = game.player(owner_of(run.who.self));
                for (const CardIndex card : pokemon.stack) {
                    p.deck.push_back(card);
                }
                for (const CardIndex card : pokemon.energy) {
                    p.deck.push_back(card);
                }
                if (pokemon.tool != kNoCard) {
                    p.deck.push_back(pokemon.tool);
                }
                shuffle_deck(game, owner_of(run.who.self));
            }

            // The top `look` cards that weren't taken go to the bottom of the deck, in the same order.
            void rest_to_bottom(GameState& game, const EffectRun& run, const Step& step) {
                auto& deck = game.player(run.who.player).deck;
                const int left = std::max(0, step.amount - run.value);
                const std::size_t rest = std::min<std::size_t>(static_cast<std::size_t>(left), deck.size());
                CardZone reordered;
                for (std::size_t i = deck.size() - rest; i < deck.size(); ++i) {
                    reordered.push_back(deck[i]);
                }
                for (std::size_t i = 0; i + rest < deck.size(); ++i) {
                    reordered.push_back(deck[i]);
                }
                deck = reordered;
            }

            FixedVector<Slot, kMaxBenchSize + 1> damaged_slots(const GameState& game, int player) {
                FixedVector<Slot, kMaxBenchSize + 1> slots;
                const PlayerState& p = game.player(player);
                for (const Slot slot : in_play_slots(p)) {
                    if (pokemon_at(p, slot).damage_counters > 0) {
                        slots.push_back(slot);
                    }
                }
                return slots;
            }

            void move_counters(GameState& game, const EffectRun& run, const Step& step) {
                PokemonInPlay* from = run.chosen_pokemon == kNoCard ? nullptr : find_pokemon(game, run.chosen_pokemon);
                PokemonInPlay* to = run.targets.empty() ? nullptr : find_pokemon(game, run.targets[0]);
                if (from == nullptr || to == nullptr) {
                    return;
                }
                const int moved = std::min<int>(step.max, from->damage_counters);
                from->damage_counters = static_cast<std::uint16_t>(from->damage_counters - moved);
                to->damage_counters = static_cast<std::uint16_t>(to->damage_counters + moved);
            }

            // "This Pokemon" moves from the Bench into the Active Spot; the old Active goes to the Bench.
            void switch_to_active(GameState& game, const EffectRun& run) {
                PlayerState& p = game.player(run.who.player);
                if (!p.active || p.active->id() == run.who.self) {
                    return;
                }
                for (PokemonInPlay& benched : p.bench) {
                    if (benched.id() == run.who.self) {
                        const CardIndex leaving = p.active->id();
                        std::swap(*p.active, benched);
                        clear_special_conditions(benched);
                        end_effects_on(game, benched.id());
                        raise(game, Event{EventKind::MovedToBench, run.who.player, leaving, kNoCard});
                        return;
                    }
                }
            }

            // Your other Pokemon: anywhere in play except the targets (where something is being moved from).
            FixedVector<Slot, kMaxBenchSize + 1> destinations(const GameState& game, const EffectRun& run) {
                FixedVector<Slot, kMaxBenchSize + 1> slots;
                const PlayerState& p = game.player(run.who.player);
                for (const Slot slot : in_play_slots(p)) {
                    if (!run.targets.contains(pokemon_at(p, slot).id())) {
                        slots.push_back(slot);
                    }
                }
                return slots;
            }

            // Cards always go to their owner's hand, deck or discard pile, even when the effect is the opponent's.
            CardZone* owner_zone(GameState& game, const EffectRun& run, Zone zone, CardIndex card) {
                PlayerState& owner = game.player(owner_of(card));
                switch (zone) {
                    case Zone::Hand:
                        return &owner.hand;
                    case Zone::Discard:
                        return &owner.discard;
                    case Zone::Deck:
                    case Zone::DeckTop:
                        return &owner.deck;
                    default:
                        return zone_of(game, run, zone);
                }
            }

            // The slots of that side's Pokemon this step may choose: on the Bench, or anywhere in play.
            FixedVector<Slot, kMaxBenchSize + 1> targetable(const GameState& game, const EffectRun& run, const Step& step) {
                FixedVector<Slot, kMaxBenchSize + 1> slots;
                const PlayerState& p = game.player(player_on(step.side, run.who.player));
                for (const Slot slot : in_play_slots(p)) {
                    const PokemonInPlay& pokemon = pokemon_at(p, slot);
                    if (slot == kActiveSlot && step.from == Zone::Bench) {
                        continue;
                    }
                    if (run.targets.contains(pokemon.id()) || (step.needs_energy && pokemon.energy.empty()) ||
                        !matches(step.filter, game.def(pokemon.top()))) {
                        continue;
                    }
                    slots.push_back(slot);
                }
                return slots;
            }

            bool has_copy(const Program& program) {
                return std::any_of(program.begin(), program.end(), [](const Step& step) {
                    return step.op == Op::UseChosenAttack;
                });
            }

            // The attacks a copy attack may use: each attack of each matching Benched Pokemon, except other copy attacks.
            std::vector<Action> copy_options(const GameState& game, const EffectRun& run, const Step& step) {
                std::vector<Action> options;
                std::vector<std::pair<const CardDef*, std::uint8_t>> seen;
                const PlayerState& p = game.player(run.who.player);
                for (std::size_t i = 0; i < p.bench.size(); ++i) {
                    const CardIndex top = p.bench[i].top();
                    const CardDef& def = game.def(top);
                    if (!matches(step.filter, def)) {
                        continue;
                    }
                    for (std::size_t a = 0; a < def.attacks.size(); ++a) {
                        const auto key = std::make_pair(&def, static_cast<std::uint8_t>(a));
                        if (has_copy(def.attacks[a].program) || std::find(seen.begin(), seen.end(), key) != seen.end()) {
                            continue;
                        }
                        seen.push_back(key);
                        options.push_back(Action{ActionKind::ChooseAttack, top, bench_slot(i), static_cast<std::uint8_t>(a)});
                    }
                }
                return options;
            }

            EffectRun make_run(const GameState& game, const EffectContext& who, ProgramRef program, AfterEffect then) {
                EffectRun run;
                run.who = who;
                run.program = program;
                run.then = then;
                if (program.kind == ProgramKind::Attack) {
                    run.damage = static_cast<std::int16_t>(game.def(program.card).attacks[program.index].damage);
                }
                return run;
            }

            void damage_targets(GameState& game, const EffectRun& run, const Step& step) {
                const PokemonInPlay* attacker = find_pokemon(game, run.who.self);
                if (attacker == nullptr) {
                    return;
                }
                for (const CardIndex id : run.targets) {
                    PokemonInPlay* target = find_pokemon(game, id);
                    if (target == nullptr) {
                        continue;
                    }
                    const int base = step.amount + step.per * count_value(game, run, step.count, target);
                    const int damage = rules::attack_damage(game, *attacker, *target, base);
                    target->damage_counters = static_cast<std::uint16_t>(target->damage_counters + damage / 10);
                    if (damage > 0) {
                        raise(game, Event{EventKind::DamagedByAttack, static_cast<std::uint8_t>(owner_of(id)), id, run.who.self});
                    }
                }
            }

            void discard_all_energy(GameState& game, const EffectRun& run) {
                PokemonInPlay* self = find_pokemon(game, run.who.self);
                if (self == nullptr) {
                    return;
                }
                for (const CardIndex card : self->energy) {
                    game.player(owner_of(card)).discard.push_back(card);
                }
                self->energy.clear();
            }

            void return_self_to_hand(GameState& game, const EffectRun& run) {
                PlayerState& p = game.player(owner_of(run.who.self));
                PokemonInPlay pokemon;
                if (p.active && p.active->id() == run.who.self) {
                    pokemon = *p.active;
                    p.active.reset();
                } else {
                    bool found = false;
                    for (std::size_t i = 0; i < p.bench.size() && !found; ++i) {
                        if (p.bench[i].id() == run.who.self) {
                            pokemon = p.bench[i];
                            p.bench.erase_at(i);
                            found = true;
                        }
                    }
                    if (!found) {
                        return;
                    }
                }
                end_effects_on(game, pokemon.id());
                for (const CardIndex card : pokemon.stack) {
                    p.hand.push_back(card);
                }
                for (const CardIndex card : pokemon.energy) {
                    p.hand.push_back(card);
                }
                if (pokemon.tool != kNoCard) {
                    p.hand.push_back(pokemon.tool);
                }
            }

            bool take_card(GameState& game, const EffectRun& run, Zone from, CardIndex card) {
                if (from == Zone::YourAttachedEnergy) {
                    PlayerState& p = game.player(run.who.player);
                    for (const Slot slot : in_play_slots(p)) {
                        if (pokemon_at(p, slot).energy.remove(card)) {
                            return true;
                        }
                    }
                    return false;
                }
                CardZone* zone = zone_of(game, run, from);
                return zone != nullptr && zone->remove(card);
            }

            void move_chosen(GameState& game, EffectRun& run, const Step& step) {
                int moved = 0;
                for (const CardIndex card : run.chosen) {
                    if (!take_card(game, run, step.from, card)) {
                        continue;
                    }
                    ++moved;
                    if (step.reveal) {
                        show(game, opponent_of(run.who.player), card);
                    }
                    if (step.to == Zone::DeckTop) {
                        show(game, run.who.player, card); // you put it there
                    }
                    if (step.to == Zone::Bench) {
                        game.player(run.who.player).bench.push_back(new_pokemon(card, game.turn));
                        raise(game, Event{EventKind::PutOnBench, run.who.player, card, kNoCard});
                    } else if (CardZone* to = owner_zone(game, run, step.to, card)) {
                        to->push_back(card);
                    }
                }
                run.chosen.clear();
                run.value = static_cast<std::int16_t>(moved);
                if (moved > 0 && step.to == Zone::TargetEnergy && run.target_index < run.targets.size()) {
                    const PlayerState& p = game.player(run.who.player);
                    run.attached_to_active = run.attached_to_active || (p.active && p.active->id() == run.targets[run.target_index]);
                }
            }

            int hand_to_deck_bottom(GameState& game, int player) {
                PlayerState& p = game.player(player);
                const int moved = static_cast<int>(p.hand.size());
                game.rng.shuffle(p.hand);
                CardZone deck = p.hand; // the bottom of the deck is the front
                for (const CardIndex card : p.deck) {
                    deck.push_back(card);
                }
                p.deck = deck;
                p.hand.clear();
                return moved;
            }

            int count_value(const GameState& game, const EffectRun& run, const Count& count, const PokemonInPlay* target) {
                int total = 0;
                for (const int player : players_on(count.side, run.who.player)) {
                    const PlayerState& p = game.player(player);
                    switch (count.kind) {
                        case CountKind::None:
                            break;
                        case CountKind::Value:
                            return run.value;
                        case CountKind::Hand:
                            total += static_cast<int>(p.hand.size());
                            break;
                        case CountKind::Discard:
                            for (const CardIndex card : p.discard) {
                                total += matches(count.filter, game.def(card)) ? 1 : 0;
                            }
                            break;
                        case CountKind::Benched:
                            total += static_cast<int>(p.bench.size());
                            break;
                        case CountKind::PrizesTaken:
                            total += static_cast<int>(kPrizeCount - p.prizes.size());
                            break;
                        case CountKind::ActiveEnergy:
                            total += p.active ? static_cast<int>(p.active->energy.size()) : 0;
                            break;
                        case CountKind::DamageCountersOnSelf: {
                            const PokemonInPlay* self = find_pokemon(game, run.who.self);
                            return self != nullptr ? self->damage_counters : 0;
                        }
                        case CountKind::TargetDamageCounters:
                            return target != nullptr ? target->damage_counters : 0;
                    }
                }
                return total;
            }

            bool shares_type(const GameState& game, int player) {
                const PlayerState& mine = game.player(player);
                const PlayerState& theirs = game.player(opponent_of(player));
                for (const Slot a : in_play_slots(mine)) {
                    const auto my_types = rules::types(game, pokemon_at(mine, a));
                    for (const Slot b : in_play_slots(theirs)) {
                        for (const EnergyType type : rules::types(game, pokemon_at(theirs, b))) {
                            if (my_types.contains(type)) {
                                return true;
                            }
                        }
                    }
                }
                return false;
            }

            void shuffle_hand_into_deck(GameState& game, int player) {
                PlayerState& p = game.player(player);
                while (!p.hand.empty()) {
                    p.deck.push_back(p.hand.pop_back());
                }
                shuffle_deck(game, player);
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
                        clear_special_conditions(benched);
                        end_effects_on(game, benched.id());
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
                        deal_attack_damage(game, run.who, run.damage, run.ignore_defender);
                        break;
                    case Op::Draw: {
                        int drawn = 0;
                        for (const int player : players_on(step.side, run.who.player)) {
                            drawn += draw_cards(game, player, step.max);
                        }
                        run.value = static_cast<std::int16_t>(drawn);
                        break;
                    }
                    case Op::DiscardHand:
                        while (!p.hand.empty()) {
                            p.discard.push_back(p.hand.pop_back());
                        }
                        break;
                    case Op::ShuffleDeck:
                        shuffle_deck(game, run.who.player);
                        break;
                    case Op::ShuffleHandIntoDeck:
                        for (const int player : players_on(step.side, run.who.player)) {
                            shuffle_hand_into_deck(game, player);
                        }
                        break;
                    case Op::ChooseCards: {
                        if (step.from == Zone::DeckTop) { // looking at the top cards: you now know where they are
                            const auto& deck = p.deck;
                            const std::size_t look = std::min<std::size_t>(static_cast<std::size_t>(step.amount), deck.size());
                            for (std::size_t i = deck.size() - look; i < deck.size(); ++i) {
                                show(game, run.who.player, deck[i]);
                            }
                        } else if (step.from == Zone::Deck) { // searching: you look through the whole deck
                            for (const CardIndex card : p.deck) {
                                show(game, run.who.player, card);
                            }
                        }
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
                    case Op::ChooseBenched: {
                        run.chosen_pokemon = kNoCard;
                        const PlayerState& side = game.player(player_on(step.side, run.who.player));
                        if (std::none_of(side.bench.begin(), side.bench.end(), [&](const PokemonInPlay& pokemon) {
                                return matches(step.filter, game.def(pokemon.top()));
                            })) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChoosePokemon, 1, 1);
                        return false;
                    }
                    case Op::SwitchWithChosen:
                        switch_with_chosen(game, run, step);
                        break;
                    case Op::AskYesNo:
                        ask(game, run, DecisionKind::ChooseYesNo, 0, 0);
                        return false;
                    case Op::JumpUnless:
                        if (!condition_holds(game, run.who.player, step.condition, run.who.source, &run)) {
                            run.pc = step.jump;
                            return true;
                        }
                        break;
                    case Op::Jump:
                        run.pc = step.jump;
                        return true;
                    case Op::SetDamage:
                        run.damage = static_cast<std::int16_t>(step.amount + step.per * count_value(game, run, step.count));
                        break;
                    case Op::AddDamage:
                        run.damage = static_cast<std::int16_t>(run.damage + step.amount + step.per * count_value(game, run, step.count));
                        break;
                    case Op::FlipCoins: {
                        int heads = 0;
                        for (int i = 0; i < step.max; ++i) {
                            heads += game.rng.coin_flip() ? 1 : 0;
                        }
                        run.value = static_cast<std::int16_t>(heads);
                        break;
                    }
                    case Op::FlipUntilTails: {
                        int heads = 0;
                        while (game.rng.coin_flip()) {
                            ++heads;
                        }
                        run.value = static_cast<std::int16_t>(heads);
                        break;
                    }
                    case Op::HandToDeckBottom: {
                        int moved = 0;
                        for (const int player : players_on(step.side, run.who.player)) {
                            moved += hand_to_deck_bottom(game, player);
                        }
                        run.value = static_cast<std::int16_t>(moved);
                        break;
                    }
                    case Op::ChooseTargets: {
                        run.targets.clear();
                        run.target_index = 0;
                        const std::size_t max = std::min<std::size_t>(step.max, targetable(game, run, step).size());
                        if (max == 0) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChoosePokemon, max, std::min<std::size_t>(step.min, max));
                        return false;
                    }
                    case Op::DamageTargets:
                        damage_targets(game, run, step);
                        break;
                    case Op::PlaceCounters:
                        if (step.max == 0 || targetable(game, run, step).empty()) {
                            break;
                        }
                        run.value = 0; // the lowest slot still allowed, so each spread of counters is offered once
                        ask(game, run, DecisionKind::ChoosePokemon, step.max, step.max);
                        return false;
                    case Op::DiscardAllEnergy:
                        discard_all_energy(game, run);
                        break;
                    case Op::ReturnSelfToHand:
                        return_self_to_hand(game, run);
                        break;
                    case Op::ChooseAttack:
                        run.chosen_attack = AttackRef{};
                        if (copy_options(game, run, step).empty()) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChooseAttack, 1, 1);
                        return false;
                    case Op::UseChosenAttack: {
                        if (run.chosen_attack.text_from == kNoCard) {
                            break;
                        }
                        const ProgramRef copied{run.chosen_attack.text_from, ProgramKind::Attack, run.chosen_attack.index};
                        const EffectRun nested = make_run(game, run.who, copied, AfterEffect::Nothing);
                        ++run.pc;
                        game.effects.push_back(nested); // runs next; this effect carries on after it
                        return true;
                    }
                    case Op::CountersOnTargets:
                        for (const CardIndex id : run.targets) {
                            if (PokemonInPlay* target = find_pokemon(game, id)) {
                                target->damage_counters = static_cast<std::uint16_t>(target->damage_counters + step.max);
                            }
                        }
                        break;
                    case Op::ShuffleSelfIntoDeck:
                        shuffle_self_into_deck(game, run);
                        break;
                    case Op::RestToBottom:
                        rest_to_bottom(game, run, step);
                        break;
                    case Op::ChooseCounterSource:
                        run.chosen_pokemon = kNoCard;
                        if (damaged_slots(game, run.who.player).empty()) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChoosePokemon, 1, 1);
                        return false;
                    case Op::MoveCounters:
                        move_counters(game, run, step);
                        break;
                    case Op::CountersOnSelf:
                        if (PokemonInPlay* self = find_pokemon(game, run.who.self)) {
                            self->damage_counters = static_cast<std::uint16_t>(self->damage_counters + step.max);
                        }
                        break;
                    case Op::DiscardStadium:
                        for (int player = 0; player < kPlayers; ++player) {
                            PlayerState& owner = game.player(player);
                            if (owner.stadium != kNoCard) {
                                owner.discard.push_back(owner.stadium);
                                owner.stadium = kNoCard;
                            }
                        }
                        break;
                    case Op::SwitchToActive:
                        switch_to_active(game, run);
                        break;
                    case Op::EachTarget:
                        if (run.target_index >= run.targets.size()) {
                            run.pc = step.jump;
                            return true;
                        }
                        break;
                    case Op::NextTarget:
                        ++run.target_index;
                        run.pc = step.jump;
                        return true;
                    case Op::Custom:
                        if (!run_custom(game, run, step)) {
                            return false;
                        }
                        break;
                    case Op::ChooseDestination:
                        run.chosen_pokemon = kNoCard;
                        if (destinations(game, run).empty()) {
                            break;
                        }
                        ask(game, run, DecisionKind::ChoosePokemon, 1, 1);
                        return false;
                    case Op::IgnoreDefenderEffects:
                        run.ignore_defender = true;
                        break;
                    case Op::AddLasting:
                        add_lasting(game, run, step);
                        break;
                    case Op::ApplySpecialCondition:
                        for (const int player : players_on(step.side, run.who.player)) {
                            PlayerState& target = game.player(player);
                            if (target.active) {
                                apply_special_condition(game, *target.active, step.special, step.max);
                            }
                        }
                        break;
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
                    case AfterEffect::FinishAbility:
                        handle_knockouts(game);
                        break;
                    case AfterEffect::DiscardTrainer: {
                        PlayerState& p = game.player(run.who.player);
                        if (p.resolving.remove(run.who.source)) {
                            p.discard.push_back(run.who.source);
                        }
                        handle_knockouts(game); // Transformation Tome can swap in a Basic with less HP than its damage; also checks Bench limits
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
            return kind == DecisionKind::ChooseCards || kind == DecisionKind::ChoosePokemon || kind == DecisionKind::ChooseYesNo ||
                   kind == DecisionKind::ChooseAttack || kind == DecisionKind::CustomChoice;
        }

        bool condition_holds(const GameState& game, int player, const Condition& condition, CardIndex playing, const EffectRun* run) {
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
                case ConditionKind::KnockedOutByAttackLastTurn:
                    return game.history.opponents_last_turn().knocked_out_by_attack[static_cast<std::size_t>(player)] > 0;
                case ConditionKind::HandHas:
                    return std::any_of(p.hand.begin(), p.hand.end(), [&](CardIndex card) {
                        return card != playing && matches(condition.filter, game.def(card));
                    });
                case ConditionKind::BenchHas:
                    return std::any_of(p.bench.begin(), p.bench.end(), [&](const PokemonInPlay& pokemon) {
                        return matches(condition.filter, game.def(pokemon.top()));
                    });
                case ConditionKind::ActiveIs:
                    return p.active && matches(condition.filter, game.def(p.active->top()));
                case ConditionKind::SelfHasEnergy: {
                    const PokemonInPlay* self = run != nullptr ? find_pokemon(game, run->who.self) : nullptr;
                    return self != nullptr && std::any_of(self->energy.begin(), self->energy.end(), [&](CardIndex card) {
                               return matches(condition.filter, game.def(card));
                           });
                }
                case ConditionKind::YouHaveDamaged:
                    return !damaged_slots(game, player).empty();
                case ConditionKind::SelfIs: {
                    const PokemonInPlay* self = run != nullptr ? find_pokemon(game, run->who.self) : nullptr;
                    return self != nullptr && matches(condition.filter, game.def(self->top()));
                }
                case ConditionKind::StadiumInPlay:
                    return rules::stadium_in_play(game) != kNoCard;
                case ConditionKind::MorePrizesThanOpponent:
                    return p.prizes.size() > opponent.prizes.size();
                case ConditionKind::InPlayHas:
                    for (const Slot slot : in_play_slots(p)) {
                        if (matches(condition.filter, game.def(pokemon_at(p, slot).top()))) {
                            return true;
                        }
                    }
                    return false;
                case ConditionKind::YourEnergyHas:
                    for (const Slot slot : in_play_slots(p)) {
                        for (const CardIndex card : pokemon_at(p, slot).energy) {
                            if (matches(condition.filter, game.def(card))) {
                                return true;
                            }
                        }
                    }
                    return false;
                case ConditionKind::AttachedToActive:
                    return run != nullptr && run->attached_to_active;
                case ConditionKind::Custom:
                    return custom_condition(game, player, condition.custom);
                case ConditionKind::HasBenched:
                    return !p.bench.empty();
                case ConditionKind::OpponentHasBenched:
                    return !opponent.bench.empty();
                case ConditionKind::BenchNotFull:
                    return p.bench.size() < rules::bench_size(game, player);
                case ConditionKind::DiscardHas:
                    return discard_has(game, player, condition.filter);
                case ConditionKind::ValueAtLeast:
                    return run != nullptr && run->value >= condition.value;
                case ConditionKind::OpponentPrizesAtMost:
                    return static_cast<int>(opponent.prizes.size()) <= condition.value;
                case ConditionKind::SharesType:
                    return shares_type(game, player);
                case ConditionKind::SelfEnergyAtLeast: {
                    const PokemonInPlay* self = run != nullptr ? find_pokemon(game, run->who.self) : nullptr;
                    return self != nullptr && static_cast<int>(self->energy.size()) >= condition.value;
                }
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
            game.effects.push_back(make_run(game, who, program, then));
            run_effects(game);
        }

        void queue_effect(GameState& game, const EffectContext& who, ProgramRef program, AfterEffect then) {
            if (game.effects.empty()) {
                start_effect(game, who, program, then);
                return;
            }
            game.effects.push_back(make_run(game, who, program, then)); // the running loop picks it up next
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
                    const Step& step = current_step(game);
                    if (step.op == Op::ChooseBenched) {
                        const PlayerState& p = game.player(player_on(step.side, run.who.player));
                        for (std::size_t i = 0; i < p.bench.size(); ++i) {
                            if (matches(step.filter, game.def(p.bench[i].top()))) {
                                out.push_back(Action{ActionKind::ChoosePokemon, kNoCard, bench_slot(i), 0});
                            }
                        }
                        break;
                    }
                    if (step.op == Op::ChooseCounterSource) {
                        for (const Slot slot : damaged_slots(game, run.who.player)) {
                            out.push_back(Action{ActionKind::ChoosePokemon, kNoCard, slot, 0});
                        }
                        break;
                    }
                    if (step.op == Op::ChooseDestination) {
                        for (const Slot slot : destinations(game, run)) {
                            out.push_back(Action{ActionKind::ChoosePokemon, kNoCard, slot, 0});
                        }
                        break;
                    }
                    for (const Slot slot : targetable(game, run, step)) {
                        if (step.op != Op::PlaceCounters || slot >= run.value) {
                            out.push_back(Action{ActionKind::ChoosePokemon, kNoCard, slot, 0});
                        }
                    }
                    if (step.op == Op::ChooseTargets && run.targets.size() >= decision.target) {
                        out.push_back(Action{ActionKind::ChooseDone, kNoCard, kActiveSlot, 0});
                    }
                    break;
                }
                case DecisionKind::ChooseAttack:
                    for (const Action& option : copy_options(game, run, current_step(game))) {
                        out.push_back(option);
                    }
                    break;
                case DecisionKind::CustomChoice:
                    add_custom_actions(game, run, current_step(game), out);
                    break;
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
                    if (game.decision().kind == DecisionKind::CustomChoice) {
                        apply_custom(game, run, current_step(game), action);
                        resume(game);
                        break;
                    }
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
                    const Step& step = current_step(game);
                    const bool yours = step.op == Op::ChooseCounterSource || step.op == Op::ChooseDestination;
                    const int owner = yours ? run.who.player : player_on(step.side, run.who.player);
                    PokemonInPlay& pokemon = pokemon_at(game.player(owner), action.target);
                    if (step.op == Op::ChooseBenched || step.op == Op::ChooseCounterSource || step.op == Op::ChooseDestination) {
                        run.chosen_pokemon = pokemon.id();
                        resume(game);
                    } else if (step.op == Op::ChooseTargets) {
                        run.targets.push_back(pokemon.id());
                        if (run.targets.size() >= game.decision().amount) {
                            resume(game);
                        }
                    } else {
                        pokemon.damage_counters = static_cast<std::uint16_t>(pokemon.damage_counters + 1);
                        run.value = static_cast<std::int16_t>(action.target);
                        if (--game.pending.back().amount == 0) {
                            resume(game);
                        }
                    }
                    break;
                }
                case ActionKind::ChooseAttack:
                    run.chosen_attack = AttackRef{action.card, action.amount};
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