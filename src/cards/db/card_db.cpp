#include "cards/db/card_db.hpp"

#include <fstream>
#include <utility>

namespace pokebot {

    namespace {
        using nlohmann::json;

        json read_json(const std::filesystem::path& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                throw CardDataError("cannot open " + path.string());
            }
            try {
                return json::parse(in);
            } catch (const json::parse_error& e) {
                throw CardDataError(path.string() + ": " + e.what());
            }
        }

        void merge_named(json& list, const json& overrides, const std::string& what) {
            for (const auto& [name, patch] : overrides.items()) {
                bool found = false;
                for (auto& entry : list) {
                    if (entry.at("name") == name) {
                        entry.update(patch);
                        found = true;
                    }
                }
                if (!found) {
                    throw CardDataError("effects file names unknown " + what + " '" + name + "'");
                }
            }
        }
    } // namespace

    std::string card_id(const DeckEntry& entry) {
        return entry.set_code + "-" + std::to_string(entry.number);
    }

    json merge_effects(json generated, const json& overrides) {
        for (const auto& [key, value] : overrides.items()) {
            if (key == "attacks") {
                merge_named(generated["attacks"], value, "attack");
            } else if (key == "abilities") {
                merge_named(generated["abilities"], value, "ability");
            } else {
                generated[key] = value;
            }
        }
        return generated;
    }

    CardDB CardDB::load(const std::filesystem::path& generated_dir, const std::filesystem::path& effects_dir) {
        CardDB db;
        for (const auto& file : std::filesystem::directory_iterator(generated_dir)) {
            if (file.path().extension() != ".json") {
                continue;
            }
            try {
                json card = read_json(file.path());
                const auto effects_file = effects_dir / file.path().filename();
                if (std::filesystem::exists(effects_file)) {
                    card = merge_effects(std::move(card), read_json(effects_file));
                } else if (const std::string same_as = card.value("same_as", ""); !same_as.empty()) {
                    const auto linked = effects_dir / (same_as + ".json");
                    if (!std::filesystem::exists(linked)) {
                        throw CardDataError("same_as names " + same_as + ", which has no effects file");
                    }
                    card = merge_effects(std::move(card), read_json(linked));
                }
                db.add(card.get<CardDef>());
            } catch (const CardDataError& e) {
                throw CardDataError(file.path().filename().string() + ": " + e.what());
            }
        }

        if (std::filesystem::exists(effects_dir)) {
            for (const auto& file : std::filesystem::directory_iterator(effects_dir)) {
                if (file.path().extension() == ".json" && !db.find(file.path().stem().string())) {
                    throw CardDataError("effects file " + file.path().filename().string() + " has no matching card");
                }
            }
        }
        return db;
    }

    void CardDB::add(CardDef card) {
        for (Attack& attack : card.attacks) {
            try {
                attack.program = compile_attack(attack.effects);
            } catch (const CardDataError& e) {
                throw CardDataError(card.id + " attack '" + attack.name + "': " + e.what());
            }
        }
        try {
            if (card.trainer_kind == TrainerKind::Item || card.trainer_kind == TrainerKind::Supporter || card.trainer_kind == TrainerKind::Stadium) {
                card.program = compile_trainer(card.effects);
            }
            card.conditions = compile_conditions(card.requirements);
            card.modifiers = compile_passive(card.passive);
            card.conditional_provides = compile_provides_when(card.provides_when);
        } catch (const CardDataError& e) {
            throw CardDataError(card.id + ": " + e.what());
        }
        for (Ability& ability : card.abilities) {
            try {
                ability.modifiers = compile_passive(ability.passive);
                ability.program = compile_trainer(ability.effects);
                ability.conditions = compile_conditions(ability.requirements);
            } catch (const CardDataError& e) {
                throw CardDataError(card.id + " ability '" + ability.name + "': " + e.what());
            }
        }
        const std::string id = card.id;
        cards_.insert_or_assign(id, std::move(card));
    }

    const CardDef* CardDB::find(std::string_view id) const {
        const auto it = cards_.find(id);
        return it == cards_.end() ? nullptr : &it->second;
    }

    const CardDef& CardDB::at(std::string_view id) const {
        if (const auto* card = find(id)) {
            return *card;
        }
        throw CardDataError("unknown card '" + std::string(id) + "'");
    }

    std::vector<std::string> missing_effects(const CardDef& c) {
        std::vector<std::string> missing;
        const std::string label = c.id + " " + c.name;
        if (!c.text.empty() && c.effects.empty() && c.passive.empty() && c.provides_when.empty()) {
            missing.push_back(label);
        }
        for (const auto& a : c.abilities) {
            if (!a.text.empty() && a.effects.empty() && a.passive.empty() && a.no_effect.empty()) {
                missing.push_back(label + ": ability " + a.name);
            }
        }
        for (const auto& a : c.attacks) {
            if (!a.text.empty() && a.effects.empty()) {
                missing.push_back(label + ": attack " + a.name);
            }
        }
        return missing;
    }

    std::vector<std::string> CardDB::missing_effects() const {
        std::vector<std::string> missing;
        for (const auto& entry : cards_) {
            for (std::string& item : pokebot::missing_effects(entry.second)) {
                missing.push_back(std::move(item));
            }
        }
        return missing;
    }

} // namespace pokebot