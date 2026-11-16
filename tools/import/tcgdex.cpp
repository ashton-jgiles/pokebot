#include "tcgdex.hpp"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace pokebot::import {
    namespace {

        constexpr const char* kBaseUrl = "https://api.tcgdex.net/v2/en";

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

    nlohmann::json TcgdexClient::fetch_with_retries(const std::string& api_path) {
        const std::string url = std::string(kBaseUrl) + api_path;
        auto delay = options_.retry_delay;
        std::string last_error;

        for (int attempt = 1; attempt <= options_.max_attempts; ++attempt) {
            if (network_requests_ > 0) {
                std::this_thread::sleep_for(options_.request_gap);
            }
            ++network_requests_;
            try {
                auto j = nlohmann::json::parse(http_.get(url));
                if (j.is_object()) {
                    return j;
                }
                last_error = "response is not a JSON object";
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
        throw std::runtime_error("GET " + url + " failed after " + std::to_string(options_.max_attempts) + " attempts: " + last_error);
    }
} // namespace pokebot::import
