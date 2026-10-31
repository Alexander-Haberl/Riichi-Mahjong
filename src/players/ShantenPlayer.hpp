#pragma once

#include <vector>

#include "game/Player.hpp"

namespace mj {

// A basic AI. It takes every win, declares riichi whenever it can and aborts
// with kyuushu kyuuhai. It never calls, so an open hand without yaku cannot
// happen. Discards aim for the lowest shanten, then the most tiles that lower
// it (ukeire); on a tie it keeps red fives.
class ShantenPlayer : public IPlayer {
public:
    Action chooseAction(const PlayerView& view, const std::vector<Action>& legal) override;
};

}  // namespace mj
