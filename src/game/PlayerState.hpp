#pragma once

#include <stdexcept>
#include <vector>

#include "core/Hand.hpp"
#include "core/Tile.hpp"
#include "core/Decomposer.hpp"

namespace mj {

struct RiverTile {
    Tile tile;
    bool tsumogiri = false;   // the tile was discarded right after drawing it
    bool riichi = false;      // the riichi declaration tile
    bool calledAway = false;  // taken by another player's call
};

struct PlayerState {
    Hand hand;
    std::vector<RiverTile> river;  // every discard, including those called away
    int score = 0;
    Honor seatWind = Honor::East;

    void discard(Tile tile, bool tsumogiri, bool riichiTile) {
        if (hand.remove(tile)){
            river.push_back({tile, tsumogiri, riichiTile, false});
        }
        else {
            throw std::logic_error{"discarded tile is not in hand"};
        }
    }

    // Sets the riichi flags. Call before discarding the riichi tile, so that
    // double riichi can see whether this is the first discard.
    void declareRiichi(bool anyCall) {
        riichi = true;
        ippatsu = true;
        doubleRiichi = !anyCall && firstTurn();
    }

    // The player could have ronned and did not.
    void passedOnRon() {
        temporaryFuriten = true;
        if (riichi) riichiFuriten = true;
    }

    // Only the drawn tile while the player has just drawn.
    Tile tileDrawn() const { return hand.lastTile(); }

    void lastRiverTileCalled(){
        river.back().calledAway = true;
    }

    bool isDealer() const {
        return seatWind == Honor::East;
    }

    bool firstTurn() const { return river.empty(); }

    bool isFuriten() const {
        if (temporaryFuriten || riichiFuriten) return true;
        for (Tile wait : waits(hand.counts())) {
            for (const RiverTile& discard : river) {
                if (discard.tile.sameKind(wait)) return true;
            }
        }
        return false;
    }

    bool riichi = false;
    bool doubleRiichi = false;
    bool ippatsu = false;           // riichi declared and not yet broken by a call or own discard
    bool temporaryFuriten = false;  // passed on a ron since the own last discard
    bool riichiFuriten = false;     // passed on a ron after declaring riichi
};

}  // namespace mj
