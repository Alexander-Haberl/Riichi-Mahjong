#include "ai/CallingPlayer.hpp"

#include <algorithm>

namespace mj {

Action CallingPlayer::chooseAction(const PlayerView& view, const std::vector<Action>& legal) {
    for (ActionType type : {ActionType::Tsumo, ActionType::Ron, ActionType::OpenKan, ActionType::Pon, ActionType::Chi,
                            ActionType::ClosedKan, ActionType::KanFromPon}) {
        auto it = std::find_if(legal.begin(), legal.end(), [type](const Action& a) { return a.type == type; });
        if (it != legal.end()) return *it;
    }

    // After a draw: the drawn tile (tsumogiri).
    auto tsumogiri = std::find_if(legal.begin(), legal.end(), [](const Action& a) {
        return a.type == ActionType::Discard && a.tsumogiri;
    });
    if (tsumogiri != legal.end()) return *tsumogiri;

    // After a call: the most recently added tile, if kuikae allows it.
    if (!view.hand().concealed().empty()) {
        const Tile last = view.hand().concealed().back();
        auto discard = std::find_if(legal.begin(), legal.end(), [last](const Action& a) {
            return a.type == ActionType::Discard && a.tile == last;
        });
        if (discard != legal.end()) return *discard;
    }
    auto first = std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.type == ActionType::Discard; });
    if (first != legal.end()) return *first;

    // A reaction it does not take.
    auto pass = std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.type == ActionType::Pass; });
    return pass != legal.end() ? *pass : legal.front();
}

}  // namespace mj
