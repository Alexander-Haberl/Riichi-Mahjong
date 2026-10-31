#pragma once

#include "core/Tile.hpp"

// Shanten = number of tile exchanges away from tenpai.
// -1 = complete hand, 0 = tenpai, 1 = one away from tenpai, ...
//
// `concealed` holds the concealed tiles only (13 or 14, minus 3 per meld);
// `meldCount` is the number of called melds, closed kans included.
namespace mj::shanten {

int regular(const TileCounts& concealed, int meldCount = 0);
int chiitoitsu(const TileCounts& concealed);  // only valid for a closed hand
int kokushi(const TileCounts& concealed);     // only valid for a closed hand

// Minimum over all hand forms that are possible with the given meld count.
int calc(const TileCounts& concealed, int meldCount = 0);

// Ukeire: how many tiles would lower the shanten of these 3n + 1 concealed
// tiles. `visible` counts the tiles the player can see (the own hand, rivers,
// melds, dora indicators); only the unseen copies of a kind count.
int ukeire(const TileCounts& concealed, int meldCount, const TileCounts& visible);

}  // namespace mj::shanten
