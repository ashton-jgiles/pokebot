#include "http_client.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#define POKEBOT_POPEN _popen
#define POKEBOT_PCLOSE _pclose
constexpr const char* kPipeMode = "rb";
#else
#define POKEBOT_POPEN popen
#define POKEBOT_PCLOSE pclose
constexpr const char* kPipeMode = "r";
#endif

namespace pokebot::import {
    namespace {
        bool is_safe_url(const std::string& url) {
            if (!url.starts_with("https://")) {
                return false;
            }
            constexpr std::string_view allowed = "-._~:/?=&%";
            for (const char c : url) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && allowed.find(c) == std::string_view::npos) {
                    return false;
                }
            }
            return true;
        }
        bool is_safe_path(const std::string& path) {
            constexpr std::string_view allowed = "-._~:/\\ ()";
            for (const char c : path) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && allowed.find(c) == std::string_view::npos) {
                    return false;
                }
            }
            return !path.empty();
        }

        std::string run_curl(const std::string& command, const std::string& url) {
            FILE* pipe = POKEBOT_POPEN(command.c_str(), kPipeMode);
            if (pipe == nullptr) {
                throw HttpError("could not start curl");
            }

            std::string body;
            std::array<char, 8192> buffer{};
            std::size_t n = 0;
            while ((n = std::fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
                body.append(buffer.data(), n);
            }

            const int status = POKEBOT_PCLOSE(pipe);
            if (status != 0) {
                throw HttpError("curl exited with status " + std::to_string(status) + " for " + url);
            }
            return body;
        }
    } // namespace

    std::string CurlProcessClient::get(const std::string& url) {
        if (!is_safe_url(url)) {
            throw HttpError("refusing unsafe URL: " + url);
        }

        // --silent: no progress bar   --show-error: still print real errors
        // --fail: non-zero exit on HTTP 4xx/5xx   --location: follow redirects
        return run_curl("curl --silent --show-error --fail --location --max-time " + std::to_string(timeout_seconds_) + " \"" + url + "\"", url);
    }

    std::string CurlProcessClient::post_json(const std::string& url, const std::string& body) {
        if (!is_safe_url(url)) {
            throw HttpError("refusing unsafe URL: " + url);
        }
        std::random_device random;
        const std::filesystem::path file = std::filesystem::temp_directory_path() / ("pokebot-post-" + std::to_string(random()) + ".json");
        if (!is_safe_path(file.string())) {
            throw HttpError("refusing unsafe temporary path: " + file.string());
        }
        {
            std::ofstream out(file, std::ios::binary);
            out << body;
            if (!out) {
                throw HttpError("cannot write " + file.string());
            }
        }
        const std::string command = "curl --silent --show-error --fail --location --max-time " + std::to_string(timeout_seconds_) +
                                    " --header \"Content-Type: application/json\" --data-binary \"@" + file.string() + "\" \"" + url + "\"";
        std::string response;
        try {
            response = run_curl(command, url);
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
            throw;
        }
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        return response;
    }
} // namespace pokebot::import
