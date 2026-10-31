#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "game/Player.hpp"

using namespace mj;
using namespace mj::test;

namespace {

RoundState emptyRound() {
    return RoundState(Wall(Wall::fullSet()), Honor::East, 0, {25000, 25000, 25000, 25000});
}

int count(const TileCounts& counts, const char* t) {
    return counts[tile(t).kind()];
}

}  // namespace

TEST_CASE("Visible tiles: the own hand, rivers, melds and dora indicators") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("123m"));
    state.players[1].river.push_back({tile("2m")});
    state.players[2].hand = Hand(parseTiles("99m"));
    state.players[2].hand.addMeld({MeldType::Pon, parseTiles("99m"), tile("9m"), 1});

    const TileCounts visible = PlayerView(state, 0).visibleTiles();
    CHECK(count(visible, "1m") == 1);
    CHECK(count(visible, "2m") == 2);  // in the hand and in a river
    CHECK(count(visible, "9m") == 3);  // the whole pon, called tile included
    CHECK(visible[state.wall.doraIndicators()[0].kind()] == 1);
    CHECK(totalTiles(visible) == 8);
}

TEST_CASE("Visible tiles: other players' concealed tiles stay hidden") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("1m"));
    state.players[3].hand = Hand(parseTiles("111s"));

    CHECK(count(PlayerView(state, 0).visibleTiles(), "1s") == 0);
    CHECK(count(PlayerView(state, 3).visibleTiles(), "1s") == 3);  // but a player sees their own
}

TEST_CASE("Visible tiles: the own melds count") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("7777p1s"));
    state.players[0].hand.addMeld({MeldType::ClosedKan, parseTiles("7777p"), std::nullopt, 0});

    CHECK(count(PlayerView(state, 0).visibleTiles(), "7p") == 4);
}

TEST_CASE("Visible tiles: a tile called away from a river counts once") {
    RoundState state = emptyRound();
    state.players[1].river.push_back({tile("1m"), false, false, true});  // called away
    state.players[2].hand = Hand(parseTiles("11m"));
    state.players[2].hand.addMeld({MeldType::Pon, parseTiles("11m"), tile("1m"), 1});

    CHECK(count(PlayerView(state, 0).visibleTiles(), "1m") == 3);
}

TEST_CASE("A view counts the concealed tiles of every seat") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("123m"));
    state.players[2].hand = Hand(parseTiles("99m12p"));
    state.players[2].hand.addMeld({MeldType::Pon, parseTiles("99m"), tile("9m"), 1});

    const PlayerView view(state, 0);
    CHECK(view.concealedCount(0) == 3);
    CHECK(view.concealedCount(2) == 2);  // the two tiles of the pon left the hand
    CHECK(view.concealedCount(3) == 0);
}

TEST_CASE("A view is a copy: it does not change with the round") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("123m"));
    state.players[1].river.push_back({tile("2m")});

    const PlayerView view(state, 0);
    const int wallBefore = view.wallRemaining();

    state.players[0].hand.add(tile("9p"));
    state.players[1].river.push_back({tile("3m")});
    state.players[1].score = 0;
    state.riichiSticks = 3;
    state.wall.draw();

    CHECK(view.hand().concealedCount() == 3);
    CHECK(view.river(1).size() == 1);
    CHECK(view.score(1) == 25000);
    CHECK(view.riichiSticks() == 0);
    CHECK(view.wallRemaining() == wallBefore);
}

TEST_CASE("Furiten of the own seat") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("123m456p789s11z22z"));  // waits on 1z and 2z
    state.players[1].hand = state.players[0].hand;

    CHECK_FALSE(PlayerView(state, 0).furiten());

    SECTION("a wait in the own river") {
        state.players[0].river.push_back({tile("2z")});
        CHECK(PlayerView(state, 0).furiten());
    }
    SECTION("a ron passed on") {
        state.players[0].temporaryFuriten = true;
        CHECK(PlayerView(state, 0).furiten());
    }
    SECTION("a ron passed on after riichi") {
        state.players[0].riichiFuriten = true;
        CHECK(PlayerView(state, 0).furiten());
    }
    SECTION("the furiten of another seat does not show") {
        state.players[1].river.push_back({tile("2z")});
        state.players[1].temporaryFuriten = true;
        CHECK_FALSE(PlayerView(state, 0).furiten());
    }
    SECTION("a hand with a drawn tile has no waits, but keeps a ron passed on") {
        state.players[0].river.push_back({tile("2z")});
        state.players[0].hand.add(tile("9m"));
        CHECK_FALSE(PlayerView(state, 0).furiten());

        state.players[0].temporaryFuriten = true;
        CHECK(PlayerView(state, 0).furiten());
    }
}
