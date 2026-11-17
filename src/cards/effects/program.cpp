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
            throw CardDataError("unknown destination '" + to + "'");
        }

        CardFilter parse_filter(const json& block) {
            CardFilter filter;
            if (!block.contains("filter")) {
                return filter;
            }
            for (const auto& [key, value] : block.at("filter").items()) {
                if (key == "kind") {
                    filter.kind = card_kind_from_string(value.get<std::string>());
                } else if (key == "type") {
                    filter.type = energy_type_from_string(value.get<std::string>());
                } else if (key == "basic") {
                    filter.basic = value.get<bool>();
                } else if (key == "name") {
                    filter.name = value.get<std::string>();
                } else {
                    throw CardDataError("unknown filter field '" + key + "'");
                }
            }
            return filter;
        }

        bool places_damage(const json& effects) {
            for (const auto& block : effects) {
                if (!block.is_object()) {
                    continue;
                }
                if (block.value("op", "") == "damage") {
                    return true;
                }
                if (block.contains("effects") && places_damage(block.at("effects"))) {
                    return true;
                }
            }
            return false;
        }

        class Compiler {
        public:
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
            void choose_and_move(Zone from, Zone to, std::uint8_t min, std::uint8_t max, const CardFilter& filter) {
                Step choose;
                choose.op = Op::ChooseCards;
                choose.from = from;
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

            void one_block(const json& block) {
                if (!block.is_object() || !block.contains("op")) {
                    throw CardDataError("every effect needs an \"op\"");
                }
                const std::string op = block.at("op").get<std::string>();
                if (op == "damage") {
                    add(Op::Damage);
                } else if (op == "draw") {
                    Step draw;
                    draw.op = Op::Draw;
                    draw.max = count_of(block, 1);
                    add(draw);
                } else if (op == "discard_hand") {
                    add(Op::DiscardHand);
                } else if (op == "shuffle_deck") {
                    add(Op::ShuffleDeck);
                } else if (op == "search_deck") {
                    choose_and_move(Zone::Deck, destination(block), 0, count_of(block, 1), parse_filter(block));
                    add(Op::ShuffleDeck);
                } else if (op == "move_attached_energy") {
                    const std::uint8_t count = count_of(block, 1);
                    choose_and_move(Zone::AttachedEnergy, destination(block), count, count, parse_filter(block));
                } else if (op == "switch_self") {
                    add(Op::ChooseBenched);
                    add(Op::SwitchWithChosen);
                } else if (op == "may") {
                    const std::size_t ask = program.size();
                    add(Op::AskYesNo);
                    blocks(block.value("effects", json::array()));
                    program[ask].jump = static_cast<std::uint8_t>(program.size());
                } else {
                    throw CardDataError("unknown effect op '" + op + "'");
                }
            }
        };

    } // namespace

    bool matches(const CardFilter& filter, const CardDef& card) {
        if (filter.kind && card.kind != *filter.kind) {
            return false;
        }
        if (!filter.name.empty() && card.name != filter.name) {
            return false;
        }
        if (filter.basic) {
            const bool basic = card.kind == CardKind::Pokemon ? card.stage == Stage::Basic : card.energy_kind == EnergyKind::Basic;
            if (!basic) {
                return false;
            }
        }
        if (filter.type) {
            const auto& types = card.kind == CardKind::Energy ? card.provides : card.types;
            if (std::find(types.begin(), types.end(), *filter.type) == types.end()) {
                return false;
            }
        }
        return true;
    }

    Program compile_attack(const nlohmann::json& effects) {
        if (!effects.is_array()) {
            throw CardDataError("effects must be a list");
        }
        try {
            Compiler compiler;
            if (!places_damage(effects)) {
                compiler.add(Op::Damage);
            }
            compiler.blocks(effects);
            return compiler.program;
        } catch (const nlohmann::json::exception& e) {
            throw CardDataError(std::string("badly formed effect: ") + e.what());
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
        }
        return "unknown op";
    }

} // namespace pokebot