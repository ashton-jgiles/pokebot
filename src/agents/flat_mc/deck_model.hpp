#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"

namespace pokebot {

    struct ArchetypeCard {
        const CardDef* def = nullptr;
        int count = 0;
    };

    struct ArchetypeList {
        std::string archetype;
        std::string variant;
        Decklist list;
        std::vector<ArchetypeCard> cards;

        int count(std::string_view id) const;
    };

    class ArchetypeError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class ArchetypeLibrary {
    public:
        static ArchetypeLibrary load(const std::filesystem::path& dir, const CardDB& db);

        void add(const std::string& archetype, const std::string& variant, const Decklist& list, const CardDB& db);
        const std::vector<ArchetypeList>& lists() const { return lists_; }
        std::vector<std::string> archetypes() const;
        const ArchetypeList* find(std::string_view archetype, std::string_view variant) const;
        std::size_t size() const { return lists_.size(); }
        bool empty() const { return lists_.empty(); }

    private:
        std::vector<ArchetypeList> lists_;
    };

} // namespace pokebot
