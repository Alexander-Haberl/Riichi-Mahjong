#pragma once

#include <vector>

#include "core/Hand.hpp"
#include "core/Tile.hpp"

namespace mj {

// Everything about a win that yaku and scoring need besides the decomposition.
struct WinContext {
    Hand hand;     // the complete hand, winning tile included
    Tile winTile;  // the tile that completed the hand

    bool tsumo = false;     // self-drawn; otherwise ron
    bool riichi = false;
    bool doubleRiichi = false;
    bool ippatsu = false;
    bool haitei = false;    // tsumo on the last tile of the wall
    bool houtei = false;    // ron on the last discard
    bool rinshan = false;   // tsumo on the replacement tile after a kan
    bool chankan = false;   // ron on a tile added to a pon
    // Tsumo on the player's first draw, with no call or kan before it (tenhou, chiihou).
    bool firstDraw = false;

    Honor roundWind = Honor::East;
    Honor seatWind = Honor::East;

    std::vector<Tile> doraIndicators;
    std::vector<Tile> uraDoraIndicators;
};

}  // namespace mj
