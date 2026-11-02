#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "core/Hand.hpp"
#include "core/Tile.hpp"

// Splits a complete hand into its building blocks. Yaku and fu evaluation
// run on each reading and the scorer keeps the most valuable one.
namespace mj {

enum class BlockType { Sequence, Triplet, Kan, Pair };

struct Block {
    BlockType type;
    Tile first;  // lowest tile of the block (never red)
    // Counts as open for fu and yaku: from a call (chi / pon / open kan), or a
    // triplet completed by ron in that reading.
    bool open = false;

    bool operator==(const Block&) const = default;
};

enum class HandForm { Regular, Chiitoitsu, Kokushi };

// Which shape the winning tile completed.
enum class WaitType {
    Ryanmen,  // two-sided: 23 waiting on 1 or 4
    Kanchan,  // closed: 13 waiting on 2
    Penchan,  // edge: 12 waiting on 3, 89 waiting on 7
    Shanpon,  // one of two pairs becomes a triplet
    Tanki,    // single tile waiting for its pair (also chiitoitsu and single-wait kokushi)
};

// One way to read a complete hand.
struct Reading {
    HandForm form;
    std::vector<Block> blocks;  // concealed blocks and called melds; empty for kokushi
    bool handOpen = false;      // the hand has an open meld (closed kans keep it closed)

    // Only set by readings(): the wait the winning tile completed.
    std::optional<WaitType> wait{};

    bool operator==(const Reading&) const = default;
};

// The wait the winning tile completed if it is part of this concealed block;
// empty if the block does not hold it. E.g. 3m makes 1-2-3 a penchan, 3-4-5 a ryanmen.
std::optional<WaitType> waitIn(const Block& block, Tile winTile);

// All readings of a complete hand, one for each block the winning tile could
// have completed. With `ron`, a triplet completed by the winning tile is open.
// Empty if the hand does not form a winning shape.
std::vector<Reading> readings(const Hand& hand, Tile winTile, bool ron);

// All decompositions of a complete hand, called melds included as blocks,
// without wait information. Empty if the hand does not form a winning shape.
std::vector<Reading> decompose(const Hand& hand);

// All decompositions of the concealed tiles alone (3n + 2 tiles; called melds
// are not included). Empty if the tiles do not form a winning shape.
std::vector<Reading> decomposeConcealed(const TileCounts& concealed);

bool isComplete(const TileCounts& concealed);

// Tile kinds that would complete a hand of 3n + 1 concealed tiles (never red).
// Kinds the hand already holds all four of are excluded.
std::vector<Tile> waits(const TileCounts& concealed);

// Whether 3n + 1 concealed tiles wait on at least one tile. Waiting only on
// a kind the hand holds all four of does not count.
bool isTenpai(const TileCounts& concealed);
bool isTenpai(const Hand& hand);

}  // namespace mj
