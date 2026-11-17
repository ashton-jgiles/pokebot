#pragma once

#include <initializer_list>
#include <string>
#include <utility>

#include "cards/card_db.hpp"
#include "core/decklist.hpp"

namespace test {

    inline pokebot::CardDef basic_pokemon(const std::string& id, const std::string& name, int hp = 60, int retreat = 1) {
        pokebot::CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Pokemon;
        c.stage = pokebot::Stage::Basic;
        c.hp = hp;
        c.types = {pokebot::EnergyType::Colorless};
        c.retreat = retreat;
        return c;
    }

    inline pokebot::CardDef evolution(const std::string& id, const std::string& name, pokebot::Stage stage, const std::string& evolves_from, int hp = 90, int retreat = 1) {
        pokebot::CardDef c = basic_pokemon(id, name, hp, retreat);
        c.stage = stage;
        c.evolves_from = evolves_from;
        return c;
    }

    inline pokebot::CardDef basic_energy(const std::string& id, const std::string& name, pokebot::EnergyType type) {
        pokebot::CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Energy;
        c.energy_kind = pokebot::EnergyKind::Basic;
        c.provides = {type};
        return c;
    }

    // A decklist from {count, card number} pairs, all in set "TST": deck({{4, 1}, {56, 6}}).
    inline pokebot::Decklist deck(std::initializer_list<std::pair<int, int>> cards) {
        pokebot::Decklist list;
        for (const auto& [count, number] : cards) {
            list.entries.push_back(pokebot::DeckEntry{count, "Card " + std::to_string(number), "TST", number});
        }
        return list;
    }

} // namespace test