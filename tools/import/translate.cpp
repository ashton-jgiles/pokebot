#include "translate.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <exception>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace pokebot::import {
    namespace {

        using nlohmann::json;

        constexpr std::array<std::string_view, 4> kReminders = {
            "(Don't apply Weakness and Resistance for Benched Pokémon.)",
            "(Your opponent chooses the new Active Pokémon.)",
            "(Pokémon ex, Pokémon V, etc. have Rule Boxes.)",
            "(Apply Weakness as \xC3\x97"
            "2.)",
        };

        constexpr std::array<std::pair<std::string_view, std::string_view>, 11> kTypes = {{
            {"{G}", "grass"},
            {"{R}", "fire"},
            {"{W}", "water"},
            {"{L}", "lightning"},
            {"{P}", "psychic"},
            {"{F}", "fighting"},
            {"{D}", "darkness"},
            {"{M}", "metal"},
            {"{Y}", "fairy"},
            {"{N}", "dragon"},
            {"{C}", "colorless"},
        }};

        constexpr std::array<std::pair<std::string_view, std::string_view>, 5> kConditions = {{
            {"Asleep", "asleep"},
            {"Burned", "burned"},
            {"Confused", "confused"},
            {"Paralyzed", "paralyzed"},
            {"Poisoned", "poisoned"},
        }};

        constexpr int kAll = 60;

        bool same_word(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }

        std::vector<std::string> split_words(std::string_view text) {
            std::vector<std::string> out;
            std::size_t at = 0;
            while (at < text.size()) {
                while (at < text.size() && text[at] == ' ') {
                    ++at;
                }
                const std::size_t end = std::min(text.find(' ', at), text.size());
                if (end > at) {
                    out.emplace_back(text.substr(at, end - at));
                }
                at = end;
            }
            return out;
        }

        std::vector<std::string> tokenize(std::string_view sentence) {
            std::vector<std::string> tokens;
            for (std::string word : split_words(sentence)) {
                while (!word.empty() && word.front() == '(') {
                    tokens.emplace_back("(");
                    word.erase(0, 1);
                }
                std::vector<std::string> tail;
                while (!word.empty() && (word.back() == ',' || word.back() == ')')) {
                    tail.insert(tail.begin(), std::string(1, word.back()));
                    word.pop_back();
                }
                if (!word.empty()) {
                    tokens.push_back(word);
                }
                tokens.insert(tokens.end(), tail.begin(), tail.end());
            }
            return tokens;
        }

        class Words {
        public:
            explicit Words(std::vector<std::string> tokens) : tokens_(std::move(tokens)) {}

            bool done() const { return at_ == tokens_.size(); }
            std::size_t mark() const { return at_; }
            void reset(std::size_t mark) { at_ = mark; }
            const std::vector<std::string>& tokens() const { return tokens_; }

            bool take(std::string_view phrase) {
                const std::size_t start = at_;
                for (const std::string& word : split_words(phrase)) {
                    if (done() || !same_word(tokens_[at_], word)) {
                        at_ = start;
                        return false;
                    }
                    ++at_;
                }
                return true;
            }

            std::optional<int> number() {
                if (done()) {
                    return std::nullopt;
                }
                const std::string& token = tokens_[at_];
                int value = 0;
                const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
                if (ec != std::errc{} || ptr != token.data() + token.size()) {
                    return std::nullopt;
                }
                ++at_;
                return value;
            }

            std::optional<std::string> energy_type() {
                if (done()) {
                    return std::nullopt;
                }
                for (const auto& [symbol, name] : kTypes) {
                    if (tokens_[at_] == symbol) {
                        ++at_;
                        return std::string(name);
                    }
                }
                return std::nullopt;
            }

            std::optional<std::string> owner() {
                if (take("Team Rocket's")) {
                    return std::string("Team Rocket's");
                }
                if (done()) {
                    return std::nullopt;
                }
                const std::string& token = tokens_[at_];
                if (token.size() > 2 && token.ends_with("'s") && std::isupper(static_cast<unsigned char>(token.front())) && token != "Pokémon's") {
                    ++at_;
                    return token;
                }
                return std::nullopt;
            }

            std::string rest() {
                std::string out;
                for (; at_ < tokens_.size(); ++at_) {
                    out += (out.empty() ? "" : " ") + tokens_[at_];
                }
                return out;
            }

            Words replaced(std::string_view from, std::string_view to) const {
                std::vector<std::string> tokens = tokens_;
                for (std::string& token : tokens) {
                    if (same_word(token, from)) {
                        token = std::string(to);
                    }
                }
                return Words(std::move(tokens));
            }

            std::optional<std::string> token() {
                if (done()) {
                    return std::nullopt;
                }
                return tokens_[at_++];
            }

            Words slice(std::size_t from, std::size_t to) const {
                return Words(std::vector<std::string>(tokens_.begin() + static_cast<std::ptrdiff_t>(from), tokens_.begin() + static_cast<std::ptrdiff_t>(to)));
            }

        private:
            std::vector<std::string> tokens_;
            std::size_t at_ = 0;
        };

        struct Quantity {
            int count = 1;
            bool up_to = false;
            bool all = false;
        };

        std::optional<Quantity> quantity(Words& w) {
            const std::size_t start = w.mark();
            if (w.take("up to")) {
                if (const auto n = w.number(); n && *n >= 1 && *n <= kAll) {
                    return Quantity{*n, true, false};
                }
                w.reset(start);
                return std::nullopt;
            }
            if (w.take("any number of") || w.take("any amount of")) {
                return Quantity{kAll, true, false};
            }
            if (w.take("all")) {
                return Quantity{kAll, false, true};
            }
            if (w.take("a") || w.take("an")) {
                return Quantity{1, false, false};
            }
            if (const auto n = w.number(); n && *n >= 1 && *n <= kAll) {
                return Quantity{*n, false, false};
            }
            w.reset(start);
            return std::nullopt;
        }

        bool card_word(Words& w) {
            return w.take("cards") || w.take("card");
        }

        std::optional<json> card_noun(Words& w) {
            const std::size_t start = w.mark();
            const auto fail = [&]() -> std::optional<json> {
                w.reset(start);
                return std::nullopt;
            };
            json filter = json::object();
            for (const auto& [words, kind] : {std::pair{"Pokémon Tool", "tool"}, std::pair{"Item", "item"}, std::pair{"Supporter", "supporter"}, std::pair{"Stadium", "stadium"}}) {
                if (w.take(words)) {
                    if (!card_word(w)) {
                        return fail();
                    }
                    filter["trainer"] = kind;
                    return filter;
                }
            }
            bool basic = false;
            bool special = false;
            bool tera = false;
            std::string stage;
            std::optional<std::string> type;
            std::optional<std::string> not_type;
            std::string prefix;
            for (;;) {
                if (!w.done() && w.tokens()[w.mark()].starts_with("non-{")) {
                    Words symbol({w.tokens()[w.mark()].substr(4)});
                    if (const auto t = symbol.energy_type(); t && symbol.done()) {
                        not_type = *t;
                        w.reset(w.mark() + 1);
                        continue;
                    }
                }
                if (w.take("Basic")) {
                    basic = true;
                } else if (w.take("Special")) {
                    special = true;
                } else if (w.take("Tera")) {
                    tera = true;
                } else if (w.take("Stage 1")) {
                    stage = "stage1";
                } else if (w.take("Stage 2")) {
                    stage = "stage2";
                } else if (w.take("Evolution")) {
                    stage = "evolution";
                } else if (const auto t = w.energy_type()) {
                    type = *t;
                } else if (const auto p = w.owner()) {
                    prefix = *p;
                } else {
                    break;
                }
            }
            if (w.take("Energy")) {
                card_word(w);
                if (!stage.empty() || !prefix.empty() || not_type || tera || (basic && special)) {
                    return fail();
                }
                filter["kind"] = "energy";
                if (basic) {
                    filter["basic"] = true;
                }
                if (special) {
                    filter["special"] = true;
                }
                if (type) {
                    filter["type"] = *type;
                }
                return filter;
            }
            if (w.take("Pokémon")) {
                if (special) {
                    return fail();
                }
                filter["kind"] = "pokemon";
                if (tera) {
                    filter["tera"] = true;
                }
                if (basic) {
                    if (!stage.empty()) {
                        return fail();
                    }
                    stage = "basic";
                }
                if (!stage.empty()) {
                    filter["stage"] = stage;
                }
                if (type) {
                    filter["type"] = *type;
                }
                if (not_type) {
                    filter["not_type"] = *not_type;
                }
                if (!prefix.empty()) {
                    filter["name_prefix"] = prefix;
                }
                if (w.take("ex")) {
                    filter["rule_box"] = true;
                }
                if (const std::size_t before_with = w.mark(); w.take("with")) {
                    const auto hp = w.number();
                    if (hp && w.take("HP or less")) {
                        filter["max_hp"] = *hp;
                    } else {
                        w.reset(before_with);
                    }
                }
                if (w.take("that doesn't have a Rule Box") || w.take("that don't have a Rule Box")) {
                    if (filter.contains("rule_box")) {
                        return fail();
                    }
                    filter["rule_box"] = false;
                }
                return filter;
            }
            if (basic || special || tera || !stage.empty() || type || not_type || !prefix.empty()) {
                return fail();
            }
            if (w.take("Trainer")) {
                if (!card_word(w)) {
                    return fail();
                }
                filter["kind"] = "trainer";
                return filter;
            }
            if (card_word(w)) {
                return filter;
            }
            return fail();
        }

        json without_kind(json filter) {
            filter.erase("kind");
            return filter;
        }

        void set_filter(json& block, const json& filter) {
            if (!filter.empty()) {
                block["filter"] = filter;
            }
        }

        std::optional<json> energy_noun(Words& w, bool attached) {
            const std::size_t start = w.mark();
            const auto noun = card_noun(w);
            if (!noun || noun->value("kind", "") != "energy") {
                w.reset(start);
                return std::nullopt;
            }
            return attached ? without_kind(*noun) : *noun;
        }

        std::optional<json> count_phrase(Words& w, bool on_target) {
            const std::size_t start = w.mark();
            if (w.take("heads")) {
                return json{{"of", "heads"}};
            }
            if (w.take("damage counter on this Pokémon")) {
                return json{{"of", "damage_counters"}};
            }
            if (on_target && w.take("damage counter on that Pokémon")) {
                return json{{"of", "target_damage_counters"}};
            }
            if (w.take("card in your opponent's hand")) {
                return json{{"of", "hand"}, {"player", "opponent"}};
            }
            if (w.take("card in your hand")) {
                return json{{"of", "hand"}};
            }
            if (w.take("Prize card your opponent has taken")) {
                return json{{"of", "prizes_taken"}, {"player", "opponent"}};
            }
            if (w.take("Prize card you have taken")) {
                return json{{"of", "prizes_taken"}};
            }
            if (w.take("of your opponent's Benched Pokémon")) {
                return json{{"of", "benched"}, {"player", "opponent"}};
            }
            if (w.take("of your Benched Pokémon")) {
                return json{{"of", "benched"}};
            }
            if (w.take("Benched Pokémon ( both yours and your opponent's )")) {
                return json{{"of", "benched"}, {"player", "each"}};
            }
            if (w.take("Energy attached to both Active Pokémon")) {
                return json{{"of", "active_energy"}, {"player", "each"}};
            }
            if (w.take("Energy attached to your opponent's Active Pokémon")) {
                return json{{"of", "active_energy"}, {"player", "opponent"}};
            }
            if (w.take("Energy attached to this Pokémon")) {
                return json{{"of", "active_energy"}};
            }
            {
                const std::size_t before_energy = w.mark();
                std::optional<std::string> type = w.energy_type();
                if (!type) {
                    for (const auto& [word, name] : {std::pair{"Grass", "grass"}, std::pair{"Fire", "fire"}, std::pair{"Water", "water"}, std::pair{"Lightning", "lightning"},
                                                     std::pair{"Psychic", "psychic"}, std::pair{"Fighting", "fighting"}, std::pair{"Darkness", "darkness"}, std::pair{"Metal", "metal"}}) {
                        if (w.take(word)) {
                            type = name;
                            break;
                        }
                    }
                }
                const bool basic = !type && w.take("Basic");
                if ((type || basic) && w.take("Energy attached to this Pokémon")) {
                    return json{{"of", "self_energy"}, {"filter", type ? json{{"type", *type}} : json{{"basic", true}}}};
                }
                w.reset(before_energy);
            }
            if (w.take("of your Pokémon in play that has the")) {
                std::string name;
                for (;;) {
                    const auto token = w.token();
                    if (!token) {
                        w.reset(start);
                        return std::nullopt;
                    }
                    if (*token == "attack" && w.done() && !name.empty()) {
                        return json{{"of", "in_play"}, {"filter", {{"has_attack", name}}}};
                    }
                    name += (name.empty() ? "" : " ") + *token;
                }
            }
            if (w.take("damage counter on your opponent's Active Pokémon")) {
                return json{{"of", "active_damage_counters"}, {"player", "opponent"}};
            }
            if (w.take("of your Pokémon in play")) {
                return json{{"of", "in_play"}};
            }
            if (w.take("of your opponent's Pokémon in play")) {
                return json{{"of", "in_play"}, {"player", "opponent"}};
            }
            if (w.take("Pokémon Tool attached to all of your Pokémon")) {
                return json{{"of", "tools_in_play"}};
            }
            if (w.take("Energy attached to all of your opponent's Pokémon")) {
                return json{{"of", "energy_in_play"}, {"player", "opponent"}};
            }
            if (w.take("Energy attached to all of your Pokémon")) {
                return json{{"of", "energy_in_play"}};
            }
            if (w.take("card you discarded in this way")) {
                return json{{"of", "moved"}};
            }
            if (const auto noun = card_noun(w)) {
                json count = {{"of", "discard"}};
                set_filter(count, *noun);
                if (w.take("in your opponent's discard pile")) {
                    count["player"] = "opponent";
                    return count;
                }
                if (w.take("in your discard pile")) {
                    return count;
                }
            }
            w.reset(start);
            return std::nullopt;
        }

        std::optional<std::string> special_condition(Words& w);

        std::optional<json> condition_phrase(Words& w) {
            const std::size_t start = w.mark();
            if (w.take("heads") || w.take("either of them is heads")) {
                return json{{"heads_at_least", 1}};
            }
            if (w.take("both of them are heads")) {
                return json{{"heads_at_least", 2}};
            }
            if (w.take("any of your Pokémon were Knocked Out by damage from an attack during your opponent's last turn")) {
                return json{{"knocked_out_by_attack_last_turn", true}};
            }
            if (w.take("any of your Pokémon were Knocked Out during your opponent's last turn")) {
                return json{{"knocked_out_last_turn", true}};
            }
            if (w.take("you attached Energy to a Pokémon in this way") || w.take("they put any cards on the bottom of their deck in this way")) {
                return json{{"moved_at_least", 1}};
            }
            if (w.take("you drew any cards in this way")) {
                return json{{"drew_at_least", 1}};
            }
            if (w.take("you have any Tera Pokémon in play")) {
                return json{{"in_play_has", {{"tera", true}}}};
            }
            if (w.take("a Stadium is in play")) {
                return json{{"stadium_in_play", true}};
            }
            if (w.take("you have more Prize cards remaining than your opponent")) {
                return json{{"more_prizes_than_opponent", true}};
            }
            if (w.take("any of your Pokémon in play are the same type as any of your opponent's Pokémon in play")) {
                return json{{"shares_type", true}};
            }
            if (w.take("your opponent's Active Pokémon is a Pokémon ex or Pokémon V") || w.take("your opponent's Active Pokémon is a Pokémon ex")) {
                return json{{"opponent_active_is", {{"rule_box", true}}}};
            }
            if (w.take("your opponent's Active Pokémon is an Evolution Pokémon")) {
                return json{{"opponent_active_is", {{"stage", "evolution"}}}};
            }
            if (w.take("your opponent's Active Pokémon is a Basic Pokémon")) {
                return json{{"opponent_active_is", {{"stage", "basic"}}}};
            }
            if (w.take("your opponent's Active Pokémon already has any damage counters on it") || w.take("your opponent's Active Pokémon has any damage counters on it")) {
                return json{{"opponent_active_damaged", true}};
            }
            if (w.take("your opponent's Active Pokémon is")) {
                if (const auto special = special_condition(w)) {
                    return json{{"opponent_active_has", *special}};
                }
                if (w.take("a")) {
                    if (const auto type = w.energy_type(); type && w.take("Pokémon")) {
                        return json{{"opponent_active_is", {{"type", *type}}}};
                    }
                }
                w.reset(start);
            }
            if (w.take("this Pokémon has any damage counters on it")) {
                return json{{"self_damaged", true}};
            }
            if (w.take("your Benched Pokémon have any damage counters on them")) {
                return json{{"bench_damaged", true}};
            }
            if (w.take("this Pokémon has no damage counters on it")) {
                return json{{"self_damaged", false}};
            }
            if (w.take("this Pokémon has at least")) {
                if (const auto n = w.number(); n && w.take("extra Energy attached ( in addition to this attack's cost )")) {
                    return json{{"extra_energy_at_least", *n}};
                }
                w.reset(start);
            }
            if (w.take("you have any")) {
                if (const auto noun = card_noun(w); noun && noun->value("kind", "") == "pokemon" && w.take("on your Bench")) {
                    return json{{"bench_has", without_kind(*noun)}};
                }
                w.reset(start);
            }
            {
                std::string name;
                for (int words = 0; words < 4 && !w.done(); ++words) {
                    if (!name.empty() && w.take("is on your Bench")) {
                        return json{{"bench_has", {{"name", name}}}};
                    }
                    const std::string& next = w.tokens()[w.mark()];
                    if (next == "," || !std::isupper(static_cast<unsigned char>(next.front()))) {
                        break;
                    }
                    name += (name.empty() ? "" : " ") + *w.token();
                }
                w.reset(start);
            }
            if (w.take("you discarded a Pokémon Tool in this way")) {
                return json{{"moved_at_least", 1}};
            }
            if (w.take("this Pokémon has a Pokémon Tool attached")) {
                return json{{"self_has_tool", true}};
            }
            if (w.take("your opponent has")) {
                if (const auto n = w.number(); n && w.take("or fewer Prize cards remaining")) {
                    return json{{"opponent_prizes_at_most", *n}};
                }
            }
            w.reset(start);
            if (w.take("you have exactly")) {
                if (const auto n = w.number(); n && (w.take("Prize cards remaining") || w.take("Prize card remaining"))) {
                    return json{{"prizes_exactly", *n}};
                }
            }
            w.reset(start);
            if (w.take("this Pokémon has any")) {
                if (const auto type = w.energy_type(); type && w.take("Energy attached")) {
                    return json{{"self_has_energy", {{"type", *type}}}};
                }
            }
            w.reset(start);
            return std::nullopt;
        }

        std::optional<json> opponent_targets(Words& w) {
            const std::size_t start = w.mark();
            json block = json::object();
            const auto n = w.number();
            if (!n || *n < 1 || *n > 6 || !w.take("of your opponent's")) {
                w.reset(start);
                return std::nullopt;
            }
            if (*n != 1) {
                block["targets"] = *n;
            }
            if (w.take("Benched Pokémon")) {
                block["on"] = "bench";
            } else if (!w.take("Pokémon")) {
                w.reset(start);
                return std::nullopt;
            }
            return block;
        }

        std::optional<json> each_target(Words& w) {
            if (w.take("each of your Benched Pokémon")) {
                return json{{"player", "you"}, {"on", "bench"}, {"each", true}};
            }
            if (w.take("each of your opponent's Benched Pokémon")) {
                return json{{"on", "bench"}, {"each", true}};
            }
            if (w.take("each of your opponent's Pokémon")) {
                return json{{"each", true}};
            }
            return std::nullopt;
        }

        std::optional<std::string> special_condition(Words& w) {
            for (const auto& [word, name] : kConditions) {
                if (w.take(word)) {
                    return std::string(name);
                }
            }
            return std::nullopt;
        }

        bool uses(const json& block, std::string_view op) {
            if (block.is_array()) {
                for (const json& item : block) {
                    if (uses(item, op)) {
                        return true;
                    }
                }
                return false;
            }
            if (!block.is_object()) {
                return false;
            }
            if (block.value("op", "") == op) {
                return true;
            }
            for (const char* list : {"effects", "then", "else"}) {
                if (block.contains(list) && uses(block.at(list), op)) {
                    return true;
                }
            }
            return false;
        }

        json* last_may(json& blocks) {
            if (blocks.empty()) {
                return nullptr;
            }
            json& last = blocks.back();
            if (last.value("op", "") == "may") {
                return &last;
            }
            if (last.value("op", "") == "if" && last.contains("then") && !last.at("then").empty() && last.at("then").back().value("op", "") == "may") {
                return &last.at("then").back();
            }
            return nullptr;
        }

        struct CardList {
            Quantity quantity;
            std::vector<json> filters;
            bool any = false;
        };

        std::optional<CardList> card_list(Words& w) {
            const std::size_t start = w.mark();
            const auto fail = [&]() -> std::optional<CardList> {
                w.reset(start);
                return std::nullopt;
            };
            const auto q = quantity(w);
            if (!q) {
                return fail();
            }
            CardList list{*q, {}, false};
            if (w.take("in any combination of")) {
                const auto first = card_noun(w);
                if (!first) {
                    return fail();
                }
                list.filters.push_back(*first);
                while (w.take("and") || w.take("or")) {
                    const auto next = card_noun(w);
                    if (!next) {
                        return fail();
                    }
                    list.filters.push_back(*next);
                }
                if (list.filters.size() < 2) {
                    return fail();
                }
                list.any = true;
                return list;
            }
            const auto first = card_noun(w);
            if (!first) {
                return fail();
            }
            list.filters.push_back(*first);
            for (;;) {
                const std::size_t at = w.mark();
                if (w.take("or")) {
                    if (!w.take("a")) {
                        w.take("an");
                    }
                    if (const auto next = card_noun(w)) {
                        list.filters.push_back(*next);
                        list.any = true;
                        continue;
                    }
                }
                w.reset(at);
                break;
            }
            if (list.any) {
                return list;
            }
            for (;;) {
                const std::size_t at = w.mark();
                if (w.take(", and") || w.take("and") || w.take(",")) {
                    if (w.take("a") || w.take("an")) {
                        if (const auto next = card_noun(w)) {
                            list.filters.push_back(*next);
                            continue;
                        }
                    }
                }
                w.reset(at);
                break;
            }
            return list;
        }

        json list_filter(const CardList& list) {
            if (list.filters.size() == 1) {
                return list.filters.front();
            }
            json any = json::array();
            for (const json& filter : list.filters) {
                any.push_back(filter);
            }
            return json{{"any_of", any}};
        }

        std::optional<std::pair<json, json>> discard_cost(Words& w) {
            const std::size_t start = w.mark();
            const auto fail = [&]() -> std::optional<std::pair<json, json>> {
                w.reset(start);
                return std::nullopt;
            };
            if (!w.take("discard")) {
                return fail();
            }
            int count = 1;
            if (!w.take("another")) {
                const auto q = quantity(w);
                if (!q || q->up_to || q->all) {
                    return fail();
                }
                count = q->count;
                w.take("other");
            }
            const auto noun = card_noun(w);
            if (!noun || !w.take("from your hand") || (!noun->empty() && count != 1)) {
                return fail();
            }
            json block = {{"op", "discard_from_hand"}, {"count", count}};
            set_filter(block, *noun);
            json requirement = noun->empty() ? json{{"hand_besides", count}} : json{{"hand_has", *noun}};
            return std::pair{requirement, block};
        }

        std::optional<std::vector<std::string>> energy_units(Words& w) {
            const auto token = w.token();
            if (!token) {
                return std::nullopt;
            }
            std::vector<std::string> units;
            for (std::size_t at = 0; at < token->size(); at += 3) {
                Words symbol({token->substr(at, 3)});
                const auto type = symbol.energy_type();
                if (!type) {
                    return std::nullopt;
                }
                units.push_back(*type);
            }
            if (units.empty()) {
                return std::nullopt;
            }
            return units;
        }

        void add_requirement(json& requirements, const json& condition) {
            if (std::find(requirements.begin(), requirements.end(), condition) == requirements.end()) {
                requirements.push_back(condition);
            }
        }

        void drop_marks(json& blocks) {
            for (json& block : blocks) {
                if (!block.is_object()) {
                    continue;
                }
                block.erase("_if_you_do");
                for (const char* list : {"effects", "then", "else"}) {
                    if (block.contains(list)) {
                        drop_marks(block.at(list));
                    }
                }
            }
        }

        void implied_requirements(const json& blocks, json& requirements) {
            for (const json& block : blocks) {
                if (block.value("_if_you_do", false)) {
                    continue;
                }
                const std::string op = block.value("op", "");
                const json filter = block.value("filter", json::object());
                if (op == "may") {
                    implied_requirements(block.at("effects"), requirements);
                } else if (op == "from_discard") {
                    add_requirement(requirements, {{"discard_has", filter}});
                } else if (op == "attach_to" && block.value("from", "discard") == "discard") {
                    add_requirement(requirements, {{"discard_has", filter}});
                    const json target = block.value("target", json::object());
                    if (block.value("on", "any") == "bench") {
                        add_requirement(requirements, target.empty() ? json{{"has_benched", true}} : json{{"bench_has", target}});
                    } else if (!target.empty()) {
                        add_requirement(requirements, {{"in_play_has", target}});
                    }
                } else if (op == "switch_active") {
                    if (block.value("player", "you") == "opponent") {
                        add_requirement(requirements, {{"opponent_has_benched", true}});
                    } else {
                        add_requirement(requirements, filter.empty() ? json{{"has_benched", true}} : json{{"bench_has", filter}});
                    }
                } else if (op == "move_energy") {
                    add_requirement(requirements, {{"your_energy_has", filter}});
                    add_requirement(requirements, {{"has_benched", true}});
                } else if (op == "search_deck" && block.value("to", "hand") == "bench") {
                    add_requirement(requirements, {{"bench_not_full", true}});
                } else if (op == "attach_from_hand") {
                    add_requirement(requirements, {{"hand_has", filter}});
                } else if (op == "discard_stadium") {
                    add_requirement(requirements, {{"stadium_in_play", true}});
                } else if (op == "attach_spread") {
                    const std::string from = block.value("from", "discard");
                    if (from == "discard") {
                        add_requirement(requirements, {{"discard_has", filter}});
                    } else if (from == "hand") {
                        add_requirement(requirements, {{"hand_has", filter}});
                    }
                    const json target = block.value("target", json::object());
                    if (block.value("on", "any") == "bench") {
                        add_requirement(requirements, target.empty() ? json{{"has_benched", true}} : json{{"bench_has", target}});
                    } else if (!target.empty()) {
                        add_requirement(requirements, {{"in_play_has", target}});
                    }
                } else if (op == "heal_to" && block.value("on", "any") == "any") {
                    add_requirement(requirements, {{"has_damaged", true}});
                }
            }
        }

        class AttackReader {
        public:
            explicit AttackReader(const AttackContext& context) : context_(context) {}

            std::optional<json> sentence(Words w) {
                for (const auto& rule : rules()) {
                    w.reset(0);
                    if (auto blocks = rule(w); blocks && w.done()) {
                        return blocks;
                    }
                }
                const auto& tokens = w.tokens();
                for (std::size_t i = 1; i + 1 < tokens.size(); ++i) {
                    if (!same_word(tokens[i], "and")) {
                        continue;
                    }
                    const std::size_t left_end = tokens[i - 1] == "," ? i - 1 : i;
                    auto left = sentence(w.slice(0, left_end));
                    if (!left) {
                        continue;
                    }
                    auto right = sentence(w.slice(i + 1, tokens.size()));
                    if (!right) {
                        continue;
                    }
                    left->insert(left->end(), right->begin(), right->end());
                    return left;
                }
                return std::nullopt;
            }

        private:
            using Rule = std::function<std::optional<json>(Words&)>;

            static json one(json block) {
                return json::array({std::move(block)});
            }

            std::optional<json> rest_as_sentence(Words& w) {
                const std::size_t from = w.mark();
                auto blocks = sentence(w.slice(from, w.tokens().size()));
                if (blocks) {
                    w.reset(w.tokens().size());
                }
                return blocks;
            }

            std::vector<Rule> rules() {
                std::vector<Rule> out;

                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("draw a card")) {
                        return one({{"op", "draw"}, {"count", 1}});
                    }
                    if (w.take("draw")) {
                        if (const auto n = w.number(); n && w.take("cards")) {
                            return one({{"op", "draw"}, {"count", *n}});
                        }
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("discard your hand")) {
                        return one({{"op", "discard_hand"}});
                    }
                    if (w.take("shuffle your hand into your deck")) {
                        return one({{"op", "shuffle_hand_into_deck"}});
                    }
                    if (w.take("shuffle your deck") || w.take("then , shuffle your deck")) {
                        return one({{"op", "shuffle_deck"}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("search your deck for")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto noun = q ? card_noun(w) : std::nullopt;
                    if (!noun || q->all) {
                        return std::nullopt;
                    }
                    json block = {{"op", "search_deck"}, {"count", q->count}, {"shuffle", false}};
                    set_filter(block, *noun);
                    if (w.take(", reveal it , and put it") || w.take(", reveal them , and put them")) {
                        block["reveal"] = true;
                    } else if (!w.take("and put it") && !w.take("and put them")) {
                        return std::nullopt;
                    }
                    if (w.take("onto your Bench")) {
                        block["to"] = "bench";
                    } else if (!w.take("into your hand")) {
                        return std::nullopt;
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("search your deck for")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q && !q->all ? energy_noun(w, false) : std::nullopt;
                    if (!energy || !(w.take("and attach it to") || w.take("and attach them to"))) {
                        return std::nullopt;
                    }
                    json block = {{"op", "attach_to"}, {"from", "deck"}, {"count", q->count}, {"shuffle", false}};
                    if (q->up_to) {
                        block["up_to"] = true;
                    }
                    set_filter(block, *energy);
                    if (!w.take("1 of your")) {
                        return std::nullopt;
                    }
                    if (w.take("Benched")) {
                        block["on"] = "bench";
                    }
                    const auto target = card_noun(w);
                    if (!target || target->value("kind", "") != "pokemon") {
                        return std::nullopt;
                    }
                    if (const json t = without_kind(*target); !t.empty()) {
                        block["target"] = t;
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("attach")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q && !q->all ? energy_noun(w, false) : std::nullopt;
                    if (!energy) {
                        return std::nullopt;
                    }
                    if (w.take("from your hand to this Pokémon")) {
                        if (q->up_to) {
                            return std::nullopt;
                        }
                        json block = {{"op", "attach_from_hand"}, {"count", q->count}};
                        set_filter(block, *energy);
                        return one(block);
                    }
                    if (!w.take("from your discard pile to 1 of your")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "attach_to"}, {"count", q->count}};
                    if (q->up_to) {
                        block["up_to"] = true;
                    }
                    set_filter(block, *energy);
                    if (w.take("Benched")) {
                        block["on"] = "bench";
                    }
                    const auto target = card_noun(w);
                    if (!target || target->value("kind", "") != "pokemon") {
                        return std::nullopt;
                    }
                    if (const json t = without_kind(*target); !t.empty()) {
                        block["target"] = t;
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("discard")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q ? energy_noun(w, true) : std::nullopt;
                    if (!energy) {
                        return std::nullopt;
                    }
                    const bool bench = w.take("from 1 of your opponent's Benched Pokémon");
                    const bool active = !bench && w.take("from your opponent's Active Pokémon");
                    if (bench || active || w.take("from 1 of your opponent's Pokémon")) {
                        if (q->up_to || q->all) {
                            return std::nullopt;
                        }
                        json target = {{"op", "discard_target_energy"}};
                        if (bench || active) {
                            target["on"] = bench ? "bench" : "active";
                        }
                        if (q->count != 1) {
                            target["count"] = q->count;
                        }
                        set_filter(target, *energy);
                        return one(target);
                    }
                    json block = {{"op", "move_attached_energy"}, {"count", q->count}, {"to", "discard"}};
                    if (w.take("from your Pokémon")) {
                        block["from"] = "your_pokemon";
                    } else if (!w.take("from this Pokémon")) {
                        return std::nullopt;
                    } else if (q->all && energy->empty()) {
                        return one({{"op", "discard_all_attached_energy"}});
                    }
                    if (q->up_to) {
                        block["up_to"] = true;
                    }
                    set_filter(block, *energy);
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("put")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q && !q->all ? energy_noun(w, true) : std::nullopt;
                    if (!energy || !w.take("attached to this Pokémon into your hand")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "move_attached_energy"}, {"count", q->count}, {"to", "hand"}};
                    if (q->up_to) {
                        block["up_to"] = true;
                    }
                    set_filter(block, *energy);
                    return one(block);
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("this attack does")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n) {
                        return std::nullopt;
                    }
                    if (w.take("damage for each")) {
                        const auto count = count_phrase(w, false);
                        if (!count || context_.damage_mod != DamageMod::Times) {
                            return std::nullopt;
                        }
                        return one({{"op", "set_damage"}, {"per", *n}, {"for_each", *count}});
                    }
                    if (w.take("more damage for each")) {
                        const auto count = count_phrase(w, false);
                        if (!count || context_.damage_mod != DamageMod::Plus) {
                            return std::nullopt;
                        }
                        return one({{"op", "add_damage"}, {"per", *n}, {"for_each", *count}});
                    }
                    if (w.take("less damage for each")) {
                        const auto count = count_phrase(w, false);
                        if (!count || context_.damage_mod != DamageMod::Minus) {
                            return std::nullopt;
                        }
                        return one({{"op", "add_damage"}, {"per", -*n}, {"for_each", *count}});
                    }
                    if (w.take("more damage")) {
                        if (context_.damage_mod != DamageMod::Plus) {
                            return std::nullopt;
                        }
                        return one({{"op", "add_damage"}, {"amount", *n}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("this attack does") && !w.take("this attack also does")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || !w.take("damage to")) {
                        return std::nullopt;
                    }
                    auto block = w.take("the new Active Pokémon") ? std::optional<json>(json{{"on", "active"}}) : each_target(w);
                    if (!block) {
                        block = opponent_targets(w);
                    }
                    if (!block) {
                        return std::nullopt;
                    }
                    (*block)["op"] = "damage_to";
                    if (w.take("for each")) {
                        const auto count = count_phrase(w, true);
                        if (!count) {
                            return std::nullopt;
                        }
                        (*block)["per"] = *n;
                        (*block)["for_each"] = *count;
                    } else {
                        (*block)["amount"] = *n;
                    }
                    return one(*block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("flip a coin until you get tails")) {
                        return one({{"op", "flip_until_tails"}});
                    }
                    if (w.take("flip a coin for each")) {
                        const auto count = count_phrase(w, false);
                        if (!count) {
                            return std::nullopt;
                        }
                        return one({{"op", "flip_coins"}, {"for_each", *count}});
                    }
                    if (w.take("flip a coin")) {
                        return one({{"op", "flip_coins"}});
                    }
                    if (w.take("flip")) {
                        if (const auto n = w.number(); n && *n >= 2 && w.take("coins")) {
                            return one({{"op", "flip_coins"}, {"count", *n}});
                        }
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    std::string player;
                    if (w.take("your opponent's Active Pokémon is now")) {
                        player = "opponent";
                    } else if (w.take("this Pokémon is now")) {
                        player = "you";
                    } else {
                        return std::nullopt;
                    }
                    std::vector<std::string> conditions;
                    for (;;) {
                        const auto condition = special_condition(w);
                        if (!condition) {
                            return std::nullopt;
                        }
                        conditions.push_back(*condition);
                        if (w.done()) {
                            break;
                        }
                        if (!(w.take(", and") || w.take("and") || w.take(","))) {
                            return std::nullopt;
                        }
                    }
                    json blocks = json::array();
                    for (const std::string& condition : conditions) {
                        json block = {{"op", "special_condition"}, {"condition", condition}};
                        if (player == "you") {
                            block["player"] = "you";
                        }
                        blocks.push_back(block);
                    }
                    return blocks;
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (w.take("during your next turn , this Pokémon can't attack") || w.take("during your next turn , this Pokémon can't use attacks")) {
                        return one({{"op", "lasting"}, {"effect", "cant_attack"}, {"until", "your_next_turn"}});
                    }
                    if (w.take("during your next turn , this Pokémon can't use")) {
                        if (w.rest() == context_.name && !context_.name.empty()) {
                            return one({{"op", "lasting"}, {"effect", "cant_use_attack"}, {"until", "your_next_turn"}});
                        }
                        return std::nullopt;
                    }
                    if (w.take("during your opponent's next turn , the Defending Pokémon can't retreat")) {
                        return one({{"op", "lasting"}, {"effect", "cant_retreat"}, {"on", "defending"}, {"until", "opponents_next_turn"}});
                    }
                    if (w.take("during your opponent's next turn , the Defending Pokémon can't attack")) {
                        return one({{"op", "lasting"}, {"effect", "cant_attack"}, {"on", "defending"}, {"until", "opponents_next_turn"}});
                    }
                    if (w.take("during your opponent's next turn , they can't play any Item cards from their hand")) {
                        return one({{"op", "lasting"}, {"effect", "no_items"}, {"on", "player"}, {"player", "opponent"}, {"until", "opponents_next_turn"}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("switch this Pokémon with 1 of your Benched Pokémon")) {
                        return one({{"op", "switch_self"}});
                    }
                    if (w.take("switch in 1 of your opponent's Benched Pokémon to the Active Spot")) {
                        return one({{"op", "switch_active"}, {"player", "opponent"}});
                    }
                    if (w.take("put this Pokémon and all attached cards into your hand")) {
                        return one({{"op", "return_self_to_hand"}});
                    }
                    if (w.take("shuffle this Pokémon and all attached cards into your deck")) {
                        return one({{"op", "shuffle_self_into_deck"}});
                    }
                    if (w.take("this attack's damage isn't affected by any effects on your opponent's Active Pokémon")) {
                        return one({{"op", "ignore_defender_effects"}});
                    }
                    if (w.take("this attack's damage isn't affected by Weakness or Resistance , or by any effects on your opponent's Active Pokémon")) {
                        return json::array({{{"op", "ignore_weakness"}}, {{"op", "ignore_resistance"}}, {{"op", "ignore_defender_effects"}}});
                    }
                    if (w.take("this attack's damage isn't affected by Weakness or Resistance")) {
                        return json::array({{{"op", "ignore_weakness"}}, {{"op", "ignore_resistance"}}});
                    }
                    if (w.take("this attack's damage isn't affected by Weakness")) {
                        return one({{"op", "ignore_weakness"}});
                    }
                    if (w.take("this attack's damage isn't affected by Resistance")) {
                        return one({{"op", "ignore_resistance"}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("put") && !w.take("place")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || *n > kAll || !(w.take("damage counters on") || w.take("damage counter on"))) {
                        return std::nullopt;
                    }
                    if (w.take("your opponent's Benched Pokémon in any way you like")) {
                        return one({{"op", "place_counters"}, {"count", *n}, {"on", "bench"}});
                    }
                    if (w.take("your opponent's Pokémon in any way you like")) {
                        return one({{"op", "place_counters"}, {"count", *n}});
                    }
                    auto block = each_target(w);
                    if (!block) {
                        block = opponent_targets(w);
                    }
                    if (!block) {
                        return std::nullopt;
                    }
                    (*block)["op"] = "counters_to";
                    (*block)["count"] = *n;
                    return one(*block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("choose 1 of your Benched")) {
                        return std::nullopt;
                    }
                    const auto prefix = w.owner();
                    if (!w.take("Pokémon's attacks and use it as this attack")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "use_attack"}};
                    if (prefix) {
                        block["filter"] = {{"name_prefix", *prefix}};
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("heal")) {
                        return std::nullopt;
                    }
                    json block = json::object();
                    if (w.take("all damage")) {
                        block["all"] = true;
                    } else {
                        const auto n = w.number();
                        if (!n || *n <= 0 || *n % 10 != 0 || *n > 990 || !w.take("damage")) {
                            return std::nullopt;
                        }
                        block["amount"] = *n;
                    }
                    if (!w.take("from")) {
                        return std::nullopt;
                    }
                    if (w.take("this Pokémon")) {
                        block["op"] = "heal_self";
                        return one(block);
                    }
                    if (w.take("your Active Pokémon")) {
                        block["op"] = "heal_to";
                        block["on"] = "active";
                        return one(block);
                    }
                    block["op"] = "heal_to";
                    if (w.take("each of your")) {
                        block["each"] = true;
                    } else if (!w.take("1 of your")) {
                        return std::nullopt;
                    }
                    if (w.take("Benched")) {
                        block["on"] = "bench";
                    }
                    if (!w.take("Pokémon")) {
                        return std::nullopt;
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("this Pokémon also does") && !w.take("this Pokémon does")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || *n <= 0 || *n % 10 != 0 || *n > 600 || !w.take("damage to itself")) {
                        return std::nullopt;
                    }
                    return one({{"op", "damage_self"}, {"amount", *n}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    const auto lasting = [](const char* effect, const char* until, const char* on) {
                        json block = {{"op", "lasting"}, {"effect", effect}, {"until", until}};
                        if (std::string_view(on) != "self") {
                            block["on"] = on;
                        }
                        return block;
                    };
                    if (w.take("during your opponent's next turn , this Pokémon takes")) {
                        const auto n = w.number();
                        if (!n || !w.take("less damage from attacks ( after applying Weakness and Resistance )")) {
                            return std::nullopt;
                        }
                        json block = lasting("takes_less", "opponents_next_turn", "self");
                        block["amount"] = *n;
                        return one(block);
                    }
                    if (w.take("during your opponent's next turn , prevent all damage from and effects of attacks done to this Pokémon")) {
                        json block = lasting("prevent_damage", "opponents_next_turn", "self");
                        block["prevent_effects"] = true;
                        return one(block);
                    }
                    if (w.take("during your opponent's next turn , prevent all damage done to this Pokémon by attacks")) {
                        json block = lasting("prevent_damage", "opponents_next_turn", "self");
                        if (w.take("from Basic Pokémon")) {
                            block["from_basic"] = true;
                        } else if (w.take("if that damage is")) {
                            const auto n = w.number();
                            if (!n || !w.take("or less")) {
                                return std::nullopt;
                            }
                            block["amount"] = *n;
                        }
                        return one(block);
                    }
                    if (w.take("during your opponent's next turn , attacks used by the Defending Pokémon do")) {
                        const auto n = w.number();
                        if (!n || !w.take("less damage ( before applying Weakness and Resistance )")) {
                            return std::nullopt;
                        }
                        json block = lasting("deals_less", "opponents_next_turn", "defending");
                        block["amount"] = *n;
                        return one(block);
                    }
                    if (w.take("during your next turn , the Defending Pokémon takes")) {
                        const auto n = w.number();
                        if (!n || !w.take("more damage from attacks ( after applying Weakness and Resistance )")) {
                            return std::nullopt;
                        }
                        json block = lasting("takes_more", "your_next_turn", "defending");
                        block["amount"] = *n;
                        return one(block);
                    }
                    if (w.take("during your next turn , attacks used by this Pokémon do")) {
                        const auto n = w.number();
                        if (!n || !w.take("more damage to your opponent's Active Pokémon ( before applying Weakness and Resistance )")) {
                            return std::nullopt;
                        }
                        json block = lasting("damage_bonus", "your_next_turn", "self");
                        block["amount"] = *n;
                        return one(block);
                    }
                    if (w.take("during your opponent's next turn , the Defending Pokémon can't use attacks")) {
                        return one(lasting("cant_attack", "opponents_next_turn", "defending"));
                    }
                    if (w.take("during your opponent's next turn , if the Defending Pokémon tries to use an attack , your opponent flips a coin")) {
                        return one(lasting("attack_flip", "opponents_next_turn", "defending"));
                    }
                    if (w.take("if tails , that attack doesn't happen")) {
                        return one({{"op", "_attack_fails"}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    const auto passive = [](json modifier) {
                        return json{{"op", "_passive"}, {"modifier", std::move(modifier)}};
                    };
                    if (w.take("this Pokémon takes") || w.take("the Pokémon this card is attached to takes")) {
                        const auto n = w.number();
                        if (!n || !w.take("less damage from attacks ( after applying Weakness and Resistance )")) {
                            return std::nullopt;
                        }
                        return one(passive({{"modify", "damage_taken"}, {"add", -*n}}));
                    }
                    if (w.take("prevent all damage done to this Pokémon by attacks from your opponent's Pokémon ex")) {
                        return one(passive({{"modify", "prevent_damage"}, {"from", {{"rule_box", true}}}}));
                    }
                    if (w.take("as long as this Pokémon is on your Bench , prevent all damage from and effects of attacks from your opponent's Pokémon done to this Pokémon")) {
                        return json::array({passive({{"modify", "prevent_damage"}, {"if_benched", true}}), passive({{"modify", "prevent_effects"}, {"if_benched", true}})});
                    }
                    if (w.take("as long as this Pokémon is in the Active Spot , attacks used by your opponent's Active Pokémon do")) {
                        const auto n = w.number();
                        if (!n || !w.take("less damage ( before applying Weakness and Resistance )")) {
                            return std::nullopt;
                        }
                        return one(passive({{"modify", "damage_dealt"}, {"add", -*n}, {"applies_to", "opponents"}, {"if_active", true}}));
                    }
                    if (w.take("prevent all effects of attacks used by your opponent's Pokémon done to this Pokémon")) {
                        return one(passive({{"modify", "prevent_effects"}}));
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("switch out your opponent's Active Pokémon to the Bench")) {
                        return one({{"op", "switch_active"}, {"player", "opponent"}, {"chooser", "opponent"}});
                    }
                    if (w.take("draw cards until you have")) {
                        const auto n = w.number();
                        if (!n || *n < 1 || *n > kAll || !w.take("cards in your hand")) {
                            return std::nullopt;
                        }
                        return one({{"op", "draw"}, {"until", *n}});
                    }
                    if (w.take("before doing damage , discard all Pokémon Tools from your opponent's Active Pokémon")) {
                        return json::array({{{"op", "discard_tools"}}, {{"op", "damage"}}});
                    }
                    if (w.take("during your next turn , this Pokémon can't retreat")) {
                        return one({{"op", "lasting"}, {"effect", "cant_retreat"}, {"until", "your_next_turn"}});
                    }
                    const bool each_heads = w.take("for each heads ,");
                    if (each_heads && w.take("discard an Energy from your opponent's Active Pokémon")) {
                        return one({{"op", "discard_target_energy"}, {"on", "active"}, {"for_each", {{"of", "heads"}}}});
                    }
                    if (w.take("discard a random card from your opponent's hand")) {
                        json block = {{"op", "discard_random"}};
                        if (each_heads) {
                            block["for_each"] = {{"of", "heads"}};
                        }
                        return one(block);
                    }
                    if (!w.take("discard the top")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "discard_deck_top"}};
                    if (!w.take("card")) {
                        const auto n = w.number();
                        if (!n || *n < 2 || *n > kAll || !w.take("cards") || each_heads) {
                            return std::nullopt;
                        }
                        block["count"] = *n;
                    }
                    if (w.take("of your opponent's deck")) {
                        block["player"] = "opponent";
                    } else if (!w.take("of your deck")) {
                        return std::nullopt;
                    }
                    if (each_heads) {
                        block["for_each"] = {{"of", "heads"}};
                    }
                    return one(block);
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (w.take("you may do")) {
                        const auto n = w.number();
                        if (!n || !w.take("more damage") || context_.damage_mod != DamageMod::Plus) {
                            return std::nullopt;
                        }
                        return one({{"op", "may"}, {"effects", json::array({{{"op", "add_damage"}, {"amount", *n}}})}});
                    }
                    if (w.take("discard a card from your hand")) {
                        return one({{"op", "discard_from_hand"}});
                    }
                    if (w.take("if you go first , you can use this attack during your first turn") || w.take("if you go first , you may use this card during your first turn")) {
                        return one({{"op", "_first_turn"}});
                    }
                    json block = {{"op", "attach_spread"}};
                    const bool deck = w.take("search your deck for");
                    if (!deck && !w.take("attach")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q && !q->all && q->count <= 6 ? energy_noun(w, false) : std::nullopt;
                    if (!energy) {
                        return std::nullopt;
                    }
                    if (deck) {
                        if (!w.take("and attach them to your") && !w.take("and attach it to your")) {
                            return std::nullopt;
                        }
                        block["from"] = "deck";
                        block["shuffle"] = false;
                    } else if (w.take("from your discard pile to your")) {
                        block["from"] = "discard";
                    } else if (w.take("from your hand to your")) {
                        block["from"] = "hand";
                    } else {
                        return std::nullopt;
                    }
                    if (w.take("Benched")) {
                        block["on"] = "bench";
                    }
                    const auto target = card_noun(w);
                    if (!target || target->value("kind", "") != "pokemon" || !w.take("in any way you like")) {
                        return std::nullopt;
                    }
                    if (const json t = without_kind(*target); !t.empty()) {
                        block["target"] = t;
                    }
                    block["count"] = q->count;
                    if (q->up_to) {
                        block["up_to"] = true;
                    }
                    set_filter(block, *energy);
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("if this Pokémon is in the Active Spot and is damaged by an attack from your opponent's Pokémon ( even if this Pokémon is Knocked Out ) ,")) {
                        return std::nullopt;
                    }
                    json effect;
                    if (w.take("the Attacking Pokémon is now")) {
                        const auto condition = special_condition(w);
                        if (!condition) {
                            return std::nullopt;
                        }
                        effect = {{"op", "special_condition"}, {"condition", *condition}};
                    } else {
                        if (!w.take("put") && !w.take("place")) {
                            return std::nullopt;
                        }
                        const auto n = w.number();
                        if (!n || *n < 1 || *n > kAll || !(w.take("damage counters on the Attacking Pokémon") || w.take("damage counter on the Attacking Pokémon"))) {
                            return std::nullopt;
                        }
                        effect = {{"op", "counters_to"}, {"count", *n}, {"on", "active"}};
                    }
                    return json::array({{{"op", "_ability"}, {"trigger", "damaged_by_attack"}, {"once_per_turn", false}}, effect});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("if tails , this attack does nothing")) {
                        return one({{"op", "_does_nothing"}, {"on", "tails"}});
                    }
                    if (w.take("if either of them is tails , this attack does nothing") || w.take("if any of them are tails , this attack does nothing")) {
                        return one({{"op", "_does_nothing"}, {"on", "any"}});
                    }
                    if (w.take("if both of them are tails , this attack does nothing")) {
                        return one({{"op", "_does_nothing"}, {"on", "all"}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("if")) {
                        return std::nullopt;
                    }
                    const bool tails = w.take("tails");
                    const auto condition = tails ? std::optional<json>(json{{"heads_at_least", 1}}) : condition_phrase(w);
                    if (!condition || !w.take(",")) {
                        return std::nullopt;
                    }
                    const auto then = rest_as_sentence(w);
                    if (!then) {
                        return std::nullopt;
                    }
                    if (tails) {
                        return one({{"op", "if"}, {"condition", *condition}, {"then", json::array()}, {"else", *then}});
                    }
                    return one({{"op", "if"}, {"condition", *condition}, {"then", *then}});
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("then ,")) {
                        return std::nullopt;
                    }
                    return rest_as_sentence(w);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("you may shuffle")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || !w.take("Energy attached to this Pokémon into your deck")) {
                        return std::nullopt;
                    }
                    const json may = {{"op", "may"}, {"effects", {{{"op", "move_attached_energy"}, {"count", *n}, {"to", "deck"}}, {{"op", "shuffle_deck"}}}}};
                    return one({{"op", "if"}, {"condition", {{"self_energy_at_least", *n}}}, {"then", json::array({may})}});
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("you may")) {
                        return std::nullopt;
                    }
                    const auto& tokens = w.tokens();
                    const std::size_t from = w.mark();
                    for (std::size_t i = from; i + 6 < tokens.size() + 1; ++i) {
                        Words tail = w.slice(i, tokens.size());
                        if (!tail.take("and have this attack do")) {
                            continue;
                        }
                        const auto n = tail.number();
                        if (!n || !tail.take("more damage") || !tail.done() || context_.damage_mod != DamageMod::Plus) {
                            continue;
                        }
                        auto cost = sentence(w.slice(from, i));
                        if (!cost) {
                            continue;
                        }
                        w.reset(tokens.size());
                        return json::array({{{"op", "may"}, {"effects", *cost}},
                                            {{"op", "if"}, {"condition", {{"moved_at_least", 1}}}, {"then", json::array({{{"op", "add_damage"}, {"amount", *n}}})}}});
                    }
                    auto body = rest_as_sentence(w);
                    if (!body) {
                        return std::nullopt;
                    }
                    if (body->size() == 1 && body->at(0).value("up_to", false)) {
                        return body;
                    }
                    return one({{"op", "may"}, {"effects", *body}});
                });
                add_card_rules(out);
                return out;
            }

            void add_card_rules(std::vector<Rule>& out) {
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("you draw a card")) {
                        return one({{"op", "draw"}, {"count", 1}});
                    }
                    std::string player;
                    if (w.take("you draw")) {
                        player = "you";
                    } else if (w.take("your opponent draws") || w.take("they draw")) {
                        player = "opponent";
                    } else if (w.take("each player draws")) {
                        player = "each";
                    } else {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || !(w.take("cards") || (*n == 1 && w.take("card")))) {
                        return std::nullopt;
                    }
                    json block = {{"op", "draw"}, {"count", *n}};
                    if (player != "you") {
                        block["player"] = player;
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("each player shuffles their hand into their deck")) {
                        return std::nullopt;
                    }
                    json blocks = one({{"op", "shuffle_hand_into_deck"}, {"player", "each"}});
                    if (w.take("and draws")) {
                        const auto n = w.number();
                        if (!n || !w.take("cards")) {
                            return std::nullopt;
                        }
                        blocks.push_back({{"op", "draw"}, {"count", *n}, {"player", "each"}});
                    }
                    return blocks;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("your opponent shuffles their hand and puts it on the bottom of their deck")) {
                        return one({{"op", "hand_to_deck_bottom"}, {"player", "opponent"}});
                    }
                    if (w.take("switch your Active Pokémon with 1 of your Benched Pokémon")) {
                        return one({{"op", "switch_active"}});
                    }
                    if (w.take("switch it with your Active Pokémon")) {
                        return one({{"op", "switch_to_active"}});
                    }
                    if (w.take("discard a Stadium in play")) {
                        return one({{"op", "discard_stadium"}});
                    }
                    if (w.take("during your opponent's next turn , that Pokémon can't retreat")) {
                        return one({{"op", "lasting"}, {"effect", "cant_retreat"}, {"on", "defending"}, {"until", "opponents_next_turn"}});
                    }
                    if (w.take("move any amount of Energy from your other Pokémon to this Pokémon")) {
                        return one({{"op", "move_attached_energy"}, {"from", "your_pokemon"}, {"count", kAll}, {"up_to", true}, {"to", "self"}});
                    }
                    if (w.take("put the other card on the bottom of your deck") || w.take("put the other cards on the bottom of your deck") ||
                        w.take("put the other cards on the bottom of your deck in any order")) {
                        return one({{"op", "_rest"}});
                    }
                    return std::nullopt;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("the new Active Pokémon is now")) {
                        return std::nullopt;
                    }
                    const auto condition = special_condition(w);
                    if (!condition) {
                        return std::nullopt;
                    }
                    return one({{"op", "special_condition"}, {"condition", *condition}, {"player", "you"}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("switch")) {
                        return std::nullopt;
                    }
                    if (w.take("your Active")) {
                        const auto active = card_noun(w);
                        if (!active || active->value("kind", "") != "pokemon" || !w.take("with 1 of your Benched")) {
                            return std::nullopt;
                        }
                        const auto benched = card_noun(w);
                        if (!benched || *benched != *active) {
                            return std::nullopt;
                        }
                        return json::array({{{"op", "_requires"}, {"condition", {{"active_is", *active}}}}, {{"op", "switch_active"}, {"filter", *active}}});
                    }
                    if (!w.take("1 of your Benched")) {
                        return std::nullopt;
                    }
                    auto benched = card_noun(w);
                    if (!benched || benched->value("kind", "") != "pokemon") {
                        return std::nullopt;
                    }
                    if (w.take(", except any")) {
                        std::string name;
                        while (!w.done() && w.tokens()[w.mark()] != ",") {
                            name += (name.empty() ? "" : " ") + *w.token();
                        }
                        if (name.empty() || !w.take(",")) {
                            return std::nullopt;
                        }
                        (*benched)["not_name"] = name;
                    }
                    if (!w.take("with your Active Pokémon")) {
                        return std::nullopt;
                    }
                    return one({{"op", "switch_active"}, {"filter", *benched}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("put")) {
                        return std::nullopt;
                    }
                    const auto list = card_list(w);
                    if (!list || list->quantity.all || (list->filters.size() > 1 && !list->any) || !w.take("from your discard pile into your hand")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "from_discard"}, {"count", list->quantity.count}};
                    if (list->quantity.up_to) {
                        block["up_to"] = true;
                    }
                    set_filter(block, list_filter(*list));
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("move")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q && q->count == 1 && !q->up_to ? energy_noun(w, false) : std::nullopt;
                    if (!energy || !w.take("from 1 of your Pokémon to another of your Pokémon")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "move_energy"}};
                    set_filter(block, *energy);
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("search your deck for")) {
                        return std::nullopt;
                    }
                    const std::size_t start = w.mark();
                    if (const auto n = w.number(); n && w.take("cards , shuffle your deck , then put those cards on top of it in any order")) {
                        return one({{"op", "search_to_top"}, {"count", *n}});
                    }
                    w.reset(start);
                    const auto list = card_list(w);
                    if (!list || list->filters.size() < 2 || list->quantity.all) {
                        return std::nullopt;
                    }
                    bool reveal = false;
                    if (w.take(", reveal it , and put it") || w.take(", reveal them , and put them")) {
                        reveal = true;
                    } else if (!w.take("and put it") && !w.take("and put them")) {
                        return std::nullopt;
                    }
                    if (!w.take("into your hand")) {
                        return std::nullopt;
                    }
                    json blocks = json::array();
                    if (list->any) {
                        json block = {{"op", "search_deck"}, {"count", list->quantity.count}, {"shuffle", false}, {"filter", list_filter(*list)}};
                        if (reveal) {
                            block["reveal"] = true;
                        }
                        blocks.push_back(block);
                        return blocks;
                    }
                    if (list->quantity.count != 1 || list->quantity.up_to) {
                        return std::nullopt;
                    }
                    for (const json& filter : list->filters) {
                        json block = {{"op", "search_deck"}, {"count", 1}, {"shuffle", false}};
                        set_filter(block, filter);
                        if (reveal) {
                            block["reveal"] = true;
                        }
                        blocks.push_back(block);
                    }
                    return blocks;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("look at the top")) {
                        return std::nullopt;
                    }
                    const auto look = w.number();
                    if (!look || !w.take("cards of your deck and put")) {
                        return std::nullopt;
                    }
                    const auto take = w.number();
                    if (!take || *take >= *look || !w.take("of them into your hand")) {
                        return std::nullopt;
                    }
                    return one({{"op", "look_at_top"}, {"count", *look}, {"take", *take}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("during this turn , attacks used by your Pokémon do")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || !w.take("more damage to your opponent's Active Pokémon")) {
                        return std::nullopt;
                    }
                    const bool ex = w.take("ex");
                    if (!w.take("( before applying Weakness and Resistance )")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "lasting"}, {"effect", "damage_bonus"}, {"on", "player"}, {"amount", *n}, {"until", "this_turn"}};
                    if (ex) {
                        block["only_ex"] = true;
                    }
                    return one(block);
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("choose up to")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || *n < 1 || *n > 6 || !w.take("of your")) {
                        return std::nullopt;
                    }
                    const bool bench = w.take("Benched");
                    const auto target = card_noun(w);
                    if (!target || target->value("kind", "") != "pokemon" || !w.take("and attach")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto energy = q && !q->up_to && !q->all ? energy_noun(w, false) : std::nullopt;
                    if (!energy || !w.take("from your discard pile to each of them")) {
                        return std::nullopt;
                    }
                    json block = {{"op", "attach_to"}, {"targets", *n}, {"up_to_targets", true}, {"count", q->count}};
                    if (bench) {
                        block["on"] = "bench";
                    }
                    if (const json t = without_kind(*target); !t.empty()) {
                        block["target"] = t;
                    }
                    set_filter(block, *energy);
                    return one(block);
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("you can use this card only if")) {
                        return std::nullopt;
                    }
                    const std::size_t at = w.mark();
                    if (w.take("you")) {
                        if (const auto cost = discard_cost(w)) {
                            return json::array({{{"op", "_requires"}, {"condition", cost->first}}, cost->second});
                        }
                    }
                    w.reset(at);
                    const auto condition = condition_phrase(w);
                    if (!condition) {
                        return std::nullopt;
                    }
                    return one({{"op", "_requires"}, {"condition", *condition}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("you must")) {
                        return std::nullopt;
                    }
                    const auto cost = discard_cost(w);
                    if (!cost || !w.take("in order to use this Ability")) {
                        return std::nullopt;
                    }
                    return json::array({{{"op", "_requires"}, {"condition", cost->first}}, cost->second});
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (w.take("as often as you like during your turn , you may use this Ability")) {
                        return one({{"op", "_ability"}, {"once_per_turn", false}});
                    }
                    if (!w.take("once during your turn ,")) {
                        return std::nullopt;
                    }
                    json flags = {{"op", "_ability"}};
                    if (w.take("when you play this Pokémon from your hand onto your Bench ,")) {
                        flags["trigger"] = "played_to_bench";
                    } else if (w.take("when you play this Pokémon from your hand to evolve 1 of your Pokémon ,")) {
                        flags["trigger"] = "evolved";
                    } else if (w.take("if this Pokémon is in the Active Spot ,")) {
                        flags["active_only"] = true;
                    } else if (w.take("if")) {
                        const auto condition = condition_phrase(w);
                        if (!condition || !w.take(",")) {
                            return std::nullopt;
                        }
                        flags["requires"] = json::array({*condition});
                    }
                    if (!w.take("you may")) {
                        return std::nullopt;
                    }
                    if (w.take("use this Ability")) {
                        if (flags.contains("trigger")) {
                            flags["wrap"] = true;
                        }
                        return one(flags);
                    }
                    if (flags.contains("trigger")) {
                        return std::nullopt;
                    }
                    const std::size_t at = w.mark();
                    if (const auto cost = discard_cost(w); cost && w.take("in order to use this Ability")) {
                        return json::array({flags, {{"op", "_requires"}, {"condition", cost->first}}, cost->second});
                    }
                    w.reset(at);
                    auto body = rest_as_sentence(w);
                    if (!body) {
                        return std::nullopt;
                    }
                    body->insert(body->begin(), flags);
                    return body;
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    std::string trigger = "played_to_bench";
                    if (w.take("when you play this Pokémon from your hand to evolve 1 of your Pokémon during your turn , you may")) {
                        trigger = "evolved";
                    } else if (!w.take("when you play this Pokémon from your hand onto your Bench during your turn , you may")) {
                        return std::nullopt;
                    }
                    auto body = rest_as_sentence(w);
                    if (!body) {
                        return std::nullopt;
                    }
                    return json::array({{{"op", "_ability"}, {"trigger", trigger}}, {{"op", "may"}, {"effects", *body}}});
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("you can't use more than 1")) {
                        return std::nullopt;
                    }
                    if (w.take("Ability that has")) {
                        auto quoted = w.token();
                        if (!quoted || quoted->size() < 3 || quoted->front() != '"' || quoted->back() != '"' || !w.take("in its name each turn")) {
                            return std::nullopt;
                        }
                        const std::string part = quoted->substr(1, quoted->size() - 2);
                        if (context_.name.find(part) == std::string::npos) {
                            return std::nullopt;
                        }
                        return one({{"op", "_ability"}, {"one_per_name", true}});
                    }
                    const auto& tokens = w.tokens();
                    if (tokens.size() < w.mark() + 3) {
                        return std::nullopt;
                    }
                    const std::size_t name_end = tokens.size() - 3;
                    std::string name;
                    for (std::size_t i = w.mark(); i < name_end; ++i) {
                        name += (name.empty() ? "" : " ") + tokens[i];
                    }
                    w.reset(name_end);
                    if (name != context_.name || !w.take("Ability each turn")) {
                        return std::nullopt;
                    }
                    return one({{"op", "_ability"}, {"one_per_name", true}});
                });
                out.emplace_back([this](Words& w) -> std::optional<json> {
                    if (!w.take("once during each player's turn , that player may")) {
                        return std::nullopt;
                    }
                    const Words rest = w.slice(w.mark(), w.tokens().size()).replaced("their", "your").replaced("they", "you");
                    auto body = sentence(rest);
                    if (!body) {
                        return std::nullopt;
                    }
                    w.reset(w.tokens().size());
                    return body;
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("whenever any player puts")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto noun = q && q->count == 1 && !q->up_to ? card_noun(w) : std::nullopt;
                    if (!noun || noun->value("kind", "") != "pokemon" || !w.take("onto their Bench during their turn , place")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || !w.take("damage counters on that Pokémon")) {
                        return std::nullopt;
                    }
                    return json::array({{{"op", "_trigger"}, {"trigger", "put_on_bench"}},
                                        {{"op", "if"}, {"condition", {{"self_is", *noun}}}, {"then", json::array({{{"op", "counters_on_self"}, {"count", *n}}})}}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("the Pokémon this card is attached to gets")) {
                        const auto plus = w.token();
                        if (!plus || plus->size() < 2 || plus->front() != '+') {
                            return std::nullopt;
                        }
                        Words digits({plus->substr(1)});
                        const auto n = digits.number();
                        if (!n || !w.take("HP")) {
                            return std::nullopt;
                        }
                        return one({{"op", "_passive"}, {"modifier", {{"modify", "max_hp"}, {"add", *n}}}});
                    }
                    if (w.take("the Retreat Cost of the Pokémon this card is attached to is")) {
                        const auto units = energy_units(w);
                        if (!units || !w.take("less") || std::any_of(units->begin(), units->end(), [](const std::string& u) { return u != "colorless"; })) {
                            return std::nullopt;
                        }
                        return one({{"op", "_passive"}, {"modifier", {{"modify", "retreat_cost"}, {"add", -static_cast<int>(units->size())}}}});
                    }
                    if (w.take("the Pokémon this card is attached to has no Retreat Cost")) {
                        return one({{"op", "_passive"}, {"modifier", {{"modify", "retreat_cost"}, {"set", 0}}}});
                    }
                    if (!w.take("attacks used by the")) {
                        return std::nullopt;
                    }
                    const bool poisoned = w.take("Poisoned");
                    if (!w.take("Pokémon this card is attached to do")) {
                        return std::nullopt;
                    }
                    const auto n = w.number();
                    if (!n || !w.take("more damage to your opponent's Active Pokémon ( before applying Weakness and Resistance )")) {
                        return std::nullopt;
                    }
                    json modifier = {{"modify", "damage_dealt"}, {"add", *n}};
                    if (poisoned) {
                        modifier["if_poisoned"] = true;
                    }
                    return one({{"op", "_passive"}, {"modifier", modifier}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    std::string applies_to = "all";
                    if (w.take("your")) {
                        applies_to = "yours";
                    }
                    const auto noun = card_noun(w);
                    if (!noun || noun->value("kind", "") != "pokemon" || !w.take("in play")) {
                        return std::nullopt;
                    }
                    if (applies_to == "all" && !w.take("( both yours and your opponent's )")) {
                        return std::nullopt;
                    }
                    if (!w.take("have no Retreat Cost")) {
                        return std::nullopt;
                    }
                    json modifier = {{"modify", "retreat_cost"}, {"set", 0}, {"applies_to", applies_to}};
                    if (const json filter = without_kind(*noun); !filter.empty()) {
                        modifier["filter"] = filter;
                    }
                    return one({{"op", "_passive"}, {"modifier", modifier}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (!w.take("the Weakness of each of your opponent's")) {
                        return std::nullopt;
                    }
                    const auto noun = card_noun(w);
                    if (!noun || noun->value("kind", "") != "pokemon" || !w.take("in play is now")) {
                        return std::nullopt;
                    }
                    const auto type = w.energy_type();
                    if (!type) {
                        return std::nullopt;
                    }
                    json modifier = {{"modify", "weakness_type"}, {"type", *type}, {"applies_to", "opponents"}};
                    if (const json filter = without_kind(*noun); !filter.empty()) {
                        modifier["filter"] = filter;
                    }
                    return one({{"op", "_passive"}, {"modifier", modifier}});
                });
                out.emplace_back([](Words& w) -> std::optional<json> {
                    if (w.take("as long as this card is attached to a Pokémon , it provides")) {
                        const auto units = energy_units(w);
                        if (!units || !w.take("Energy")) {
                            return std::nullopt;
                        }
                        return one({{"op", "_provides"}, {"provides", *units}});
                    }
                    if (w.take("if this card is attached to 1 of your Pokémon , discard it at the end of your turn")) {
                        return one({{"op", "_discard_at_end_of_turn"}});
                    }
                    if (!w.take("if this card is attached to")) {
                        return std::nullopt;
                    }
                    const auto q = quantity(w);
                    const auto holder = q && q->count == 1 ? card_noun(w) : std::nullopt;
                    if (!holder || holder->value("kind", "") != "pokemon" || !w.take(",")) {
                        return std::nullopt;
                    }
                    json provides;
                    if (w.take("this card provides every type of Energy but provides only")) {
                        const auto n = w.number();
                        if (!n || *n < 1 || *n > 4 || !w.take("Energy at a time")) {
                            return std::nullopt;
                        }
                        provides = json::array();
                        for (int i = 0; i < *n; ++i) {
                            provides.push_back("any");
                        }
                    } else if (w.take("it provides")) {
                        const auto units = energy_units(w);
                        if (!units || !w.take("Energy instead")) {
                            return std::nullopt;
                        }
                        provides = *units;
                    } else {
                        return std::nullopt;
                    }
                    return one({{"op", "_provides_when"}, {"entry", {{"holder", without_kind(*holder)}, {"provides", provides}}}});
                });
            }

            const AttackContext& context_;
        };

    } // namespace

    std::string normalize_text(const std::string& text) {
        std::string out;
        bool in_tag = false;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if (c == '<') {
                in_tag = true;
                continue;
            }
            if (in_tag) {
                in_tag = c != '>';
                continue;
            }
            if (text.compare(i, 3, "\xE2\x80\x99") == 0) {
                out += '\'';
                i += 2;
                continue;
            }
            if (text.compare(i, 3, "\xE2\x80\x9C") == 0 || text.compare(i, 3, "\xE2\x80\x9D") == 0) {
                out += '"';
                i += 2;
                continue;
            }
            if (text.compare(i, 2, "\xC2\xA0") == 0) {
                out += ' ';
                i += 1;
                continue;
            }
            out += std::isspace(static_cast<unsigned char>(c)) ? ' ' : c;
        }
        for (const std::string_view reminder : kReminders) {
            for (std::size_t at = out.find(reminder); at != std::string::npos; at = out.find(reminder)) {
                out.erase(at, reminder.size());
            }
        }
        std::string collapsed;
        for (const char c : out) {
            if (c == ' ' && (collapsed.empty() || collapsed.back() == ' ')) {
                continue;
            }
            collapsed += c;
        }
        while (!collapsed.empty() && collapsed.back() == ' ') {
            collapsed.pop_back();
        }
        return collapsed;
    }

    std::vector<std::string> split_sentences(const std::string& text) {
        std::vector<std::string> out;
        std::size_t start = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '.' && (i + 1 == text.size() || text[i + 1] == ' ')) {
                std::string sentence = text.substr(start, i - start);
                while (!sentence.empty() && sentence.front() == ' ') {
                    sentence.erase(0, 1);
                }
                if (!sentence.empty()) {
                    out.push_back(sentence);
                }
                start = i + 1;
            }
        }
        std::string last = text.substr(std::min(start, text.size()));
        while (!last.empty() && last.front() == ' ') {
            last.erase(0, 1);
        }
        if (!last.empty()) {
            out.push_back(last);
        }
        return out;
    }

    namespace {

        std::optional<std::string> read_sentences(const std::string& text, AttackReader& reader, json& blocks) {
            for (const std::string& sentence : split_sentences(normalize_text(text))) {
                Words words(tokenize(sentence));
                if (words.take("if you do ,")) {
                    auto then = reader.sentence(words.slice(words.mark(), words.tokens().size()));
                    if (!then) {
                        return sentence;
                    }
                    for (json& block : *then) {
                        block["_if_you_do"] = true;
                    }
                    if (json* may = last_may(blocks)) {
                        may->at("effects").insert(may->at("effects").end(), then->begin(), then->end());
                        continue;
                    }
                    if (!blocks.empty() && blocks.back().value("op", "") == "switch_active") {
                        blocks.insert(blocks.end(), then->begin(), then->end());
                        continue;
                    }
                    if (!blocks.empty() && blocks.back().value("op", "") == "discard_from_hand") {
                        blocks.push_back({{"op", "if"}, {"condition", {{"moved_at_least", 1}}}, {"then", *then}});
                        continue;
                    }
                    return sentence;
                }
                words.reset(0);
                const auto& tokens = words.tokens();
                if (!tokens.empty() && same_word(tokens.back(), "instead") && words.take("if")) {
                    const auto condition = condition_phrase(words);
                    auto then = condition && words.take(",") ? reader.sentence(words.slice(words.mark(), tokens.size() - 1)) : std::nullopt;
                    if (then && !blocks.empty() && !blocks.back().value("op", "").starts_with("_")) {
                        json otherwise = json::array({blocks.back()});
                        blocks.back() = {{"op", "if"}, {"condition", *condition}, {"then", *then}, {"else", otherwise}};
                        continue;
                    }
                }
                words.reset(0);
                auto read = reader.sentence(std::move(words));
                if (!read) {
                    return sentence;
                }
                if (read->size() == 1 && read->at(0).value("op", "") == "shuffle_deck") {
                    if (json* may = last_may(blocks); may && uses(may->at("effects"), "search_deck")) {
                        may->at("effects").push_back(read->at(0));
                        continue;
                    }
                }
                if (read->size() == 1 && read->at(0).value("op", "") == "_attack_fails") {
                    if (blocks.empty() || blocks.back().value("effect", "") != "attack_flip") {
                        return sentence;
                    }
                    continue;
                }
                if (read->size() == 1 && read->at(0).value("op", "") == "_rest") {
                    if (blocks.empty() || blocks.back().value("op", "") != "look_at_top") {
                        return sentence;
                    }
                    continue;
                }
                blocks.insert(blocks.end(), read->begin(), read->end());
            }
            return std::nullopt;
        }

        bool any_card_block(const json& blocks) {
            return std::any_of(blocks.begin(), blocks.end(), [](const json& block) { return block.value("op", "").starts_with("_"); });
        }

        Translation not_read(std::string why) {
            Translation out;
            out.unread = std::move(why);
            return out;
        }

    } // namespace

    Translation translate_attack(const std::string& text, const AttackContext& context) {
        Translation out;
        AttackReader reader(context);
        json blocks = json::array();
        if (const auto unread = read_sentences(text, reader, blocks)) {
            out.unread = *unread;
            return out;
        }
        if (const auto first = std::find_if(blocks.begin(), blocks.end(), [](const json& block) { return block.value("op", "") == "_first_turn"; });
            first != blocks.end()) {
            if (first != blocks.begin()) {
                return not_read("\"if you go first\" not at the start");
            }
            out.first_turn = true;
            blocks.erase(blocks.begin());
        }
        std::optional<json> unless;
        json before = json::array();
        if (const auto nothing = std::find_if(blocks.begin(), blocks.end(), [](const json& block) { return block.value("op", "") == "_does_nothing"; });
            nothing != blocks.end()) {
            const auto at = nothing - blocks.begin();
            if (at == 0 || blocks[static_cast<std::size_t>(at) - 1].value("op", "") != "flip_coins") {
                return not_read("\"this attack does nothing\" not just after a flip");
            }
            const int coins = blocks[static_cast<std::size_t>(at) - 1].value("count", 1);
            const std::string on = nothing->at("on").get<std::string>();
            if (on == "tails" && coins != 1) {
                return not_read("\"if tails\" after more than one coin");
            }
            unless = json{{"heads_at_least", on == "all" ? 1 : coins}};
            before.insert(before.end(), blocks.begin(), nothing);
            json after = json::array();
            after.insert(after.end(), nothing + 1, blocks.end());
            blocks = std::move(after);
        }
        if (any_card_block(blocks) || any_card_block(before)) {
            out.unread = "a card's rule in an attack";
            return out;
        }

        const auto changes = [](const json& block) {
            return uses(block, "set_damage") || uses(block, "add_damage") || uses(block, "ignore_defender_effects") || uses(block, "ignore_weakness") ||
                   uses(block, "ignore_resistance");
        };
        if (std::any_of(before.begin(), before.end(), changes)) {
            return not_read("a change to the damage before \"this attack does nothing\"");
        }
        std::size_t last_change = blocks.size();
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            if (changes(blocks[i])) {
                last_change = i;
            }
        }
        const bool changes_damage = last_change != blocks.size();
        if ((context.damage_mod == DamageMod::Times) != uses(blocks, "set_damage") ||
            (context.damage_mod == DamageMod::Plus || context.damage_mod == DamageMod::Minus) != uses(blocks, "add_damage")) {
            out.unread = "the attack's damage (" + std::string(context.damage_mod == DamageMod::Times ? "×" : context.damage_mod == DamageMod::Plus ? "+"
                                                                                                          : context.damage_mod == DamageMod::Minus  ? "-"
                                                                                                                                                    : "plain") +
                         ") doesn't match the text";
            return out;
        }
        const bool placed = uses(blocks, "damage");
        if (placed && changes_damage) {
            return not_read("the damage placed twice");
        }
        if (changes_damage) {
            blocks.insert(blocks.begin() + static_cast<std::ptrdiff_t>(last_change) + 1, json{{"op", "damage"}});
        } else if (unless && !placed) {
            blocks.insert(blocks.begin(), json{{"op", "damage"}});
        }
        if (unless) {
            before.push_back({{"op", "if"}, {"condition", *unless}, {"then", std::move(blocks)}});
            blocks = std::move(before);
        }
        out.ok = true;
        drop_marks(blocks);
        out.effects = std::move(blocks);
        return out;
    }

    Translation translate_text(const std::string& text, TextPart part, const std::string& name) {
        const AttackContext context{name, DamageMod::None};
        AttackReader reader(context);
        json blocks = json::array();
        if (const auto unread = read_sentences(text, reader, blocks)) {
            return not_read(*unread);
        }
        json effects = json::array();
        json requirements = json::array();
        json passive = json::array();
        json fields = json::object();
        bool wrap = false;
        for (json& block : blocks) {
            const std::string op = block.value("op", "");
            if (!op.starts_with("_")) {
                effects.push_back(std::move(block));
                continue;
            }
            if (op == "_requires" && part != TextPart::Tool && part != TextPart::SpecialEnergy) {
                add_requirement(requirements, block.at("condition"));
            } else if (op == "_ability" && part == TextPart::Ability) {
                for (const auto& [key, value] : block.items()) {
                    if (key == "requires") {
                        for (const json& condition : value) {
                            add_requirement(requirements, condition);
                        }
                    } else if (key == "wrap") {
                        wrap = true;
                    } else if (key != "op") {
                        fields[key] = value;
                    }
                }
            } else if (op == "_passive" && (part == TextPart::Tool || part == TextPart::Stadium || part == TextPart::Ability)) {
                passive.push_back(block.at("modifier"));
            } else if (op == "_trigger" && part == TextPart::Stadium) {
                fields["trigger"] = block.at("trigger");
            } else if (op == "_provides" && part == TextPart::SpecialEnergy) {
                fields["provides"] = block.at("provides");
            } else if (op == "_provides_when" && part == TextPart::SpecialEnergy) {
                if (!fields.contains("provides_when")) {
                    fields["provides_when"] = json::array();
                }
                fields["provides_when"].push_back(block.at("entry"));
            } else if (op == "_discard_at_end_of_turn" && part == TextPart::SpecialEnergy) {
                fields["discard_at_end_of_turn"] = true;
            } else if (op == "_first_turn" && part == TextPart::Trainer) {
                fields["first_turn"] = true;
            } else {
                return not_read("a rule that doesn't belong on this kind of card");
            }
        }
        if (wrap && !effects.empty()) {
            effects = json::array({{{"op", "may"}, {"effects", effects}}});
        }
        if (part == TextPart::SpecialEnergy || part == TextPart::Tool) {
            if (!effects.empty() || !requirements.empty() || (part == TextPart::Tool && passive.empty()) || (part == TextPart::SpecialEnergy && !fields.contains("provides"))) {
                return not_read("effects this kind of card can't have");
            }
        } else if (effects.empty() && passive.empty()) {
            return not_read("no effect");
        }
        if (!effects.empty()) {
            implied_requirements(effects, requirements);
            drop_marks(effects);
            fields["effects"] = effects;
        }
        if (!requirements.empty()) {
            fields["requires"] = requirements;
        }
        if (!passive.empty()) {
            fields["passive"] = passive;
        }
        try {
            compile_trainer(fields.value("effects", json::array()));
            compile_conditions(fields.value("requires", json::array()));
            compile_passive(fields.value("passive", json::array()));
            compile_provides_when(fields.value("provides_when", json::array()));
            trigger_from_string(fields.value("trigger", "none"));
        } catch (const std::exception& e) {
            return not_read(std::string("doesn't compile: ") + e.what());
        }
        Translation out;
        out.ok = true;
        out.effects = std::move(fields);
        return out;
    }

    CardTranslation translate_card(const CardDef& card) {
        CardTranslation out;
        const auto label = [&](const std::string& part) {
            return card.id + " " + card.name + (part.empty() ? "" : ": " + part);
        };
        for (const Attack& attack : card.attacks) {
            if (attack.text.empty()) {
                continue;
            }
            ++out.parts;
            const Translation t = translate_attack(attack.text, AttackContext{attack.name, attack.damage_mod});
            if (t.ok) {
                out.overlay["attacks"][attack.name] = {{"effects", t.effects}};
                if (t.first_turn) {
                    out.overlay["attacks"][attack.name]["first_turn"] = true;
                }
            } else {
                out.unread.emplace_back(label("attack " + attack.name), t.unread);
            }
        }
        for (const Ability& ability : card.abilities) {
            if (ability.text.empty()) {
                continue;
            }
            ++out.parts;
            const Translation t = translate_text(ability.text, TextPart::Ability, ability.name);
            if (t.ok) {
                out.overlay["abilities"][ability.name] = t.effects;
            } else {
                out.unread.emplace_back(label("ability " + ability.name), t.unread);
            }
        }
        if (!card.text.empty()) {
            std::optional<TextPart> part;
            if (card.kind == CardKind::Trainer) {
                part = card.trainer_kind == TrainerKind::Stadium ? TextPart::Stadium : card.trainer_kind == TrainerKind::Tool ? TextPart::Tool
                                                                                                                              : TextPart::Trainer;
            } else if (card.kind == CardKind::Energy && card.energy_kind == EnergyKind::Special) {
                part = TextPart::SpecialEnergy;
            }
            ++out.parts;
            const Translation t = part ? translate_text(card.text, *part, card.name) : not_read("text on a kind of card the translator doesn't read");
            if (t.ok) {
                for (const auto& [key, value] : t.effects.items()) {
                    out.overlay[key] = value;
                }
            } else {
                out.unread.emplace_back(label(""), t.unread);
            }
        }
        return out;
    }

    std::vector<std::string> translation_differences(const CardDef& written, const CardDef& translated, const json& overlay) {
        std::vector<std::string> out;
        const auto same_conditions = [](const std::vector<Condition>& a, const std::vector<Condition>& b) {
            if (a.size() != b.size()) {
                return false;
            }
            return std::all_of(a.begin(), a.end(), [&](const Condition& c) { return std::find(b.begin(), b.end(), c) != b.end(); });
        };
        const std::string label = written.id + " " + written.name;
        if (overlay.contains("attacks")) {
            for (const auto& [name, unused] : overlay.at("attacks").items()) {
                for (std::size_t i = 0; i < written.attacks.size() && i < translated.attacks.size(); ++i) {
                    if (written.attacks[i].name == name && written.attacks[i].program != translated.attacks[i].program) {
                        out.push_back(label + ": attack " + name);
                    }
                }
            }
        }
        if (overlay.contains("abilities")) {
            for (const auto& [name, unused] : overlay.at("abilities").items()) {
                for (std::size_t i = 0; i < written.abilities.size() && i < translated.abilities.size(); ++i) {
                    const Ability& a = written.abilities[i];
                    const Ability& b = translated.abilities[i];
                    if (a.name == name && (a.program != b.program || !same_conditions(a.conditions, b.conditions) || a.modifiers != b.modifiers ||
                                           a.once_per_turn != b.once_per_turn || a.one_per_name != b.one_per_name || a.active_only != b.active_only ||
                                           a.trigger != b.trigger)) {
                        out.push_back(label + ": ability " + name);
                    }
                }
            }
        }
        const bool card_part = std::any_of(overlay.items().begin(), overlay.items().end(), [](const auto& item) { return item.key() != "attacks" && item.key() != "abilities"; });
        if (card_part && (written.program != translated.program || !same_conditions(written.conditions, translated.conditions) || written.modifiers != translated.modifiers ||
                          written.trigger != translated.trigger || written.provides != translated.provides || written.conditional_provides != translated.conditional_provides ||
                          written.discard_at_end_of_turn != translated.discard_at_end_of_turn)) {
            out.push_back(label);
        }
        return out;
    }

} // namespace pokebot::import
