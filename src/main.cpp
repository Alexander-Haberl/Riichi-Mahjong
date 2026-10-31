// Placeholder entry point: deals a random 14-tile hand and analyses it.
// Will be replaced by the Game loop once the game layer exists.

#include <iostream>
#include <random>
#include <string>

#include "core/Hand.hpp"
#include "core/Wall.hpp"
#include "rules/Shanten.hpp"

int main(int argc, char** argv) {
    unsigned seed = argc > 1 ? static_cast<unsigned>(std::stoul(argv[1])) : std::random_device{}();
    std::mt19937 rng(seed);

    mj::Wall wall(rng);
    mj::Hand hand;
    for (int i = 0; i < 14; ++i) hand.add(wall.draw());

    std::cout << "Seed:    " << seed << '\n'
              << "Dora:    " << wall.doraIndicators().front().doraFromIndicator().toString() << '\n'
              << "Hand:    " << hand.toString() << '\n'
              << "Shanten: " << mj::shanten::calc(hand.counts()) << '\n';

    // Show which discards keep the hand closest to tenpai.
    std::cout << "Best discards:";
    int best = 99;
    std::vector<mj::Tile> bestDiscards;
    mj::TileCounts counts = hand.counts();
    for (size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] == 0) continue;
        --counts[i];
        int s = mj::shanten::calc(counts);
        ++counts[i];
        if (s < best) {
            best = s;
            bestDiscards.clear();
        }
        if (s == best) bestDiscards.emplace_back(static_cast<uint8_t>(i));
    }
    for (mj::Tile t : bestDiscards) std::cout << ' ' << t.toString();
    std::cout << " (-> shanten " << best << ")\n";
}
