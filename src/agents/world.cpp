#include "agents/world.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "agents/flat_mc/deck_model.hpp"
#include "engine/knowledge/knowledge.hpp"

namespace pokebot {

    namespace {

        using Cards = FixedVector<CardIndex, kDeckSize>;

        // The cards dealt into one player's hidden places.
        struct Dealt {
            Cards deck;
            Cards hand;
        };

        CardTable table_for(const Observation& observation, const DeckModel* model, Rng& rng, const ArchetypeList*& drawn) {
            std::array<const CardDef*, kMaxCards> defs{};
            for (std::size_t i = 0; i < kMaxCards; ++i) {
                defs[i] = observation.defs[i] != nullptr ? observation.defs[i] : &unknown_card();
            }
            if (model == nullptr) {
                return CardTable(defs);
            }
            const auto first = static_cast<std::size_t>(opponent_of(observation.viewer)) * kDeckSize;
            Cards unseen;
            for (std::size_t i = first; i < first + kDeckSize; ++i) {
                if (observation.defs[i] == nullptr) {
                    unseen.push_back(static_cast<CardIndex>(i));
                }
            }
            drawn = &model->library().lists()[model->draw(model->belief(observation), rng)];
            const DealtCards dealt = DeckModel::deal(*drawn, opponent_cards_seen(observation), unseen.size(), rng);
            for (std::size_t i = 0; i < unseen.size(); ++i) {
                defs[unseen[i]] = dealt[i];
            }
            return CardTable(defs);
        }

        template <class Visit>
        void each_placed_card(const Observation& observation, Visit visit) {
            for (const PlayerView& player : observation.players) {
                for (const auto* zone : {&player.hand, &player.discard, &player.lost_zone}) {
                    for (const CardIndex card : *zone) {
                        visit(card);
                    }
                }
                for (const DeckCard& card : player.deck) {
                    visit(card.card);
                }
                for (const CardIndex card : player.resolving) {
                    visit(card);
                }
                if (player.stadium != kNoCard) {
                    visit(player.stadium);
                }
                const auto in_play = [&](const PokemonView& seen) {
                    for (const CardIndex card : seen.pokemon.stack) {
                        visit(card);
                    }
                    for (const CardIndex card : seen.pokemon.energy) {
                        visit(card);
                    }
                    if (seen.pokemon.tool != kNoCard) {
                        visit(seen.pokemon.tool);
                    }
                };
                if (player.active) {
                    in_play(*player.active);
                }
                for (const PokemonView& pokemon : player.bench) {
                    in_play(pokemon);
                }
            }
        }

        PlayerState deal(const PlayerView& view, const Cards& pool, Dealt& dealt) {
            const std::size_t deck_size = view.deck_size;
            const std::size_t hand_size = view.hand_size;
            const std::size_t hidden_hand = hand_size - view.hand.size();
            const std::size_t prizes = view.prize_count;
            if (view.hand.size() > hand_size || pool.size() != deck_size - view.deck.size() + hidden_hand + prizes) {
                throw std::logic_error("World: the observation's hidden cards don't add up (" + std::to_string(pool.size()) + " unplaced)");
            }
            std::size_t next = 0;
            PlayerState player;
            std::array<CardIndex, kDeckSize> deck{};
            deck.fill(kNoCard);
            for (const DeckCard& card : view.deck) {
                deck[deck_size - 1 - std::size_t{card.from_top}] = card.card;
            }
            for (std::size_t i = 0; i < deck_size; ++i) {
                if (deck[i] == kNoCard) {
                    deck[i] = pool[next++];
                    dealt.deck.push_back(deck[i]);
                }
                player.deck.push_back(deck[i]);
            }
            player.hand = view.hand;
            for (std::size_t i = 0; i < hidden_hand; ++i) {
                player.hand.push_back(pool[next++]);
                dealt.hand.push_back(player.hand.back());
            }
            for (std::size_t i = 0; i < prizes; ++i) {
                player.prizes.push_back(pool[next++]);
            }
            player.discard = view.discard;
            player.lost_zone = view.lost_zone;
            if (view.active) {
                player.active = view.active->pokemon;
            }
            for (const PokemonView& pokemon : view.bench) {
                player.bench.push_back(pokemon.pokemon);
            }
            player.resolving = view.resolving;
            player.stadium = view.stadium;
            player.mulligans = view.mulligans;
            return player;
        }

        // The cards that can stand in for the ones `run` chose that the viewer can't see: those dealt
        // into the zone its current step chooses from, its player's hand or deck.
        const Cards& stand_ins(const Observation& observation, const EffectRun& run, const Dealt& dealt) {
            const CardDef* def = run.program.card < kMaxCards ? observation.def(run.program.card) : nullptr;
            const Program* program = nullptr;
            if (def != nullptr) {
                const std::size_t index = run.program.index;
                switch (run.program.kind) {
                    case ProgramKind::Attack:
                        program = index < def->attacks.size() ? &def->attacks[index].program : nullptr;
                        break;
                    case ProgramKind::Trainer:
                        program = &def->program;
                        break;
                    case ProgramKind::Ability:
                        program = index < def->abilities.size() ? &def->abilities[index].program : nullptr;
                        break;
                }
            }
            const bool from_hand = program != nullptr && run.pc < program->size() && (*program)[run.pc].from == Zone::Hand;
            return from_hand ? dealt.hand : dealt.deck;
        }

    } // namespace

    const CardDef& unknown_card() {
        static const CardDef card = [] {
            CardDef c;
            c.id = "unknown";
            c.name = "unknown card";
            c.kind = CardKind::Trainer;
            c.trainer_kind = TrainerKind::Item;
            c.text = "A card you haven't seen.";
            return c;
        }();
        return card;
    }

    World::World(const Observation& observation, Rng& rng, const DeckModel* model) : cards_(table_for(observation, model, rng, list_)) {
        if (observation.turn == 0 || observation.is_over()) {
            throw std::invalid_argument("World: the observation must be from a game past setup and not over");
        }
        const int viewer = observation.viewer;
        CardSet placed;
        each_placed_card(observation, [&](CardIndex card) { placed.insert(card); });

        game_.cards = &cards_;
        std::array<Dealt, kPlayers> dealt;
        for (int p = 0; p < kPlayers; ++p) {
            Cards pool;
            for (std::size_t i = 0; i < kDeckSize; ++i) {
                const auto card = static_cast<CardIndex>(static_cast<std::size_t>(p) * kDeckSize + i);
                if (!placed.contains(card)) {
                    pool.push_back(card);
                }
            }
            rng.shuffle(pool);
            const auto index = static_cast<std::size_t>(p);
            game_.players[index] = deal(observation.players[index], pool, dealt[index]);
        }

        game_.current_player = observation.current_player;
        game_.first_player = observation.first_player;
        game_.turn = observation.turn;
        game_.phase = observation.phase;
        game_.flags = observation.flags;
        game_.history = observation.history;
        game_.pending = observation.pending;
        game_.lasting = observation.lasting;
        game_.result = observation.result;
        for (EffectRun run : observation.effects) {
            const Cards& candidates = stand_ins(observation, run, dealt[run.who.player]);
            for (CardIndex& card : run.chosen) {
                if (card != kNoCard) {
                    continue;
                }
                for (const CardIndex candidate : candidates) {
                    if (!run.chosen.contains(candidate)) {
                        card = candidate;
                        break;
                    }
                }
                if (card == kNoCard) {
                    throw std::logic_error("World: no hidden card to stand in for one an effect chose");
                }
            }
            game_.effects.push_back(run);
        }

        each_placed_card(observation, [&](CardIndex card) { detail::show(game_, viewer, card); });
        for (const CardIndex card : observation.opponent_seen) {
            game_.knowledge.seen[static_cast<std::size_t>(viewer)].insert(card);
        }
        detail::note_visible(game_);
        game_.rng = Rng(rng.next());
    }

} // namespace pokebot