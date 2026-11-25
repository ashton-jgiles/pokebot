#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
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
#include "tcgdex.hpp"

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
                     "  a directory means every .txt decklist in it and its subdirectories\n"
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
    std::map<std::string, std::vector<std::string>> review; // card label -> notes
    std::vector<std::filesystem::path> decklists;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--refresh") {
            options.refresh = true;
        } else if (arg == "--offline") {
            options.offline = true;
        } else if (arg.starts_with("--")) {
            print_usage();
            return 2;
        } else {
            add_decklists(arg, decklists);
        }
    }
    if (decklists.empty()) {
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

        std::set<std::pair<std::string, int>> seen;
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

                    if (api_name != entry.name && !entry.name.starts_with("Basic {")) {
                        ++name_mismatches;
                        std::cout << "        warning: decklist says \"" << entry.name << "\"\n";
                    }

                    // Convert to our format and save it.
                    const std::string id = card_id(entry);
                    const auto conversion = convert_card(card, id);
                    std::filesystem::create_directories(generated_dir);
                    std::ofstream(generated_dir / (id + ".json"), std::ios::binary)
                        << nlohmann::json(conversion.card).dump(2) << '\n';
                    const bool provides_written = effects_file_sets(effects_dir, id, "provides");
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

        // Load everything back exactly as the engine will, merging effects files.
        const CardDB db = CardDB::load(generated_dir, effects_dir);
        const auto missing = db.missing_effects();
        std::cout << db.size() << " cards load cleanly; " << missing.size()
                  << " attacks/abilities/cards still need hand-written effects\n";
        for (const auto& item : missing) {
            std::cout << "  needs effects: " << item << '\n';
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
