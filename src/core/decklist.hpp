#pragma once

#include <filesystem>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pokebot {

    struct DeckEntry {
        int count = 0;
        std::string name;
        std::string set_code;
        int number = 0;

        bool operator==(const DeckEntry&) const = default;
    };

    struct Decklist {
        std::vector<DeckEntry> entries;
        int total_cards() const;
        bool operator==(const Decklist&) const = default;
    };

    class DecklistError : public std::runtime_error {
    public:
        DecklistError(int line, const std::string& message)
            : std::runtime_error("line " + std::to_string(line) + ": " + message), line_(line) {}
        int line() const { return line_; }

    private:
        int line_;
    };

    Decklist parse_decklist(std::istream& in);
    Decklist load_decklist(const std::filesystem::path& path);

    // One line of a decklist file, such as "4 Dreepy TWM 128".
    std::string to_line(const DeckEntry& entry);

} // namespace pokebot
