#include "core/Tile.hpp"

#include <numeric>
#include <stdexcept>

namespace mj {

namespace {

constexpr char kSuitChars[] = {'m', 'p', 's', 'z'};

int suitIndex(char c) {
    switch (c) {
        case 'm': return 0;
        case 'p': return 1;
        case 's': return 2;
        case 'z': return 3;
        default: return -1;
    }
}

}  // namespace

Tile Tile::doraFromIndicator() const {
    // Each group cycles on its own: 1-9, E S W N (1-4), white green red (5-7).
    if (isWind()) return make(Suit::Honor, number() % 4 + 1);
    if (isDragon()) return make(Suit::Honor, (number() - 4) % 3 + 5);
    return make(suit(), number() % 9 + 1);
}

std::string Tile::toString() const {
    return {static_cast<char>(red_ ? '0' : '0' + number()), kSuitChars[static_cast<int>(suit())]};
}

std::vector<Tile> parseTiles(std::string_view text) {
    std::vector<Tile> result;
    std::vector<int> pendingNumbers;  // 0 = red five
    for (char c : text) {
        if (c >= '0' && c <= '9') {
            pendingNumbers.push_back(c - '0');
            continue;
        }
        int suit = suitIndex(c);
        if (suit < 0) throw std::invalid_argument("unexpected character in tile string: " + std::string(1, c));
        if (pendingNumbers.empty()) throw std::invalid_argument("suit letter without numbers: " + std::string(1, c));
        for (int n : pendingNumbers) {
            if (suit == 3 && (n == 0 || n > 7)) throw std::invalid_argument("honor number out of range: " + std::to_string(n));
            bool red = n == 0;
            result.push_back(Tile::make(static_cast<Suit>(suit), red ? 5 : n, red));
        }
        pendingNumbers.clear();
    }
    if (!pendingNumbers.empty()) throw std::invalid_argument("numbers without suit letter at end of tile string");
    return result;
}

TileCounts parseCounts(std::string_view text) {
    TileCounts counts{};
    for (Tile t : parseTiles(text)) {
        if (++counts[t.kind()] > 4) throw std::invalid_argument("more than four copies of " + t.toString());
    }
    return counts;
}

int totalTiles(const TileCounts& counts) {
    return std::accumulate(counts.begin(), counts.end(), 0);
}

std::string toString(const TileCounts& counts) {
    std::string result;
    for (int suit = 0; suit < 4; ++suit) {
        std::string numbers;
        int size = suit == 3 ? 7 : 9;
        for (int n = 1; n <= size; ++n) {
            numbers.append(counts[static_cast<size_t>(suit * 9 + n - 1)], static_cast<char>('0' + n));
        }
        if (!numbers.empty()) result += numbers + kSuitChars[suit];
    }
    return result;
}

}  // namespace mj
