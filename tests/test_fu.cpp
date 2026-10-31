#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "TestHelpers.hpp"
#include "rules/Scorer.hpp"

using namespace mj;
using namespace mj::test;

namespace {

Reading onlyReading(const WinContext& ctx) {
    auto rs = readings(ctx.hand, ctx.winTile, !ctx.tsumo);
    REQUIRE(rs.size() == 1);
    return rs[0];
}

int unrounded(const WinContext& ctx) {
    return unroundedFu(onlyReading(ctx), ctx);
}

int rounded(const WinContext& ctx) {
    return calculateFu(onlyReading(ctx), ctx);
}

// Three sequences and a ryanmen wait on 2p, so only the tested set and the
// pair decide the fu. A closed ron starts at 30, a tsumo at 22.
constexpr const char* kWin = "2p";

std::string hand(const char* set, const char* pair) {
    return std::string(set) + "456s789s234p" + pair;
}

}  // namespace

TEST_CASE("Pinfu: 30 fu on ron, 20 on tsumo") {
    CHECK(unrounded(win("123m456s789s234p55s", kWin, false)) == 30);
    CHECK(rounded(win("123m456s789s234p55s", kWin, false)) == 30);

    CHECK(unrounded(win("123m456s789s234p55s", kWin, true)) == 20);
    CHECK(rounded(win("123m456s789s234p55s", kWin, true)) == 20);
}

TEST_CASE("Chiitoitsu is always 25 fu, kokushi has none") {
    for (bool tsumo : {false, true}) {
        CHECK(unrounded(win("1133557799m1155p", "5p", tsumo)) == 25);
        CHECK(rounded(win("1133557799m1155p", "5p", tsumo)) == 25);
    }
    CHECK(unrounded(win("19m19p19s12345677z", "7z", false)) == 0);
}

TEST_CASE("Kanchan, penchan and tanki give 2 fu") {
    // Closed ron: 30 plus the wait.
    CHECK(unrounded(win("123m456s789s234p55s", "3p", false)) == 32);  // kanchan
    CHECK(unrounded(win("123m456s789s123p55s", "3p", false)) == 32);  // penchan 1-2 waiting on 3
    CHECK(unrounded(win("789m456s789s234p55s", "7m", false)) == 32);  // penchan 8-9 waiting on 7
    CHECK(unrounded(win("123m456s789s234p44z", "4z", false)) == 32);  // tanki
    CHECK(unrounded(win("123m456s789s234p55s", "2p", false)) == 30);  // ryanmen: nothing
}

TEST_CASE("Triplets and kans") {
    struct Case {
        const char* name;
        const char* set;  // the tested set as the player holds it
        std::vector<Meld> melds;
        int ron;  // unrounded fu
        int tsumo;
    };
    const std::vector<Case> cases = {
        {"closed simple triplet", "222m", {}, 34, 26},
        {"closed terminal triplet", "111m", {}, 38, 30},
        {"closed simple kan", "2222m", {{MeldType::ClosedKan, parseTiles("2222m"), std::nullopt, 0}}, 46, 38},
        {"closed terminal kan", "1111m", {{MeldType::ClosedKan, parseTiles("1111m"), std::nullopt, 0}}, 62, 54},
        // Open hands: no 10 fu for the ron.
        {"open simple triplet", "22m", {{MeldType::Pon, parseTiles("22m"), tile("2m"), 2}}, 22, 24},
        {"open terminal triplet", "11m", {{MeldType::Pon, parseTiles("11m"), tile("1m"), 2}}, 24, 26},
        {"open simple kan", "222m", {{MeldType::OpenKan, parseTiles("222m"), tile("2m"), 2}}, 28, 30},
        {"open terminal kan", "111m", {{MeldType::OpenKan, parseTiles("111m"), tile("1m"), 2}}, 36, 38},
    };

    for (const Case& c : cases) {
        INFO(c.name);
        CHECK(unrounded(win(hand(c.set, "44z").c_str(), kWin, false, c.melds)) == c.ron);
        CHECK(unrounded(win(hand(c.set, "44z").c_str(), kWin, true, c.melds)) == c.tsumo);
    }
}

TEST_CASE("Fu for a triplet completed by ron") {
    // Shanpon on 1m: on ron the 111m is an open terminal triplet (4), on tsumo a closed one (8).
    WinContext ron = win("111m456s789s234p44z", "1m", false);
    CHECK(onlyReading(ron).wait == WaitType::Shanpon);
    CHECK(unrounded(ron) == 34);  // the hand still gets the 10 fu for a closed ron
    CHECK(rounded(ron) == 40);

    WinContext tsumo = win("111m456s789s234p44z", "1m", true);
    CHECK(unrounded(tsumo) == 30);
    CHECK(rounded(tsumo) == 30);
}

TEST_CASE("Pair fu from dragons and winds") {
    auto fu = [](const char* pair, Honor seat, Honor round, bool tsumo) {
        WinContext ctx = win(hand("123m", pair).c_str(), kWin, tsumo);
        ctx.seatWind = seat;
        ctx.roundWind = round;
        return unrounded(ctx);
    };

    // Closed ron, so 30 without pair fu.
    CHECK(fu("55s", Honor::East, Honor::East, false) == 30);
    CHECK(fu("44z", Honor::East, Honor::East, false) == 30);  // North is neither seat nor round wind
    for (const char* dragon : {"55z", "66z", "77z"}) {
        INFO(dragon);
        CHECK(fu(dragon, Honor::East, Honor::East, false) == 32);
    }
    CHECK(fu("22z", Honor::South, Honor::East, false) == 32);  // seat wind
    CHECK(fu("11z", Honor::South, Honor::East, false) == 32);  // round wind
    CHECK(fu("11z", Honor::South, Honor::South, false) == 30);
    CHECK(fu("11z", Honor::East, Honor::East, false) == 34);   // both
    CHECK(fu("22z", Honor::South, Honor::South, true) == 26);

    // A pair with fu rules out pinfu: no flat 20 for the tsumo.
    CHECK(fu("55z", Honor::East, Honor::East, true) == 24);
    CHECK(fu("44z", Honor::East, Honor::East, true) == 20);
}

TEST_CASE("Open hands get nothing for ron, and never fewer than 30 fu") {
    const std::vector<Meld> chi = {{MeldType::Chi, parseTiles("89s"), tile("7s"), 3}};

    WinContext ron = win("123m456m234p55s89s", kWin, false, chi);
    CHECK(onlyReading(ron).handOpen);
    CHECK(unrounded(ron) == 20);
    CHECK(rounded(ron) == 30);

    WinContext tsumo = win("123m456m234p55s89s", kWin, true, chi);
    CHECK(unrounded(tsumo) == 22);
    CHECK(rounded(tsumo) == 30);
}

TEST_CASE("Fu are rounded up to the next 10") {
    CHECK(rounded(win("123m456s789s234p55z", kWin, false)) == 40);  // 32
    CHECK(rounded(win("123m456s789s234p55z", kWin, true)) == 30);   // 24
    CHECK(rounded(win("1111m456s789s234p44z", kWin, false,
                      {{MeldType::ClosedKan, parseTiles("1111m"), std::nullopt, 0}})) == 70);  // 62
}

TEST_CASE("The scorer keeps the reading with the most fu") {
    // 34555m won on 5m: ryanmen with a plain pair (pinfu, 30 fu) or tanki on the pair (32, so 40).
    Scorer scorer = alwaysYaku();
    auto result = scorer.score(win("123m456p789s34555m", "5m", false));
    REQUIRE(result.has_value());
    CHECK(result->han == 1);
    CHECK(result->fu == 40);
    CHECK(result->reading.wait == WaitType::Tanki);
}
