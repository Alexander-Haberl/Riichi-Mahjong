#include "core/Wall.hpp"

#include <algorithm>
#include <stdexcept>

namespace mj {

namespace {

// Dead wall layout, as offsets from its start.
constexpr int kRinshanOffset = 0;
constexpr int kDoraOffset = kRinshanOffset + Wall::kRinshanTiles;
constexpr int kUraDoraOffset = kDoraOffset + Wall::kMaxDoraIndicators;
constexpr int kDeadWallStart = kNumTiles - Wall::kDeadWallSize;

}  // namespace

Wall::Wall(std::mt19937& rng, int redFivesPerSuit) : tiles_(fullSet(redFivesPerSuit)) {
    std::shuffle(tiles_.begin(), tiles_.end(), rng);
}

Wall::Wall(std::vector<Tile> tiles) : tiles_(std::move(tiles)) {
    if (tiles_.size() != kNumTiles) throw std::invalid_argument("a wall needs exactly 136 tiles");
}

std::vector<Tile> Wall::fullSet(int redFivesPerSuit) {
    std::vector<Tile> tiles;
    tiles.reserve(kNumTiles);
    for (int kind = 0; kind < kNumTileKinds; ++kind) {
        Tile tile(static_cast<uint8_t>(kind));
        bool isFive = !tile.isHonor() && tile.number() == 5;
        for (int copy = 0; copy < 4; ++copy) {
            tiles.emplace_back(tile.kind(), isFive && copy < redFivesPerSuit);
        }
    }
    return tiles;
}

Tile Wall::draw() {
    if (remaining() <= 0) throw std::logic_error("draw from an empty wall");
    return tiles_[static_cast<size_t>(next_++)];
}

Tile Wall::drawRinshan() {
    if (rinshanDrawn_ >= kRinshanTiles) throw std::logic_error("no rinshan tiles left");
    if (remaining() <= 0) throw std::logic_error("no live tile left to refill the dead wall");
    --liveEnd_;
    return tiles_[static_cast<size_t>(kDeadWallStart + kRinshanOffset + rinshanDrawn_++)];
}

void Wall::revealNextDora() {
    if (doraRevealed_ >= kMaxDoraIndicators) throw std::logic_error("all dora indicators are revealed");
    ++doraRevealed_;
}

std::vector<Tile> Wall::doraIndicators() const {
    auto first = tiles_.begin() + kDeadWallStart + kDoraOffset;
    return {first, first + doraRevealed_};
}

std::vector<Tile> Wall::uraDoraIndicators() const {
    auto first = tiles_.begin() + kDeadWallStart + kUraDoraOffset;
    return {first, first + doraRevealed_};
}

}  // namespace mj
