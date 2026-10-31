#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <numeric>
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
        const int nextDealer = (before.dealer + 1) % 4;

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
