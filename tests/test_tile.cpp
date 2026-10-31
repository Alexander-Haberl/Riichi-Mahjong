#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "TestHelpers.hpp"
#include "core/Tile.hpp"

using namespace mj;
using namespace mj::test;

TEST_CASE("Tile kinds map to suit and number") {
    CHECK(Tile(0).toString() == "1m");
    CHECK(Tile(8).toString() == "9m");
    CHECK(Tile(9).toString() == "1p");
    CHECK(Tile(26).toString() == "9s");
    CHECK(Tile(27).toString() == "1z");
    CHECK(Tile(33).toString() == "7z");

    CHECK(Tile::make(Suit::Pin, 5).kind() == 13);
    CHECK(Tile::make(Suit::Honor, 7).kind() == 33);
}

TEST_CASE("Honor tiles by name") {
    CHECK(Tile::honor(Honor::East).toString() == "1z");
    CHECK(Tile::honor(Honor::North).toString() == "4z");
    CHECK(Tile::honor(Honor::White).toString() == "5z");
    CHECK(Tile::honor(Honor::Red).toString() == "7z");

    CHECK(Tile::honor(Honor::West).isWind());
    CHECK(Tile::honor(Honor::Green).isDragon());

    for (int i = 0; i < 7; ++i) {
        auto h = static_cast<Honor>(i);
        CHECK(Tile::honor(h).asHonor() == h);
    }
}

TEST_CASE("Tile classification") {
    CHECK(tile("1m").isTerminal());
    CHECK(tile("9s").isTerminal());
    CHECK_FALSE(tile("1z").isTerminal());
    CHECK(tile("1z").isHonor());
    CHECK(tile("1z").isTerminalOrHonor());
    CHECK(tile("5p").isSimple());
    CHECK_FALSE(tile("9p").isSimple());

    CHECK(tile("4z").isWind());
    CHECK_FALSE(tile("5z").isWind());
    CHECK(tile("5z").isDragon());
    CHECK(tile("7z").isDragon());
}

TEST_CASE("Dora from indicator wraps within its group") {
    auto dora = [](const char* s) { return tile(s).doraFromIndicator().toString(); };

    CHECK(dora("1m") == "2m");
    CHECK(dora("9m") == "1m");
    CHECK(dora("9p") == "1p");
    CHECK(dora("9s") == "1s");
    CHECK(dora("4z") == "1z");  // North -> East
    CHECK(dora("2z") == "3z");
    CHECK(dora("7z") == "5z");  // red -> white
    CHECK(dora("5z") == "6z");
    CHECK(dora("0m") == "6m");  // a red five indicates 6
}

TEST_CASE("Red fives") {
    Tile red = Tile::make(Suit::Man, 5, true);
    Tile normal = Tile::make(Suit::Man, 5);

    CHECK(red.isRed());
    CHECK_FALSE(normal.isRed());
    CHECK(red.sameKind(normal));
    CHECK(red != normal);
    CHECK(normal < red);
    CHECK(red.toString() == "0m");
    CHECK(normal.toString() == "5m");
    CHECK(tile("0p") == Tile::make(Suit::Pin, 5, true));
}

TEST_CASE("Parsing and printing tile strings") {
    TileCounts c = parseCounts("123m406p789s1122z");
    CHECK(totalTiles(c) == 13);
    CHECK(c[Tile::make(Suit::Pin, 5).kind()] == 1);  // red five counts as a five
    CHECK(toString(c) == "123m456p789s1122z");

    CHECK_THROWS_AS(parseTiles("123"), std::invalid_argument);
    CHECK_THROWS_AS(parseTiles("m"), std::invalid_argument);
    CHECK_THROWS_AS(parseTiles("8z"), std::invalid_argument);
    CHECK_THROWS_AS(parseTiles("0z"), std::invalid_argument);
    CHECK_THROWS_AS(parseTiles("12x"), std::invalid_argument);
    CHECK_THROWS_AS(parseCounts("11111m"), std::invalid_argument);
    CHECK_THROWS_AS(parseCounts("05555m"), std::invalid_argument);
}
