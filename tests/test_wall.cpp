#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>

#include "TestHelpers.hpp"
#include "core/Wall.hpp"

using namespace mj;
using namespace mj::test;

namespace {

// Wall whose tiles are kinds 0..33 repeated, so each position is recognisable.
std::vector<Tile> orderedTiles() {
    std::vector<Tile> tiles;
    for (int i = 0; i < kNumTiles; ++i) tiles.emplace_back(static_cast<uint8_t>(i % kNumTileKinds));
    return tiles;
}

}  // namespace

TEST_CASE("Full tile set") {
    auto tiles = Wall::fullSet();
    REQUIRE(tiles.size() == kNumTiles);

    TileCounts counts{};
    for (Tile t : tiles) ++counts[t.kind()];
    CHECK(std::all_of(counts.begin(), counts.end(), [](uint8_t n) { return n == 4; }));

    auto reds = std::count_if(tiles.begin(), tiles.end(), [](Tile t) { return t.isRed(); });
    CHECK(reds == 3);
    CHECK(std::count(tiles.begin(), tiles.end(), Tile::make(Suit::Pin, 5, true)) == 1);

    auto noReds = Wall::fullSet(0);
    CHECK(std::none_of(noReds.begin(), noReds.end(), [](Tile t) { return t.isRed(); }));
}

TEST_CASE("Shuffled wall contains every tile once") {
    std::mt19937 rng(1);
    Wall wall(rng);
    CHECK(wall.remaining() == 122);

    std::vector<Tile> drawn;
    while (wall.remaining() > 0) drawn.push_back(wall.draw());
    CHECK(drawn.size() == 122);
    CHECK_THROWS_AS(wall.draw(), std::logic_error);

    for (Tile t : wall.doraIndicators()) drawn.push_back(t);
    CHECK(drawn.size() == 123);
}

TEST_CASE("Same seed gives the same wall") {
    std::mt19937 a(42), b(42);
    Wall wa(a), wb(b);
    for (int i = 0; i < 20; ++i) CHECK(wa.draw() == wb.draw());
}

TEST_CASE("Draws come from the live wall in order") {
    Wall wall(orderedTiles());
    CHECK(wall.draw() == Tile(0));
    CHECK(wall.draw() == Tile(1));
    CHECK(wall.remaining() == 120);
}

TEST_CASE("Dead wall layout") {
    Wall wall(orderedTiles());
    // Dead wall starts at position 122: rinshan 122-125, dora 126-130, ura 131-135.
    REQUIRE(wall.doraIndicators().size() == 1);
    CHECK(wall.doraIndicators()[0] == Tile(126 % kNumTileKinds));
    CHECK(wall.uraDoraIndicators()[0] == Tile(131 % kNumTileKinds));

    CHECK(wall.drawRinshan() == Tile(122 % kNumTileKinds));
    wall.revealNextDora();
    CHECK(wall.doraIndicators().size() == 2);
    CHECK(wall.uraDoraIndicators().size() == 2);
    CHECK(wall.doraIndicators()[1] == Tile(127 % kNumTileKinds));
}

TEST_CASE("Kans shrink the live wall and are limited to four") {
    Wall wall(orderedTiles());
    for (int i = 0; i < 4; ++i) {
        wall.drawRinshan();
        wall.revealNextDora();
    }
    CHECK(wall.remaining() == 118);
    CHECK(wall.rinshanRemaining() == 0);
    CHECK(wall.doraIndicators().size() == 5);
    CHECK_THROWS_AS(wall.drawRinshan(), std::logic_error);
    CHECK_THROWS_AS(wall.revealNextDora(), std::logic_error);
}

TEST_CASE("Wall needs exactly 136 tiles") {
    CHECK_THROWS_AS(Wall(std::vector<Tile>(10)), std::invalid_argument);
}

TEST_CASE("Stacked wall: the start tiles come first, then the rest of the set") {
    std::mt19937 rng(5);
    const std::vector<Tile> start = parseTiles("1111z0m555m9p");
    Wall wall = Wall::stacked(start, rng);

    for (Tile expected : start) CHECK(wall.draw() == expected);

    // The live wall and the dead wall still hold each tile of the set once.
    std::vector<Tile> tiles = start;
    for (int i = 0; i < Wall::kRinshanTiles; ++i) tiles.push_back(wall.drawRinshan());
    while (wall.remaining() > 0) tiles.push_back(wall.draw());
    for (int i = 1; i < Wall::kMaxDoraIndicators; ++i) wall.revealNextDora();
    for (Tile t : wall.doraIndicators()) tiles.push_back(t);
    for (Tile t : wall.uraDoraIndicators()) tiles.push_back(t);
    REQUIRE(tiles.size() == kNumTiles - 4);  // the 4 tiles that refill the dead wall are not reachable
    TileCounts counts{};
    int red = 0;
    for (Tile t : tiles) {
        ++counts[t.kind()];
        if (t.isRed()) ++red;
    }
    CHECK(*std::max_element(counts.begin(), counts.end()) <= 4);
    CHECK(counts[tile("1z").kind()] == 4);
    CHECK(red <= 3);
}

TEST_CASE("Stacked wall: only tiles the set still has") {
    std::mt19937 rng(5);
    CHECK_THROWS_AS(Wall::stacked(parseTiles("11111z"), rng), std::invalid_argument);
    CHECK_THROWS_AS(Wall::stacked(parseTiles("00m"), rng), std::invalid_argument);
    CHECK_THROWS_AS(Wall::stacked(parseTiles("0m"), rng, 0), std::invalid_argument);
    CHECK_NOTHROW(Wall::stacked(parseTiles("0m555m"), rng));
}

