#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "http_client.hpp"

namespace pokebot::import {

    // A card as a decklist names it: {"TWM", 130}.
    struct CardRef {
        std::string set_code;
        int number = 0;
    };

    struct TcgdexOptions {
        std::filesystem::path cache_dir = "data/cards/raw";
        std::filesystem::path sets_file = "data/cards/sets.json";
        bool refresh = false; // ignore cached files and download again
        bool offline = false; // use only cached files, never the network
        int max_attempts = 3;
        std::chrono::milliseconds retry_delay{500}; // doubles after each failed attempt
        std::chrono::milliseconds request_gap{150}; // pause between requests (be polite)
        int page_size = 250;
        std::vector<std::string> series = {"sv", "me"};
    };

    class TcgdexClient {
    public:
        TcgdexClient(HttpClient& http, TcgdexOptions options);

        std::string set_id(const std::string& set_code);

        std::string card_id(const CardRef& ref);

        nlohmann::json card(const CardRef& ref);

        nlohmann::json set_cards(const std::string& set_code);

        std::vector<std::string> set_codes() const;

        int network_requests() const { return network_requests_; }

    private:
        const nlohmann::json& set_detail(const std::string& set_id);
        nlohmann::json cached_get(const std::filesystem::path& cache_file, const std::string& api_path);
        nlohmann::json fetch_with_retries(const std::string& api_path);
        nlohmann::json with_retries(const std::string& what, const std::function<nlohmann::json()>& attempt);

        HttpClient& http_;
        TcgdexOptions options_;
        std::map<std::string, std::string> set_ids_;
        std::map<std::string, nlohmann::json> sets_;
        std::map<std::string, std::string> found_codes_;
        bool series_read_ = false;
        int network_requests_ = 0;
    };

} // namespace pokebot::import
