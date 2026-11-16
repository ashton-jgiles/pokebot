#include <exception>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/decklist.hpp"
#include "http_client.hpp"
#include "tcgdex.hpp"

namespace {

void print_usage() {
  std::cerr << "usage: pokebot-import [--refresh] [--offline] <decklist.txt>...\n"
               "  --refresh  download every card again, ignoring the cache\n"
               "  --offline  use only cached files\n";
}

}  // namespace

int main(int argc, char** argv) {
  using namespace pokebot;
  using namespace pokebot::import;

  TcgdexOptions options;
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
        if (!seen.emplace(entry.set_code, entry.number).second) continue;
        const std::string label = entry.set_code + " " + std::to_string(entry.number);
        try {
          const auto card = tcgdex.card({entry.set_code, entry.number});
          const auto api_name = card.at("name").get<std::string>();
          std::cout << "  ok    " << label << "  " << api_name << '\n';

          if (api_name != entry.name && !entry.name.starts_with("Basic {")) {
            ++name_mismatches;
            std::cout << "        warning: decklist says \"" << entry.name << "\"\n";
          }
        } catch (const std::exception& e) {
          ++failures;
          std::cout << "  FAIL  " << label << "  " << e.what() << '\n';
        }
      }
    }

    std::cout << '\n'
              << seen.size() << " distinct cards, " << tcgdex.network_requests()
              << " network requests, " << failures << " failures, " << name_mismatches
              << " name mismatches\n";
    return failures == 0 ? 0 : 1;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
