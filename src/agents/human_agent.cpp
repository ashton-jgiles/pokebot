#include "agents/human_agent.hpp"

#include <iomanip>

#include "engine/describe/describe.hpp"

namespace pokebot {

    namespace {

        // Choices made partway through an effect, where the board hasn't changed since the last one.
        bool is_sub_choice(DecisionKind kind) {
            switch (kind) {
                case DecisionKind::RetreatDiscard:
                case DecisionKind::ChooseCards:
                case DecisionKind::ChoosePokemon:
                case DecisionKind::ChooseYesNo:
                case DecisionKind::ChooseAttack:
                case DecisionKind::CustomChoice:
                    return true;
                default:
                    return false;
            }
        }

        std::string trim(const std::string& text) {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        // The number typed, or 0 if it isn't one.
        std::size_t number(const std::string& text) {
            if (text.empty() || text.size() > 4 || text.find_first_not_of("0123456789") != std::string::npos) {
                return 0;
            }
            return static_cast<std::size_t>(std::stoi(text));
        }

    } // namespace

    void HumanAgent::show_choices(const Observation& observation, const std::vector<Action>& actions) {
        out_ << describe_decision(observation) << ":\n";
        for (std::size_t i = 0; i < actions.size(); ++i) {
            out_ << "  " << std::setw(2) << i + 1 << ". " << describe(observation, actions[i]) << '\n';
        }
    }

    Action HumanAgent::choose(const Observation& observation, const std::vector<Action>& actions) {
        if (!is_sub_choice(observation.decision.kind) || observation.turn != board_turn_) {
            board_turn_ = observation.turn;
            out_ << '\n'
                 << describe(observation);
        }
        if (actions.size() == 1) {
            out_ << describe_decision(observation) << ": " << describe(observation, actions.front()) << " (the only choice)\n";
            return actions.front();
        }
        show_choices(observation, actions);
        while (true) {
            out_ << "> " << std::flush;
            std::string line;
            if (!std::getline(in_, line)) {
                throw QuitGame();
            }
            line = trim(line);
            if (line == "q" || line == "quit") {
                throw QuitGame();
            }
            if (line == "b" || line == "board") {
                out_ << '\n'
                     << describe(observation);
                show_choices(observation, actions);
                continue;
            }
            const std::size_t choice = number(line);
            if (choice >= 1 && choice <= actions.size()) {
                return actions[choice - 1];
            }
            out_ << "Type a number from 1 to " << actions.size() << ", b to see the board again, or q to quit.\n";
        }
    }

} // namespace pokebot
