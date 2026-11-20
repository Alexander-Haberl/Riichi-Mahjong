#pragma once

#include "game/IPlayer.hpp"

namespace mj {

// A test opponent that calls whenever it can, so the UI shows melds of other
// players. It takes every win, then any kan, pon or chi; it never declares
// riichi or nine terminals. Otherwise it discards the last tile of its hand
// (the drawn tile, or after a call the most recently added one), which keeps
// its play predictable for the UI scenarios.
class CallingPlayer : public IPlayer {
public:
    Action chooseAction(const PlayerView& view, const std::vector<Action>& legal) override;
};

}  // namespace mj
