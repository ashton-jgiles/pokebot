#include "cards/effects/program.hpp"

#include <algorithm>

#include "cards/card/card.hpp"

namespace pokebot {

    namespace {

        using nlohmann::json;

        constexpr int kMaxCount = 60;
        constexpr std::size_t kMaxSteps = 255;

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
            throw CardDataError("\"on\" is \"any\" or \"bench\", not '" + on + "'");
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

            const bool on_player = spec.kind == LastingKind::NoItems || spec.kind == LastingKind::DamageBonus;
            if (on_player != (spec.on == LastingOn::Player)) {
                throw CardDataError("'" + effect + "' doesn't go \"on\" \"" + on + "\"");
            }
            spec.only_ex = block.value("only_ex", false);
            return spec;
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
            } else if (key == "has_benched") {
                condition.kind = ConditionKind::HasBenched;
            } else if (key == "opponent_has_benched") {
                condition.kind = ConditionKind::OpponentHasBenched;
            } else if (key == "bench_not_full") {
                condition.kind = ConditionKind::BenchNotFull;
            } else if (key == "discard_has") {
                condition.kind = ConditionKind::DiscardHas;
                condition.filter = parse_filter_fields(value);
            } else if (key == "heads_at_least" || key == "moved_at_least") {
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

            void choose_and_move(Zone from, Zone to, std::uint8_t min, std::uint8_t max, const CardFilter& filter) {
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
                add(move);
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

            void choose_targets(const json& block, bool needs_energy) {
                Step choose;
                choose.op = Op::ChooseTargets;
                choose.side = opponent_unless_said(block);
                if (choose.side == Side::Each) {
                    throw CardDataError("targets belong to \"you\" or \"opponent\"");
                }
                choose.from = where_on(block);
                choose.max = static_cast<std::uint8_t>(block.value("targets", 1));
                choose.min = block.value("up_to", false) ? 0 : choose.max;
                choose.needs_energy = needs_energy;
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

            void switch_active(Side side) {
                Step choose;
                choose.op = Op::ChooseBenched;
                choose.side = side;
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
                    choose_and_move(Zone::Deck, destination(block), 0, count_of(block, 1), parse_filter(block));
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
                    switch_active(side);
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
                if (attack && !placed && (uses_op(effects, "set_damage") || uses_op(effects, "add_damage"))) {
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
        }
        return "unknown op";
    }

} // namespace pokebot