#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "tcgdex.hpp"

using pokebot::import::HttpClient;
using pokebot::import::HttpError;
using pokebot::import::TcgdexClient;
using pokebot::import::TcgdexOptions;

namespace {

    constexpr const char* kBase = "https://api.tcgdex.net/v2/en";

    // Returns queued responses per URL. An empty string in the queue simulates the
    // "200 with empty body" glitch; "!error" simulates a failed request.
    class FakeHttp final : public HttpClient {
    public:
        void queue_post(std::string body) {
            posts_.push_back(std::move(body));
        }
        std::string post_json(const std::string& url, const std::string& body) override {
            ++calls;
            bodies.push_back(body);
            if (url != "https://api.tcgdex.net/v2/graphql" || posts_.empty()) {
                throw HttpError("no post queued for " + url);
            }
            std::string response = posts_.front();
            posts_.pop_front();
            if (response == "!error") {
                throw HttpError("simulated failure");
            }
            return response;
        }
        std::vector<std::string> bodies;
        void queue(const std::string& path, std::string body) {
            responses_[kBase + path].push_back(std::move(body));
        }
        std::string get(const std::string& url) override {
            ++calls;
            auto& q = responses_[url];
            if (q.empty()) {
                throw HttpError("no response queued for " + url);
            }
            std::string body = q.front();
            q.pop_front();
            if (body == "!error") {
                throw HttpError("simulated failure");
            }
            return body;
        }
        int calls = 0;

    private:
        std::map<std::string, std::deque<std::string>> responses_;
        std::deque<std::string> posts_;
    };

    // A fresh temporary folder with a sets.json, removed when the test ends.
    struct TempDir {
        std::filesystem::path path;
        TempDir() {
            std::random_device rd;
            path = std::filesystem::temp_directory_path() / ("pokebot-test-" + std::to_string(rd()));
            std::filesystem::create_directories(path);
            std::ofstream(path / "sets.json") << R"({"POR": "me03"})";
        }
        ~TempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };

    TcgdexOptions test_options(const TempDir& dir) {
        TcgdexOptions o;
        o.cache_dir = dir.path / "raw";
        o.sets_file = dir.path / "sets.json";
        o.retry_delay = std::chrono::milliseconds(0);
        o.request_gap = std::chrono::milliseconds(0);
        return o;
    }

    const std::string kSet =
        R"({"id": "me03", "cards": [{"id": "me03-061", "localId": "061"},
                                {"id": "me03-062", "localId": "062"}]})";
    const std::string kCard = R"({"id": "me03-062", "name": "Meowth ex", "hp": 170})";

} // namespace

TEST_CASE("resolves a padded card id through the set's card list", "[tcgdex]") {
    TempDir dir;
    FakeHttp http;
    http.queue("/sets/me03", kSet);
    TcgdexClient client(http, test_options(dir));

    CHECK(client.set_id("POR") == "me03");
    CHECK(client.card_id({"POR", 62}) == "me03-062");
    CHECK_THROWS(client.card_id({"POR", 999})); // not in the set
    CHECK_THROWS(client.set_id("XYZ"));         // unknown set code
}

TEST_CASE("fetches a card once, then serves it from the cache", "[tcgdex]") {
    TempDir dir;
    {
        FakeHttp http;
        http.queue("/sets/me03", kSet);
        http.queue("/cards/me03-062", kCard);
        TcgdexClient client(http, test_options(dir));
        CHECK(client.card({"POR", 62}).at("name") == "Meowth ex");
        CHECK(http.calls == 2);
    }
    {
        FakeHttp http; // nothing queued: any network call would throw
        auto options = test_options(dir);
        options.offline = true;
        TcgdexClient client(http, options);
        CHECK(client.card({"POR", 62}).at("hp") == 170);
        CHECK(http.calls == 0);
    }
}

TEST_CASE("retries empty bodies and failed requests", "[tcgdex]") {
    TempDir dir;
    FakeHttp http;
    http.queue("/sets/me03", "");       // 200 with empty body
    http.queue("/sets/me03", "!error"); // curl failure
    http.queue("/sets/me03", kSet);     // third attempt works
    TcgdexClient client(http, test_options(dir));

    CHECK(client.card_id({"POR", 62}) == "me03-062");
    CHECK(http.calls == 3);
}

TEST_CASE("gives up after the maximum number of attempts", "[tcgdex]") {
    TempDir dir;
    FakeHttp http;
    for (int i = 0; i < 3; ++i) {
        http.queue("/sets/me03", "");
    }
    TcgdexClient client(http, test_options(dir));

    CHECK_THROWS(client.card_id({"POR", 62}));
    CHECK(http.calls == 3);
}

TEST_CASE("offline mode fails cleanly when a card isn't cached", "[tcgdex]") {
    TempDir dir;
    FakeHttp http;
    auto options = test_options(dir);
    options.offline = true;
    TcgdexClient client(http, options);

    CHECK_THROWS(client.card({"POR", 62}));
    CHECK(http.calls == 0);
}

TEST_CASE("downloads a whole set in pages, keeps only its cards, and caches it", "[tcgdex]") {
    TempDir dir;
    {
        FakeHttp http;
        http.queue_post(R"({"data": {"cards": [{"id": "me03-001", "name": "A"}, {"id": "me03.5-001", "name": "Other set"}]}})");
        http.queue_post(R"({"errors": [{"message": "busy"}]})");
        http.queue_post("!error");
        http.queue_post(R"({"data": {"cards": [{"id": "me03-002", "name": "B"}]}})");
        auto options = test_options(dir);
        options.page_size = 2;
        TcgdexClient client(http, options);
        const auto cards = client.set_cards("POR");
        REQUIRE(cards.size() == 2);
        CHECK(cards[0].at("name") == "A");
        CHECK(cards[1].at("name") == "B");
        CHECK(http.calls == 4);
        CHECK(http.bodies[0].find("page: 1") != std::string::npos);
        CHECK(http.bodies[0].find("me03-") != std::string::npos);
        CHECK(http.bodies[1].find("page: 2") != std::string::npos);
    }
    {
        FakeHttp http;
        auto options = test_options(dir);
        options.offline = true;
        TcgdexClient client(http, options);
        CHECK(client.set_cards("POR").size() == 2);
        CHECK(http.calls == 0);
        CHECK(client.set_codes() == std::vector<std::string>{"POR"});
    }
}

TEST_CASE("a whole set fails cleanly offline or after too many attempts", "[tcgdex]") {
    TempDir dir;
    FakeHttp http;
    auto offline = test_options(dir);
    offline.offline = true;
    CHECK_THROWS(TcgdexClient(http, offline).set_cards("POR"));
    for (int i = 0; i < 3; ++i) {
        http.queue_post(R"({"data": null})");
    }
    TcgdexClient client(http, test_options(dir));
    CHECK_THROWS(client.set_cards("POR"));
    CHECK(http.calls == 3);
}
