#pragma once

#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "agents/agent.hpp"

namespace pokebot {

    // Thrown when the person at the keyboard stops the game.
    class QuitGame : public std::runtime_error {
    public:
        QuitGame() : std::runtime_error("the player quit") {}
    };

    // A person at the keyboard. Shows them what their player can see and the numbered choices, and
    // reads the number they type: b shows the board again, q quits (as does the end of the input).
    class HumanAgent final : public Agent {
    public:
        HumanAgent(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

        std::string name() const override { return "you"; }
        Action choose(const Observation& observation, const std::vector<Action>& actions) override;

    private:
        void show_choices(const Observation& observation, const std::vector<Action>& actions);

        std::istream& in_;
        std::ostream& out_;
        int board_turn_ = -1; // the turn whose board was shown last
    };

} // namespace pokebot
