#pragma once

#include <array>
#include <string>

#include "game/Game.hpp"
#include "ai/ShantenPlayer.hpp"

// What the browser sees of the engine. Everything goes out as a JSON string
// (see Json.hpp) and comes back as an index, so this class needs no Emscripten
// and can be tested natively; src/web/Bindings.cpp only exposes it to JavaScript.
namespace mj {

// A game of one human (player 0) against three AIs, with fixed rules. The
// methods are those of Game; docs/ui-design.md has the loop that drives them.
class WebGame {
public:
    explicit WebGame(unsigned seed);

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
    std::array<ShantenPlayer, 3> ais_;
    Game game_;
};

// The rules of the browser game: the standard rules for now.
inline RuleSet webRules() { return {}; }

}  // namespace mj
