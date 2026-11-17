#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/card/card.hpp"
#include "core/decklist.hpp"

namespace pokebot {
    std::string card_id(const DeckEntry& entry);

    nlohmann::json merge_effects(nlohmann::json generated, const nlohmann::json& overrides);

    class CardDB {
    public:
        static CardDB load(const std::filesystem::path& generated_dir, const std::filesystem::path& effects_dir);
        void add(CardDef card);
        const CardDef* find(std::string_view id) const;
        const CardDef& at(std::string_view id) const;
        std::size_t size() const { return cards_.size(); }
        std::vector<std::string> missing_effects() const;

    private:
        std::map<std::string, CardDef, std::less<>> cards_;
    };

} // namespace pokebot
