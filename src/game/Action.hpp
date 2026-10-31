#pragma once

#include <vector>

#include "core/Tile.hpp"

namespace mj {

enum class ActionType {
    // On the player's own turn
    Discard,
    Riichi,  // declared together with a discard
    Tsumo,
    ClosedKan,
    KanFromPon,
    KyuushuKyuuhai,
    // On another player's discard (only Ron on a kan from pon)
    Ron,
    Chi,
    Pon,
    OpenKan,
    Pass,
};

struct Action {
    ActionType type;
    // Discard / Riichi: the tile discarded. Tsumo: the drawn tile.
    // ClosedKan: the kind. KanFromPon: the tile added to the pon.
    // Ron / Chi / Pon / OpenKan: the claimed tile.
    Tile tile{};
    // Chi / Pon / OpenKan / ClosedKan: the tiles taken from the hand.
    std::vector<Tile> handTiles{};

    bool operator==(const Action&) const = default;
};

}  // namespace mj
