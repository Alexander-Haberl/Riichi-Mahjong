#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "TestHelpers.hpp"
#include "rules/Decomposer.hpp"

using namespace mj;
using namespace mj::test;

namespace {

long countForm(const std::vector<Reading>& ds, HandForm form) {
    return std::count_if(ds.begin(), ds.end(), [form](const Reading& d) { return d.form == form; });
}

}  // namespace

TEST_CASE("Simple complete hand has one decomposition") {
    auto ds = decomposeConcealed(parseCounts("123m456p789s11z222z"));
    REQUIRE(ds.size() == 1);
    CHECK(ds[0].form == HandForm::Regular);

    const auto& b = ds[0].blocks;
    REQUIRE(b.size() == 5);
    CHECK(b[0] == Block{BlockType::Pair, tile("1z")});
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Sequence, tile("1m")}) == 1);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Triplet, tile("2z")}) == 1);
}

TEST_CASE("Triplets of the last honor kinds") {
    // Kinds 32 (6z) and 33 (7z) have no i + 1 / i + 2 inside the counts array.
    CHECK(decomposeConcealed(parseCounts("123m456p789s11z777z")).size() == 1);
    CHECK(decomposeConcealed(parseCounts("123m456p789s11z666z")).size() == 1);
    CHECK(waits(parseCounts("123m456p789s11z77z")) == parseTiles("17z"));
}

TEST_CASE("Three triplets vs three sequences") {
    auto ds = decomposeConcealed(parseCounts("111222333m44p"));
    CHECK(ds.size() == 2);
    CHECK(countForm(ds, HandForm::Regular) == 2);
}

TEST_CASE("Same split is not found twice") {
    // 1111m with 23m: triplet + sequence, whichever is taken first.
    auto ds = decomposeConcealed(parseCounts("111123m55p"));
    REQUIRE(ds.size() == 1);
    const auto& b = ds[0].blocks;
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Triplet, tile("1m")}) == 1);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Sequence, tile("1m")}) == 1);

    // 112233m twice over: only two sequences each time, never duplicated.
    CHECK(decomposeConcealed(parseCounts("112233m11z")).size() == 1);
}

TEST_CASE("Hand that is both chiitoitsu and regular") {
    auto ds = decomposeConcealed(parseCounts("11223344556677m"));
    CHECK(countForm(ds, HandForm::Chiitoitsu) == 1);
    CHECK(countForm(ds, HandForm::Regular) == 3);
}

TEST_CASE("Kokushi decomposition") {
    auto ds = decomposeConcealed(parseCounts("19m19p19s12345677z"));
    REQUIRE(ds.size() == 1);
    CHECK(ds[0].form == HandForm::Kokushi);
}

TEST_CASE("Incomplete hands") {
    CHECK_FALSE(isComplete(parseCounts("123m456p789s1z2z22z")));
    CHECK_FALSE(isComplete(parseCounts("123m456p789s1z222z")));  // 13 tiles
    CHECK_FALSE(isComplete(parseCounts("1111m2233p4455s66z")));  // four of a kind is not two pairs
}

TEST_CASE("Hand with called melds only needs the concealed part") {
    CHECK(isComplete(parseCounts("55z")));
    CHECK(isComplete(parseCounts("234s99m")));
}

TEST_CASE("Waits") {
    CHECK(waits(parseCounts("123m456p789s1z222z")) == parseTiles("1z"));
    CHECK(waits(parseCounts("123m456p78s11z222z")) == parseTiles("69s"));
    CHECK(waits(parseCounts("2223444m")) == parseTiles("12345m"));
    CHECK(waits(parseCounts("1112345678999m")) == parseTiles("123456789m"));  // nine gates
    CHECK(waits(parseCounts("19m19p19s1234567z")) == parseTiles("19m19p19s1234567z"));
    CHECK(waits(parseCounts("1111m")).empty());  // would need a fifth 1m
}

TEST_CASE("Full decomposition includes called melds") {
    Hand hand(parseTiles("234s99m56m11z7777p"));
    hand.addMeld({MeldType::Chi, parseTiles("56m"), tile("4m"), 3});
    hand.addMeld({MeldType::Pon, parseTiles("11z"), tile("1z"), 1});
    hand.addMeld({MeldType::ClosedKan, parseTiles("7777p"), std::nullopt, 0});

    auto ds = decompose(hand);
    REQUIRE(ds.size() == 1);
    const auto& b = ds[0].blocks;
    REQUIRE(b.size() == 5);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Pair, tile("9m"), false}) == 1);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Sequence, tile("2s"), false}) == 1);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Sequence, tile("4m"), true}) == 1);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Triplet, tile("1z"), true}) == 1);
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Kan, tile("7p"), false}) == 1);
}

TEST_CASE("Kan from a pon becomes an open kan block without the red flag") {
    Hand hand(parseTiles("55s0s123m44p"));
    hand.addMeld({MeldType::Pon, parseTiles("55s"), tile("5s"), 2});
    hand.addKanToPon(tile("0s"));

    auto ds = decompose(hand);
    REQUIRE(ds.size() == 1);
    const auto& b = ds[0].blocks;
    CHECK(std::count(b.begin(), b.end(), Block{BlockType::Kan, tile("5s"), true}) == 1);
}

TEST_CASE("Every reading gets the meld blocks") {
    Hand hand(parseTiles("111222333m44p77z"));
    hand.addMeld({MeldType::Pon, parseTiles("77z"), tile("7z"), 1});

    auto ds = decompose(hand);
    REQUIRE(ds.size() == 2);
    for (const auto& d : ds) {
        CHECK(d.blocks.size() == 5);
        CHECK(d.blocks.back() == Block{BlockType::Triplet, tile("7z"), true});
    }
}

TEST_CASE("Incomplete hand with melds has no decomposition") {
    Hand hand(parseTiles("123m45p111z"));
    hand.addMeld({MeldType::Pon, parseTiles("11z"), tile("1z"), 1});
    CHECK(decompose(hand).empty());
}

// --- Readings ---------------------------------------------------------------

namespace {

// The wait of the only reading of a closed hand.
WaitType onlyWait(const char* hand, const char* winTile) {
    auto rs = readings(Hand(parseTiles(hand)), tile(winTile), true);
    REQUIRE(rs.size() == 1);
    REQUIRE(rs[0].wait.has_value());
    return *rs[0].wait;
}

}  // namespace

TEST_CASE("Wait types of a single reading") {
    CHECK(onlyWait("123m456p789s345s11z", "5s") == WaitType::Ryanmen);  // 34 waiting on 2-5
    CHECK(onlyWait("123m456p789s345s11z", "3s") == WaitType::Ryanmen);  // 45 waiting on 3-6
    CHECK(onlyWait("123m456p789s345s11z", "4s") == WaitType::Kanchan);
    CHECK(onlyWait("123m456p789m123s11z", "3s") == WaitType::Penchan);  // 12 waiting on 3
    CHECK(onlyWait("123m456p789m789s11z", "7s") == WaitType::Penchan);  // 89 waiting on 7
    CHECK(onlyWait("123m456p789m789s11z", "1z") == WaitType::Tanki);
    CHECK(onlyWait("11335577m1199p55z", "5z") == WaitType::Tanki);      // chiitoitsu
    CHECK(onlyWait("19m19p19s12345677z", "7z") == WaitType::Tanki);     // kokushi
}

TEST_CASE("One reading per block the winning tile could have completed") {
    // 345m + 55m: the 5m completed either the sequence or the pair.
    auto rs = readings(Hand(parseTiles("123m456p789s34555m")), tile("5m"), true);
    REQUIRE(rs.size() == 2);
    CHECK(rs[0].blocks == rs[1].blocks);  // same blocks, different wait
    auto hasWait = [&rs](WaitType w) {
        return std::any_of(rs.begin(), rs.end(), [w](const Reading& r) { return r.wait == w; });
    };
    CHECK(hasWait(WaitType::Ryanmen));
    CHECK(hasWait(WaitType::Tanki));
}

TEST_CASE("Identical readings are kept once") {
    // 123m twice: the 1m completes either copy the same way.
    auto rs = readings(Hand(parseTiles("112233m456p789s11z")), tile("1m"), true);
    REQUIRE(rs.size() == 1);
    CHECK(rs[0].wait == WaitType::Ryanmen);
}

TEST_CASE("A triplet completed by ron counts as open") {
    Hand hand(parseTiles("123m456p789s11z222z"));
    const Block closed{BlockType::Triplet, tile("2z"), false};
    const Block open{BlockType::Triplet, tile("2z"), true};

    auto ron = readings(hand, tile("2z"), true);
    REQUIRE(ron.size() == 1);
    CHECK(ron[0].wait == WaitType::Shanpon);
    CHECK(std::count(ron[0].blocks.begin(), ron[0].blocks.end(), open) == 1);
    CHECK_FALSE(ron[0].handOpen);  // the hand itself stays closed

    auto tsumo = readings(hand, tile("2z"), false);
    REQUIRE(tsumo.size() == 1);
    CHECK(std::count(tsumo[0].blocks.begin(), tsumo[0].blocks.end(), closed) == 1);
}

TEST_CASE("Called melds are never the winning block") {
    // 123p concealed and 123p called: the winning 3p can only be in the concealed one.
    Hand hand(parseTiles("123m456s11z123p12p"));
    hand.addMeld({MeldType::Chi, parseTiles("12p"), tile("3p"), 3});

    auto rs = readings(hand, tile("3p"), true);
    REQUIRE(rs.size() == 1);
    CHECK(rs[0].wait == WaitType::Penchan);
    CHECK(rs[0].handOpen);
    CHECK(decompose(hand)[0].handOpen);
}

TEST_CASE("No readings for an incomplete hand") {
    CHECK(readings(Hand(parseTiles("123m456p789s1z2z22z")), tile("2z"), true).empty());
}
