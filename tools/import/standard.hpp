#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/card/card.hpp"

namespace pokebot::import {

    struct StandardCard {
        CardDef card;
        std::vector<std::string> printings;
        std::vector<std::string> problems;
        std::vector<std::string> missing;
        std::string effects_from;
        bool tera_unchecked = false;
    };

    struct Coverage {
        int printings = 0;
        int unnumbered = 0;
        std::map<std::string, int> not_legal;
        std::vector<StandardCard> cards;
    };

    std::string printing_id(const std::string& set_code, const std::string& local_id);

    std::string card_signature(const CardDef& card);

    Coverage standard_coverage(const std::vector<std::pair<std::string, nlohmann::json>>& sets, const nlohmann::json& corrections,
                               const std::filesystem::path& effects_dir);

} // namespace pokebot::import
