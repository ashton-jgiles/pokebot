#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cards/card_db.hpp"
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
        std::cerr << "usage: pokebot-import [--refresh] [--offline] <decklist.txt>...\n"
                     "  --refresh  download every card again, ignoring the cache\n"
                     "  --offline  use only cached files\n";
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
            decklists.emplace_back(arg);
        }
    }
    if (decklists.empty()) {
        print_usage();
        return 2;
    }

    try {
        CurlProcessClient http;
        TcgdexClient tcgdex(http, options);

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
                    const auto card = tcgdex.card({entry.set_code, entry.number});
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
                    for (const auto& note : conversion.notes) {
                        review[id + " " + api_name].push_back(note);
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
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
