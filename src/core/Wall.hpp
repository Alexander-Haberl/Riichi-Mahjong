#pragma once

#include <random>
#include <vector>

#include "core/Tile.hpp"

namespace mj {

// The 136 tiles of one hand: the live wall players draw from, and the
// 14-tile dead wall (4 rinshan tiles, 5 dora and 5 ura dora indicators).
// Every rinshan draw moves one tile from the end of the live wall into the
// dead wall, so the live wall shrinks by one per kan.
class Wall {
public:
    static constexpr int kDeadWallSize = 14;
    static constexpr int kRinshanTiles = 4;
    static constexpr int kMaxDoraIndicators = 5;

    // Shuffles a full tile set with redFivesPerSuit red fives in each suit.
    explicit Wall(std::mt19937& rng, int redFivesPerSuit = 1);
    // Uses the tiles in the given order (live wall first, then the dead wall).
    // Meant for tests and replays. Throws std::invalid_argument unless there are 136 tiles.
    explicit Wall(std::vector<Tile> tiles);

    // Four of each kind, with redFivesPerSuit of the fives in each suit red.
    static std::vector<Tile> fullSet(int redFivesPerSuit = 1);

    // Throws std::logic_error if the live wall is empty.
    Tile draw();
    // Replacement draw after a kan. Throws std::logic_error after four kans.
    Tile drawRinshan();
    // Reveals the next dora indicator after a kan. Throws std::logic_error after the fifth.
    void revealNextDora();

    // Tiles left to draw in the live wall. 0 means the hand ends after this discard.
    int remaining() const { return liveEnd_ - next_; }
    int rinshanRemaining() const { return kRinshanTiles - rinshanDrawn_; }

    std::vector<Tile> doraIndicators() const;
    // Only looked at for a riichi win; as many as there are revealed dora indicators.
    std::vector<Tile> uraDoraIndicators() const;

private:
    std::vector<Tile> tiles_;
    int next_ = 0;                                   // next live wall tile
    int liveEnd_ = kNumTiles - kDeadWallSize;        // one past the last live tile
    int rinshanDrawn_ = 0;
    int doraRevealed_ = 1;
};

}  // namespace mj
