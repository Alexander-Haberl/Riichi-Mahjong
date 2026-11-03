#include "players/CallingPlayer.hpp"

#include <algorithm>

namespace mj {

Action CallingPlayer::chooseAction(const PlayerView& view, const std::vector<Action>& legal) {
    for (ActionType type : {ActionType::Tsumo, ActionType::Ron, ActionType::OpenKan, ActionType::Pon, ActionType::Chi,
                            ActionType::ClosedKan, ActionType::KanFromPon}) {
        auto it = std::find_if(legal.begin(), legal.end(), [type](const Action& a) { return a.type == type; });
        if (it != legal.end()) return *it;
    }

    // The last tile of the hand, if discarding it is allowed (kuikae may forbid it).
    const auto discards = [&legal](Tile tile) {
        return std::find_if(legal.begin(), legal.end(), [tile](const Action& a) {
            return a.type == ActionType::Discard && a.tile == tile;
        });
    };
    if (!view.hand().concealed().empty()) {
        auto last = discards(view.hand().concealed().back());
        if (last != legal.end()) return *last;
    }
    auto first = std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.type == ActionType::Discard; });
    if (first != legal.end()) return *first;

    // A reaction it does not take.
    auto pass = std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.type == ActionType::Pass; });
    return pass != legal.end() ? *pass : legal.front();
}

}  // namespace mj
