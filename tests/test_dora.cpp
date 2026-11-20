#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "TestHelpers.hpp"
#include "scoring/Scorer.hpp"

using namespace mj;
using namespace mj::test;

namespace {

// `tiles` are all the player's own tiles, including those that go into `melds`.
WinContext withDora(const char* tiles, const char* indicators, const std::vector<Meld>& melds = {}) {
    WinContext ctx;
    ctx.hand = Hand(parseTiles(tiles));
    for (const Meld& meld : melds) ctx.hand.addMeld(meld);
    ctx.doraIndicators = parseTiles(indicators);
    return ctx;
}

int dora(const WinContext& ctx) { return countDora(ctx).dora; }
int aka(const WinContext& ctx) { return countDora(ctx).akaDora; }

constexpr const char* kHand = "123m456s789s234p55z";  // no red fives

}  // namespace

TEST_CASE("Each tile of the indicated kind is one dora") {
    CHECK(dora(withDora(kHand, "")) == 0);
    CHECK(dora(withDora(kHand, "1m")) == 1);    // 2m
    CHECK(dora(withDora(kHand, "7z")) == 2);    // red dragon indicates white: 55z
    CHECK(dora(withDora(kHand, "9p")) == 0);    // 1p, not in the hand
    CHECK(dora(withDora(kHand, "1m7z")) == 3);
    CHECK(dora(withDora(kHand, "7z7z")) == 4);  // two indicators for the same kind
}

TEST_CASE("Red fives are aka dora, and also count as normal fives") {
    CHECK(aka(withDora(kHand, "")) == 0);
    CHECK(aka(withDora("123m406s789s234p55z", "")) == 1);
    CHECK(aka(withDora("123m406s789s034p55z", "")) == 2);  // "034p" is 0p 3p 4p

    DoraCount both = countDora(withDora("123m406s789s234p55z", "4s"));
    CHECK(both.dora == 1);  // the red 5s is a dora 5s ...
    CHECK(both.akaDora == 1);  // ... and an aka dora
}

TEST_CASE("Called melds count, a kan with all four tiles") {
    // Pon of 2m: two from the hand, one called.
    CHECK(dora(withDora("22m456s789s234p55z", "1m", {{MeldType::Pon, parseTiles("22m"), tile("2m"), 2}})) == 3);
    CHECK(dora(withDora("2222m456s789s234p55z", "1m",
                        {{MeldType::ClosedKan, parseTiles("2222m"), std::nullopt, 0}})) == 4);
    CHECK(dora(withDora("222m456s789s234p55z", "1m", {{MeldType::OpenKan, parseTiles("222m"), tile("2m"), 2}})) == 4);
}

TEST_CASE("A red five in a meld counts, also as the called tile") {
    CHECK(aka(withDora("05p456s789s123m55z", "", {{MeldType::Pon, parseTiles("05p"), tile("5p"), 2}})) == 1);
    CHECK(aka(withDora("55p456s789s123m55z", "", {{MeldType::Pon, parseTiles("55p"), tile("0p"), 2}})) == 1);
}

TEST_CASE("Ura dora only count in riichi") {
    WinContext ctx = withDora(kHand, "1m");
    ctx.uraDoraIndicators = parseTiles("7z");

    CHECK(countDora(ctx).uraDora == 0);
    ctx.riichi = true;
    CHECK(countDora(ctx).uraDora == 2);
    CHECK(countDora(ctx).dora == 1);  // counted separately
}

TEST_CASE("Dora are counted without checking the hand") {
    // An incomplete hand: countDora does not care whether there is a win.
    CHECK(dora(withDora("1239m", "8m")) == 1);
}
