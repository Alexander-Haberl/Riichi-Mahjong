#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "TestHelpers.hpp"
#include "core/Hand.hpp"

using namespace mj;
using namespace mj::test;

TEST_CASE("Last added tile stays at the back") {
    Hand hand(parseTiles("9s1m"));
    hand.add(tile("5p"));
    CHECK(hand.lastTile() == tile("5p"));

    // Discarding another tile keeps the drawn one last.
    CHECK(hand.remove(tile("9s")));
    CHECK(hand.lastTile() == tile("5p"));
}

TEST_CASE("Removing tiles") {
    Hand hand(parseTiles("112m"));

    CHECK(hand.remove(tile("1m")));
    CHECK(hand.count(tile("1m")) == 1);
    CHECK(hand.remove(tile("1m")));
    CHECK_FALSE(hand.remove(tile("1m")));
    CHECK_FALSE(hand.remove(tile("3m")));
    CHECK(hand.concealed().size() == 1);
}

TEST_CASE("Removing distinguishes red fives") {
    Hand hand(parseTiles("05m"));
    CHECK(hand.count(tile("5m")) == 2);

    CHECK(hand.remove(tile("5m")));
    REQUIRE(hand.concealed().size() == 1);
    CHECK(hand.concealed()[0].isRed());
    CHECK_FALSE(hand.remove(tile("5m")));
}

TEST_CASE("Calling pon opens the hand") {
    Hand hand(parseTiles("23m11z"));
    Tile called = tile("1z");

    hand.addMeld({MeldType::Pon, parseTiles("11z"), called, 3});

    CHECK(hand.meldCount() == 1);
    CHECK_FALSE(hand.isClosed());
    CHECK(hand.toString() == "23m [1z1z1z]");
    CHECK(totalTiles(hand.counts()) == 2);
}

TEST_CASE("Closed kan keeps the hand closed") {
    Hand hand(parseTiles("7777p1s"));

    hand.addMeld({MeldType::ClosedKan, parseTiles("7777p"), std::nullopt, 0});

    CHECK(hand.isClosed());
    CHECK(hand.meldCount() == 1);
    CHECK(hand.melds()[0].isKan());
    CHECK(hand.concealed().size() == 1);
}

TEST_CASE("Adding a kan to a pon upgrades it in place") {
    Hand hand(parseTiles("55s9m"));
    Tile called = tile("5s");
    hand.addMeld({MeldType::Pon, parseTiles("55s"), called, 2});
    hand.add(tile("0s"));

    hand.addKanToPon(tile("0s"));

    REQUIRE(hand.meldCount() == 1);
    const Meld& kan = hand.melds()[0];
    CHECK(kan.type == MeldType::OpenKan);
    CHECK(kan.isKan());
    CHECK(kan.kanFromPon);
    CHECK(kan.tiles().size() == 4);
    CHECK(kan.calledTile == called);  // kept from the pon
    CHECK(kan.fromSeat == 2);
    CHECK(hand.concealed().size() == 1);
    CHECK(hand.toString() == "9m [5s5s5s0s]");
}

TEST_CASE("Invalid meld leaves the hand unchanged") {
    Hand hand(parseTiles("23m"));
    Tile called = tile("1m");

    CHECK_THROWS_AS(hand.addMeld({MeldType::Chi, parseTiles("24m"), called, 3}), std::logic_error);
    CHECK(hand.concealed().size() == 2);
    CHECK(hand.meldCount() == 0);

    CHECK_THROWS_AS(hand.addKanToPon(tile("2m")), std::logic_error);  // no pon of 2m
    CHECK(hand.concealed().size() == 2);
}

TEST_CASE("Adding a kan needs the tile in hand") {
    Hand hand(parseTiles("11z"));
    hand.addMeld({MeldType::Pon, parseTiles("11z"), tile("1z"), 1});

    CHECK_THROWS_AS(hand.addKanToPon(tile("1z")), std::logic_error);
    CHECK(hand.melds()[0].type == MeldType::Pon);
    CHECK_FALSE(hand.melds()[0].kanFromPon);
}
