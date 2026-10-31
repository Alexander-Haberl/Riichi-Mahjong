#include "game/Game.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace mj {

namespace {

int roundUpToHundred(int points) {
    return (points + 99) / 100 * 100;
}

}  // namespace

std::array<int, 4> payments(const RoundResult& result, int dealer, int honba, int riichiSticks) {
    std::array<int, 4> change{};
    auto pay = [&change](int from, int to, int points) {
        change[static_cast<size_t>(from)] -= points;
        change[static_cast<size_t>(to)] += points;
    };

    switch (result.end) {
        case RoundEnd::Tsumo: {
            const Win& win = result.wins.front();
            for (int seat = 0; seat < 4; ++seat) {
                if (seat == win.seat) continue;
                // The dealer pays double, and a dealer's tsumo is paid double by everyone.
                const int factor = (seat == dealer || win.seat == dealer) ? 2 : 1;
                pay(seat, win.seat, roundUpToHundred(win.score.basicPoints * factor) + 100 * honba);
            }
            change[static_cast<size_t>(win.seat)] += 1000 * riichiSticks;
            break;
        }
        case RoundEnd::Ron: {
            // The wins are in turn order from the discarder: honba and sticks go to the first.
            for (size_t i = 0; i < result.wins.size(); ++i) {
                const Win& win = result.wins[i];
                const int factor = win.seat == dealer ? 6 : 4;
                const int honbaBonus = i == 0 ? 300 * honba : 0;
                pay(win.fromSeat, win.seat, roundUpToHundred(win.score.basicPoints * factor) + honbaBonus);
                if (i == 0) change[static_cast<size_t>(win.seat)] += 1000 * riichiSticks;
            }
            break;
        }
        case RoundEnd::ExhaustiveDraw: {
            const int tenpai = static_cast<int>(std::count(result.tenpai.begin(), result.tenpai.end(), true));
            if (tenpai == 0 || tenpai == 4) break;
            for (size_t seat = 0; seat < 4; ++seat) {
                change[seat] = result.tenpai[seat] ? 3000 / tenpai : -3000 / (4 - tenpai);
            }
            break;
        }
        case RoundEnd::Aborted:
            break;
    }
    return change;
}

Game::Game(std::array<IPlayer*, 4> players, Scorer scorer, unsigned seed, std::optional<int> viewerPlayer)
    : rng_(seed), scorer_(std::move(scorer)) {
    std::array<int, 4> playerOfSeat{};
    std::iota(playerOfSeat.begin(), playerOfSeat.end(), 0);
    std::shuffle(playerOfSeat.begin(), playerOfSeat.end(), rng_);
    for (int seat = 0; seat < 4; ++seat) {
        const int player = playerOfSeat[static_cast<size_t>(seat)];
        playerAtSeat_[static_cast<size_t>(seat)] = players[static_cast<size_t>(player)];
        seatOfPlayer_[static_cast<size_t>(player)] = seat;
    }
    if (viewerPlayer) viewerSeat_ = seatOfPlayer_[static_cast<size_t>(*viewerPlayer)];
    scores_.fill(rules().startingPoints);
}

void Game::start() {
    startRound();
    continueGame();
}

std::optional<Decision> Game::pending() const {
    if (!round_) return std::nullopt;
    return round_->pending();
}

void Game::submit(Action action) {
    if (!round_) throw std::logic_error("no decision is pending");
    round_->submit(std::move(action));
    continueGame();
}

std::vector<GameStep> Game::takeSteps() {
    std::vector<GameStep> steps = std::move(steps_);
    steps_.clear();
    return steps;
}

GameResult Game::play() {
    while (!over_) playRound();
    return result();
}

RoundResult Game::playRound() {
    startRound();
    if (!runRound()) throw std::logic_error("playRound needs an AI in every seat");
    return finishRound();
}

void Game::startRound() {
    if (over_) throw std::logic_error("the game is already over");

    RoundState state(Wall(rng_, rules().redFivesPerSuit), roundWind_, dealer_, scores_, honba_, riichiSticks_);
    round_.emplace(std::move(state), scorer_, viewerSeat_);
    round_->start();
}

bool Game::runRound() {
    while (true) {
        collectSteps();
        const std::optional<Decision>& decision = round_->pending();
        if (!decision) return true;
        IPlayer* player = playerAtSeat_[static_cast<size_t>(decision->seat)];
        if (!player) return false;
        round_->submit(player->chooseAction(PlayerView(round_->state(), decision->seat), decision->legal));
    }
}

RoundResult Game::finishRound() {
    const RoundResult result = *round_->result();
    RoundState finished = round_->state();

    const std::array<int, 4> change = payments(result, dealer_, honba_, finished.riichiSticks);
    advance(result, finished, change);

    // The table after the payments.
    for (int seat = 0; seat < 4; ++seat) finished.player(seat).score = scores_[static_cast<size_t>(seat)];
    finished.riichiSticks = riichiSticks_;

    GameEvent roundEnd{EventType::RoundEnd};
    roundEnd.scoreChanges = change;
    record(std::move(roundEnd), finished);
    if (over_) record({EventType::GameEnd}, finished);

    round_.reset();
    return result;
}

void Game::continueGame() {
    while (runRound()) {
        finishRound();
        if (over_) return;
        startRound();
    }
}

void Game::collectSteps() {
    for (GameStep& step : round_->takeSteps()) steps_.push_back(std::move(step));
}

void Game::record(GameEvent event, const RoundState& state) {
    if (viewerSeat_) steps_.push_back({std::move(event), PlayerView(state, *viewerSeat_)});
}

void Game::advance(const RoundResult& result, const RoundState& finished, const std::array<int, 4>& change) {
    // The finished round's scores already include its riichi deposits.
    for (int seat = 0; seat < 4; ++seat) {
        scores_[static_cast<size_t>(seat)] = finished.player(seat).score + change[static_cast<size_t>(seat)];
    }

    bool dealerKeeps = false;
    switch (result.end) {
        case RoundEnd::Tsumo:
        case RoundEnd::Ron:
            dealerKeeps = std::any_of(result.wins.begin(), result.wins.end(),
                                      [this](const Win& win) { return win.seat == dealer_; });
            honba_ = dealerKeeps ? honba_ + 1 : 0;
            riichiSticks_ = 0;  // taken by the winner
            break;
        case RoundEnd::ExhaustiveDraw:
            dealerKeeps = rules().renchanOnDealerTenpai && result.tenpai[static_cast<size_t>(dealer_)];
            ++honba_;
            riichiSticks_ = finished.riichiSticks;
            break;
        case RoundEnd::Aborted:
            dealerKeeps = rules().renchanOnAbort;
            ++honba_;
            riichiSticks_ = finished.riichiSticks;
            break;
    }

    if (rules().endOnBust && std::any_of(scores_.begin(), scores_.end(), [](int score) { return score < 0; })) {
        over_ = true;
        return;
    }
    if (dealerKeeps) return;

    dealer_ = seatAdd(dealer_, 1);
    if (dealer_ != 0) return;
    // The seat went round the table: next round wind, or the end of the game.
    const Honor lastWind = rules().gameLength == GameLength::Tonpuusen ? Honor::East : Honor::South;
    if (roundWind_ == lastWind) {
        over_ = true;
        return;
    }
    roundWind_ = static_cast<Honor>(static_cast<int>(roundWind_) + 1);
}

GameResult Game::result() const {
    GameResult result{};
    for (int player = 0; player < 4; ++player) {
        result.scores[static_cast<size_t>(player)] = scores_[static_cast<size_t>(seatOfPlayer_[static_cast<size_t>(player)])];
    }

    // Seats by score; stable_sort keeps tied seats in seat order, so the seat
    // closer to the first dealer (seat 0) ranks higher.
    std::array<int, 4> seatsByScore{};
    std::iota(seatsByScore.begin(), seatsByScore.end(), 0);
    std::stable_sort(seatsByScore.begin(), seatsByScore.end(), [this](int a, int b) {
        return scores_[static_cast<size_t>(a)] > scores_[static_cast<size_t>(b)];
    });
    for (int rank = 0; rank < 4; ++rank) {
        const int seat = seatsByScore[static_cast<size_t>(rank)];
        for (int player = 0; player < 4; ++player) {
            if (seatOfPlayer_[static_cast<size_t>(player)] == seat) result.ranks[static_cast<size_t>(player)] = rank + 1;
        }
    }
    result.riichiSticksLeft = riichiSticks_;
    return result;
}

}  // namespace mj
