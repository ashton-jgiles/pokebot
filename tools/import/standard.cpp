#include "standard.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <set>
#include <string_view>

#include "cards/db/card_db.hpp"
#include "convert.hpp"
#include "translate.hpp"

namespace pokebot::import {
    namespace {

        using nlohmann::json;

        std::string clean(std::string_view text) {
            std::string out;
            bool in_tag = false;
            bool space = false;
            for (const char c : text) {
                if (c == '<') {
                    in_tag = true;
                } else if (c == '>' && in_tag) {
                    in_tag = false;
                } else if (!in_tag) {
                    if (std::isspace(static_cast<unsigned char>(c))) {
                        space = !out.empty();
                    } else {
                        if (space) {
                            out += ' ';
                            space = false;
                        }
                        out += c;
                    }
                }
            }
            return out;
        }

        json read_effects(const std::filesystem::path& file) {
            std::ifstream in(file, std::ios::binary);
            try {
                return json::parse(in);
            } catch (const json::parse_error& e) {
                throw CardDataError(file.filename().string() + ": " + e.what());
            }
        }

        void check_effects(StandardCard& entry, const std::filesystem::path& effects_dir) {
            json effects;
            for (const std::string& id : entry.printings) {
                const auto file = effects_dir / (id + ".json");
                if (std::filesystem::exists(file)) {
                    effects = read_effects(file);
                    entry.effects_from = id;
                    break;
                }
            }
            if (!effects.is_object()) {
                json overlay = translate_card(entry.card).overlay;
                if (!overlay.empty()) {
                    effects = std::move(overlay);
                    entry.translated = true;
                }
            }
            if (entry.card.rules.ex && !entry.card.rules.mega && !(effects.is_object() && effects.contains("rules"))) {
                entry.tera_unchecked = true;
            }
            try {
                CardDB db;
                db.add(effects.is_object() ? merge_effects(json(entry.card), effects).get<CardDef>() : entry.card);
                entry.card = db.at(entry.card.id);
                entry.missing = missing_effects(entry.card);
            } catch (const std::exception& e) {
                entry.problems.push_back(std::string(entry.translated ? "its translation doesn't load: " : "its effects don't load: ") + e.what());
            }
        }

    } // namespace

    std::string printing_id(const std::string& set_code, const std::string& local_id) {
        int number = 0;
        const auto [ptr, ec] = std::from_chars(local_id.data(), local_id.data() + local_id.size(), number);
        if (ec != std::errc{} || ptr != local_id.data() + local_id.size() || number <= 0) {
            return "";
        }
        return set_code + "-" + std::to_string(number);
    }

    bool standard_legal(const json& tcgdex) {
        const auto it = tcgdex.find("legal");
        if (it == tcgdex.end() || !it->is_object()) {
            return false;
        }
        const auto standard = it->find("standard");
        return standard != it->end() && standard->is_boolean() && standard->get<bool>();
    }

    std::string expand_energy_symbols(std::string name) {
        static const std::map<char, std::string> kTypes = {{'G', "Grass"}, {'R', "Fire"}, {'W', "Water"}, {'L', "Lightning"}, {'P', "Psychic"}, {'F', "Fighting"}, {'D', "Darkness"}, {'M', "Metal"}, {'N', "Dragon"}, {'C', "Colorless"}};
        for (std::size_t at = name.find('{'); at != std::string::npos && at + 2 < name.size(); at = name.find('{', at + 1)) {
            const auto type = kTypes.find(name[at + 1]);
            if (name[at + 2] == '}' && type != kTypes.end()) {
                name.replace(at, 3, type->second);
            }
        }
        return name;
    }

    std::string card_signature(const CardDef& card) {
        std::string s = card.name + '|' + std::to_string(static_cast<int>(card.kind)) + '|' + std::to_string(static_cast<int>(card.stage)) + '|' +
                        std::to_string(card.hp) + '|' + clean(card.text);
        for (const Ability& a : card.abilities) {
            s += "|A:" + a.name + ':' + clean(a.text);
        }
        for (const Attack& a : card.attacks) {
            s += "|T:" + a.name + ':' + std::to_string(a.damage) + std::to_string(static_cast<int>(a.damage_mod)) + ':' + clean(a.text);
        }
        return s;
    }

    Coverage standard_coverage(const std::vector<std::pair<std::string, json>>& sets, const json& corrections, const std::filesystem::path& effects_dir) {
        Coverage out;
        std::map<std::string, std::size_t> by_signature;
        for (const auto& [code, cards] : sets) {
            for (const json& tcgdex : cards) {
                ++out.printings;
                if (!standard_legal(tcgdex)) {
                    ++out.not_legal[code];
                    continue;
                }
                const std::string id = printing_id(code, tcgdex.value("localId", ""));
                if (id.empty()) {
                    ++out.unnumbered;
                    continue;
                }
                StandardCard entry;
                std::string signature;
                try {
                    Conversion conversion = convert_card(apply_corrections(tcgdex, corrections), id);
                    entry.card = std::move(conversion.card);
                    entry.problems = std::move(conversion.problems);
                    signature = card_signature(entry.card);
                } catch (const std::exception& e) {
                    entry.card.id = id;
                    entry.card.name = tcgdex.value("name", "");
                    entry.problems.push_back(e.what());
                    signature = "unreadable " + id;
                }
                const auto [it, added] = by_signature.emplace(signature, out.cards.size());
                if (added) {
                    entry.printings.push_back(id);
                    out.cards.push_back(std::move(entry));
                } else {
                    out.cards[it->second].printings.push_back(id);
                }
            }
        }

        std::map<std::string, std::set<Stage>> stages;
        for (const StandardCard& entry : out.cards) {
            if (entry.card.kind == CardKind::Pokemon) {
                stages[entry.card.name].insert(entry.card.stage);
            }
        }
        for (StandardCard& entry : out.cards) {
            if (entry.card.kind == CardKind::Pokemon && entry.card.stage == Stage::Basic && stages[entry.card.name].size() > 1) {
                entry.problems.push_back("a Basic, but another printing with this name is an evolution; check the card and add it to corrections.json");
            }
            if (entry.problems.empty()) {
                check_effects(entry, effects_dir);
            }
        }
        return out;
    }

    std::map<std::string, std::string> link_reprints(const std::filesystem::path& generated_dir, const std::filesystem::path& effects_dir) {
        std::vector<std::pair<std::filesystem::path, json>> cards;
        for (const auto& file : std::filesystem::directory_iterator(generated_dir)) {
            if (file.path().extension() == ".json") {
                std::ifstream in(file.path(), std::ios::binary);
                cards.emplace_back(file.path(), json::parse(in));
            }
        }
        std::sort(cards.begin(), cards.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::map<std::string, std::string> written;
        for (const auto& [path, card] : cards) {
            if (std::filesystem::exists(effects_dir / path.filename())) {
                written.emplace(card_signature(card.get<CardDef>()), card.at("id").get<std::string>());
            }
        }
        std::map<std::string, std::string> links;
        for (auto& [path, card] : cards) {
            std::string target;
            if (!std::filesystem::exists(effects_dir / path.filename())) {
                if (const auto it = written.find(card_signature(card.get<CardDef>())); it != written.end()) {
                    target = it->second;
                    links.emplace(card.at("id").get<std::string>(), target);
                }
            }
            if (card.value("same_as", "") == target) {
                continue;
            }
            if (target.empty()) {
                card.erase("same_as");
            } else {
                card["same_as"] = target;
            }
            std::ofstream(path, std::ios::binary) << card.dump(2) << '\n';
        }
        return links;
    }

    StoredTranslations translate_generated(const std::filesystem::path& generated_dir, const std::filesystem::path& effects_dir) {
        std::vector<std::filesystem::path> files;
        for (const auto& file : std::filesystem::directory_iterator(generated_dir)) {
            if (file.path().extension() == ".json") {
                files.push_back(file.path());
            }
        }
        std::sort(files.begin(), files.end());
        StoredTranslations out;
        for (const auto& path : files) {
            json card;
            {
                std::ifstream in(path, std::ios::binary);
                card = json::parse(in);
            }
            json translation;
            if (!std::filesystem::exists(effects_dir / path.filename()) && card.value("same_as", "").empty()) {
                json plain = card;
                plain.erase("translation");
                const CardDef def = plain.get<CardDef>();
                const CardTranslation read = translate_card(def);
                if (!read.overlay.empty()) {
                    try {
                        CardDB db;
                        db.add(merge_effects(plain, read.overlay).get<CardDef>());
                        translation = read.overlay;
                        out.cards.insert(def.id);
                        out.complete += read.unread.empty() ? 1 : 0;
                    } catch (const std::exception& e) {
                        out.problems.push_back(def.id + " " + def.name + ": its translation doesn't load: " + e.what());
                    }
                }
            }
            if (translation.is_null() ? !card.contains("translation") : card.contains("translation") && card.at("translation") == translation) {
                continue;
            }
            if (translation.is_null()) {
                card.erase("translation");
            } else {
                card["translation"] = translation;
            }
            std::ofstream(path, std::ios::binary) << card.dump(2) << '\n';
        }
        return out;
    }

} // namespace pokebot::import
