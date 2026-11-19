#include "core/decklist.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>

namespace pokebot {
    namespace {

        std::string trim(const std::string& s) {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        std::optional<int> parse_int(const std::string& s) {
            int value = 0;
            const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
            if (ec != std::errc{} || ptr != s.data() + s.size()) {
                return std::nullopt;
            }
            return value;
        }

        bool is_set_code(const std::string& s) {
            if (s.empty() || s.size() > 6) {
                return false;
            }
            for (const char c : s) {
                const auto u = static_cast<unsigned char>(c);
                if (!std::isupper(u) && !std::isdigit(u)) {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    int Decklist::total_cards() const {
        int total = 0;
        for (const auto& e : entries) {
            total += e.count;
        }
        return total;
    }

    Decklist parse_decklist(std::istream& in) {
        Decklist deck;
        std::optional<int> expected_total;
        std::string raw;
        int line_no = 0;

        while (std::getline(in, raw)) {
            ++line_no;
            const std::string line = trim(raw);
            if (line.empty()) {
                continue;
            }

            if (line.starts_with("Total Cards:")) {
                expected_total = parse_int(trim(line.substr(12)));
                if (!expected_total) {
                    throw DecklistError(line_no, "bad total: " + line);
                }
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(line[0]))) {
                continue;
            }

            std::vector<std::string> tokens;
            std::istringstream words(line);
            for (std::string w; words >> w;) {
                tokens.push_back(w);
            }
            if (tokens.size() < 4) {
                throw DecklistError(line_no, "expected '<count> <name> <SET> <number>'");
            }

            const auto count = parse_int(tokens.front());
            const auto number = parse_int(tokens.back());
            const std::string& set_code = tokens[tokens.size() - 2];
            if (!count || *count <= 0) {
                throw DecklistError(line_no, "bad count '" + tokens.front() + "'");
            }
            if (!number || *number <= 0) {
                throw DecklistError(line_no, "bad card number '" + tokens.back() + "'");
            }
            if (!is_set_code(set_code)) {
                throw DecklistError(line_no, "bad set code '" + set_code + "'");
            }

            DeckEntry entry;
            entry.count = *count;
            entry.number = *number;
            entry.set_code = set_code;
            for (std::size_t i = 1; i + 2 < tokens.size(); ++i) {
                if (!entry.name.empty()) {
                    entry.name += ' ';
                }
                entry.name += tokens[i];
            }
            deck.entries.push_back(std::move(entry));
        }

        if (expected_total && *expected_total != deck.total_cards()) {
            throw DecklistError(line_no, "cards add up to " + std::to_string(deck.total_cards()) + " but 'Total Cards' says " + std::to_string(*expected_total));
        }
        return deck;
    }

    std::string to_line(const DeckEntry& entry) {
        return std::to_string(entry.count) + ' ' + entry.name + ' ' + entry.set_code + ' ' + std::to_string(entry.number);
    }

    Decklist load_decklist(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            throw std::runtime_error("cannot open decklist " + path.string());
        }
        return parse_decklist(in);
    }
} // namespace pokebot
