#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include "TestHelpers.hpp"
#include "game/Game.hpp"

using namespace mj;
using namespace mj::test;

namespace {

std::array<IPlayer*, 4> pointers(std::array<TestPlayer, 4>& players) {
    return {&players[0], &players[1], &players[2], &players[3]};
}

// Players that improve their hands, declare riichi and take every win.
void playToWin(std::array<TestPlayer, 4>& players) {
    for (TestPlayer& p : players) {
        p.minimizeShanten = true;
        p.declareRiichi = true;
        p.takeWins = true;
    }
}

// The state before a round, and how the round ended.
struct RoundRecord {
    Honor wind;
    int dealer;
    int honba;
    int riichiSticks;
    std::array<int, 4> scores;  // by seat
    RoundResult result;
};

std::vector<RoundRecord> playAll(Game& game) {
    std::vector<RoundRecord> rounds;
    while (!game.isOver()) {
        RoundRecord record{game.roundWind(), game.dealer(), game.honba(), game.riichiSticks(), game.scores(), {}};
        record.result = game.playRound();
        rounds.push_back(std::move(record));
    }
    return rounds;
}

int pointsOnTable(const Game& game) {
    return std::accumulate(game.scores().begin(), game.scores().end(), 0) + 1000 * game.riichiSticks();
}

// Every round ends in a draw: nobody can win, nobody calls or declares anything.
RuleSet drawOnlyRules() {
    RuleSet rules = testRules();
    rules.renchanOnDealerTenpai = false;
    rules.renchanOnAbort = false;
    rules.endOnBust = false;
    return rules;
}

}  // namespace

TEST_CASE("Without renchan every dealer deals once per round wind") {
    std::array<TestPlayer, 4> players;
    RuleSet rules = drawOnlyRules();

    SECTION("hanchan") {
        rules.gameLength = GameLength::Hanchan;
        Game game(pointers(players), noYaku(rules), 1);
        auto rounds = playAll(game);

        REQUIRE(rounds.size() == 8);
        for (size_t i = 0; i < rounds.size(); ++i) {
            INFO("round " << i);
            CHECK(rounds[i].wind == (i < 4 ? Honor::East : Honor::South));
            CHECK(rounds[i].dealer == static_cast<int>(i % 4));
            CHECK(rounds[i].honba == static_cast<int>(i));  // every draw adds one
        }
    }
    SECTION("tonpuusen") {
        rules.gameLength = GameLength::Tonpuusen;
        Game game(pointers(players), noYaku(rules), 1);
        auto rounds = playAll(game);

        REQUIRE(rounds.size() == 4);
        CHECK(rounds.back().wind == Honor::East);
        CHECK(rounds.back().dealer == 3);
    }
}

TEST_CASE("Dealer rotation, honba and sticks follow the round result") {
    std::array<TestPlayer, 4> players;
    playToWin(players);
    RuleSet rules = testRules();
    rules.renchanOnDealerTenpai = true;
    rules.renchanOnAbort = true;
    rules.endOnBust = false;
    Game game(pointers(players), alwaysYaku(rules), 3);
    auto rounds = playAll(game);
    REQUIRE(rounds.size() >= 8);

    int wins = 0;
    for (size_t i = 0; i + 1 < rounds.size(); ++i) {
        INFO("round " << i);
        const RoundRecord& before = rounds[i];
        const RoundRecord& after = rounds[i + 1];
        const RoundResult& result = before.result;
        const int nextDealer = seatAdd(before.dealer, 1);

        if (result.end == RoundEnd::Tsumo || result.end == RoundEnd::Ron) {
            ++wins;
            const bool dealerWon = std::any_of(result.wins.begin(), result.wins.end(),
                                               [&](const Win& w) { return w.seat == before.dealer; });
            CHECK(after.dealer == (dealerWon ? before.dealer : nextDealer));
            CHECK(after.honba == (dealerWon ? before.honba + 1 : 0));
            CHECK(after.riichiSticks == 0);
        } else if (result.end == RoundEnd::ExhaustiveDraw) {
            const bool dealerTenpai = result.tenpai[static_cast<size_t>(before.dealer)];
            CHECK(after.dealer == (dealerTenpai ? before.dealer : nextDealer));
            CHECK(after.honba == before.honba + 1);
            CHECK(after.riichiSticks >= before.riichiSticks);  // carried over, plus this round's riichi
        } else {
            CHECK(after.dealer == before.dealer);  // renchanOnAbort
            CHECK(after.honba == before.honba + 1);
        }
    }
    CHECK(wins > 0);
}

TEST_CASE("Points are neither created nor lost") {
    std::array<TestPlayer, 4> players;
    playToWin(players);
    RuleSet rules = testRules();
    rules.endOnBust = false;
    Game game(pointers(players), alwaysYaku(rules), 5);

    CHECK(pointsOnTable(game) == 100000);
    while (!game.isOver()) {
        game.playRound();
        CHECK(pointsOnTable(game) == 100000);  // scores plus the riichi sticks still on the table
    }
    GameResult result = game.result();
    CHECK(std::accumulate(result.scores.begin(), result.scores.end(), 0) + 1000 * result.riichiSticksLeft == 100000);
}

TEST_CASE("The game ends as soon as a player busts") {
    std::array<TestPlayer, 4> players;
    for (TestPlayer& p : players) p.minimizeShanten = true;  // so that some get tenpai
    RuleSet rules = drawOnlyRules();
    rules.startingPoints = 500;  // any noten payment busts
    rules.endOnBust = true;
    Game game(pointers(players), noYaku(rules), 2);
    auto rounds = playAll(game);

    REQUIRE(rounds.size() < 8);
    CHECK(*std::min_element(game.scores().begin(), game.scores().end()) < 0);
    for (size_t i = 0; i < rounds.size(); ++i) {
        INFO("round " << i);
        CHECK(*std::min_element(rounds[i].scores.begin(), rounds[i].scores.end()) >= 0);  // nobody busted before
    }
}

TEST_CASE("The final result is reported by player") {
    std::array<TestPlayer, 4> players;
    playToWin(players);
    Game game(pointers(players), alwaysYaku(), 4);
    GameResult result = game.play();

    std::array<int, 4> ranks = result.ranks;
    std::sort(ranks.begin(), ranks.end());
    CHECK(ranks == std::array<int, 4>{1, 2, 3, 4});

    for (int a = 0; a < 4; ++a) {
        CHECK(result.scores[static_cast<size_t>(a)] == game.scores()[static_cast<size_t>(players[static_cast<size_t>(a)].seat)]);
        for (int b = 0; b < 4; ++b) {
            if (result.scores[static_cast<size_t>(a)] > result.scores[static_cast<size_t>(b)]) {
                CHECK(result.ranks[static_cast<size_t>(a)] < result.ranks[static_cast<size_t>(b)]);
            }
        }
    }
}

TEST_CASE("Ties go to the player seated closer to the first dealer") {
    std::array<TestPlayer, 4> players;
    Game game(pointers(players), noYaku(drawOnlyRules()), 6);
    GameResult atStart = game.result();  // everyone still has the starting points

    game.playRound();  // now every player knows their seat
    for (size_t p = 0; p < 4; ++p) {
        INFO("player " << p);
        CHECK(atStart.ranks[p] == players[p].seat + 1);
    }
}

TEST_CASE("The seed decides seating and walls") {
    auto playWithSeed = [](unsigned seed) {
        std::array<TestPlayer, 4> players;
        playToWin(players);
        Game game(pointers(players), alwaysYaku(), seed);
        GameResult result = game.play();
        std::array<int, 4> seats{};
        for (size_t p = 0; p < 4; ++p) seats[p] = players[p].seat;
        return std::make_pair(result, seats);
    };

    auto [first, firstSeats] = playWithSeed(11);
    auto [again, againSeats] = playWithSeed(11);
    CHECK(first.scores == again.scores);
    CHECK(first.ranks == again.ranks);
    CHECK(firstSeats == againSeats);

    // Over several seeds, different players get to be the first dealer.
    std::set<int> firstDealers;
    for (unsigned seed = 0; seed < 10; ++seed) {
        std::array<TestPlayer, 4> players;
        Game game(pointers(players), noYaku(drawOnlyRules()), seed);
        game.playRound();
        for (int p = 0; p < 4; ++p) {
            if (players[static_cast<size_t>(p)].seat == 0) firstDealers.insert(p);
        }
    }
    CHECK(firstDealers.size() > 1);
}

TEST_CASE("No rounds after the end") {
    std::array<TestPlayer, 4> players;
    RuleSet rules = drawOnlyRules();
    rules.gameLength = GameLength::Tonpuusen;
    Game game(pointers(players), noYaku(rules), 1);
    game.play();

    CHECK(game.isOver());
    CHECK_THROWS_AS(game.playRound(), std::logic_error);
}

// --- Stepping ---------------------------------------------------------------

namespace {

long countSteps(const std::vector<GameStep>& steps, EventType type) {
    return std::count_if(steps.begin(), steps.end(), [type](const GameStep& s) { return s.event.type == type; });
}

}  // namespace

TEST_CASE("A game of AIs runs through to its end on start") {
    std::array<TestPlayer, 4> players;
    playToWin(players);
    RuleSet rules = testRules();
    rules.gameLength = GameLength::Tonpuusen;
    Game game(pointers(players), alwaysYaku(rules), 7, 0);

    game.start();
    CHECK(game.isOver());
    CHECK_FALSE(game.pending().has_value());

    const auto steps = game.takeSteps();
    REQUIRE_FALSE(steps.empty());
    CHECK(steps.front().event.type == EventType::RoundStart);
    CHECK(steps.back().event.type == EventType::GameEnd);
    CHECK(countSteps(steps, EventType::GameEnd) == 1);
    CHECK(countSteps(steps, EventType::RoundStart) == countSteps(steps, EventType::RoundEnd));
    CHECK(countSteps(steps, EventType::RoundStart) >= 4);
    CHECK(steps.front().view.seat() == players[0].seat);
}

TEST_CASE("The round end step shows the table after the payments") {
    std::array<TestPlayer, 4> players;
    playToWin(players);
    RuleSet rules = testRules();
    rules.gameLength = GameLength::Tonpuusen;
    Game game(pointers(players), alwaysYaku(rules), 7, 0);
    game.start();

    const auto steps = game.takeSteps();
    int roundEnds = 0;
    for (size_t i = 1; i < steps.size(); ++i) {
        if (steps[i].event.type != EventType::RoundEnd) continue;
        ++roundEnds;
        for (int seat = 0; seat < 4; ++seat) {
            INFO("step " << i << ", seat " << seat);
            const int change = steps[i].event.scoreChanges[static_cast<size_t>(seat)];
            CHECK(steps[i].view.score(seat) == steps[i - 1].view.score(seat) + change);
        }
    }
    CHECK(roundEnds >= 4);

    for (int seat = 0; seat < 4; ++seat) {
        CHECK(steps.back().view.score(seat) == game.scores()[static_cast<size_t>(seat)]);
    }
    CHECK(steps.back().view.riichiSticks() == game.riichiSticks());
}

TEST_CASE("The game stops at human decisions and plays the same as with an AI") {
    auto setHabits = [](TestPlayer& p) {
        p.minimizeShanten = true;
        p.declareRiichi = true;
        p.takeWins = true;
    };
    constexpr unsigned kSeed = 9;

    // Player 1 as an AI.
    std::array<TestPlayer, 4> ais;
    playToWin(ais);
    Game blocking(pointers(ais), alwaysYaku(), kSeed);
    const GameResult expected = blocking.play();

    // Player 1 answered from outside, with the same habits.
    std::array<TestPlayer, 4> others;
    playToWin(others);
    std::array<IPlayer*, 4> seats = pointers(others);
    seats[1] = nullptr;
    Game game(seats, alwaysYaku(), kSeed, 1);
    TestPlayer human;
    setHabits(human);

    game.start();
    std::optional<PlayerView> view;
    int decisions = 0;
    while (true) {
        for (const GameStep& step : game.takeSteps()) view = step.view;
        const std::optional<Decision> decision = game.pending();
        if (!decision) break;
        REQUIRE(view.has_value());
        CHECK(decision->seat == view->seat());  // only the human's decisions get here
        game.submit(human.chooseAction(*view, decision->legal));
        ++decisions;
    }

    CHECK(decisions > 0);
    CHECK(game.isOver());
    CHECK(game.result().scores == expected.scores);
    CHECK(game.result().ranks == expected.ranks);
}

TEST_CASE("Blocking play needs an AI in every seat") {
    std::array<TestPlayer, 4> players;
    std::array<IPlayer*, 4> seats = pointers(players);
    seats[2] = nullptr;
    Game game(seats, noYaku(), 1);

    CHECK_THROWS_AS(game.play(), std::logic_error);
}

TEST_CASE("Submit needs a pending decision") {
    std::array<TestPlayer, 4> players;
    Game game(pointers(players), noYaku(drawOnlyRules()), 1);

    CHECK_THROWS_AS(game.submit({ActionType::Pass}), std::logic_error);  // not started
}

TEST_CASE("A stacked first wall deals the chosen hands, later rounds are random") {
    std::array<TestPlayer, 4> players;
    std::array<IPlayer*, 4> seats = pointers(players);
    seats[0] = nullptr;  // the human, as viewer
    Game game(seats, alwaysYaku(), 11, 0);
    const int humanSeat = game.seatOf(0);

    // 13 tiles to each seat from the dealer (seat 0), then the dealer's draw.
    const std::array<const char*, 4> hands = {"1112345678999m", "1112345678999p", "1112345678999s", "1122334455667z"};
    std::vector<Tile> start;
    for (int seat = 0; seat < 4; ++seat) {
        const std::vector<Tile> hand = parseTiles(hands[static_cast<size_t>(seat)]);
        start.insert(start.end(), hand.begin(), hand.end());
    }
    start.push_back(tile("7z"));
    game.stackFirstWall(start);
    game.start();

    const auto steps = game.takeSteps();
    REQUIRE_FALSE(steps.empty());
    // The RoundStart step shows the hands right after the deal.
    REQUIRE(steps.front().event.type == EventType::RoundStart);
    const PlayerView& view = steps.front().view;
    CHECK(view.seat() == humanSeat);
    CHECK(view.hand().counts() == parseCounts(hands[static_cast<size_t>(humanSeat)]));

    CHECK_THROWS_AS(game.stackFirstWall(start), std::logic_error);
}

