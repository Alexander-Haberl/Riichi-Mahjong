#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "game/Game.hpp"

using namespace mj;

namespace {

using Change = std::array<int, 4>;

Win win(int seat, int fromSeat, int basicPoints) {
    Win w{seat, fromSeat, ScoreResult{}};
    w.score.basicPoints = basicPoints;
    return w;
}

RoundResult ron(std::vector<Win> wins) {
    return RoundResult{RoundEnd::Ron, std::move(wins), {}, std::nullopt};
}

RoundResult tsumo(int seat, int basicPoints) {
    return RoundResult{RoundEnd::Tsumo, {win(seat, seat, basicPoints)}, {}, std::nullopt};
}

RoundResult exhaustiveDraw(std::array<bool, 4> tenpai) {
    return RoundResult{RoundEnd::ExhaustiveDraw, {}, tenpai, std::nullopt};
}

}  // namespace

TEST_CASE("Ron: the discarder pays 4x the basic points, 6x to the dealer") {
    // Dealer is seat 0. 1 han 30 fu = 240 basic points.
    CHECK(payments(ron({win(1, 2, 240)}), 0, 0, 0) == Change{0, 1000, -1000, 0});  // 960 -> 1000
    CHECK(payments(ron({win(0, 2, 240)}), 0, 0, 0) == Change{1500, 0, -1500, 0});  // 1440 -> 1500
    CHECK(payments(ron({win(1, 0, 2000)}), 0, 0, 0) == Change{-8000, 8000, 0, 0});    // mangan
    CHECK(payments(ron({win(0, 3, 2000)}), 0, 0, 0) == Change{12000, 0, 0, -12000});  // dealer mangan
}

TEST_CASE("Payments are rounded up to the next 100") {
    CHECK(payments(ron({win(1, 2, 1920)}), 0, 0, 0) == Change{0, 7700, -7700, 0});    // 7680
    CHECK(payments(ron({win(0, 2, 1920)}), 0, 0, 0) == Change{11600, 0, -11600, 0});  // 11520
    CHECK(payments(ron({win(1, 2, 320)}), 0, 0, 0) == Change{0, 1300, -1300, 0});     // 1 han 40 fu: 1280
}

TEST_CASE("Tsumo: everyone pays, the dealer double") {
    // Non-dealer 1 han 30 fu: 300 from each non-dealer, 500 from the dealer.
    CHECK(payments(tsumo(1, 240), 0, 0, 0) == Change{-500, 1100, -300, -300});
    // Dealer 1 han 30 fu: 500 from everyone.
    CHECK(payments(tsumo(0, 240), 0, 0, 0) == Change{1500, -500, -500, -500});
    // Non-dealer mangan: 2000 / 4000.
    CHECK(payments(tsumo(2, 2000), 0, 0, 0) == Change{-4000, -2000, 8000, -2000});
    // Dealer mangan: 4000 all.
    CHECK(payments(tsumo(3, 2000), 3, 0, 0) == Change{-4000, -4000, -4000, 12000});
}

TEST_CASE("Honba: 300 per honba, split on tsumo") {
    CHECK(payments(ron({win(1, 2, 240)}), 0, 2, 0) == Change{0, 1600, -1600, 0});
    CHECK(payments(tsumo(1, 240), 0, 2, 0) == Change{-700, 1700, -500, -500});
}

TEST_CASE("The winner takes the riichi sticks") {
    // The sticks come from the table, so the changes add up to their value.
    Change onRon = payments(ron({win(1, 2, 240)}), 0, 0, 2);
    CHECK(onRon == Change{0, 3000, -1000, 0});

    Change onTsumo = payments(tsumo(1, 240), 0, 0, 2);
    CHECK(onTsumo == Change{-500, 3100, -300, -300});
}

TEST_CASE("Several rons: honba and sticks go to the first winner") {
    // Seat 0 discards; seats 1 and 2 win, seat 1 is first in turn order. Dealer is seat 3.
    Change change = payments(ron({win(1, 0, 240), win(2, 0, 240)}), 3, 1, 1);
    CHECK(change == Change{-2300, 2300, 1000, 0});  // seat 1: 1000 + 300 honba + 1000 stick
}

TEST_CASE("Noten payments at an exhaustive draw") {
    CHECK(payments(exhaustiveDraw({true, false, false, false}), 0, 0, 0) == Change{3000, -1000, -1000, -1000});
    CHECK(payments(exhaustiveDraw({true, false, true, false}), 0, 0, 0) == Change{1500, -1500, 1500, -1500});
    CHECK(payments(exhaustiveDraw({true, true, false, true}), 0, 0, 0) == Change{1000, 1000, -3000, 1000});
    CHECK(payments(exhaustiveDraw({false, false, false, false}), 0, 0, 0) == Change{});
    CHECK(payments(exhaustiveDraw({true, true, true, true}), 0, 0, 0) == Change{});
}

TEST_CASE("Draws keep honba and sticks on the table") {
    CHECK(payments(exhaustiveDraw({true, false, false, false}), 0, 3, 2) == Change{3000, -1000, -1000, -1000});
    CHECK(payments(RoundResult{RoundEnd::Aborted, {}, {}, AbortReason::SuufonRenda}, 0, 3, 2) == Change{});
}
