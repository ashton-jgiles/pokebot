#include "agents/flat_mc/deck_model.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "cards/validator/deck_validator.hpp"

namespace pokebot {

    namespace {

        double log_choose(int n, int k) {
            return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
        }

        std::string label(const std::string& archetype, const std::string& variant) {
            return archetype + "/" + variant;
        }

        std::string joined(const std::vector<std::string>& items) {
            std::string out;
            for (const std::string& item : items) {
                if (!out.empty()) {
                    out += "; ";
                }
                out += item;
            }
            return out;
        }

        std::vector<std::filesystem::path> sorted_entries(const std::filesystem::path& dir) {
            std::vector<std::filesystem::path> entries;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                entries.push_back(entry.path());
            }
            std::sort(entries.begin(), entries.end());
            return entries;
        }

    } // namespace

    int ArchetypeList::count(std::string_view id) const {
        const auto it = std::lower_bound(cards.begin(), cards.end(), id, [](const ArchetypeCard& card, std::string_view key) { return card.def->id < key; });
        return it != cards.end() && it->def->id == id ? it->count : 0;
    }

    ArchetypeLibrary ArchetypeLibrary::load(const std::filesystem::path& dir, const CardDB& db) {
        if (!std::filesystem::is_directory(dir)) {
            throw ArchetypeError("no archetype library at " + dir.string());
        }
        ArchetypeLibrary library;
        for (const std::filesystem::path& archetype_dir : sorted_entries(dir)) {
            if (!std::filesystem::is_directory(archetype_dir)) {
                continue;
            }
            const std::string archetype = archetype_dir.filename().string();
            int variants = 0;
            for (const std::filesystem::path& file : sorted_entries(archetype_dir)) {
                if (file.extension() != ".txt" || !std::filesystem::is_regular_file(file)) {
                    continue;
                }
                Decklist list;
                try {
                    list = load_decklist(file);
                } catch (const std::exception& e) {
                    throw ArchetypeError(file.string() + ": " + e.what());
                }
                library.add(archetype, file.stem().string(), list, db);
                ++variants;
            }
            if (variants == 0) {
                throw ArchetypeError("archetype " + archetype + " has no lists in " + archetype_dir.string());
            }
        }
        if (library.empty()) {
            throw ArchetypeError("no archetypes in " + dir.string());
        }
        return library;
    }

    void ArchetypeLibrary::add(const std::string& archetype, const std::string& variant, const Decklist& list, const CardDB& db) {
        const std::string name = label(archetype, variant);
        if (find(archetype, variant) != nullptr) {
            throw ArchetypeError(name + " is listed twice");
        }
        const DeckValidation validation = validate_deck(list, db);
        if (!validation.legal()) {
            throw ArchetypeError(name + " is not a legal deck: " + joined(validation.problems));
        }
        std::map<std::string, ArchetypeCard, std::less<>> counted;
        std::vector<std::string> missing;
        for (const DeckEntry& entry : list.entries) {
            const CardDef& def = db.at(card_id(entry));
            for (const std::string& item : missing_effects(def)) {
                missing.push_back(item);
            }
            ArchetypeCard& card = counted[def.id];
            card.def = &def;
            card.count += entry.count;
        }
        if (!missing.empty()) {
            throw ArchetypeError(name + " has cards without effects: " + joined(missing));
        }

        ArchetypeList added{archetype, variant, list, {}};
        for (const auto& [id, card] : counted) {
            added.cards.push_back(card);
        }
        const auto before = [](const ArchetypeList& a, const ArchetypeList& b) {
            return std::tie(a.archetype, a.variant) < std::tie(b.archetype, b.variant);
        };
        lists_.insert(std::upper_bound(lists_.begin(), lists_.end(), added, before), std::move(added));
    }

    std::vector<std::string> ArchetypeLibrary::archetypes() const {
        std::vector<std::string> names;
        for (const ArchetypeList& list : lists_) {
            if (names.empty() || names.back() != list.archetype) {
                names.push_back(list.archetype);
            }
        }
        return names;
    }

    const ArchetypeList* ArchetypeLibrary::find(std::string_view archetype, std::string_view variant) const {
        for (const ArchetypeList& list : lists_) {
            if (list.archetype == archetype && list.variant == variant) {
                return &list;
            }
        }
        return nullptr;
    }

    SeenCards opponent_cards_seen(const Observation& observation) {
        SeenCards seen;
        for (const CardIndex card : observation.opponent_seen) {
            const CardDef* def = observation.def(card);
            if (def != nullptr) {
                ++seen[def->id];
            }
        }
        return seen;
    }

    DeckModel::DeckModel(const ArchetypeLibrary& library, double unexplained) : library_(library) {
        if (library.empty()) {
            throw std::invalid_argument("DeckModel: the library has no lists");
        }
        if (!(unexplained > 0.0 && unexplained <= 1.0)) {
            throw std::invalid_argument("DeckModel: the chance of a card no list explains must be in (0, 1]");
        }
        log_unexplained_ = std::log(unexplained);
        std::map<std::string, int, std::less<>> variants;
        for (const ArchetypeList& list : library.lists()) {
            ++variants[list.archetype];
        }
        const double archetypes = static_cast<double>(variants.size());
        for (const ArchetypeList& list : library.lists()) {
            prior_.push_back(1.0 / (archetypes * static_cast<double>(variants[list.archetype])));
        }
    }

    std::vector<double> DeckModel::belief(const SeenCards& seen) const {
        const std::vector<ArchetypeList>& lists = library_.lists();
        std::vector<double> weight(lists.size());
        for (std::size_t i = 0; i < lists.size(); ++i) {
            double log_weight = std::log(prior_[i]);
            for (const auto& [id, k] : seen) {
                const int n = lists[i].count(id);
                log_weight += k <= n ? log_choose(n, k) : (k - n) * log_unexplained_;
            }
            weight[i] = log_weight;
        }
        const double top = *std::max_element(weight.begin(), weight.end());
        double total = 0.0;
        for (double& w : weight) {
            w = std::exp(w - top);
            total += w;
        }
        for (double& w : weight) {
            w /= total;
        }
        return weight;
    }

    std::vector<double> DeckModel::belief(const Observation& observation) const {
        return belief(opponent_cards_seen(observation));
    }

    std::vector<std::pair<std::string, double>> DeckModel::by_archetype(const std::vector<double>& belief) const {
        std::vector<std::pair<std::string, double>> sums;
        const std::vector<ArchetypeList>& lists = library_.lists();
        for (std::size_t i = 0; i < lists.size() && i < belief.size(); ++i) {
            if (sums.empty() || sums.back().first != lists[i].archetype) {
                sums.emplace_back(lists[i].archetype, 0.0);
            }
            sums.back().second += belief[i];
        }
        return sums;
    }

} // namespace pokebot
