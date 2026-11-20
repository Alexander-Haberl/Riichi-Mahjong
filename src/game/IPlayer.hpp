#pragma once

#include <vector>

#include "game/Action.hpp"
#include "game/PlayerView.hpp"

namespace mj {

// A seat at the table that decides by itself, like the AIs in src/ai/. A human
// seat has no IPlayer: the game stops at its decisions instead (see Game).
class IPlayer {
public:
    virtual ~IPlayer() = default;

    // Picks one of `legal` (never empty). Called on the player's own turn and
    // whenever they can react to another player's tile.
    virtual Action chooseAction(const PlayerView& view, const std::vector<Action>& legal) = 0;
};

}  // namespace mj
