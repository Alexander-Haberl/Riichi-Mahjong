#include "ai/ShantenPlayer.hpp"

#include <algorithm>
#include <climits>

#include "core/Shanten.hpp"

namespace mj {

namespace {

// The action of this type whose discard leaves the hand best off.
Action bestDiscard(const PlayerView& view, const std::vector<Action>& legal, ActionType type) {
    const TileCounts counts = view.hand().counts();
    const int meldCount = view.hand().meldCount();
    const TileCounts visible = view.visibleTiles();

    const Action* best = nullptr;
    int bestShanten = INT_MAX;
    int bestUkeire = -1;
    for (const Action& action : legal) {
        if (action.type != type) continue;
        TileCounts rest = counts;
        --rest[action.tile.kind()];
        const int shanten = shanten::calc(rest, meldCount);
        if (shanten > bestShanten) continue;  // ukeire is the slow part: skip it when it cannot matter

        const int ukeire = shanten::ukeire(rest, meldCount, visible);
        const bool better = shanten < bestShanten || ukeire > bestUkeire ||
                            (ukeire == bestUkeire && best->tile.isRed() && !action.tile.isRed());
        if (better) {
            best = &action;
            bestShanten = shanten;
            bestUkeire = ukeire;
        }
    }
    return *best;
}

}  // namespace

Action ShantenPlayer::chooseAction(const PlayerView& view, const std::vector<Action>& legal) {
    auto has = [&legal](ActionType type) {
        return std::any_of(legal.begin(), legal.end(), [type](const Action& a) { return a.type == type; });
    };

    // A reaction list always has Pass, so this never calls.
    for (ActionType type : {ActionType::Tsumo, ActionType::Ron, ActionType::KyuushuKyuuhai, ActionType::Pass}) {
        auto it = std::find_if(legal.begin(), legal.end(), [type](const Action& a) { return a.type == type; });
        if (it != legal.end()) return *it;
    }
    return bestDiscard(view, legal, has(ActionType::Riichi) ? ActionType::Riichi : ActionType::Discard);
}

}  // namespace mj
