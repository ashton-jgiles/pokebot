#pragma once

#include <stdexcept>
#include <string>

namespace pokebot::import {
    class HttpError : public std::runtime_error {
        public:
            using std::runtime_error::runtime_error;
    };

    class HttpClient {
        public:
            virtual ~HttpClient() = default;
            virtual std::string get(const std::string& url) = 0;
    };

    class CurlProcessClient final : public HttpClient {
        public:
            explicit CurlProcessClient(int timeout_seconds = 20) : timeout_seconds_(timeout_seconds) {}
            std::string get(const std::string& url) override;
        private:
            int timeout_seconds_;
    };
    
} // namespace pokebot::import
