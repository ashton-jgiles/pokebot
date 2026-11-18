#include "cards/card/card.hpp"

#include <array>

namespace pokebot {
    namespace {

        using nlohmann::json;

        constexpr std::array<std::string_view, 11> kEnergyTypes{"grass", "fire", "water", "lightning", "psychic", "fighting", "darkness", "metal", "fairy", "dragon", "colorless"};
        constexpr std::array<std::string_view, 3> kCardKinds{"pokemon", "trainer", "energy"};
        constexpr std::array<std::string_view, 4> kStages{"none", "basic", "stage1", "stage2"};
        constexpr std::array<std::string_view, 5> kTrainerKinds{"none", "item", "supporter", "stadium", "tool"};
        constexpr std::array<std::string_view, 3> kEnergyKinds{"none", "basic", "special"};
        constexpr std::array<std::string_view, 4> kDamageMods{"none", "plus", "minus", "times"};

        template <class E, std::size_t N>
        std::string_view name_of(const std::array<std::string_view, N>& names, E value) {
            return names.at(static_cast<std::size_t>(value));
        }

        template <class E, std::size_t N>
        E parse_enum(const std::array<std::string_view, N>& names, std::string_view s, const char* what) {
            for (std::size_t i = 0; i < N; ++i) {
                if (names[i] == s) {
                    return static_cast<E>(i);
                }
            }
            throw CardDataError(std::string("unknown ") + what + " '" + std::string(s) + "'");
        }

        template <class T>
        T required(const json& j, const char* key) {
            if (!j.contains(key)) {
                throw CardDataError(std::string("missing field '") + key + "'");
            }
            return j.at(key).get<T>();
        }

        json energy_list(const std::vector<EnergyType>& types) {
            json out = json::array();
            for (const auto t : types) {
                out.push_back(to_string(t));
            }
            return out;
        }

        std::vector<EnergyType> parse_energy_list(const json& j) {
            std::vector<EnergyType> out;
            for (const auto& t : j) {
                out.push_back(energy_type_from_string(t.get<std::string>()));
            }
            return out;
        }

        json effects_or_empty(const json& j) {
            return j.contains("effects") ? j.at("effects") : json::array();
        }

    } // namespace

    std::string_view to_string(EnergyType type) {
        return name_of(kEnergyTypes, type);
    }

    EnergyType energy_type_from_string(std::string_view name) {
        return parse_enum<EnergyType>(kEnergyTypes, name, "energy type");
    }

    CardKind card_kind_from_string(std::string_view name) {
        return parse_enum<CardKind>(kCardKinds, name, "card kind");
    }

    int CardDef::prize_value() const {
        if (rules.ex && rules.mega) {
            return 3;
        }
        return rules.ex ? 2 : 1;
    }

    void to_json(json& j, const CardDef& c) {
        j = json{{"id", c.id}, {"tcgdex_id", c.tcgdex_id}, {"name", c.name}, {"kind", name_of(kCardKinds, c.kind)}};

        json rules = json::array();
        if (c.rules.ex) {
            rules.push_back("ex");
        }
        if (c.rules.mega) {
            rules.push_back("mega");
        }
        if (c.rules.tera) {
            rules.push_back("tera");
        }
        if (c.rules.ace_spec) {
            rules.push_back("ace_spec");
        }
        j["rules"] = rules;

        if (c.kind == CardKind::Pokemon) {
            j["stage"] = name_of(kStages, c.stage);
            if (!c.evolves_from.empty()) {
                j["evolves_from"] = c.evolves_from;
            }
            j["hp"] = c.hp;
            j["types"] = energy_list(c.types);
            j["weakness"] = c.weakness ? json{{"type", to_string(c.weakness->type)}, {"multiplier", c.weakness->multiplier}} : json(nullptr);
            j["resistance"] = c.resistance ? json{{"type", to_string(c.resistance->type)}, {"amount", c.resistance->amount}} : json(nullptr);
            j["retreat"] = c.retreat;

            json abilities = json::array();
            for (const auto& a : c.abilities) {
                json ability = {{"name", a.name}, {"text", a.text}, {"effects", a.effects}};
                if (!a.passive.empty()) {
                    ability["passive"] = a.passive;
                }
                if (!a.requirements.empty()) {
                    ability["requires"] = a.requirements;
                }
                if (!a.once_per_turn) {
                    ability["once_per_turn"] = false;
                }
                if (a.one_per_name) {
                    ability["one_per_name"] = true;
                }
                if (a.active_only) {
                    ability["active_only"] = true;
                }
                if (a.trigger != Trigger::None) {
                    ability["trigger"] = to_string(a.trigger);
                }
                abilities.push_back(ability);
            }
            j["abilities"] = abilities;

            json attacks = json::array();
            for (const auto& a : c.attacks) {
                attacks.push_back({{"name", a.name},
                                   {"cost", energy_list(a.cost)},
                                   {"damage", a.damage},
                                   {"damage_mod", name_of(kDamageMods, a.damage_mod)},
                                   {"text", a.text},
                                   {"effects", a.effects}});
            }
            j["attacks"] = attacks;

        } else {
            if (c.kind == CardKind::Trainer) {
                j["trainer_kind"] = name_of(kTrainerKinds, c.trainer_kind);
            }
            if (c.kind == CardKind::Energy) {
                j["energy_kind"] = name_of(kEnergyKinds, c.energy_kind);
                j["provides"] = energy_list(c.provides);
            }
            j["text"] = c.text;
            j["effects"] = c.effects;

            if (!c.requirements.empty()) {
                j["requires"] = c.requirements;
            }
            if (!c.passive.empty()) {
                j["passive"] = c.passive;
            }
            if (c.trigger != Trigger::None) {
                j["trigger"] = to_string(c.trigger);
            }
            if (!c.provides_when.empty()) {
                j["provides_when"] = c.provides_when;
            }
            if (c.discard_at_end_of_turn) {
                j["discard_at_end_of_turn"] = true;
            }
        }
    }

    void from_json(const json& j, CardDef& c) {
        try {
            c = CardDef{};
            c.id = required<std::string>(j, "id");
            c.tcgdex_id = j.value("tcgdex_id", "");
            c.name = required<std::string>(j, "name");
            c.kind = parse_enum<CardKind>(kCardKinds, required<std::string>(j, "kind"), "card kind");

            for (const auto& r : j.value("rules", json::array())) {
                const auto rule = r.get<std::string>();
                if (rule == "ex") {
                    c.rules.ex = true;
                } else if (rule == "mega") {
                    c.rules.mega = true;
                } else if (rule == "tera") {
                    c.rules.tera = true;
                } else if (rule == "ace_spec") {
                    c.rules.ace_spec = true;
                } else {
                    throw CardDataError("unknown rule '" + rule + "'");
                }
            }

            if (c.kind == CardKind::Pokemon) {
                c.stage = parse_enum<Stage>(kStages, required<std::string>(j, "stage"), "stage");
                c.evolves_from = j.value("evolves_from", "");
                c.hp = required<int>(j, "hp");
                c.types = parse_energy_list(required<json>(j, "types"));
                if (const auto& w = j.value("weakness", json(nullptr)); !w.is_null()) {
                    c.weakness = Weakness{energy_type_from_string(required<std::string>(w, "type")), required<int>(w, "multiplier")};
                }
                if (const auto& r = j.value("resistance", json(nullptr)); !r.is_null()) {
                    c.resistance = Resistance{energy_type_from_string(required<std::string>(r, "type")), required<int>(r, "amount")};
                }
                c.retreat = j.value("retreat", 0);
                for (const auto& a : j.value("abilities", json::array())) {
                    Ability ability;
                    ability.name = required<std::string>(a, "name");
                    ability.text = a.value("text", "");
                    ability.effects = effects_or_empty(a);
                    ability.passive = a.value("passive", json::array());
                    ability.requirements = a.value("requires", json::array());
                    ability.once_per_turn = a.value("once_per_turn", true);
                    ability.one_per_name = a.value("one_per_name", false);
                    ability.active_only = a.value("active_only", false);
                    ability.trigger = trigger_from_string(a.value("trigger", "none"));
                    c.abilities.push_back(std::move(ability));
                }
                for (const auto& a : j.value("attacks", json::array())) {
                    Attack atk;
                    atk.name = required<std::string>(a, "name");
                    atk.cost = parse_energy_list(a.value("cost", json::array()));
                    atk.damage = a.value("damage", 0);
                    atk.damage_mod = parse_enum<DamageMod>(kDamageMods, a.value("damage_mod", "none"), "damage modifier");
                    atk.text = a.value("text", "");
                    atk.effects = effects_or_empty(a);
                    c.attacks.push_back(std::move(atk));
                }

            } else {
                if (c.kind == CardKind::Trainer) {
                    c.trainer_kind = parse_enum<TrainerKind>(kTrainerKinds, required<std::string>(j, "trainer_kind"), "trainer kind");
                } else {
                    c.energy_kind = parse_enum<EnergyKind>(kEnergyKinds, required<std::string>(j, "energy_kind"), "energy kind");
                    c.provides = parse_energy_list(j.value("provides", json::array()));
                }
                c.text = j.value("text", "");
                c.effects = effects_or_empty(j);
                c.requirements = j.value("requires", json::array());
                c.passive = j.value("passive", json::array());
                c.trigger = trigger_from_string(j.value("trigger", "none"));
                c.provides_when = j.value("provides_when", json::array());
                c.discard_at_end_of_turn = j.value("discard_at_end_of_turn", false);
            }
        } catch (const json::exception& e) {
            throw CardDataError(std::string("bad card data: ") + e.what());
        }
    }
} // namespace pokebot
