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
    const std::vector<TileBlock>& blocks = result->decomposition.concealedBlocks;
    REQUIRE(blocks.size() == 5);
    CHECK(blocks[0] == parseTiles("111m"));  // triplets, not the sequences 123m
    CHECK(blocks[1] == parseTiles("222m"));
    CHECK(blocks[2] == parseTiles("333m"));
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

namespace {

// A yaku that only counts for one wait, so the scorer picks that reading.
Yaku onlyWith(WaitType wait) {
    return {"OnlyWith", [wait](const Reading& reading, const WinContext&) { return reading.wait == wait ? 5 : 0; }};
}

std::vector<TileBlock> blocks(std::initializer_list<const char*> tiles) {
    std::vector<TileBlock> result;
    for (const char* block : tiles) result.push_back(parseTiles(block));
    return result;
}

}  // namespace

TEST_CASE("Decomposition: blocks of the hand's tiles, red fives included") {
    Scorer scorer = makeScorer({kAlways});
    const auto result = scorer.score(win("234m406p789s11z055s", "4m", false));
    REQUIRE(result.has_value());

    const WinDecomposition& decomposition = result->decomposition;
    CHECK(decomposition.winningTile == tile("4m"));
    CHECK(decomposition.concealedBlocks == blocks({"406p", "550s", "789s", "11z", "23m"}));
    CHECK(decomposition.melds.empty());
}

TEST_CASE("Decomposition: the last block is the one the winning tile completed") {
    SECTION("ryanmen or penchan") {
        const char* hand = "123345m456p789s11z";
        auto ryanmen = makeScorer({onlyWith(WaitType::Ryanmen)}).score(win(hand, "3m", false));
        REQUIRE(ryanmen.has_value());
        CHECK(ryanmen->decomposition.concealedBlocks == blocks({"123m", "456p", "789s", "11z", "45m"}));

        auto penchan = makeScorer({onlyWith(WaitType::Penchan)}).score(win(hand, "3m", false));
        REQUIRE(penchan.has_value());
        CHECK(penchan->decomposition.concealedBlocks == blocks({"345m", "456p", "789s", "11z", "12m"}));
    }
    SECTION("tanki or penchan") {
        const char* hand = "12333m456p789s111z";
        auto tanki = makeScorer({onlyWith(WaitType::Tanki)}).score(win(hand, "3m", true));
        REQUIRE(tanki.has_value());
        CHECK(tanki->decomposition.concealedBlocks == blocks({"123m", "456p", "789s", "111z", "3m"}));

        auto penchan = makeScorer({onlyWith(WaitType::Penchan)}).score(win(hand, "3m", true));
        REQUIRE(penchan.has_value());
        CHECK(penchan->decomposition.concealedBlocks == blocks({"33m", "456p", "789s", "111z", "12m"}));
    }
    SECTION("shanpon") {
        auto result = makeScorer({kAlways}).score(win("123m456p789s11z222z", "2z", false));
        REQUIRE(result.has_value());
        CHECK(result->decomposition.concealedBlocks == blocks({"123m", "456p", "789s", "11z", "22z"}));
    }
}

TEST_CASE("Decomposition of chiitoitsu and kokushi") {
    Scorer scorer = makeScorer({kAlways});

    auto chiitoitsu = scorer.score(win("1122m3344p5566s77z", "7z", false));
    REQUIRE(chiitoitsu.has_value());
    CHECK(chiitoitsu->decomposition.concealedBlocks ==
          blocks({"11m", "22m", "33p", "44p", "55s", "66s", "7z"}));

    auto kokushi = scorer.score(win("19m19p19s12345677z", "7z", true));
    REQUIRE(kokushi.has_value());
    CHECK(kokushi->decomposition.concealedBlocks == blocks({"19m19p19s1234567z"}));
}

TEST_CASE("Decomposition: called melds are listed apart, not as blocks") {
    Scorer scorer = makeScorer({kAlways});
    const Meld pon{MeldType::Pon, parseTiles("77z"), tile("7z"), 2};

    auto result = scorer.score(win("123m456p789s11z77z", "1z", false, {pon}));
    REQUIRE(result.has_value());
    CHECK(result->decomposition.concealedBlocks == blocks({"123m", "456p", "789s", "1z"}));
    REQUIRE(result->decomposition.melds.size() == 1);
    CHECK(result->decomposition.melds[0].type == MeldType::Pon);
}

