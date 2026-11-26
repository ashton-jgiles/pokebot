#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cards/db/card_db.hpp"
#include "cards/validator/deck_validator.hpp"
#include "convert.hpp"
#include "core/decklist.hpp"
#include "http_client.hpp"
#include "standard.hpp"
#include "tcgdex.hpp"
#include "translate.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

    void print_usage() {
        std::cerr << "usage: pokebot-import [--refresh] [--offline] <decklist.txt or directory>...\n"
                     "       pokebot-import [--refresh] [--offline] --standard [--list-missing]\n"
                     "       pokebot-import [--refresh] [--offline] --check-translations\n"
                     "  a directory means every .txt decklist in it and its subdirectories\n"
                     "  --standard      download every card of every set in sets.json and report how many of the\n"
                     "                  Standard-legal ones have all their effects\n"
                     "  --list-missing  with --standard: list every effect still missing\n"
                     "  --check-translations  translate every hand-written card text and compare it, then all of Standard\n"
                     "  --refresh  download every card again, ignoring the cache\n"
                     "  --offline  use only cached files\n";
    }

    void add_decklists(const std::filesystem::path& path, std::vector<std::filesystem::path>& decklists) {
        if (!std::filesystem::is_directory(path)) {
            decklists.push_back(path);
            return;
        }
        std::vector<std::filesystem::path> found;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                found.push_back(entry.path());
            }
        }
        std::sort(found.begin(), found.end());
        decklists.insert(decklists.end(), found.begin(), found.end());
    }

    // True if the card's effects file already states its rules (e.g. ["ex", "tera"]),
    // meaning someone has checked the card. A broken file is reported later by CardDB::load.
    bool effects_file_sets(const std::filesystem::path& effects_dir, const std::string& id, const char* key) {
        std::ifstream in(effects_dir / (id + ".json"), std::ios::binary);
        if (!in) {
            return false;
        }
        try {
            return nlohmann::json::parse(in).contains(key);
        } catch (const nlohmann::json::exception&) {
            return false;
        }
    }

    double percent(std::size_t part, std::size_t whole) {
        return whole == 0 ? 0.0 : 100.0 * static_cast<double>(part) / static_cast<double>(whole);
    }

    int run_standard(pokebot::import::TcgdexClient& tcgdex, const nlohmann::json& corrections, const std::filesystem::path& effects_dir, bool list_missing) {
        using namespace pokebot;
        using namespace pokebot::import;
        std::vector<std::pair<std::string, nlohmann::json>> sets;
        for (const std::string& code : tcgdex.set_codes()) {
            sets.emplace_back(code, tcgdex.set_cards(code));
            std::cout << "  " << code << "  " << tcgdex.set_id(code) << "  " << sets.back().second.size() << " cards\n";
        }
        const Coverage coverage = standard_coverage(sets, corrections, effects_dir);

        std::cout << '\n'
                  << sets.size() << " sets, " << coverage.printings << " printings, " << tcgdex.network_requests() << " network requests\n";
        std::cout << "Left out, not marked Standard-legal by TCGdex:";
        for (const auto& [code, count] : coverage.not_legal) {
            std::cout << ' ' << code << ' ' << count;
        }
        std::cout << "\nLeft out, no card number: " << coverage.unnumbered << '\n';

        std::cout << '\n'
                  << coverage.cards.size() << " distinct Standard cards\n";
        std::cout << std::setw(10) << "" << std::setw(10) << "complete" << std::setw(10) << "missing" << std::setw(10) << "problems" << std::setw(8) << "total" << '\n';
        std::size_t all[3] = {};
        const auto column = [](const StandardCard& entry) -> std::size_t {
            if (!entry.problems.empty()) {
                return 2;
            }
            return entry.missing.empty() ? 0 : 1;
        };
        for (const auto& [kind, label] : {std::pair{CardKind::Pokemon, "Pokemon"}, std::pair{CardKind::Trainer, "Trainer"}, std::pair{CardKind::Energy, "Energy"}}) {
            std::size_t counts[3] = {};
            for (const StandardCard& entry : coverage.cards) {
                if (entry.card.kind == kind) {
                    ++counts[column(entry)];
                }
            }
            std::cout << std::setw(10) << std::left << label << std::right;
            for (std::size_t i = 0; i < 3; ++i) {
                std::cout << std::setw(10) << counts[i];
                all[i] += counts[i];
            }
            std::cout << std::setw(8) << counts[0] + counts[1] + counts[2] << '\n';
        }
        std::cout << std::setw(10) << std::left << "All" << std::right << std::setw(10) << all[0] << std::setw(10) << all[1] << std::setw(10) << all[2]
                  << std::setw(8) << coverage.cards.size() << '\n';
        std::cout << std::fixed << std::setprecision(1) << "Complete: " << percent(all[0], coverage.cards.size()) << "% of distinct cards\n";

        std::size_t tera = 0;
        for (const StandardCard& entry : coverage.cards) {
            tera += entry.tera_unchecked ? 1 : 0;
        }
        std::cout << "Pokemon ex whose Tera isn't checked yet: " << tera << '\n';

        std::cout << "\nProblems:\n";
        for (const StandardCard& entry : coverage.cards) {
            for (const std::string& problem : entry.problems) {
                std::cout << "  " << entry.card.id << ' ' << entry.card.name << ": " << problem << '\n';
            }
        }
        if (list_missing) {
            std::cout << "\nMissing effects:\n";
            for (const StandardCard& entry : coverage.cards) {
                for (const std::string& item : entry.missing) {
                    std::cout << "  " << item << '\n';
                }
            }
        }
        return 0;
    }

    std::string sentence_shape(const std::string& sentence) {
        std::string out;
        bool digits = false;
        for (const char c : sentence) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                if (!digits) {
                    out += '#';
                }
                digits = true;
            } else {
                out += c;
                digits = false;
            }
        }
        return out;
    }

    std::vector<std::string> translated_parts(const pokebot::CardDef& card, const nlohmann::json& overlay) {
        std::vector<std::string> out;
        const std::string label = card.id + " " + card.name;
        for (const char* list : {"attacks", "abilities"}) {
            if (overlay.contains(list)) {
                for (const auto& [name, fields] : overlay.at(list).items()) {
                    out.push_back(label + ": " + (std::string(list) == "attacks" ? "attack " : "ability ") + name);
                }
            }
        }
        for (const auto& [key, value] : overlay.items()) {
            if (key != "attacks" && key != "abilities") {
                out.push_back(label);
                break;
            }
        }
        return out;
    }

    int run_check_translations(pokebot::import::TcgdexClient& tcgdex, const nlohmann::json& corrections, const std::filesystem::path& generated_dir,
                               const std::filesystem::path& effects_dir) {
        using namespace pokebot;
        using namespace pokebot::import;
        const CardDB db = CardDB::load(generated_dir, effects_dir);
        std::vector<std::filesystem::path> files;
        for (const auto& file : std::filesystem::directory_iterator(effects_dir)) {
            if (file.path().extension() == ".json") {
                files.push_back(file.path().filename());
            }
        }
        std::sort(files.begin(), files.end());
        int same = 0;
        int different = 0;
        int unread = 0;
        std::cout << "Hand-written cards, each attack, Ability and card text:\n";
        for (const auto& name : files) {
            std::ifstream in(generated_dir / name, std::ios::binary);
            const CardDef card = nlohmann::json::parse(in).get<CardDef>();
            const CardTranslation translation = translate_card(card);
            CardDB translated_db;
            translated_db.add(merge_effects(nlohmann::json(card), translation.overlay).get<CardDef>());
            const auto differences = translation_differences(db.at(card.id), translated_db.at(card.id), translation.overlay);
            for (const std::string& part : translated_parts(card, translation.overlay)) {
                if (std::find(differences.begin(), differences.end(), part) != differences.end()) {
                    ++different;
                    std::cout << "  DIFFERENT  " << part << "\n    translated: " << translation.overlay.dump() << '\n';
                } else {
                    ++same;
                    std::cout << "  same       " << part << '\n';
                }
            }
            for (const auto& [part, why] : translation.unread) {
                ++unread;
                std::cout << "  not read   " << part << ": " << why << '\n';
            }
        }
        std::cout << same + different + unread << " hand-written texts: " << same << " the same, " << different << " different, " << unread << " not read\n";

        std::map<std::string, int> unread_shapes;
        std::map<std::string, std::pair<int, int>> by_kind;
        std::set<std::string> seen;
        int cards = 0;
        int complete = 0;
        for (const std::string& code : tcgdex.set_codes()) {
            for (const nlohmann::json& tcgdex_card : tcgdex.set_cards(code)) {
                const std::string id = printing_id(code, tcgdex_card.value("localId", ""));
                if (!standard_legal(tcgdex_card) || id.empty()) {
                    continue;
                }
                CardDef card;
                try {
                    card = convert_card(apply_corrections(tcgdex_card, corrections), id).card;
                } catch (const std::exception&) {
                    continue;
                }
                if (!seen.insert(card_signature(card)).second) {
                    continue;
                }
                ++cards;
                const CardTranslation translation = translate_card(card);
                complete += translation.unread.empty() ? 1 : 0;
                const auto kind_of = [&](const std::string& part) {
                    const std::size_t colon = part.find(": ");
                    if (colon != std::string::npos) {
                        return part.substr(colon + 2, part.find(' ', colon + 2) - colon - 2);
                    }
                    return std::string(card.kind == CardKind::Energy ? "energy" : card.trainer_kind == TrainerKind::Stadium ? "stadium"
                                                                              : card.trainer_kind == TrainerKind::Tool      ? "tool"
                                                                                                                            : "trainer");
                };
                for (const std::string& part : translated_parts(card, translation.overlay)) {
                    ++by_kind[kind_of(part)].first;
                    ++by_kind[kind_of(part)].second;
                }
                for (const auto& [part, why] : translation.unread) {
                    ++by_kind[kind_of(part)].second;
                    ++unread_shapes[sentence_shape(why)];
                }
            }
        }
        std::cout << std::fixed << std::setprecision(1) << "\nStandard: " << cards << " distinct cards, " << complete << " with every text read (" << percent(static_cast<std::size_t>(complete), static_cast<std::size_t>(cards))
                  << "%, counting cards with no text)\n";
        for (const auto& [kind, counts] : by_kind) {
            std::cout << "  " << std::setw(8) << std::left << kind << std::right << std::setw(6) << counts.first << " of " << std::setw(5) << counts.second << " texts read ("
                      << percent(static_cast<std::size_t>(counts.first), static_cast<std::size_t>(counts.second)) << "%)\n";
        }
        std::cout << "The sentences most often not read, by shape:\n";
        std::vector<std::pair<int, std::string>> shapes;
        for (const auto& [shape, count] : unread_shapes) {
            shapes.emplace_back(count, shape);
        }
        std::sort(shapes.begin(), shapes.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        for (std::size_t i = 0; i < shapes.size() && i < 30; ++i) {
            std::cout << std::setw(6) << shapes[i].first << "  " << shapes[i].second << '\n';
        }
        return different == 0 ? 0 : 1;
    }

} // namespace

int main(int argc, char** argv) {
    using namespace pokebot;
    using namespace pokebot::import;

#ifdef _WIN32
    // Card names contain UTF-8 ("Poké Pad"); tell the Windows console to display it as UTF-8.
    SetConsoleOutputCP(CP_UTF8);
#endif

    TcgdexOptions options;
    const std::filesystem::path generated_dir = "data/cards/generated";
    const std::filesystem::path effects_dir = "data/cards/effects";
    const std::filesystem::path corrections_file = "data/cards/corrections.json";
    const std::filesystem::path archetypes_dir = "data/archetypes";
    std::map<std::string, std::vector<std::string>> review; // card label -> notes
    std::vector<std::filesystem::path> decklists;
    bool standard = false;
    bool list_missing = false;
    bool check_translations = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--refresh") {
            options.refresh = true;
        } else if (arg == "--standard") {
            standard = true;
        } else if (arg == "--list-missing") {
            list_missing = true;
        } else if (arg == "--check-translations") {
            check_translations = true;
        } else if (arg == "--offline") {
            options.offline = true;
        } else if (arg.starts_with("--")) {
            print_usage();
            return 2;
        } else {
            add_decklists(arg, decklists);
        }
    }
    if ((standard || check_translations) ? (!decklists.empty() || (standard && check_translations) || (check_translations && list_missing))
                                         : (decklists.empty() || list_missing)) {
        print_usage();
        return 2;
    }

    try {
        CurlProcessClient http;
        TcgdexClient tcgdex(http, options);
        std::ifstream corrections_in(corrections_file, std::ios::binary);
        if (!corrections_in) {
            throw std::runtime_error("cannot open " + corrections_file.string());
        }
        const nlohmann::json corrections = nlohmann::json::parse(corrections_in);
        if (standard) {
            return run_standard(tcgdex, corrections, effects_dir, list_missing);
        }
        if (check_translations) {
            return run_check_translations(tcgdex, corrections, generated_dir, effects_dir);
        }

        std::set<std::pair<std::string, int>> seen;
        std::map<std::string, bool> standard_printing;
        int failures = 0;
        int name_mismatches = 0;

        for (const auto& path : decklists) {
            std::cout << path.string() << '\n';
            const Decklist deck = load_decklist(path);

            for (const auto& entry : deck.entries) {
                if (!seen.emplace(entry.set_code, entry.number).second) {
                    continue;
                }
                const std::string label = entry.set_code + " " + std::to_string(entry.number);
                try {
                    const auto card = apply_corrections(tcgdex.card({entry.set_code, entry.number}), corrections);
                    const auto api_name = card.at("name").get<std::string>();
                    std::cout << "  ok    " << label << "  " << api_name << '\n';

                    if (api_name != expand_energy_symbols(entry.name) && !entry.name.starts_with("Basic {")) {
                        ++name_mismatches;
                        std::cout << "        warning: decklist says \"" << entry.name << "\"\n";
                    }

                    // Convert to our format and save it.
                    const std::string id = card_id(entry);
                    standard_printing[id] = standard_legal(card);
                    const auto conversion = convert_card(card, id);
                    std::filesystem::create_directories(generated_dir);
                    std::ofstream(generated_dir / (id + ".json"), std::ios::binary)
                        << nlohmann::json(conversion.card).dump(2) << '\n';
                    const bool provides_written = effects_file_sets(effects_dir, id, "provides");
                    for (const auto& problem : conversion.problems) {
                        review[id + " " + api_name].push_back(problem);
                    }
                    for (const auto& note : conversion.notes) {
                        if (provides_written && conversion.card.kind == CardKind::Energy) {
                            continue; // the effects file already says what this Energy provides
                        }
                        review[id + " " + api_name].push_back(note);
                    }
                    if (conversion.tera_unknown && !effects_file_sets(effects_dir, id, "rules")) {
                        review[id + " " + api_name].push_back("Pokemon ex: check whether it's Tera, then add effects/" + id + ".json with its rules");
                    }
                } catch (const std::exception& e) {
                    ++failures;
                    std::cout << "  FAIL  " << label << "  " << e.what() << '\n';
                }
            }
        }

        if (!review.empty()) {
            std::cout << "\nNeeds review:\n";
            for (const auto& [card_label, notes] : review) {
                for (const auto& note : notes) {
                    std::cout << "  " << card_label << ": " << note << '\n';
                }
            }
        }

        std::cout << '\n'
                  << seen.size() << " distinct cards, " << tcgdex.network_requests()
                  << " network requests, " << failures << " failures, " << name_mismatches
                  << " name mismatches\n";

        const auto links = link_reprints(generated_dir, effects_dir);
        std::cout << links.size() << " printings use the effects of a reprint with the same rules\n";

        // Load everything back exactly as the engine will, merging effects files.
        const CardDB db = CardDB::load(generated_dir, effects_dir);
        const auto missing = db.missing_effects();
        std::cout << db.size() << " cards load cleanly; " << missing.size() << " attacks/abilities/cards still need effects\n";

        std::cout << "\nDecks:\n";
        for (const auto& path : decklists) {
            std::vector<std::string> ids;
            for (const auto& entry : load_decklist(path).entries) {
                if (const std::string id = card_id(entry); std::find(ids.begin(), ids.end(), id) == ids.end()) {
                    ids.push_back(id);
                }
            }
            std::size_t ready = 0;
            std::vector<std::string> lines;
            for (const std::string& id : ids) {
                const CardDef* card = db.find(id);
                if (card == nullptr) {
                    lines.push_back("not imported: " + id);
                    continue;
                }
                const auto card_missing = pokebot::missing_effects(*card);
                if (card_missing.empty()) {
                    ++ready;
                }
                for (const auto& item : card_missing) {
                    lines.push_back("needs effects: " + item);
                }
                if (const auto legal = standard_printing.find(id); legal != standard_printing.end() && !legal->second) {
                    const auto link = links.find(id);
                    lines.push_back(link != links.end() ? "older printing: " + id + " " + card->name + ", the same card as " + link->second
                                                        : "TCGdex doesn't mark this printing Standard-legal: " + id + " " + card->name);
                }
            }
            std::cout << "  " << path.string() << ": " << ready << " of " << ids.size() << " cards ready\n";
            for (const auto& line : lines) {
                std::cout << "    " << line << '\n';
            }
            if (path.parent_path().filename() == "decks" && !std::filesystem::exists(archetypes_dir / path.stem())) {
                std::cout << "    no archetype folder " << (archetypes_dir / path.stem()).string() << ": agents can't recognise this deck as an opponent\n";
            }
        }

        // Check every deck against the deck-building rules.
        std::cout << "\nDeck legality:\n";
        int illegal_decks = 0;
        for (const auto& path : decklists) {
            const DeckValidation validation = validate_deck(load_decklist(path), db);
            if (validation.legal()) {
                std::cout << "  legal    " << path.string() << '\n';
            } else {
                ++illegal_decks;
                std::cout << "  ILLEGAL  " << path.string() << '\n';
                for (const auto& problem : validation.problems) {
                    std::cout << "           - " << problem << '\n';
                }
            }
        }

        return (failures == 0 && illegal_decks == 0) ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
