#pragma once

#include <array>
#include <memory>
#include <string>

#include "game/Game.hpp"

// What the browser sees of the engine. Everything goes out as a JSON string
// (see Json.hpp) and comes back as an index, so this class needs no Emscripten
// and can be tested natively; src/web/Bindings.cpp only exposes it to JavaScript.
namespace mj {

// A game of one human (player 0) against three AIs, with fixed rules. The
// methods are those of Game; docs/ui-design.md has the loop that drives them.
class WebGame {
public:
    // `scenario` is an index into scenarios() for the first round, or -1 for a
    // normal game. With `callingAis`, the opponents are CallingPlayers instead of
    // ShantenPlayers. Throws std::out_of_range for an unknown scenario.
    explicit WebGame(unsigned seed, int scenario = -1, bool callingAis = false);

    void start();

    // JSON array of the steps since the last call.
    std::string takeSteps();

    // JSON of the human's decision, or "null" when the game is over.
    std::string pending() const;

    // Answers the pending decision with the action at this index of its `legal`.
    // Throws std::logic_error if nothing is pending or the index is out of range.
    void submit(int actionIndex);

    // JSON of the final result.
    std::string result() const;

private:
    std::array<std::unique_ptr<IPlayer>, 3> ais_;  // created before game_, which refers to them
    Game game_;
};

// JSON array of the scenario names, in the order of their indices.
std::string scenarioNames();

// The rules of the browser game: the standard rules for now.
inline RuleSet webRules() { return {}; }

}  // namespace mj
