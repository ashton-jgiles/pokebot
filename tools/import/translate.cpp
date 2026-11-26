#include "translate.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace pokebot::import {
    namespace {

        using nlohmann::json;

        constexpr std::array<std::string_view, 2> kReminders = {
            "(Don't apply Weakness and Resistance for Benched Pokémon.)",
            "(Pokémon ex, Pokémon V, etc. have Rule Boxes.)",
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
            std::string stage;
            std::optional<std::string> type;
            std::string prefix;
            for (;;) {
                if (w.take("Basic")) {
                    basic = true;
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
                if (!stage.empty() || !prefix.empty()) {
                    return fail();
                }
                filter["kind"] = "energy";
                if (basic) {
                    filter["basic"] = true;
                }
                if (type) {
                    filter["type"] = *type;
                }
                return filter;
            }
            if (w.take("Pokémon")) {
                filter["kind"] = "pokemon";
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
                if (!prefix.empty()) {
                    filter["name_prefix"] = prefix;
                }
                if (w.take("ex")) {
                    filter["rule_box"] = true;
                }
                if (w.take("with")) {
                    const auto hp = w.number();
                    if (!hp || !w.take("HP or less")) {
                        return fail();
                    }
                    filter["max_hp"] = *hp;
                }
                if (w.take("that doesn't have a Rule Box") || w.take("that don't have a Rule Box")) {
                    if (filter.contains("rule_box")) {
                        return fail();
                    }
                    filter["rule_box"] = false;
                }
                return filter;
            }
            if (basic || !stage.empty() || type || !prefix.empty()) {
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
            if (w.take("a Stadium is in play")) {
                return json{{"stadium_in_play", true}};
            }
            if (w.take("you have more Prize cards remaining than your opponent")) {
                return json{{"more_prizes_than_opponent", true}};
            }
            if (w.take("any of your Pokémon in play are the same type as any of your opponent's Pokémon in play")) {
                return json{{"shares_type", true}};
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
                    auto block = opponent_targets(w);
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
                    auto block = opponent_targets(w);
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
                return out;
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

    Translation translate_attack(const std::string& text, const AttackContext& context) {
        Translation out;
        AttackReader reader(context);
        json blocks = json::array();
        for (const std::string& sentence : split_sentences(normalize_text(text))) {
            Words words(tokenize(sentence));
            if (words.take("if you do ,")) {
                json* may = last_may(blocks);
                auto then = may ? reader.sentence(words.slice(words.mark(), words.tokens().size())) : std::nullopt;
                if (!then) {
                    out.unread = sentence;
                    return out;
                }
                may->at("effects").insert(may->at("effects").end(), then->begin(), then->end());
                continue;
            }
            words.reset(0);
            auto read = reader.sentence(std::move(words));
            if (!read) {
                out.unread = sentence;
                return out;
            }
            if (read->size() == 1 && read->at(0).value("op", "") == "shuffle_deck") {
                if (json* may = last_may(blocks); may && uses(may->at("effects"), "search_deck")) {
                    may->at("effects").push_back(read->at(0));
                    continue;
                }
            }
            blocks.insert(blocks.end(), read->begin(), read->end());
        }

        std::size_t last_change = blocks.size();
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            if (uses(blocks[i], "set_damage") || uses(blocks[i], "add_damage") || uses(blocks[i], "ignore_defender_effects")) {
                last_change = i;
            }
        }
        const bool changes_damage = last_change != blocks.size();
        if ((context.damage_mod == DamageMod::Times) != uses(blocks, "set_damage") || (context.damage_mod == DamageMod::Plus) != uses(blocks, "add_damage") ||
            context.damage_mod == DamageMod::Minus) {
            out.unread = "the attack's damage (" + std::string(context.damage_mod == DamageMod::Times ? "×" : context.damage_mod == DamageMod::Plus ? "+"
                                                                                                          : context.damage_mod == DamageMod::Minus  ? "-"
                                                                                                                                                    : "plain") +
                         ") doesn't match the text";
            return out;
        }
        if (changes_damage) {
            blocks.insert(blocks.begin() + static_cast<std::ptrdiff_t>(last_change) + 1, json{{"op", "damage"}});
        }
        out.ok = true;
        out.effects = std::move(blocks);
        return out;
    }

} // namespace pokebot::import
