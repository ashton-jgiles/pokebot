#include "agents/is_mcts/info_set.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace pokebot {

    namespace {

        constexpr std::uint64_t kNoCardKey = 0x6E6F2D63617264ULL;
        constexpr std::uint64_t kUnseenKey = 0x756E7365656EULL;

        std::uint64_t id_key(const std::string& id) {
            std::uint64_t h = 0xCBF29CE484222325ULL;
            for (const char c : id) {
                h ^= static_cast<unsigned char>(c);
                h *= 0x100000001B3ULL;
            }
            return h;
        }

        class Hasher {
        public:
            Hasher(const Observation& observation, int numbered) : observation_(observation), numbered_(numbered) {}

            std::uint64_t value() const { return hash_; }
            void add(std::uint64_t v) { hash_ = mix(hash_ ^ (v + 0x9E3779B97F4A7C15ULL)); }
            void card(CardIndex c) { add(key(c)); }

            template <std::size_t N>
            void cards(const FixedVector<CardIndex, N>& list) {
                add(list.size());
                for (const CardIndex c : list) {
                    card(c);
                }
            }

            template <std::size_t N>
            void sorted_cards(const FixedVector<CardIndex, N>& list) {
                std::array<std::uint64_t, N> keys{};
                for (std::size_t i = 0; i < list.size(); ++i) {
                    keys[i] = key(list[i]);
                }
                std::sort(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(list.size()));
                add(list.size());
                for (std::size_t i = 0; i < list.size(); ++i) {
                    add(keys[i]);
                }
            }

            void attack(const AttackRef& a) {
                card(a.text_from);
                add(a.index);
            }

            void pokemon(const PokemonView& seen) {
                const PokemonInPlay& p = seen.pokemon;
                cards(p.stack);
                cards(p.energy);
                card(p.tool);
                add(p.damage_counters);
                add(p.turn_played);
                add(p.special.asleep);
                add(p.special.paralyzed);
                add(p.special.burned);
                add(p.special.confused);
                add(p.special.poison);
                add(p.special.paralyzed_turn);
                add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(seen.max_hp)));
            }

            void player(const PlayerView& view) {
                add(view.hand_size);
                add(view.deck_size);
                add(view.prize_count);
                add(view.mulligans);
                add(view.face_down);
                sorted_cards(view.hand);
                add(view.deck.size());
                for (const DeckCard& known : view.deck) {
                    card(known.card);
                    add(known.from_top);
                }
                cards(view.discard);
                cards(view.lost_zone);
                add(view.active.has_value());
                if (view.active) {
                    pokemon(*view.active);
                }
                add(view.bench.size());
                for (const PokemonView& benched : view.bench) {
                    pokemon(benched);
                }
                cards(view.resolving);
                card(view.stadium);
            }

            void decision(const Decision& d) {
                add(static_cast<std::uint64_t>(d.kind));
                add(d.player);
                add(d.amount);
                add(d.target);
            }

            void run(const EffectRun& r) {
                add(r.who.player);
                card(r.who.self);
                card(r.who.source);
                card(r.program.card);
                add(static_cast<std::uint64_t>(r.program.kind));
                add(r.program.index);
                add(static_cast<std::uint64_t>(r.then));
                add(r.pc);
                cards(r.chosen);
                card(r.chosen_pokemon);
                add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(r.damage)));
                add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(r.value)));
                cards(r.targets);
                attack(r.chosen_attack);
                add(r.target_index);
                add(r.attached_to_active);
                add(r.ignore_defender);
            }

        private:
            std::uint64_t key(CardIndex c) {
                if (c >= kMaxCards) {
                    return kNoCardKey;
                }
                if (keys_[c] == 0) {
                    const CardDef* def = observation_.def(c);
                    keys_[c] = def == nullptr ? kUnseenKey : id_key(def->id);
                    if (static_cast<int>(c / kDeckSize) == numbered_) {
                        keys_[c] = mix(keys_[c] ^ (static_cast<std::uint64_t>(c) + 1));
                    }
                }
                return keys_[c];
            }

            static std::uint64_t mix(std::uint64_t z) {
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                return z ^ (z >> 31);
            }

            const Observation& observation_;
            int numbered_;
            std::array<std::uint64_t, kMaxCards> keys_{};
            std::uint64_t hash_ = 0;
        };

    } // namespace

    std::uint64_t hash_observation(const Observation& o, int numbered) {
        Hasher h(o, numbered);
        h.add(o.viewer);
        h.add(o.current_player);
        h.add(o.first_player);
        h.add(o.turn);
        h.add(static_cast<std::uint64_t>(o.phase));
        h.add(o.flags.energy_attached);
        h.add(o.flags.supporter_played);
        h.add(o.flags.stadium_played);
        h.add(o.flags.stadium_used);
        h.add(o.flags.retreated);
        h.add(o.flags.attacks_used);
        h.add(o.flags.abilities_used.size());
        for (const AbilityUse& use : o.flags.abilities_used) {
            h.card(use.pokemon);
            h.card(use.card);
            h.add(use.index);
        }
        for (const TurnRecord& record : o.history.turns) {
            h.add(record.player);
            for (std::size_t p = 0; p < kPlayers; ++p) {
                h.add(record.knocked_out[p]);
                h.add(record.knocked_out_by_attack[p]);
            }
            h.card(record.attacker);
            h.attack(record.attack);
        }
        h.add(o.lasting.size());
        for (const LastingEffect& effect : o.lasting) {
            h.add(static_cast<std::uint64_t>(effect.kind));
            h.add(effect.player);
            h.card(effect.pokemon);
            h.attack(effect.attack);
            h.add(static_cast<std::uint64_t>(static_cast<std::uint16_t>(effect.amount)));
            h.add(effect.only_ex);
            h.add(effect.turn);
        }
        h.decision(o.decision);
        h.add(o.pending.size());
        for (const Decision& d : o.pending) {
            h.decision(d);
        }
        h.add(o.choice_owner);
        h.add(o.effects.size());
        for (const EffectRun& r : o.effects) {
            h.run(r);
        }
        h.add(static_cast<std::uint64_t>(o.result));
        for (const PlayerView& view : o.players) {
            h.player(view);
        }
        h.sorted_cards(o.deck_or_prizes);
        h.sorted_cards(o.opponent_seen);
        return h.value();
    }

} // namespace pokebot