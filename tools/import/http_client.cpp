#include "http_client.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <string_view>

#ifdef _WIN32
#define POKEBOT_POPEN = _popen
#define POKEBOT_PCLOSE = _pclose
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
    }
} // namespace pokebot::import
