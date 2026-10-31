#pragma once

#include <array>
#include <random>

#include "game/Player.hpp"
#include "game/RoundController.hpp"
#include "rules/RuleSet.hpp"
#include "rules/Scorer.hpp"

namespace mj {

struct GameResult {
    // By player, in the order the players were given to the Game.
    std::array<int, 4> scores;
    std::array<int, 4> ranks;  // 1 to 4; ties go to the player seated closer to the first dealer
    int riichiSticksLeft;      // sticks still on the table at the end
};

// Score changes by seat after a round. A win pays its value (ron: the
// discarder alone; tsumo: everyone, the dealer double for a non-dealer win)
// plus 300 per honba, and the winner takes the riichi sticks. With several
// winners, the discarder pays each one, and honba and sticks go to the first
// winner in turn order. An exhaustive draw pays 3000 from the noten to the
// tenpai players. Riichi deposits made during the round are not included:
// they are already in the finished round's scores.
std::array<int, 4> payments(const RoundResult& result, int dealer, int honba, int riichiSticks);

// Plays a whole game (tonpuusen or hanchan): one RoundController per round,
// with dealer rotation, honba, riichi sticks carried over, and the game end.
// The players are seated in random order; seat 0 is the first dealer. The game
// ends when the dealer of the last round (East 4 or South 4) loses the seat,
// or with endOnBust as soon as a player is below 0.
class Game {
public:
    // `seed` decides the seating and every wall, so a game can be replayed.
    // The scorer's rules are the rules of the game.
    Game(std::array<IPlayer*, 4> players, Scorer scorer, unsigned seed);

    // Plays all remaining rounds and returns the final result.
    GameResult play();

    // Plays the next round, then moves on to the following one (dealer, honba,
    // sticks, round wind). For a UI that wants to show something between rounds.
    // Throws std::logic_error if the game is already over.
    RoundResult playRound();

    bool isOver() const { return over_; }
    GameResult result() const;

    const RuleSet& rules() const { return scorer_.rules(); }

    // State of the next round to be played; seats, not players.
    Honor roundWind() const { return roundWind_; }
    int dealer() const { return dealer_; }
    int honba() const { return honba_; }
    int riichiSticks() const { return riichiSticks_; }
    const std::array<int, 4>& scores() const { return scores_; }

private:
    // Payments, dealer rotation, honba, sticks, round wind and game end after a round.
    void advance(const RoundResult& result, const RoundState& finished);

    std::mt19937 rng_;
    Scorer scorer_;
    std::array<int, 4> seatOfPlayer_;      // seatOfPlayer_[player] = seat
    std::array<IPlayer*, 4> playerAtSeat_;

    std::array<int, 4> scores_;  // by seat
    Honor roundWind_ = Honor::East;
    int dealer_ = 0;
    int honba_ = 0;
    int riichiSticks_ = 0;
    bool over_ = false;
};

}  // namespace mj
