#include "convert.hpp"

#include <cctype>
#include <charconv>
#include <string_view>

namespace pokebot::import {
    namespace {

        using nlohmann::json;

        constexpr std::string_view kTimes = "\xC3\x97"; // "×" in UTF-8

        std::string lowercase(std::string s) {
            for (auto& ch : s) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return s;
        }

        EnergyType energy_type(const json& tcgdex_name) {
            return energy_type_from_string(lowercase(tcgdex_name.get<std::string>()));
        }

        int parse_number(std::string_view s, const std::string& context) {
            int value = 0;
            const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
            if (ec != std::errc{} || ptr != s.data() + s.size()) {
                throw CardDataError("can't read " + context + " '" + std::string(s) + "'");
            }
            return value;
        }

        json without_nulls(json value) {
            if (value.is_object()) {
                for (auto it = value.begin(); it != value.end();) {
                    if (it->is_null()) {
                        it = value.erase(it);
                    } else {
                        *it = without_nulls(std::move(*it));
                        ++it;
                    }
                }
            } else if (value.is_array()) {
                for (json& item : value) {
                    item = without_nulls(std::move(item));
                }
            }
            return value;
        }

        // 70 -> {70, None};  "20×" -> {20, Times};  "30+" -> {30, Plus};  "120-" -> {120, Minus}
        void parse_damage(const json& damage, Attack& attack) {
            if (damage.is_number_integer()) {
                attack.damage = damage.get<int>();
                return;
            }
            std::string s = damage.get<std::string>();
            if (s.ends_with(kTimes)) {
                attack.damage_mod = DamageMod::Times;
                s.resize(s.size() - kTimes.size());
            } else if (s.ends_with('+')) {
                attack.damage_mod = DamageMod::Plus;
                s.pop_back();
            } else if (s.ends_with('-')) {
                attack.damage_mod = DamageMod::Minus;
                s.pop_back();
            }
            attack.damage = parse_number(s, "damage");
        }

        void convert_pokemon(const json& t, Conversion& out) {
            CardDef& c = out.card;
            const auto stage = t.at("stage").get<std::string>();
            if (stage == "Basic") {
                c.stage = Stage::Basic;
            } else if (stage == "Stage1") {
                c.stage = Stage::Stage1;
            } else if (stage == "Stage2") {
                c.stage = Stage::Stage2;
            } else {
                throw CardDataError("stage '" + stage + "' isn't supported yet");
            }

            c.evolves_from = t.value("evolveFrom", "");
            if (c.stage != Stage::Basic && c.evolves_from.empty()) {
                out.notes.push_back("an evolution card that doesn't say what it evolves from; add it to corrections.json");
            }
            c.hp = t.at("hp").get<int>();
            for (const auto& type : t.at("types")) {
                c.types.push_back(energy_type(type));
            }
            c.retreat = t.value("retreat", 0);

            if (const auto& ws = t.value("weaknesses", json::array()); !ws.empty()) {
                if (ws.size() > 1) {
                    out.notes.push_back("has more than one Weakness; only the first is kept");
                }
                std::string value = ws[0].at("value").get<std::string>(); // "×2"
                if (value.starts_with(kTimes)) {
                    value.erase(0, kTimes.size());
                } else if (value.starts_with('x')) {
                    value.erase(0, 1);
                }
                c.weakness = Weakness{energy_type(ws[0].at("type")), parse_number(value, "weakness")};
            }
            if (const auto& rs = t.value("resistances", json::array()); !rs.empty()) {
                std::string value = rs[0].at("value").get<std::string>(); // "-30"
                if (value.starts_with('-')) {
                    value.erase(0, 1);
                }
                c.resistance = Resistance{energy_type(rs[0].at("type")), parse_number(value, "resistance")};
            }

            for (const auto& a : t.value("abilities", json::array())) {
                if (a.value("type", "") != "Ability") {
                    throw CardDataError("ability type '" + a.value("type", "") + "' isn't supported yet");
                }
                Ability ability;
                ability.name = a.at("name").get<std::string>();
                ability.text = a.value("effect", "");
                c.abilities.push_back(std::move(ability));
            }
            for (const auto& a : t.value("attacks", json::array())) {
                Attack attack;
                attack.name = a.at("name").get<std::string>();
                for (const auto& e : a.value("cost", json::array())) {
                    attack.cost.push_back(energy_type(e));
                }
                if (a.contains("damage")) {
                    parse_damage(a.at("damage"), attack);
                }
                attack.text = a.value("effect", "");
                c.attacks.push_back(std::move(attack));
            }

            c.rules.ex = lowercase(t.value("suffix", "")) == "ex";
            c.rules.mega = c.rules.ex && c.name.starts_with("Mega ");
            // Mega Evolution Pokemon ex have their own rule box, so they're never Tera.
            out.tera_unknown = c.rules.ex && !c.rules.mega;
        }

        void convert_trainer(const json& t, Conversion& out) {
            CardDef& c = out.card;
            const auto kind = t.at("trainerType").get<std::string>();
            if (kind == "Item") {
                c.trainer_kind = TrainerKind::Item;
            } else if (kind == "Supporter") {
                c.trainer_kind = TrainerKind::Supporter;
            } else if (kind == "Stadium") {
                c.trainer_kind = TrainerKind::Stadium;
            } else if (kind == "Tool") {
                c.trainer_kind = TrainerKind::Tool;
            } else {
                throw CardDataError("trainer type '" + kind + "' isn't supported yet");
            }
            c.text = t.value("effect", "");
            if (c.text.empty()) {
                out.notes.push_back("Trainer with no rules text");
            }
            c.rules.ace_spec = t.value("rarity", "") == "ACE SPEC Rare";
        }

        void convert_energy(const json& t, Conversion& out) {
            CardDef& c = out.card;
            const auto kind = t.at("energyType").get<std::string>();
            const std::string effect = t.value("effect", "");

            // Basic Energy has no rules text. TCGdex sometimes labels a Special Energy
            // "Normal" (e.g. Ignition Energy), so rules text wins over the label.
            const bool mislabeled_special = kind == "Normal" && !effect.empty();
            if (mislabeled_special) {
                out.notes.push_back("TCGdex lists this as basic Energy, but it has rules text; treated as Special Energy");
            }

            if (kind == "Normal" && !mislabeled_special) {
                c.energy_kind = EnergyKind::Basic;
                // TCGdex gives basic Energy no "types" field; the type is in the name.
                std::string name = c.name; // "Darkness Energy"
                if (!name.ends_with(" Energy")) {
                    throw CardDataError("unexpected basic Energy name '" + name + "'");
                }
                if (name.starts_with("Basic ")) {
                    name.erase(0, 6);
                    c.name = name;
                }
                name.resize(name.size() - 7);
                c.provides = {energy_type_from_string(lowercase(name))};
            } else if (kind == "Special" || mislabeled_special) {
                c.energy_kind = EnergyKind::Special;
                c.text = effect;
                c.rules.ace_spec = t.value("rarity", "") == "ACE SPEC Rare";
                out.notes.push_back("Special Energy: fill in what it provides in its effects file");
            } else {
                throw CardDataError("energy type '" + kind + "' isn't supported yet");
            }
        }

    } // namespace

    Conversion convert_card(const json& raw, const std::string& our_id) {
        Conversion out;
        const json tcgdex = without_nulls(raw);
        CardDef& c = out.card;
        try {
            c.id = our_id;
            c.tcgdex_id = tcgdex.at("id").get<std::string>();
            c.name = tcgdex.at("name").get<std::string>();

            const auto category = tcgdex.at("category").get<std::string>();
            if (category == "Pokemon") {
                c.kind = CardKind::Pokemon;
                convert_pokemon(tcgdex, out);
            } else if (category == "Trainer") {
                c.kind = CardKind::Trainer;
                convert_trainer(tcgdex, out);
            } else if (category == "Energy") {
                c.kind = CardKind::Energy;
                convert_energy(tcgdex, out);
            } else {
                throw CardDataError("category '" + category + "' isn't supported");
            }
        } catch (const json::exception& e) {
            throw CardDataError(our_id + ": unexpected TCGdex data: " + e.what());
        } catch (const CardDataError& e) {
            throw CardDataError(our_id + ": " + e.what());
        }
        return out;
    }

    json apply_corrections(json tcgdex, const json& corrections) {
        const auto& cards = corrections.at("cards");
        if (const auto card = cards.find(tcgdex.at("id").get<std::string>()); card != cards.end()) {
            tcgdex.update(*card);
        }
        const auto& evolves_from = corrections.at("evolves_from");
        const json stage = tcgdex.value("stage", json());
        const json from = tcgdex.value("evolveFrom", json());
        if ((stage == "Stage1" || stage == "Stage2") && (from.is_null() || from == "")) {
            if (const auto known = evolves_from.find(tcgdex.at("name").get<std::string>()); known != evolves_from.end()) {
                tcgdex["evolveFrom"] = *known;
            }
        }
        return tcgdex;
    }

} // namespace pokebot::import
