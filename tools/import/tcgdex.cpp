#include "tcgdex.hpp"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace pokebot::import {
    namespace {

        constexpr const char* kBaseUrl = "https://api.tcgdex.net/v2/en";
        constexpr const char* kGraphqlUrl = "https://api.tcgdex.net/v2/graphql";
        constexpr const char* kFields =
            "id localId name category stage hp suffix types evolveFrom retreat regulationMark rarity "
            "effect trainerType energyType legal { standard expanded } abilities { type name effect } "
            "attacks { name cost damage effect } weaknesses { type value } resistances { type value }";

        nlohmann::json read_json_file(const std::filesystem::path& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                throw std::runtime_error("cannot open " + path.string());
            }
            return nlohmann::json::parse(in);
        }

        void write_json_file(const std::filesystem::path& path, const nlohmann::json& j) {
            std::filesystem::create_directories(path.parent_path());
            auto tmp = path;
            tmp += ".tmp";
            {
                std::ofstream out(tmp, std::ios::binary);
                out << j.dump(2) << '\n';
                if (!out) {
                    throw std::runtime_error("cannot write " + tmp.string());
                }
            }
            std::filesystem::rename(tmp, path);
        }

        int local_number(const std::string& local_id) {
            int value = -1;
            const auto [ptr, ec] = std::from_chars(local_id.data(), local_id.data() + local_id.size(), value);
            return (ec == std::errc{} && ptr == local_id.data() + local_id.size()) ? value : -1;
        }
    } // namespace

    TcgdexClient::TcgdexClient(HttpClient& http, TcgdexOptions options) : http_(http), options_(std::move(options)) {
        const nlohmann::json sets = read_json_file(options_.sets_file);
        for (const auto& [code, id] : sets.items()) {
            set_ids_[code] = id.get<std::string>();
        }
    }

    std::string TcgdexClient::set_id(const std::string& set_code) const {
        const auto it = set_ids_.find(set_code);
        if (it == set_ids_.end()) {
            throw std::runtime_error("unknown set code '" + set_code + "' - add it to " + options_.sets_file.string());
        }
        return it->second;
    }

    const nlohmann::json& TcgdexClient::set_detail(const std::string& id) {
        auto it = sets_.find(id);
        if (it == sets_.end()) {
            auto detail = cached_get(options_.cache_dir / "sets" / (id + ".json"), "/sets/" + id);
            it = sets_.emplace(id, std::move(detail)).first;
        }
        return it->second;
    }

    std::string TcgdexClient::card_id(const CardRef& ref) {
        const std::string sid = set_id(ref.set_code);
        for (const auto& entry : set_detail(sid).at("cards")) {
            if (local_number(entry.at("localId").get<std::string>()) == ref.number) {
                return entry.at("id").get<std::string>();
            }
        }
        throw std::runtime_error(ref.set_code + " " + std::to_string(ref.number) + " is not in TCGdex set " + sid);
    }

    nlohmann::json TcgdexClient::card(const CardRef& ref) {
        const std::string id = card_id(ref);
        return cached_get(options_.cache_dir / "cards" / (id + ".json"), "/cards/" + id);
    }

    nlohmann::json TcgdexClient::cached_get(const std::filesystem::path& cache_file, const std::string& api_path) {
        if (!options_.refresh && std::filesystem::exists(cache_file)) {
            return read_json_file(cache_file);
        }
        if (options_.offline) {
            throw std::runtime_error("offline, and not cached: " + cache_file.string());
        }

        auto j = fetch_with_retries(api_path);
        write_json_file(cache_file, j);
        return j;
    }

    std::vector<std::string> TcgdexClient::set_codes() const {
        std::vector<std::string> codes;
        for (const auto& [code, id] : set_ids_) {
            codes.push_back(code);
        }
        return codes;
    }

    nlohmann::json TcgdexClient::set_cards(const std::string& set_code) {
        const std::string sid = set_id(set_code);
        const auto cache_file = options_.cache_dir / "bulk" / (sid + ".json");
        if (!options_.refresh && std::filesystem::exists(cache_file)) {
            return read_json_file(cache_file).at("cards");
        }
        if (options_.offline) {
            throw std::runtime_error("offline, and not cached: " + cache_file.string());
        }
        nlohmann::json cards = nlohmann::json::array();
        for (int page = 1;; ++page) {
            const std::string query = "{ cards(pagination: {page: " + std::to_string(page) + ", itemsPerPage: " + std::to_string(options_.page_size) +
                                      "}, filters: {id: \"" + sid + "-\"}) { " + kFields + " } }";
            const std::string body = nlohmann::json{{"query", query}}.dump();
            const nlohmann::json batch = with_retries("POST " + std::string(kGraphqlUrl) + " for " + sid + " page " + std::to_string(page), [&] {
                const auto j = nlohmann::json::parse(http_.post_json(kGraphqlUrl, body));
                if (!j.is_object() || j.contains("errors") || !j.contains("data") || !j.at("data").is_object()) {
                    return nlohmann::json();
                }
                const auto& found = j.at("data").value("cards", nlohmann::json());
                return found.is_null() ? nlohmann::json::array() : found;
            });
            if (!batch.is_array()) {
                throw std::runtime_error("TCGdex's cards for " + sid + " aren't a list");
            }
            for (const auto& card : batch) {
                if (card.is_object() && card.value("id", "").starts_with(sid + "-")) {
                    cards.push_back(card);
                }
            }
            if (static_cast<int>(batch.size()) < options_.page_size) {
                break;
            }
        }
        write_json_file(cache_file, nlohmann::json{{"id", sid}, {"cards", cards}});
        return cards;
    }

    nlohmann::json TcgdexClient::fetch_with_retries(const std::string& api_path) {
        const std::string url = std::string(kBaseUrl) + api_path;
        return with_retries("GET " + url, [&] {
            auto j = nlohmann::json::parse(http_.get(url));
            return j.is_object() ? j : nlohmann::json();
        });
    }

    nlohmann::json TcgdexClient::with_retries(const std::string& what, const std::function<nlohmann::json()>& attempt_once) {
        auto delay = options_.retry_delay;
        std::string last_error;

        for (int attempt = 1; attempt <= options_.max_attempts; ++attempt) {
            if (network_requests_ > 0) {
                std::this_thread::sleep_for(options_.request_gap);
            }
            ++network_requests_;
            try {
                auto j = attempt_once();
                if (!j.is_null()) {
                    return j;
                }
                last_error = "response isn't what was expected";
            } catch (const HttpError& e) {
                last_error = e.what();
            } catch (const nlohmann::json::parse_error& e) {
                last_error = std::string("invalid JSON: ") + e.what();
            }
            if (attempt < options_.max_attempts) {
                std::this_thread::sleep_for(delay);
                delay *= 2;
            }
        }
        throw std::runtime_error(what + " failed after " + std::to_string(options_.max_attempts) + " attempts: " + last_error);
    }
} // namespace pokebot::import
