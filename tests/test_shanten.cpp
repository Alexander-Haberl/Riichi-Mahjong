#include <catch2/catch_test_macros.hpp>

#include "rules/Shanten.hpp"

using namespace mj;

TEST_CASE("Regular hands") {
    CHECK(shanten::calc(parseCounts("123m456p789s11z222z")) == -1);  // complete
    CHECK(shanten::calc(parseCounts("123m456p789s1z222z")) == 0);    // tanki wait
    CHECK(shanten::calc(parseCounts("123m456p78s11z222z")) == 0);    // ryanmen wait
    CHECK(shanten::calc(parseCounts("123m456p78s1z2z333z")) == 1);
    CHECK(shanten::calc(parseCounts("1112345678999m")) == 0);        // nine gates
    CHECK(shanten::calc(parseCounts("11123455678999m")) == -1);
}

TEST_CASE("Hand with called melds") {
    CHECK(shanten::calc(parseCounts("123m11z"), 3) == -1);
    CHECK(shanten::calc(parseCounts("1m"), 4) == 0);
    CHECK(shanten::calc(parseCounts("13m5p8s"), 3) == 1);
    // Chiitoitsu and kokushi are impossible once a meld is called.
    CHECK(shanten::calc(parseCounts("1133557m"), 2) == shanten::regular(parseCounts("1133557m"), 2));
}

TEST_CASE("Chiitoitsu") {
    TileCounts sixPairs = parseCounts("113355m1133p11s2z");
    CHECK(shanten::chiitoitsu(sixPairs) == 0);
    CHECK(shanten::calc(sixPairs) == 0);

    // Four of a kind is only one pair: 5 pairs + 6z still need two new pairs.
    CHECK(shanten::chiitoitsu(parseCounts("1111m2233p4455s6z")) == 2);
}

TEST_CASE("Kokushi shanten") {
    CHECK(shanten::kokushi(parseCounts("19m19p19s1234567z")) == 0);   // 13-sided wait
    CHECK(shanten::kokushi(parseCounts("119m19p19s1234567z")) == -1);
    CHECK(shanten::kokushi(parseCounts("119m19p19s123456z5m")) == 0);
}

TEST_CASE("Terrible starting hand takes the best form") {
    TileCounts c = parseCounts("147m258p369s1234z");
    CHECK(shanten::regular(c) == 8);
    CHECK(shanten::chiitoitsu(c) == 6);
    CHECK(shanten::kokushi(c) == 7);
    CHECK(shanten::calc(c) == 6);
}

TEST_CASE("Ukeire counts the unseen tiles that lower the shanten") {
    // Tenpai on 6s and 9s.
    TileCounts tenpai = parseCounts("123m456p78s11z222z");
    CHECK(shanten::ukeire(tenpai, 0, tenpai) == 8);

    // One away from tenpai: a pair with 1z or 2z (3 + 3), or the set with 6s or 9s (4 + 4).
    TileCounts oneAway = parseCounts("123m456p78s1z2z333z");
    CHECK(shanten::ukeire(oneAway, 0, oneAway) == 14);

    // 13-sided kokushi: three unseen copies of 13 kinds.
    TileCounts kokushi = parseCounts("19m19p19s1234567z");
    CHECK(shanten::ukeire(kokushi, 0, kokushi) == 39);
}

TEST_CASE("Ukeire only counts tiles that are still unseen") {
    TileCounts tenpai = parseCounts("123m456p78s11z222z");
    TileCounts visible = tenpai;
    const size_t nine = Tile::make(Suit::Sou, 9).kind();

    visible[nine] = 2;
    CHECK(shanten::ukeire(tenpai, 0, visible) == 6);
    visible[nine] = 4;
    CHECK(shanten::ukeire(tenpai, 0, visible) == 4);  // only the 6s are left
}

TEST_CASE("Ukeire with called melds") {
    // Three melds called. 2m completes the kanchan; 5p or 8s make the pair (3 each).
    TileCounts concealed = parseCounts("13m5p8s");
    CHECK(shanten::calc(concealed, 3) == 1);
    CHECK(shanten::ukeire(concealed, 3, concealed) == 10);
}
