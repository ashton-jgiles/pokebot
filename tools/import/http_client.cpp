#include "http_client.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <string_view>

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
    } // namespace

    std::string CurlProcessClient::get(const std::string& url) {
        if (!is_safe_url(url)) {
            throw HttpError("refusing unsafe URL: " + url);
        }

        // --silent: no progress bar   --show-error: still print real errors
        // --fail: non-zero exit on HTTP 4xx/5xx   --location: follow redirects
        const std::string command = "curl --silent --show-error --fail --location --max-time " + std::to_string(timeout_seconds_) + " \"" + url + "\"";

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
} // namespace pokebot::import
