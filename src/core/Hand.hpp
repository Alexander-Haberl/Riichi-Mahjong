#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/Tile.hpp"

namespace mj {

enum class MeldType { Chi, Pon, OpenKan, ClosedKan };

struct Meld {
    MeldType type;
    std::vector<Tile> handTiles;     // tiles that came from the player's own hand
    std::optional<Tile> calledTile;  // tile taken from a discard; empty for a closed kan
    int fromSeat = 0;                // seat of the discarder; the owner's own seat for a closed kan
    // Open kan made by adding a tile to an existing pon (shouminkan). Scores like
    // any open kan; only the display and some optional rules care about it.
    bool kanFromPon = false;

    bool isOpen() const { return type != MeldType::ClosedKan; }
    bool isKan() const { return type == MeldType::OpenKan || type == MeldType::ClosedKan; }
    // All tiles of the meld, sorted.
    std::vector<Tile> tiles() const {
        std::vector<Tile> all = handTiles;
        if (calledTile) all.push_back(*calledTile);
        std::sort(all.begin(), all.end());
        return all;
    }

    // Lowest tile in the meld (the first tile of a chi, a tile of the pon/kan).
    Tile baseTile() const {
        Tile lowest = *std::min_element(handTiles.begin(), handTiles.end());
        return calledTile ? std::min(lowest, *calledTile) : lowest;
    }
};

class Hand {
public:
    Hand() = default;
    explicit Hand(std::vector<Tile> concealed);

    // Concealed tiles keep the order they were added in, so a drawn tile is
    // last until something else is added.
    void add(Tile tile);
    // Removes one exactly matching tile (red flag included), keeping the order
    // of the others. Returns false if the tile is not in the concealed part.
    bool remove(Tile tile);

    // The most recently added concealed tile. It is the drawn tile only while
    // the player has just drawn; after a call it is an ordinary hand tile.
    // Throws std::logic_error if the concealed part is empty.
    Tile lastTile() const;

    // Number of concealed tiles of the same kind, red or not.
    int count(Tile kind) const;
    bool contains(Tile kind) const { return count(kind) > 0; }

    // Moves the meld's handTiles out of the concealed part.
    // Throws std::logic_error (leaving the hand unchanged) if tiles are missing.
    void addMeld(const Meld& meld);

    // Turns the pon of this tile's type into an open kan (shouminkan),
    // moving the tile out of the concealed part.
    // Throws std::logic_error (leaving the hand unchanged) if there is no such
    // pon or the tile is not in the hand.
    void addKanToPon(Tile tile);

    // Counts of the concealed tiles only.
    TileCounts counts() const;

    const std::vector<Tile>& concealed() const { return concealed_; }
    const std::vector<Meld>& melds() const { return melds_; }
    int concealedCount() const { return static_cast<int>(concealed_.size()); }
    int meldCount() const { return static_cast<int>(melds_.size()); }
    // True if there are no open melds (closed kans keep the hand closed).
    bool isClosed() const;

    std::string toString() const;

private:
    std::vector<Tile> concealed_;
    std::vector<Meld> melds_;
};

}  // namespace mj
