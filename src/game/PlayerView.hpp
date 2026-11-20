#pragma once

#include <array>
#include <vector>

#include "game/RoundState.hpp"

namespace mj {

// What one seat is allowed to see of the round at one moment: its own hand, and
// the public information about everyone. No other hands, no wall order. A view
// is a copy: it stays valid and unchanged while the round goes on.
class PlayerView {
public:
    PlayerView(const RoundState& state, int seat)
        : seat_(seat), hand_(state.player(seat).hand), furiten_(state.player(seat).isFuriten()),
          roundWind_(state.roundWind), dealer_(state.dealer), honba_(state.honba),
          riichiSticks_(state.riichiSticks), currentSeat_(state.current),
          wallRemaining_(state.wall.remaining()), doraIndicators_(state.wall.doraIndicators()) {
        for (int who = 0; who < 4; ++who) {
            const PlayerState& player = state.player(who);
            seats_[static_cast<size_t>(who)] = {player.river,  player.hand.melds(), player.hand.concealedCount(),
                                                player.score,  player.riichi,       player.seatWind};
        }
    }

    int seat() const { return seat_; }
    const Hand& hand() const { return hand_; }
    // Whether this seat is furiten; only known for the own seat. A hand that has
    // just drawn or called has no waits, so until its discard this only reports
    // a ron passed on, not a wait in the own river.
    bool furiten() const { return furiten_; }

    const std::vector<RiverTile>& river(int who) const { return at(who).river; }
    const std::vector<Meld>& melds(int who) const { return at(who).melds; }
    int concealedCount(int who) const { return at(who).concealedCount; }
    int score(int who) const { return at(who).score; }
    bool riichi(int who) const { return at(who).riichi; }
    Honor seatWind(int who) const { return at(who).seatWind; }

    Honor roundWind() const { return roundWind_; }
    int dealer() const { return dealer_; }
    int honba() const { return honba_; }
    int riichiSticks() const { return riichiSticks_; }
    int currentSeat() const { return currentSeat_; }
    int wallRemaining() const { return wallRemaining_; }
    const std::vector<Tile>& doraIndicators() const { return doraIndicators_; }

    // All tiles this seat can see: its own hand, all rivers and melds, and the
    // dora indicators. A tile called away from a river counts once, in its meld.
    TileCounts visibleTiles() const {
        TileCounts visible = hand_.counts();
        for (Tile t : doraIndicators_) ++visible[t.kind()];
        for (const Seat& seat : seats_) {
            for (const RiverTile& discard : seat.river) {
                if (!discard.calledAway) ++visible[discard.tile.kind()];
            }
            for (const Meld& meld : seat.melds) {
                for (Tile t : meld.tiles()) ++visible[t.kind()];
            }
        }
        return visible;
    }

private:
    // The public information about one seat.
    struct Seat {
        std::vector<RiverTile> river;
        std::vector<Meld> melds;
        int concealedCount = 0;
        int score = 0;
        bool riichi = false;
        Honor seatWind = Honor::East;
    };

    const Seat& at(int who) const { return seats_[static_cast<size_t>(who)]; }

    int seat_;
    Hand hand_;
    bool furiten_;
    std::array<Seat, 4> seats_;
    Honor roundWind_;
    int dealer_;
    int honba_;
    int riichiSticks_;
    int currentSeat_;
    int wallRemaining_;
    std::vector<Tile> doraIndicators_;
};

}  // namespace mj
