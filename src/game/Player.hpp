#pragma once

#include <vector>

#include "game/Action.hpp"
#include "game/RoundState.hpp"

namespace mj {

// What one seat is allowed to see of the round: its own hand, and the
// public information about everyone. No other hands, no wall order.
class PlayerView {
public:
    PlayerView(const RoundState& state, int seat) : state_(state), seat_(seat) {}

    int seat() const { return seat_; }
    const Hand& hand() const { return getPlayerState(seat_).hand; }

    const std::vector<RiverTile>& river(int player) const { return getPlayerState(player).river; }
    const std::vector<Meld>& melds(int player) const { return getPlayerState(player).hand.melds(); }
    int score(int player) const { return getPlayerState(player).score; }
    bool riichi(int player) const { return getPlayerState(player).riichi; }
    Honor seatWind(int player) const { return getPlayerState(player).seatWind; }

    Honor roundWind() const { return state_.roundWind; }
    int dealer() const { return state_.dealer; }
    int honba() const { return state_.honba; }
    int riichiSticks() const { return state_.riichiSticks; }
    int currentSeat() const { return state_.current; }
    int wallRemaining() const { return state_.wall.remaining(); }
    std::vector<Tile> doraIndicators() const { return state_.wall.doraIndicators(); }

    // All tiles this seat can see: its own hand, all rivers and melds, and the
    // dora indicators. A tile called away from a river counts once, in its meld.
    TileCounts visibleTiles() const {
        TileCounts visible = hand().counts();
        for (Tile t : doraIndicators()) ++visible[t.kind()];
        for (int player = 0; player < 4; ++player) {
            for (const RiverTile& discard : river(player)) {
                if (!discard.calledAway) ++visible[discard.tile.kind()];
            }
            for (const Meld& meld : melds(player)) {
                for (Tile t : meld.tiles()) ++visible[t.kind()];
            }
        }
        return visible;
    }

private:
    const PlayerState& getPlayerState(int player) const { return state_.players[static_cast<size_t>(player)]; }

    const RoundState& state_;
    int seat_;
};

// A seat at the table: a human through the UI, or an AI.
class IPlayer {
public:
    virtual ~IPlayer() = default;

    // Picks one of `legal` (never empty). Called on the player's own turn and
    // whenever they can react to another player's tile.
    virtual Action chooseAction(const PlayerView& view, const std::vector<Action>& legal) = 0;
};

}  // namespace mj
