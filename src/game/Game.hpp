#pragma once

#include <array>
#include <optional>
#include <random>
#include <vector>

#include "game/GameStep.hpp"
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
// tenpai players, or, if anyone has nagashi mangan, only a mangan tsumo to
// each of those players, without honba. Riichi deposits made during the round are not included:
// they are already in the finished round's scores.
std::array<int, 4> payments(const RoundResult& result, int dealer, int honba, int riichiSticks);

// Plays a whole game (tonpuusen or hanchan): one RoundController per round,
// with dealer rotation, honba, riichi sticks carried over, and the game end.
// The players are seated in random order; seat 0 is the first dealer. The game
// ends when the dealer of the last round (East 4 or South 4) loses the seat,
// or with endOnBust as soon as a player is below 0.
//
// A player that is nullptr is a human: the game stops at their decisions, and
// the caller answers with submit. All other players are asked directly, so
// the game runs from one human decision to the next, across rounds.
class Game {
public:
    // `seed` decides the seating and every wall, so a game can be replayed.
    // The scorer's rules are the rules of the game. With a viewerPlayer, the
    // game is recorded as GameSteps as this player sees it.
    Game(std::array<IPlayer*, 4> players, Scorer scorer, unsigned seed,
         std::optional<int> viewerPlayer = std::nullopt);

    // The rounds refer to the game's own scorer.
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // The first round's live wall will start with these tiles (see Wall::stacked);
    // later rounds are random. The deal goes 13 tiles to each seat from the dealer
    // (seat 0), then the draws in turn order. Throws std::logic_error after start().
    void stackFirstWall(std::vector<Tile> start);

    // Starts the game and runs to the first human decision, or to its end.
    void start();

    // The human decision the game is waiting for; empty if there is none.
    std::optional<Decision> pending() const;

    // Answers the pending decision and runs to the next one, or to the end.
    // Throws std::logic_error if nothing is pending or the action is not legal.
    void submit(Action action);

    // Everything recorded since the last call, in order.
    std::vector<GameStep> takeSteps();

    // Plays all remaining rounds and returns the final result. Needs an AI in every seat.
    GameResult play();

    // Plays one round and moves on to the following one. Needs an AI in every
    // seat. Throws std::logic_error if the game is already over.
    RoundResult playRound();

    bool isOver() const { return over_; }
    GameResult result() const;

    const RuleSet& rules() const { return scorer_.rules(); }

    // The seat the constructor gave this player.
    int seatOf(int player) const { return seatOfPlayer_[static_cast<size_t>(player)]; }

    // State of the current round, or the next one between rounds; seats, not players.
    Honor roundWind() const { return roundWind_; }
    int dealer() const { return dealer_; }
    int honba() const { return honba_; }
    int riichiSticks() const { return riichiSticks_; }
    const std::array<int, 4>& scores() const { return scores_; }

private:
    void startRound();
    // Answers AI decisions. Returns true when the round is over, false when
    // it waits for a human.
    bool runRound();
    // Payments and the move to the next round.
    RoundResult finishRound();
    // Runs rounds until a human has to decide or the game is over.
    void continueGame();

    // Dealer rotation, honba, sticks, round wind and game end after a round.
    void advance(const RoundResult& result, const RoundState& finished, const std::array<int, 4>& change);

    void collectSteps();
    void record(GameEvent event, const RoundState& state);

    std::mt19937 rng_;
    Scorer scorer_;
    std::array<int, 4> seatOfPlayer_;      // seatOfPlayer_[player] = seat
    std::array<IPlayer*, 4> playerAtSeat_;
    std::optional<int> viewerSeat_;

    std::optional<std::vector<Tile>> stackedStart_;  // for the first round, until it is dealt
    bool started_ = false;

    std::optional<RoundController> round_;  // the round in progress
    std::vector<GameStep> steps_;

    std::array<int, 4> scores_;  // by seat
    Honor roundWind_ = Honor::East;
    int dealer_ = 0;
    int honba_ = 0;
    int riichiSticks_ = 0;
    bool over_ = false;
};

}  // namespace mj
