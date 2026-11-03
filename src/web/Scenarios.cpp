#include "web/Scenarios.hpp"

#include <stdexcept>

namespace mj {

namespace {

// Opponent hands that can never call: no pairs, and no two tiles close enough
// to form a sequence. Together they hold each number tile once.
const std::string kRight = "147m147p147s1234z";
const std::string kAcross = "258m258p258s1567z";
const std::string kLeft = "369m369p369s2347z";

const std::string kCalls1 = "12m11222333344z";


}  // namespace

const std::vector<Scenario>& scenarios() {
    static const std::vector<Scenario> list = {
        {"Riichi with several discards",
         {"234567m456p789s1z", kRight, kAcross, kLeft},
         {{{"8m"}, {"5z"}, {"6z"}, {"6z"}}}},
        {"Tsumo",
         {"234067m406p789s1z", kRight, kAcross, kLeft},
         {{{"1z"}, {"5z"}, {"6z"}, {"6z"}}}},
        // Three 5m and the fourth from the left: two pons (with and without the red five) and an open kan.
        {"Pon chooser and open kan",
         {"550m123p789s11z23z", kRight, "28m258p258s14567z", kLeft},
         {{{"9p"}, {"6z"}, {"6z"}, {"5m"}}}},
        {"Chi options",
         {"34056m123p789s11z", kRight, kAcross, kLeft},
         {{{"9p"}, {"5z"}, {"6z"}, {"4m"}}}},
        // Four 1p for a closed kan, three 2z: pon the fourth, the one left in hand can be added later.
        {"Closed kan and kan onto a pon",
         {"1111p222z234m789s", "147m47p147s13456z", kAcross, "369m369p369s3467z"},
         {{{"9m"}, {"7z"}, {"2z"}, {"5z"}}}},
        {"Nine terminals",
         {"12589m19p19s1234z", kRight, kAcross, kLeft},
         {{{"5z"}, {"6z"}, {"6z"}, {"7z"}}}},
        {"Chiitoitsu tsumo",
         {"1122m3344p5566s7z", kRight, kAcross, kLeft},
         {{{"7z"}, {"5z"}, {"6z"}, {"6z"}}}},
        {"Kokushi ron",
         {"19m19p19s1234567z", kRight, kAcross, kLeft},
         {{{"5p"}, {"5s"}, {"7z"}, {"5m"}}}},
        // Waits on 1m and 4m. The first draw wins; discarding it instead makes the
        // hand furiten, and the 4m from the right later offers no ron.
        {"Furiten: own discard",
         {"23m456789p123s99s", kRight, kAcross, kLeft},
         {{{"1m", "9m"}, {"6z", "4m"}, {"6z", "5z"}, {"7z", "5z"}}}},
        // Waits on 1m and 4m, riichi possible. 1m from the right, then 4m from
        // across: passing on the first ron blocks the second until your discard.
        {"Furiten: passed ron",
         {"23m456789p123s99s", kRight, kAcross, kLeft},
         {{{"9m", "8m"}, {"1m", "5z"}, {"4m", "4m"}, {"7z", "5z"}}}},
        // You and the player across both wait on 5s; the left discards it.
        {"Double ron",
         {"234567m234p678s5s", kRight, "234456m345p66s34s", kLeft},
         {{{"9m"}, {"5z"}, {"6z"}, {"5s"}}}},
        // AIs make every possible call.
        {"test AI Chi",
         {"6777788889999m", "12m147s1234567z9p", "12m147s1234567z9p", "12m147s1234567z9p"},
         {{{"3m"}, {"3m"}, {"3m"}, {"3m"}}}},
        {"test AI Pon and AddToPon",
         {"6777788889999m", "11m369s1234567z1s", "11369s1234567z1p", "11p369s1234567z9s"},
         {{{"1m", "1p"}, {"9p", "9p"}, {"9p", "1m"}, {"9p", "1s"}}}},
        {"test AI OpenKan",
         {"6777788889999m", "111m69s1234567z2s", "11169s1234567z2s", "111p69s1234567z2s"},
         {{{"1m"}, {"1p"}, {"1s"}, {"3s"}}}},
        {"test AI ClosedKan",
         {"6777788889999m", "1111m9s1234567z2s", "11119s1234567z2s", "1111p9s1234567z2s"},
         {{{"9p"}, {"9p"}, {"9p"}, {"9p"}}}},
    };
    return list;
}

std::vector<Tile> scenarioWallStart(const Scenario& scenario, int humanSeat) {
    auto positionOf = [humanSeat](int seat) { return static_cast<size_t>((seat - humanSeat + 4) % 4); };

    std::vector<Tile> start;
    for (int seat = 0; seat < 4; ++seat) {
        const std::vector<Tile> hand = parseTiles(scenario.hands[positionOf(seat)]);
        if (hand.size() != 13) throw std::invalid_argument("a scenario hand needs 13 tiles: " + scenario.name);
        start.insert(start.end(), hand.begin(), hand.end());
    }
    for (size_t turn = 0;; ++turn) {
        const std::vector<std::string>& draws = scenario.draws[positionOf(static_cast<int>(turn % 4))];
        if (turn / 4 >= draws.size()) break;
        const std::vector<Tile> tile = parseTiles(draws[turn / 4]);
        start.insert(start.end(), tile.begin(), tile.end());
    }
    return start;
}

}  // namespace mj
