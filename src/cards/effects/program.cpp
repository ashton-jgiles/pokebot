#include "cards/effects/program.hpp"

#include <algorithm>
#include <limits>

#include "cards/card/card.hpp"

namespace pokebot {

    namespace {

        using nlohmann::json;

        constexpr int kMaxCount = 60;
        constexpr std::size_t kMaxSteps = 255;
        constexpr std::int16_t kHealAll = std::numeric_limits<std::int16_t>::max();

        std::uint8_t count_of(const json& block, int fallback) {
            const int count = block.value("count", fallback);
            if (count < 0 || count > kMaxCount) {
                throw CardDataError("count must be 0 to 60, not " + std::to_string(count));
            }
            return static_cast<std::uint8_t>(count);
        }

        Zone destination(const json& block) {
            const std::string to = block.value("to", "hand");
            if (to == "hand") {
                return Zone::Hand;
            }
            if (to == "discard") {
                return Zone::Discard;
            }
            if (to == "deck") {
                return Zone::Deck;
            }
            if (to == "bench") {
                return Zone::Bench;
            }
            if (to == "self") {
                return Zone::AttachedEnergy;
            }
            throw CardDataError("unknown destination '" + to + "'");
        }

        Side side_of(const json& block) {
            const std::string player = block.value("player", "you");
            if (player == "you") {
                return Side::You;
            }
            if (player == "opponent") {
                return Side::Opponent;
            }
            if (player == "each") {
                return Side::Each;
            }
            throw CardDataError("unknown player '" + player + "'");
        }

        Zone where_on(const json& block) {
            const std::string on = block.value("on", "any");
            if (on == "any") {
                return Zone::InPlay;
            }
            if (on == "bench") {
                return Zone::Bench;
            }
            if (on == "active") {
                return Zone::Active;
            }
            throw CardDataError("\"on\" is \"any\", \"bench\" or \"active\", not '" + on + "'");
        }

        Side opponent_unless_said(const json& block) {
            return block.contains("player") ? side_of(block) : Side::Opponent;
        }

        LastingSpec lasting_of(const json& block) {
            LastingSpec spec;
            const std::string effect = block.at("effect").get<std::string>();
            if (effect == "cant_attack") {
                spec.kind = LastingKind::CantAttack;
            } else if (effect == "cant_use_attack") {
                spec.kind = LastingKind::CantUseAttack;
            } else if (effect == "cant_retreat") {
                spec.kind = LastingKind::CantRetreat;
            } else if (effect == "no_items") {
                spec.kind = LastingKind::NoItems;
            } else if (effect == "damage_bonus") {
                spec.kind = LastingKind::DamageBonus;
            } else if (effect == "takes_less") {
                spec.kind = LastingKind::TakesLess;
            } else if (effect == "prevent_damage") {
                spec.kind = LastingKind::PreventDamage;
            } else if (effect == "deals_less") {
                spec.kind = LastingKind::DealsLess;
            } else if (effect == "takes_more") {
                spec.kind = LastingKind::TakesMore;
            } else if (effect == "attack_flip") {
                spec.kind = LastingKind::AttackFlip;
            } else {
                throw CardDataError("unknown lasting effect '" + effect + "'");
            }

            const std::string until = block.at("until").get<std::string>();
            if (until == "this_turn") {
                spec.until = Until::ThisTurn;
            } else if (until == "opponents_next_turn") {
                spec.until = Until::OpponentsNextTurn;
            } else if (until == "your_next_turn") {
                spec.until = Until::YourNextTurn;
            } else {
                throw CardDataError("unknown \"until\" '" + until + "'");
            }

            const std::string on = block.value("on", "self");
            if (on == "self") {
                spec.on = LastingOn::Self;
            } else if (on == "defending") {
                spec.on = LastingOn::Defending;
            } else if (on == "player") {
                spec.on = LastingOn::Player;
            } else {
                throw CardDataError("lasting effects are \"on\" \"self\", \"defending\" or \"player\", not '" + on + "'");
            }

            const bool only_player = spec.kind == LastingKind::NoItems;
            const bool may_be_player = only_player || spec.kind == LastingKind::DamageBonus;
            const bool only_self = spec.kind == LastingKind::TakesLess || spec.kind == LastingKind::PreventDamage ||
                                   (spec.kind == LastingKind::DamageBonus && spec.on != LastingOn::Player);
            const bool only_defending = spec.kind == LastingKind::DealsLess || spec.kind == LastingKind::TakesMore || spec.kind == LastingKind::AttackFlip;
            if ((spec.on == LastingOn::Player ? !may_be_player : only_player) || (only_self && spec.on != LastingOn::Self) ||
                (only_defending && spec.on != LastingOn::Defending)) {
                throw CardDataError("'" + effect + "' doesn't go \"on\" \"" + on + "\"");
            }
            const bool needs_amount = spec.kind == LastingKind::TakesLess || spec.kind == LastingKind::DealsLess || spec.kind == LastingKind::TakesMore ||
                                      spec.kind == LastingKind::DamageBonus;
            if (needs_amount && block.value("amount", 0) <= 0) {
                throw CardDataError("'" + effect + "' needs a positive \"amount\"");
            }
            spec.only_ex = block.value("only_ex", false);
            spec.prevent_effects = block.value("prevent_effects", false);
            spec.from_basic = block.value("from_basic", false);
            if ((spec.prevent_effects || spec.from_basic) && spec.kind != LastingKind::PreventDamage) {
                throw CardDataError("\"prevent_effects\" and \"from_basic\" are for 'prevent_damage'");
            }
            return spec;
        }

        CustomEffect custom_of(const std::string& name) {
            if (name == "rare_candy") {
                return CustomEffect::RareCandy;
            }
            if (name == "transformation_tome") {
                return CustomEffect::TransformationTome;
            }
            throw CardDataError("unknown custom effect '" + name + "'");
        }

        StageFilter stage_of(const std::string& stage) {
            if (stage == "basic") {
                return StageFilter::Basic;
            }
            if (stage == "stage1") {
                return StageFilter::Stage1;
            }
            if (stage == "stage2") {
                return StageFilter::Stage2;
            }
            if (stage == "evolution") {
                return StageFilter::Evolution;
            }
            throw CardDataError("unknown stage '" + stage + "'");
        }

        TrainerKind trainer_of(const std::string& kind) {
            if (kind == "item") {
                return TrainerKind::Item;
            }
            if (kind == "supporter") {
                return TrainerKind::Supporter;
            }
            if (kind == "stadium") {
                return TrainerKind::Stadium;
            }
            if (kind == "tool") {
                return TrainerKind::Tool;
            }
            throw CardDataError("unknown Trainer kind '" + kind + "'");
        }

        SpecialCondition special_condition_of(const std::string& name) {
            if (name == "asleep") {
                return SpecialCondition::Asleep;
            }
            if (name == "burned") {
                return SpecialCondition::Burned;
            }
            if (name == "confused") {
                return SpecialCondition::Confused;
            }
            if (name == "paralyzed") {
                return SpecialCondition::Paralyzed;
            }
            if (name == "poisoned") {
                return SpecialCondition::Poisoned;
            }
            throw CardDataError("unknown Special Condition '" + name + "'");
        }

        CardFilter parse_filter_fields(const json& fields) {
            CardFilter filter;
            for (const auto& [key, value] : fields.items()) {
                if (key == "kind") {
                    filter.kind = card_kind_from_string(value.get<std::string>());
                } else if (key == "trainer") {
                    filter.trainer = trainer_of(value.get<std::string>());
                } else if (key == "type") {
                    filter.type = energy_type_from_string(value.get<std::string>());
                } else if (key == "stage") {
                    filter.stage = stage_of(value.get<std::string>());
                } else if (key == "basic") {
                    filter.basic = value.get<bool>();
                } else if (key == "rule_box") {
                    filter.rule_box = value.get<bool>();
                } else if (key == "max_hp") {
                    filter.max_hp = value.get<int>();
                } else if (key == "name") {
                    filter.name = value.get<std::string>();
                } else if (key == "name_prefix") {
                    filter.name_prefix = value.get<std::string>();
                } else if (key == "not_name") {
                    filter.not_name = value.get<std::string>();
                } else if (key == "not_type") {
                    filter.not_type = energy_type_from_string(value.get<std::string>());
                } else if (key == "tera") {
                    filter.tera = value.get<bool>();
                } else if (key == "any_of") {
                    for (const auto& option : value) {
                        filter.any_of.push_back(parse_filter_fields(option));
                    }
                } else {
                    throw CardDataError("unknown filter field '" + key + "'");
                }
            }
            return filter;
        }

        CardFilter parse_filter(const json& block) {
            return block.contains("filter") ? parse_filter_fields(block.at("filter")) : CardFilter{};
        }

        Count parse_count(const json& each) {
            Count count;
            const std::string of = each.at("of").get<std::string>();
            if (of == "heads" || of == "moved") {
                count.kind = CountKind::Value;
            } else if (of == "hand") {
                count.kind = CountKind::Hand;
            } else if (of == "discard") {
                count.kind = CountKind::Discard;
            } else if (of == "benched") {
                count.kind = CountKind::Benched;
            } else if (of == "prizes_taken") {
                count.kind = CountKind::PrizesTaken;
            } else if (of == "active_energy") {
                count.kind = CountKind::ActiveEnergy;
            } else if (of == "damage_counters") {
                count.kind = CountKind::DamageCountersOnSelf;
            } else if (of == "target_damage_counters") {
                count.kind = CountKind::TargetDamageCounters;
            } else if (of == "active_damage_counters") {
                count.kind = CountKind::ActiveDamageCounters;
            } else if (of == "in_play") {
                count.kind = CountKind::InPlay;
            } else if (of == "tools_in_play") {
                count.kind = CountKind::ToolsInPlay;
            } else if (of == "energy_in_play") {
                count.kind = CountKind::EnergyInPlay;
            } else if (of == "self_energy") {
                count.kind = CountKind::SelfEnergy;
            } else {
                throw CardDataError("unknown thing to count '" + of + "'");
            }
            count.side = side_of(each);
            count.filter = parse_filter(each);
            return count;
        }

        Condition parse_condition(const json& item) {
            if (!item.is_object() || item.size() != 1) {
                throw CardDataError("a condition is an object with one key");
            }
            const auto& [key, value] = *item.items().begin();
            Condition condition;
            if (key == "hand_besides") {
                condition.kind = ConditionKind::HandBesides;
                condition.value = value.get<int>();
            } else if (key == "prizes_exactly") {
                condition.kind = ConditionKind::PrizesExactly;
                condition.value = value.get<int>();
            } else if (key == "knocked_out_last_turn") {
                condition.kind = ConditionKind::KnockedOutLastTurn;
            } else if (key == "knocked_out_by_attack_last_turn") {
                condition.kind = ConditionKind::KnockedOutByAttackLastTurn;
            } else if (key == "hand_has") {
                condition.kind = ConditionKind::HandHas;
                condition.filter = parse_filter_fields(value);
            } else if (key == "bench_has") {
                condition.kind = ConditionKind::BenchHas;
                condition.filter = parse_filter_fields(value);
            } else if (key == "active_is") {
                condition.kind = ConditionKind::ActiveIs;
                condition.filter = parse_filter_fields(value);
            } else if (key == "self_has_energy") {
                condition.kind = ConditionKind::SelfHasEnergy;
                condition.filter = parse_filter_fields(value);
            } else if (key == "has_damaged") {
                condition.kind = ConditionKind::YouHaveDamaged;
            } else if (key == "self_is") {
                condition.kind = ConditionKind::SelfIs;
                condition.filter = parse_filter_fields(value);
            } else if (key == "stadium_in_play") {
                condition.kind = ConditionKind::StadiumInPlay;
            } else if (key == "more_prizes_than_opponent") {
                condition.kind = ConditionKind::MorePrizesThanOpponent;
            } else if (key == "in_play_has") {
                condition.kind = ConditionKind::InPlayHas;
                condition.filter = parse_filter_fields(value);
            } else if (key == "your_energy_has") {
                condition.kind = ConditionKind::YourEnergyHas;
                condition.filter = parse_filter_fields(value);
            } else if (key == "attached_to_active") {
                condition.kind = ConditionKind::AttachedToActive;
            } else if (key == "custom") {
                condition.kind = ConditionKind::Custom;
                condition.custom = custom_of(value.get<std::string>());
            } else if (key == "has_benched") {
                condition.kind = ConditionKind::HasBenched;
            } else if (key == "opponent_has_benched") {
                condition.kind = ConditionKind::OpponentHasBenched;
            } else if (key == "bench_not_full") {
                condition.kind = ConditionKind::BenchNotFull;
            } else if (key == "discard_has") {
                condition.kind = ConditionKind::DiscardHas;
                condition.filter = parse_filter_fields(value);
            } else if (key == "heads_at_least" || key == "moved_at_least" || key == "drew_at_least") {
                condition.kind = ConditionKind::ValueAtLeast;
                condition.value = value.get<int>();
            } else if (key == "opponent_prizes_at_most") {
                condition.kind = ConditionKind::OpponentPrizesAtMost;
                condition.value = value.get<int>();
            } else if (key == "shares_type") {
                condition.kind = ConditionKind::SharesType;
            } else if (key == "self_energy_at_least") {
                condition.kind = ConditionKind::SelfEnergyAtLeast;
                condition.value = value.get<int>();
            } else if (key == "opponent_active_is") {
                condition.kind = ConditionKind::OpponentActiveIs;
                condition.filter = parse_filter_fields(value);
            } else if (key == "opponent_active_damaged") {
                condition.kind = ConditionKind::OpponentActiveDamaged;
            } else if (key == "opponent_active_has") {
                condition.kind = ConditionKind::OpponentActiveHas;
                condition.special = special_condition_of(value.get<std::string>());
            } else if (key == "self_damaged") {
                condition.kind = ConditionKind::SelfDamaged;
            } else if (key == "self_has_tool") {
                condition.kind = ConditionKind::SelfHasTool;
            } else if (key == "bench_damaged") {
                condition.kind = ConditionKind::BenchDamaged;
            } else {
                throw CardDataError("unknown condition '" + key + "'");
            }
            return condition;
        }

        bool uses_op(const json& effects, const std::string& name) {
            for (const auto& block : effects) {
                if (!block.is_object()) {
                    continue;
                }
                if (block.value("op", "") == name) {
                    return true;
                }
                for (const char* list : {"effects", "then", "else"}) {
                    if (block.contains(list) && uses_op(block.at(list), name)) {
                        return true;
                    }
                }
            }
            return false;
        }

        class Compiler {
        public:
            explicit Compiler(bool attack) : attack_(attack) {}

            Program program;

            void add(Op op) {
                Step step;
                step.op = op;
                add(step);
            }

            void add(const Step& step) {
                if (program.size() >= kMaxSteps) {
                    throw CardDataError("an effect can't have more than 255 steps");
                }
                program.push_back(step);
            }

            void blocks(const json& effects) {
                if (!effects.is_array()) {
                    throw CardDataError("effects must be a list");
                }
                for (const auto& block : effects) {
                    one_block(block);
                }
            }

        private:
            bool attack_;

            std::uint8_t here() const { return static_cast<std::uint8_t>(program.size()); }

            void choose_and_move(Zone from, Zone to, std::uint8_t min, std::uint8_t max, const CardFilter& filter, bool reveal = false) {
                Step choose;
                choose.op = Op::ChooseCards;
                choose.from = from;
                choose.to = to;
                choose.min = min;
                choose.max = max;
                choose.filter = filter;
                add(choose);

                Step move;
                move.op = Op::MoveChosen;
                move.from = from;
                move.to = to;
                move.reveal = reveal;
                add(move);
            }

            // "reveal them": the opponent sees the cards found.
            static bool reveal_of(const json& block) {
                const json& reveal = block.value("reveal", json(false));
                if (!reveal.is_boolean()) {
                    throw CardDataError("\"reveal\" must be true or false");
                }
                return reveal.get<bool>();
            }

            void change_damage(Op op, const json& block) {
                if (!attack_) {
                    throw CardDataError("only attacks change their damage");
                }
                Step step;
                step.op = op;
                step.amount = static_cast<std::int16_t>(block.value("amount", 0));
                step.per = static_cast<std::int16_t>(block.value("per", 0));
                if (block.contains("for_each")) {
                    step.count = parse_count(block.at("for_each"));
                } else if (step.per != 0) {
                    throw CardDataError("\"per\" needs \"for_each\"");
                }
                add(step);
            }

            void choose_targets(const json& block, bool needs_energy, Side unless_said = Side::Opponent, bool needs_damage = false) {
                Step choose;
                choose.op = Op::ChooseTargets;
                choose.side = block.contains("player") ? side_of(block) : unless_said;
                if (choose.side == Side::Each) {
                    throw CardDataError("targets belong to \"you\" or \"opponent\"");
                }
                choose.from = where_on(block);
                choose.needs_energy = needs_energy;
                choose.needs_damage = needs_damage;
                if (block.value("each", false) || choose.from == Zone::Active) {
                    if (block.contains("targets") || block.contains("up_to")) {
                        throw CardDataError("\"each\" and \"on\": \"active\" take every matching Pokemon, with no \"targets\" or \"up_to\"");
                    }
                    choose.each = true;
                    add(choose);
                    return;
                }
                choose.max = static_cast<std::uint8_t>(block.value("targets", 1));
                choose.min = block.value("up_to", false) ? 0 : choose.max;
                if (choose.max < 1 || choose.max > 6) {
                    throw CardDataError("targets must be 1 to 6");
                }
                add(choose);
            }

            void attack_only(const std::string& op) {
                if (!attack_) {
                    throw CardDataError("only attacks can use \"" + op + "\"");
                }
            }

            static std::int16_t damage_amount(const json& block, const std::string& op, int most) {
                const int amount = block.at("amount").get<int>();
                if (amount <= 0 || amount % 10 != 0 || amount > most) {
                    throw CardDataError(op + "'s \"amount\" is a multiple of 10 from 10 to " + std::to_string(most) + ", not " + std::to_string(amount));
                }
                return static_cast<std::int16_t>(amount);
            }

            static std::int16_t heal_amount(const json& block, const std::string& op) {
                if (!block.value("all", false)) {
                    return damage_amount(block, op, 990);
                }
                if (block.contains("amount")) {
                    throw CardDataError(op + " heals \"all\" or an \"amount\", not both");
                }
                return kHealAll;
            }

            // Choose your Pokemon, then for each one, choose Energy from `from` and attach it.
            void attach_to(const json& block) {
                const std::string from = block.value("from", "discard");
                Zone zone = Zone::Discard;
                if (from == "deck") {
                    zone = Zone::Deck;
                } else if (from == "hand") {
                    zone = Zone::Hand;
                } else if (from != "discard") {
                    throw CardDataError("attach_to is from \"deck\", \"discard\" or \"hand\", not '" + from + "'");
                }
                const int targets = block.value("targets", 1);
                if (targets < 1 || targets > 6) {
                    throw CardDataError("targets must be 1 to 6");
                }

                Step choose;
                choose.op = Op::ChooseTargets;
                choose.side = Side::You;
                choose.from = where_on(block);
                choose.max = static_cast<std::uint8_t>(targets);
                choose.min = block.value("up_to_targets", false) ? 0 : choose.max;
                choose.filter = block.contains("target") ? parse_filter_fields(block.at("target")) : CardFilter{};
                add(choose);

                const std::size_t each = program.size();
                add(Op::EachTarget);
                const std::uint8_t count = count_of(block, 1);
                choose_and_move(zone, Zone::TargetEnergy, block.value("up_to", false) ? 0 : count, count, parse_filter(block));
                Step next;
                next.op = Op::NextTarget;
                next.jump = static_cast<std::uint8_t>(each);
                add(next);
                program[each].jump = here();
                if (zone == Zone::Deck && block.value("shuffle", true)) {
                    add(Op::ShuffleDeck);
                }
            }

            void switch_active(Side side, const CardFilter& filter = CardFilter{}, bool opponent_chooses = false) {
                Step choose;
                choose.op = Op::ChooseBenched;
                choose.side = side;
                choose.filter = filter;
                choose.opponent_chooses = opponent_chooses;
                add(choose);

                Step swap;
                swap.op = Op::SwitchWithChosen;
                swap.side = side;
                add(swap);
            }

            void one_block(const json& block) {
                if (!block.is_object() || !block.contains("op")) {
                    throw CardDataError("every effect needs an \"op\"");
                }
                const std::string op = block.at("op").get<std::string>();
                if (op == "damage") {
                    if (!attack_) {
                        throw CardDataError("only attacks do damage");
                    }
                    add(Op::Damage);
                } else if (op == "draw") {
                    Step draw;
                    draw.op = Op::Draw;
                    draw.max = count_of(block, 1);
                    draw.side = side_of(block);
                    if (block.contains("until")) {
                        const int until = block.at("until").get<int>();
                        if (until < 1 || until > kMaxCount || block.contains("count")) {
                            throw CardDataError("draw's \"until\" is a hand size from 1 to 60, with no \"count\"");
                        }
                        draw.op = Op::DrawUntil;
                        draw.max = static_cast<std::uint8_t>(until);
                    }
                    add(draw);
                } else if (op == "discard_hand") {
                    add(Op::DiscardHand);
                } else if (op == "shuffle_deck") {
                    add(Op::ShuffleDeck);
                } else if (op == "shuffle_hand_into_deck") {
                    Step shuffle;
                    shuffle.op = Op::ShuffleHandIntoDeck;
                    shuffle.side = side_of(block);
                    add(shuffle);
                } else if (op == "search_deck") {
                    choose_and_move(Zone::Deck, destination(block), 0, count_of(block, 1), parse_filter(block), reveal_of(block));
                    if (block.value("shuffle", true)) {
                        add(Op::ShuffleDeck);
                    }
                } else if (op == "discard_from_hand") {
                    const std::uint8_t count = count_of(block, 1);
                    choose_and_move(Zone::Hand, Zone::Discard, count, count, parse_filter(block));
                } else if (op == "from_discard") {
                    const std::uint8_t count = count_of(block, 1);
                    const std::uint8_t min = block.value("up_to", false) ? 0 : count;
                    choose_and_move(Zone::Discard, destination(block), min, count, parse_filter(block));
                } else if (op == "move_attached_energy") {
                    const std::uint8_t count = count_of(block, 1);
                    const std::uint8_t min = block.value("up_to", false) ? 0 : count;
                    const std::string from = block.value("from", "self");
                    if (from != "self" && from != "your_pokemon") {
                        throw CardDataError("move_attached_energy is from \"self\" or \"your_pokemon\"");
                    }
                    const Zone zone = from == "self" ? Zone::AttachedEnergy : Zone::YourAttachedEnergy;
                    choose_and_move(zone, destination(block), min, count, parse_filter(block));
                } else if (op == "switch_self") {
                    switch_active(Side::You);
                } else if (op == "switch_active") {
                    const Side side = side_of(block);
                    if (side == Side::Each) {
                        throw CardDataError("switch_active is for \"you\" or \"opponent\"");
                    }
                    const std::string chooser = block.value("chooser", "you");
                    if ((chooser != "you" && chooser != "opponent") || (chooser == "opponent" && side != Side::Opponent)) {
                        throw CardDataError("switch_active's \"chooser\" is \"you\", or \"opponent\" when it's the opponent's Pokemon");
                    }
                    switch_active(side, parse_filter(block), chooser == "opponent");
                } else if (op == "discard_tools") {
                    Step discard;
                    discard.op = Op::DiscardTools;
                    discard.side = opponent_unless_said(block);
                    add(discard);
                } else if (op == "discard_random" || op == "discard_deck_top") {
                    Step discard;
                    discard.op = op == "discard_random" ? Op::DiscardRandom : Op::DiscardDeckTop;
                    discard.side = op == "discard_random" ? opponent_unless_said(block) : side_of(block);
                    discard.max = count_of(block, 1);
                    if (block.contains("for_each")) {
                        if (block.contains("count")) {
                            throw CardDataError(op + " takes \"count\" or \"for_each\", not both");
                        }
                        discard.count = parse_count(block.at("for_each"));
                    }
                    add(discard);
                } else if (op == "special_condition") {
                    Step apply;
                    apply.op = Op::ApplySpecialCondition;
                    apply.special = special_condition_of(block.at("condition").get<std::string>());
                    apply.side = block.contains("player") ? side_of(block) : Side::Opponent;
                    apply.max = count_of(block, 1);
                    add(apply);
                } else if (op == "set_damage") {
                    change_damage(Op::SetDamage, block);
                } else if (op == "add_damage") {
                    change_damage(Op::AddDamage, block);
                } else if (op == "flip_coins") {
                    Step flip;
                    flip.op = Op::FlipCoins;
                    flip.max = count_of(block, 1);
                    add(flip);
                } else if (op == "flip_until_tails") {
                    add(Op::FlipUntilTails);
                } else if (op == "hand_to_deck_bottom") {
                    Step bottom;
                    bottom.op = Op::HandToDeckBottom;
                    bottom.side = side_of(block);
                    add(bottom);
                } else if (op == "search_to_top") {
                    Step choose;
                    choose.op = Op::ChooseCards;
                    choose.from = Zone::Deck;
                    choose.to = Zone::DeckTop;
                    choose.max = count_of(block, 1);
                    choose.filter = parse_filter(block);
                    add(choose);
                    add(Op::ShuffleDeck);
                    Step move;
                    move.op = Op::MoveChosen;
                    move.from = Zone::Deck;
                    move.to = Zone::DeckTop;
                    move.reveal = reveal_of(block);
                    add(move);
                } else if (op == "damage_to") {
                    attack_only(op);
                    choose_targets(block, false);
                    Step hit;
                    hit.op = Op::DamageTargets;
                    hit.amount = static_cast<std::int16_t>(block.value("amount", 0));
                    hit.per = static_cast<std::int16_t>(block.value("per", 0));
                    if (block.contains("for_each")) {
                        hit.count = parse_count(block.at("for_each"));
                    } else if (hit.per != 0) {
                        throw CardDataError("\"per\" needs \"for_each\"");
                    }
                    add(hit);
                } else if (op == "place_counters") {
                    Step place;
                    place.op = Op::PlaceCounters;
                    place.side = opponent_unless_said(block);
                    place.from = where_on(block);
                    place.max = count_of(block, 1);
                    if (place.side == Side::Each) {
                        throw CardDataError("damage counters go on \"you\" or \"opponent\"");
                    }
                    add(place);
                } else if (op == "discard_all_attached_energy") {
                    add(Op::DiscardAllEnergy);
                } else if (op == "return_self_to_hand") {
                    add(Op::ReturnSelfToHand);
                } else if (op == "discard_target_energy") {
                    choose_targets(block, true);
                    const std::uint8_t count = count_of(block, 1);
                    choose_and_move(Zone::TargetEnergy, Zone::Discard, count, count, parse_filter(block));
                } else if (op == "use_attack") {
                    attack_only(op);
                    if (block.value("from", "your_bench") != "your_bench") {
                        throw CardDataError("use_attack is from \"your_bench\"");
                    }
                    Step choose;
                    choose.op = Op::ChooseAttack;
                    choose.filter = parse_filter(block);
                    add(choose);
                    add(Op::UseChosenAttack);
                } else if (op == "custom") {
                    Step custom;
                    custom.op = Op::Custom;
                    custom.custom = custom_of(block.at("name").get<std::string>());
                    add(custom);
                } else if (op == "attach_to") {
                    attach_to(block);
                } else if (op == "move_energy") {
                    Step source;
                    source.op = Op::ChooseTargets;
                    source.side = Side::You;
                    source.from = Zone::InPlay;
                    source.min = 1;
                    source.max = 1;
                    source.needs_energy = true;
                    add(source);
                    choose_and_move(Zone::TargetEnergy, Zone::ChosenEnergy, 1, 1, parse_filter(block));
                    // choose the destination between choosing the Energy and moving it
                    Step destination;
                    destination.op = Op::ChooseDestination;
                    program.insert(program.end() - 1, destination);
                } else if (op == "counters_on_self") {
                    Step put;
                    put.op = Op::CountersOnSelf;
                    put.max = count_of(block, 1);
                    add(put);
                } else if (op == "discard_stadium") {
                    add(Op::DiscardStadium);
                } else if (op == "switch_to_active") {
                    add(Op::SwitchToActive);
                } else if (op == "counters_to") {
                    choose_targets(block, false);
                    Step put;
                    put.op = Op::CountersOnTargets;
                    put.max = count_of(block, 1);
                    add(put);
                } else if (op == "shuffle_self_into_deck") {
                    add(Op::ShuffleSelfIntoDeck);
                } else if (op == "look_at_top") {
                    const std::uint8_t look = count_of(block, 1);
                    const std::uint8_t take = static_cast<std::uint8_t>(block.value("take", 1));
                    Step choose;
                    choose.op = Op::ChooseCards;
                    choose.from = Zone::DeckTop;
                    choose.to = destination(block);
                    choose.amount = look;
                    choose.min = take;
                    choose.max = take;
                    choose.filter = parse_filter(block);
                    add(choose);
                    Step move;
                    move.op = Op::MoveChosen;
                    move.from = Zone::DeckTop;
                    move.to = choose.to;
                    add(move);
                    Step rest;
                    rest.op = Op::RestToBottom;
                    rest.amount = look;
                    add(rest);
                } else if (op == "attach_from_hand") {
                    const std::uint8_t count = count_of(block, 1);
                    choose_and_move(Zone::Hand, Zone::AttachedEnergy, count, count, parse_filter(block));
                } else if (op == "move_counters") {
                    add(Op::ChooseCounterSource);
                    Step choose;
                    choose.op = Op::ChooseTargets;
                    choose.side = opponent_unless_said(block);
                    choose.from = where_on(block);
                    choose.min = 1;
                    choose.max = 1;
                    add(choose);
                    Step move;
                    move.op = Op::MoveCounters;
                    move.max = count_of(block, 1);
                    add(move);
                } else if (op == "ignore_defender_effects") {
                    attack_only(op);
                    add(Op::IgnoreDefenderEffects);
                } else if (op == "ignore_weakness") {
                    attack_only(op);
                    add(Op::IgnoreWeakness);
                } else if (op == "ignore_resistance") {
                    attack_only(op);
                    add(Op::IgnoreResistance);
                } else if (op == "damage_self") {
                    attack_only(op);
                    Step put;
                    put.op = Op::CountersOnSelf;
                    put.max = static_cast<std::uint8_t>(damage_amount(block, op, 600) / 10);
                    add(put);
                } else if (op == "heal_self") {
                    Step heal;
                    heal.op = Op::HealSelf;
                    heal.amount = heal_amount(block, op);
                    add(heal);
                } else if (op == "heal_to") {
                    choose_targets(block, false, Side::You, true);
                    Step heal;
                    heal.op = Op::HealTargets;
                    heal.amount = heal_amount(block, op);
                    add(heal);
                } else if (op == "lasting") {
                    Step step;
                    step.op = Op::AddLasting;
                    step.lasting = lasting_of(block);
                    step.side = side_of(block);
                    step.amount = static_cast<std::int16_t>(block.value("amount", 0));
                    if (step.lasting.on != LastingOn::Player) {
                        attack_only(op);
                    }
                    if (step.side == Side::Each) {
                        throw CardDataError("a lasting effect is on \"you\" or \"opponent\"");
                    }
                    add(step);
                } else if (op == "may") {
                    const std::size_t ask = program.size();
                    add(Op::AskYesNo);
                    blocks(block.value("effects", json::array()));
                    program[ask].jump = here();
                } else if (op == "if") {
                    const std::size_t test = program.size();
                    Step jump_unless;
                    jump_unless.op = Op::JumpUnless;
                    jump_unless.condition = parse_condition(block.at("condition"));
                    add(jump_unless);
                    blocks(block.value("then", json::array()));
                    if (block.contains("else")) {
                        const std::size_t skip_else = program.size();
                        add(Op::Jump);
                        program[test].jump = here();
                        blocks(block.at("else"));
                        program[skip_else].jump = here();
                    } else {
                        program[test].jump = here();
                    }
                } else {
                    throw CardDataError("unknown effect op '" + op + "'");
                }
            }
        };

        Program compile(const nlohmann::json& effects, bool attack) {
            if (!effects.is_array()) {
                throw CardDataError("effects must be a list");
            }
            try {
                Compiler compiler(attack);
                const bool placed = uses_op(effects, "damage");
                if (attack && !placed &&
                    (uses_op(effects, "set_damage") || uses_op(effects, "add_damage") || uses_op(effects, "ignore_defender_effects") ||
                     uses_op(effects, "ignore_weakness") || uses_op(effects, "ignore_resistance"))) {
                    throw CardDataError("an attack that changes its damage must place a \"damage\" step");
                }
                if (attack && !placed) {
                    compiler.add(Op::Damage);
                }
                compiler.blocks(effects);
                return compiler.program;
            } catch (const nlohmann::json::exception& e) {
                throw CardDataError(std::string("badly formed effect: ") + e.what());
            }
        }

        bool stage_matches(StageFilter stage, const CardDef& card) {
            switch (stage) {
                case StageFilter::Any:
                    return true;
                case StageFilter::Basic:
                    return card.stage == Stage::Basic;
                case StageFilter::Stage1:
                    return card.stage == Stage::Stage1;
                case StageFilter::Stage2:
                    return card.stage == Stage::Stage2;
                case StageFilter::Evolution:
                    return card.stage == Stage::Stage1 || card.stage == Stage::Stage2;
            }
            return false;
        }

    } // namespace

    bool matches(const CardFilter& filter, const CardDef& card) {
        if (filter.kind && card.kind != *filter.kind) {
            return false;
        }
        if (filter.trainer && card.trainer_kind != *filter.trainer) {
            return false;
        }
        if (!filter.name.empty() && card.name != filter.name) {
            return false;
        }
        if (!filter.name_prefix.empty() && card.name.rfind(filter.name_prefix, 0) != 0) {
            return false;
        }
        if (!filter.not_name.empty() && card.name == filter.not_name) {
            return false;
        }
        if (filter.not_type) {
            const auto& types = card.kind == CardKind::Energy ? card.provides : card.types;
            if (std::find(types.begin(), types.end(), *filter.not_type) != types.end()) {
                return false;
            }
        }
        if (!stage_matches(filter.stage, card)) {
            return false;
        }
        if (filter.basic) {
            const bool basic = card.kind == CardKind::Pokemon ? card.stage == Stage::Basic : card.energy_kind == EnergyKind::Basic;
            if (!basic) {
                return false;
            }
        }
        if (filter.rule_box && card.rules.ex != *filter.rule_box) {
            return false;
        }
        if (filter.tera && card.rules.tera != *filter.tera) {
            return false;
        }
        if (filter.max_hp > 0 && (card.kind != CardKind::Pokemon || card.hp > filter.max_hp)) {
            return false;
        }
        if (filter.type) {
            const auto& types = card.kind == CardKind::Energy ? card.provides : card.types;
            if (std::find(types.begin(), types.end(), *filter.type) == types.end()) {
                return false;
            }
        }
        if (!filter.any_of.empty()) {
            return std::any_of(filter.any_of.begin(), filter.any_of.end(), [&card](const CardFilter& option) {
                return matches(option, card);
            });
        }
        return true;
    }

    Program compile_attack(const nlohmann::json& effects) {
        return compile(effects, true);
    }

    Program compile_trainer(const nlohmann::json& effects) {
        return compile(effects, false);
    }

    std::vector<Condition> compile_conditions(const nlohmann::json& requirements) {
        if (!requirements.is_array()) {
            throw CardDataError("requires must be a list");
        }
        try {
            std::vector<Condition> conditions;
            for (const auto& item : requirements) {
                conditions.push_back(parse_condition(item));
            }
            return conditions;
        } catch (const nlohmann::json::exception& e) {
            throw CardDataError(std::string("badly formed condition: ") + e.what());
        }
    }

    std::vector<ConditionalProvides> compile_provides_when(const nlohmann::json& provides_when) {
        if (!provides_when.is_array()) {
            throw CardDataError("provides_when must be a list");
        }
        try {
            std::vector<ConditionalProvides> result;
            for (const auto& item : provides_when) {
                ConditionalProvides entry;
                entry.holder = item.contains("holder") ? parse_filter_fields(item.at("holder")) : CardFilter{};
                for (const auto& unit : item.at("provides")) {
                    const std::string name = unit.get<std::string>();
                    entry.provides.push_back(name == "any" ? kAnyEnergy : energy_mask(energy_type_from_string(name)));
                }
                result.push_back(std::move(entry));
            }
            return result;
        } catch (const nlohmann::json::exception& e) {
            throw CardDataError(std::string("badly formed provides_when: ") + e.what());
        }
    }

    std::vector<Modifier> compile_passive(const nlohmann::json& passive) {
        if (!passive.is_array()) {
            throw CardDataError("passive must be a list");
        }
        try {
            std::vector<Modifier> modifiers;
            for (const auto& item : passive) {
                Modifier m;
                const std::string what = item.at("modify").get<std::string>();
                if (what == "max_hp") {
                    m.what = Modifies::MaxHp;
                } else if (what == "retreat_cost") {
                    m.what = Modifies::RetreatCost;
                } else if (what == "weakness_type") {
                    m.what = Modifies::WeaknessType;
                } else if (what == "damage_dealt") {
                    m.what = Modifies::DamageDealt;
                } else if (what == "bench_size") {
                    m.what = Modifies::BenchSize;
                } else if (what == "damage_taken") {
                    m.what = Modifies::DamageTaken;
                } else if (what == "prevent_damage") {
                    m.what = Modifies::PreventDamage;
                } else if (what == "prevent_effects") {
                    m.what = Modifies::PreventEffects;
                } else {
                    throw CardDataError("unknown thing to modify '" + what + "'");
                }

                const std::string to = item.value("applies_to", "holder");
                if (to == "holder") {
                    m.to = AppliesTo::Holder;
                } else if (to == "yours") {
                    m.to = AppliesTo::Yours;
                } else if (to == "opponents") {
                    m.to = AppliesTo::Opponents;
                } else if (to == "all") {
                    m.to = AppliesTo::All;
                } else {
                    throw CardDataError("unknown \"applies_to\" '" + to + "'");
                }

                m.add = item.value("add", 0);
                if (item.contains("set")) {
                    m.set = item.at("set").get<int>();
                }
                if (item.contains("type")) {
                    m.type = energy_type_from_string(item.at("type").get<std::string>());
                }
                m.filter = parse_filter(item);
                m.only_if_poisoned = item.value("if_poisoned", false);
                m.only_with_tera = item.value("if_tera_in_play", false);
                m.from = item.contains("from") ? parse_filter_fields(item.at("from")) : CardFilter{};
                m.only_if_active = item.value("if_active", false);
                m.only_if_benched = item.value("if_benched", false);
                if (m.what == Modifies::BenchSize && !m.set) {
                    throw CardDataError("'bench_size' needs \"set\"");
                }
                const bool prevents = m.what == Modifies::PreventDamage || m.what == Modifies::PreventEffects;
                if (!prevents && (m.what == Modifies::WeaknessType ? !m.type : (m.add == 0 && !m.set))) {
                    throw CardDataError("'" + what + "' needs " + (m.what == Modifies::WeaknessType ? "a \"type\"" : "\"add\" or \"set\""));
                }
                modifiers.push_back(m);
            }
            return modifiers;
        } catch (const nlohmann::json::exception& e) {
            throw CardDataError(std::string("badly formed passive effect: ") + e.what());
        }
    }

    Trigger trigger_from_string(const std::string& name) {
        if (name == "none") {
            return Trigger::None;
        }
        if (name == "played_to_bench") {
            return Trigger::PlayedToBench;
        }
        if (name == "put_on_bench") {
            return Trigger::PutOnBench;
        }
        if (name == "evolved") {
            return Trigger::Evolved;
        }
        if (name == "damaged_by_attack") {
            return Trigger::DamagedByAttack;
        }
        throw CardDataError("unknown trigger '" + name + "'");
    }

    const char* to_string(Trigger trigger) {
        switch (trigger) {
            case Trigger::None:
                return "none";
            case Trigger::PlayedToBench:
                return "played_to_bench";
            case Trigger::PutOnBench:
                return "put_on_bench";
            case Trigger::Evolved:
                return "evolved";
            case Trigger::DamagedByAttack:
                return "damaged_by_attack";
        }
        return "none";
    }

    const char* to_string(Op op) {
        switch (op) {
            case Op::Damage:
                return "damage";
            case Op::Draw:
                return "draw";
            case Op::DiscardHand:
                return "discard hand";
            case Op::ShuffleDeck:
                return "shuffle deck";
            case Op::ShuffleHandIntoDeck:
                return "shuffle hand into deck";
            case Op::ChooseCards:
                return "choose cards";
            case Op::MoveChosen:
                return "move chosen";
            case Op::ChooseBenched:
                return "choose Benched";
            case Op::SwitchWithChosen:
                return "switch with chosen";
            case Op::AskYesNo:
                return "ask yes/no";
            case Op::JumpUnless:
                return "jump unless";
            case Op::Jump:
                return "jump";
            case Op::ApplySpecialCondition:
                return "apply Special Condition";
            case Op::SetDamage:
                return "set damage";
            case Op::AddDamage:
                return "add damage";
            case Op::FlipCoins:
                return "flip coins";
            case Op::FlipUntilTails:
                return "flip until tails";
            case Op::HandToDeckBottom:
                return "hand to bottom of deck";
            case Op::ChooseTargets:
                return "choose targets";
            case Op::DamageTargets:
                return "damage targets";
            case Op::PlaceCounters:
                return "place damage counters";
            case Op::DiscardAllEnergy:
                return "discard all Energy";
            case Op::ReturnSelfToHand:
                return "return to hand";
            case Op::ChooseAttack:
                return "choose an attack";
            case Op::UseChosenAttack:
                return "use the chosen attack";
            case Op::AddLasting:
                return "add a lasting effect";
            case Op::IgnoreDefenderEffects:
                return "ignore effects on the defender";
            case Op::CountersOnTargets:
                return "damage counters on targets";
            case Op::ShuffleSelfIntoDeck:
                return "shuffle into deck";
            case Op::RestToBottom:
                return "rest to the bottom of the deck";
            case Op::ChooseCounterSource:
                return "choose where damage counters come from";
            case Op::MoveCounters:
                return "move damage counters";
            case Op::CountersOnSelf:
                return "damage counters on this Pokemon";
            case Op::DiscardStadium:
                return "discard the Stadium";
            case Op::SwitchToActive:
                return "switch into the Active Spot";
            case Op::EachTarget:
                return "for each target";
            case Op::NextTarget:
                return "next target";
            case Op::ChooseDestination:
                return "choose where to move it";
            case Op::Custom:
                return "custom";
            case Op::HealSelf:
                return "heal this Pokemon";
            case Op::HealTargets:
                return "heal targets";
            case Op::IgnoreWeakness:
                return "ignore Weakness";
            case Op::IgnoreResistance:
                return "ignore Resistance";
            case Op::DrawUntil:
                return "draw until";
            case Op::DiscardTools:
                return "discard Pokemon Tools";
            case Op::DiscardRandom:
                return "discard random cards from the hand";
            case Op::DiscardDeckTop:
                return "discard from the top of the deck";
        }
        return "unknown op";
    }

} // namespace pokebot