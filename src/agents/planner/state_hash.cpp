#include "agents/planner/state_hash.hpp"

#include <algorithm>

namespace pokebot {

    namespace {

        class Hasher {
            public:
                std::uint64_t value() const { return hash_; }

                void add(std::uint64_t v) {
                    hash_ = mix(hash_ ^ (v + 0x9E3779B97F4A7C15ULL));
                }

                template <class T, std::size_t N>
                void add(const FixedVector<T, N>& items) {
                    add(items.size());
                    for (const T& item : items) {
                        add(item);
                    }
                }

                void add(const SpecialConditions& s) {
                    add(s.asleep);
                    add(s.paralyzed);
                    add(s.burned);
                    add(s.confused);
                    add(s.poison);
                    add(s.paralyzed_turn);
                }

                void add(const PokemonInPlay& pokemon) {
                    add(pokemon.stack);
                    add(pokemon.energy);
                    add(pokemon.tool);
                    add(pokemon.damage_counters);
                    add(pokemon.turn_played);
                    add(pokemon.special);
                }

                void add(const PlayerState& player) {
                    add(player.deck);
                    FixedVector<CardIndex, kDeckSize> hand = player.hand; // its order doesn't matter
                    std::sort(hand.begin(), hand.end());
                    add(hand);
                    add(player.prizes);
                    add(player.discard);
                    add(player.lost_zone);
                    add(player.active.has_value());
                    if (player.active) {
                        add(*player.active);
                    }
                    add(player.bench);
                    add(player.resolving);
                    add(player.stadium);
                    add(player.mulligans);
                }

                void add(const AbilityUse& use) {
                    add(use.pokemon);
                    add(use.card);
                    add(use.index);
                }

                void add(const TurnFlags& flags) {
                    add(flags.energy_attached);
                    add(flags.supporter_played);
                    add(flags.stadium_played);
                    add(flags.stadium_used);
                    add(flags.retreated);
                    add(flags.attacks_used);
                    add(flags.abilities_used);
                }

                void add(const AttackRef& attack) {
                    add(attack.text_from);
                    add(attack.index);
                }

                void add(const TurnRecord& record) {
                    add(record.player);
                    for (int p = 0; p < kPlayers; ++p) {
                        add(record.knocked_out[static_cast<std::size_t>(p)]);
                        add(record.knocked_out_by_attack[static_cast<std::size_t>(p)]);
                    }
                    add(record.attacker);
                    add(record.attack);
                }

                void add(const Decision& decision) {
                    add(static_cast<std::uint64_t>(decision.kind));
                    add(decision.player);
                    add(decision.amount);
                    add(decision.target);
                }

                void add(const EffectRun& run) {
                    add(run.who.player);
                    add(run.who.self);
                    add(run.who.source);
                    add(run.program.card);
                    add(static_cast<std::uint64_t>(run.program.kind));
                    add(run.program.index);
                    add(static_cast<std::uint64_t>(run.then));
                    add(run.pc);
                    add(run.chosen);
                    add(run.chosen_pokemon);
                    add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(run.damage)));
                    add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(run.value)));
                    add(run.targets);
                    add(run.chosen_attack);
                    add(run.target_index);
                    add(run.attached_to_active);
                    add(run.ignore_defender);
                }

                void add(const LastingEffect& effect) {
                    add(static_cast<std::uint64_t>(effect.kind));
                    add(effect.player);
                    add(effect.pokemon);
                    add(effect.attack);
                    add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(effect.amount)));
                    add(effect.only_ex);
                    add(effect.turn);
                }

                void add(const CardSet& cards) {
                    std::uint64_t word = 0;
                    for (std::size_t i = 0; i < kMaxCards; ++i) {
                        if (cards.contains(static_cast<CardIndex>(i))) {
                            word |= std::uint64_t{1} << (i % 64);
                        }
                        if (i % 64 == 63 || i + 1 == kMaxCards) {
                            add(word);
                            word = 0;
                        }
                    }
                }
            
            private:
                static std::uint64_t mix(std::uint64_t z) {
                    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                    return z ^ (z >> 31);
                }

            std::uint64_t hash_ = 0;
        };

    } // namespace

    std::uint64_t hash_state(const GameState& game) {
        Hasher h;
        for (const PlayerState& player : game.players) {
            h.add(player);
        }
        h.add(game.current_player);
        h.add(game.first_player);
        h.add(game.turn);
        h.add(static_cast<std::uint64_t>(game.phase));
        h.add(game.flags);
        for (const TurnRecord& record : game.history.turns) {
            h.add(record);
        }
        h.add(game.pending);
        h.add(game.effects);
        h.add(game.lasting);
        for (int p = 0; p < kPlayers; ++p) {
            h.add(game.knowledge.located[static_cast<std::size_t>(p)]);
            h.add(game.knowledge.seen[static_cast<std::size_t>(p)]);
        }
        h.add(static_cast<std::uint64_t>(game.result));
        return h.value();
    }
} // namespace pokebot