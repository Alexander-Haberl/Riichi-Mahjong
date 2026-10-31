#pragma once

#include <array>
#include <optional>

#include "core/Hand.hpp"
#include "core/Wall.hpp"
#include "game/PlayerState.hpp"

namespace mj {

// Seat of `other` as seen from `seat`: 1 = right, 2 = across, 3 = left.
constexpr int relativeSeat(int seat, int other) { return (other - seat + 4) % 4; }

// The seat `offset` after `seat` in normal turn order.
constexpr int seatAdd(int seat, int offset) { return (seat + offset) % 4; }

// Everything about one round (one hand). Plain data: the RoundController
// changes it, the legal action functions and player views read it.
struct RoundState {
    RoundState(Wall startWall, Honor round, int dealerSeat, const std::array<int, 4>& scores, int honbaCount = 0,
               int sticks = 0)
        : wall(std::move(startWall)), roundWind(round), dealer(dealerSeat), honba(honbaCount), riichiSticks(sticks),
          current(dealerSeat) {
        for (int seat = 0; seat < 4; ++seat) {
            players[seat].score = scores[seat];
            players[seat].seatWind = static_cast<Honor>(relativeSeat(dealer, seat));
        }
    }

    PlayerState& currentPlayer() { return players[static_cast<size_t>(current)]; }
    const PlayerState& currentPlayer() const { return players[static_cast<size_t>(current)]; }
    PlayerState& player(int seat) { return players[static_cast<size_t>(seat)]; }
    const PlayerState& player(int seat) const { return players[static_cast<size_t>(seat)]; }
    bool currentIsDealer() const { return dealer == current; }

    Wall wall;
    std::array<PlayerState, 4> players;
    Honor roundWind;
    int dealer;
    int honba;
    int riichiSticks;

    int current;                   // seat whose turn it is
    bool tileDraw = false;         // the current player drew or is to draw this turn (false: they called chi / pon)
    bool rinshan = false;          // the drawn tile came or is to come from the dead wall
    std::optional<Meld> lastCall;  // chi / pon the current player just made, for kuikae
    bool anyCall = false;          // a call or kan happened this round (ends the first go-around)
};

}  // namespace mj
