#include "cards/card_db.hpp"

#include <fstream>

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
            if (file.path().extension() != ".json") continue;
            try {
                json card = read_json(file.path());
                const auto effects_file = effects_dir / file.path().filename();
                if (std::filesystem::exists(effects_file)) {
                    card = merge_effects(std::move(card), read_json(effects_file));
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
        const std::string id = card.id;
        cards_.insert_or_assign(id, std::move(card));
    }

    const CardDef* CardDB::find(std::string_view id) const {
        const auto it = cards_.find(id);
        return it == cards_.end() ? nullptr : &it->second;
    }

    const CardDef& CardDB::at(std::string_view id) const {
        if (const auto* card = find(id)) return *card;
        throw CardDataError("unknown card '" + std::string(id) + "'");
    }

    std::vector<std::string> CardDB::missing_effects() const {
        std::vector<std::string> missing;
        for (const auto& [id, c] : cards_) {
            const std::string label = id + " " + c.name;
            if (!c.text.empty() && c.effects.empty()) missing.push_back(label);
            for (const auto& a : c.abilities) {
                if (!a.text.empty() && a.effects.empty()) missing.push_back(label + ": ability " + a.name);
            }
            for (const auto& a : c.attacks) {
                if (!a.text.empty() && a.effects.empty()) missing.push_back(label + ": attack " + a.name);
            }
        }
        return missing;
    }
    
} // namespace pokebot