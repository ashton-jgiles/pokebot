#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "engine/observation/observation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct ArchetypeCard {
        const CardDef* def = nullptr;
        int count = 0;
    };

    struct ArchetypeList {
        std::string archetype;
        std::string variant;
        Decklist list;
        std::vector<ArchetypeCard> cards;

        int count(std::string_view id) const;
    };

    class ArchetypeError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class ArchetypeLibrary {
    public:
        static ArchetypeLibrary load(const std::filesystem::path& dir, const CardDB& db);

        void add(const std::string& archetype, const std::string& variant, const Decklist& list, const CardDB& db);
        const std::vector<ArchetypeList>& lists() const { return lists_; }
        std::vector<std::string> archetypes() const;
        const ArchetypeList* find(std::string_view archetype, std::string_view variant) const;
        std::size_t size() const { return lists_.size(); }
        bool empty() const { return lists_.empty(); }

    private:
        std::vector<ArchetypeList> lists_;
    };

    using SeenCards = std::map<std::string, int, std::less<>>;
    using DealtCards = FixedVector<const CardDef*, kDeckSize>;

    SeenCards opponent_cards_seen(const Observation& observation);

    class DeckModel {
    public:
        explicit DeckModel(const ArchetypeLibrary& library, double unexplained = 0.01);

        const ArchetypeLibrary& library() const { return library_; }
        double prior(std::size_t list) const { return prior_[list]; }
        std::vector<double> belief(const SeenCards& seen) const;
        std::vector<double> belief(const Observation& observation) const;
        std::vector<std::pair<std::string, double>> by_archetype(const std::vector<double>& belief) const;
        std::size_t draw(const std::vector<double>& belief, Rng& rng) const;
        static DealtCards deal(const ArchetypeList& list, const SeenCards& seen, std::size_t unseen, Rng& rng);

    private:
        const ArchetypeLibrary& library_;
        double log_unexplained_ = 0.0;
        std::vector<double> prior_;
    };

} // namespace pokebot
