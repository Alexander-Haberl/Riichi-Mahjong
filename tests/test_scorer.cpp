#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "TestHelpers.hpp"
#include "rules/Scorer.hpp"

using namespace mj;
using namespace mj::test;

namespace {

WinContext context(const char* hand, const char* winTile) {
    WinContext ctx;
    ctx.hand = Hand(parseTiles(hand));
    ctx.winTile = tile(winTile);
    return ctx;
}

}  // namespace

TEST_CASE("Incomplete hand does not score") {
    Scorer scorer = makeScorer({kAlways});
    CHECK_FALSE(scorer.score(context("123m456p789s1z2z22z", "2z")).has_value());
}

TEST_CASE("Complete hand without yaku does not score") {
    Scorer scorer = makeScorer({kRiichiOnly});
    WinContext ctx = context("123m456p789s11z222z", "2z");
    CHECK_FALSE(scorer.score(ctx).has_value());

    ctx.riichi = true;
    auto result = scorer.score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 1);
}

TEST_CASE("Scorer with no yaku at all never scores") {
    Scorer scorer = makeScorer({});
    CHECK_FALSE(scorer.score(context("123m456p789s11z222z", "2z")).has_value());
}

TEST_CASE("Han of all present yaku are added up") {
    Scorer scorer = makeScorer({kAlways, kRiichiOnly});
    WinContext ctx = context("123m456p789s11z222z", "2z");
    ctx.riichi = true;

    auto result = scorer.score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 2);
    REQUIRE(result->hanSources.size() == 2);
    CHECK(result->hanSources[0].name == "Always");
    CHECK(result->hanSources[1].name == "RiichiOnly");
}

TEST_CASE("Scorer picks the reading worth the most han") {
    // Three triplets or three sequences; only the triplet reading has AllTriplets.
    Scorer scorer = makeScorer({kAlways, kAllTriplets});
    auto result = scorer.score(context("111222333m444p55s", "5s"));

    REQUIRE(result.has_value());
    CHECK(result->han == 3);
    CHECK(std::all_of(result->reading.blocks.begin(), result->reading.blocks.end(),
                      [](const Block& b) { return b.type != BlockType::Sequence; }));
}

TEST_CASE("Called melds stay in the hand for yaku to inspect") {
    WinContext ctx = context("111222333m55s", "5s");
    ctx.hand.add(tile("4p"));
    ctx.hand.add(tile("4p"));
    ctx.hand.addMeld({MeldType::Pon, parseTiles("44p"), tile("4p"), 2});

    auto result = makeScorer({kAllTriplets}).score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 2);
    CHECK_FALSE(ctx.hand.isClosed());
}
