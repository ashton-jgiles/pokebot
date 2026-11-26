#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        bool ability_allowed(const GameState& game, int player, const PokemonInPlay& pokemon, std::size_t index) {
            const Ability& ability = game.def(pokemon.top()).abilities[index];
            if (!rules::has_abilities(game, pokemon)) {
                return false;
            }
            for (const AbilityUse& use : game.flags.abilities_used) {
                const bool same = use.pokemon == pokemon.id() && use.card == pokemon.top() && use.index == index;
                if ((ability.once_per_turn && same) || (ability.one_per_name && game.def(use.card).abilities[use.index].name == ability.name)) {
                    return false;
                }
            }
            if (ability.conditions.empty()) {
                return true;
            }
            EffectRun context;
            context.who = EffectContext{static_cast<std::uint8_t>(player), pokemon.id(), pokemon.top()};
            for (const Condition& condition : ability.conditions) {
                if (!condition_holds(game, player, condition, kNoCard, &context)) {
                    return false;
                }
            }
            return true;
        }

        namespace {

            // "When you play this Pokemon from your hand onto your Bench": its own Abilities with that trigger.
            void played_to_bench(GameState& game, const Event& event, Trigger trigger = Trigger::PlayedToBench) {
                const PokemonInPlay* pokemon = find_pokemon(game, event.pokemon);
                if (pokemon == nullptr) {
                    return;
                }
                const CardIndex top = pokemon->top();
                const CardIndex id = pokemon->id();
                const auto& abilities = game.def(top).abilities;
                for (std::size_t i = 0; i < abilities.size(); ++i) {
                    if (abilities[i].trigger != trigger || abilities[i].program.empty() ||
                        !ability_allowed(game, event.player, *pokemon, i)) {
                        continue;
                    }
                    if (abilities[i].once_per_turn || abilities[i].one_per_name) {
                        game.flags.abilities_used.push_back(AbilityUse{id, top, static_cast<std::uint8_t>(i)});
                    }
                    queue_effect(game, EffectContext{event.player, id, top}, ProgramRef{top, ProgramKind::Ability, static_cast<std::uint8_t>(i)},
                                 AfterEffect::FinishAbility);
                }
            }

            void damaged_by_attack(GameState& game, const Event& event) {
                const PokemonInPlay* pokemon = find_pokemon(game, event.pokemon);
                const PlayerState& owner = game.player(event.player);
                if (pokemon == nullptr || !owner.active || owner.active->id() != pokemon->id() || event.card == kNoCard ||
                    owner_of(event.card) == event.player || !rules::has_abilities(game, *pokemon)) {
                    return;
                }
                const CardIndex top = pokemon->top();
                const auto& abilities = game.def(top).abilities;
                for (std::size_t i = 0; i < abilities.size(); ++i) {
                    if (abilities[i].trigger == Trigger::DamagedByAttack && !abilities[i].program.empty()) {
                        queue_effect(game, EffectContext{event.player, pokemon->id(), top}, ProgramRef{top, ProgramKind::Ability, static_cast<std::uint8_t>(i)},
                                     AfterEffect::Nothing);
                    }
                }
            }

            void attached_from_hand(GameState& game, const Event& event) {
                const PokemonInPlay* pokemon = find_pokemon(game, event.pokemon);
                if (pokemon == nullptr || event.card == kNoCard) {
                    return;
                }
                const CardDef& energy = game.def(event.card);
                if (energy.trigger != Trigger::AttachedFromHand || energy.program.empty()) {
                    return;
                }
                EffectRun context;
                context.who = EffectContext{event.player, pokemon->id(), event.card};
                for (const Condition& condition : energy.conditions) {
                    if (!condition_holds(game, event.player, condition, kNoCard, &context)) {
                        return;
                    }
                }
                queue_effect(game, EffectContext{event.player, pokemon->id(), event.card}, ProgramRef{event.card, ProgramKind::Trainer, 0}, AfterEffect::FinishAbility);
            }

            // "Whenever any player puts a Pokemon onto their Bench during their turn": the Stadium in play.
            void put_on_bench(GameState& game, const Event& event) {
                const CardIndex stadium = rules::stadium_in_play(game);
                if (stadium == kNoCard || game.def(stadium).trigger != Trigger::PutOnBench || event.player != game.current_player) {
                    return;
                }
                queue_effect(game, EffectContext{event.player, event.pokemon, stadium}, ProgramRef{stadium, ProgramKind::Trainer, 0},
                             AfterEffect::FinishAbility);
            }

        } // namespace

        void run_triggers(GameState& game, const Event& event) {
            switch (event.kind) {
                case EventKind::PlayedToBench:
                    played_to_bench(game, event);
                    break;
                case EventKind::PutOnBench:
                    put_on_bench(game, event);
                    break;
                case EventKind::Evolved:
                    played_to_bench(game, event, Trigger::Evolved);
                    break;
                case EventKind::DamagedByAttack:
                    damaged_by_attack(game, event);
                    break;
                case EventKind::EnergyAttached:
                    attached_from_hand(game, event);
                    break;
                default:
                    break;
            }
        }

    } // namespace detail

} // namespace pokebot
