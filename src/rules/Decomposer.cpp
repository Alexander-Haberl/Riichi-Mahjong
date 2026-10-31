#include "rules/Decomposer.hpp"

#include <algorithm>

namespace mj {

namespace {

constexpr uint8_t kTerminalsAndHonors[] = {0, 8, 9, 17, 18, 26, 27, 28, 29, 30, 31, 32, 33};

Tile kindAt(size_t i) { return Tile(static_cast<uint8_t>(i)); }

bool canStartSequence(size_t i) { return i < 27 && i % 9 <= 6; }

// Splits the remaining tiles into triplets and sequences, working up from the
// lowest kind. Since lower kinds are used up, every tile of kind i is either in
// a triplet of i or starts a sequence i, i+1, i+2. All of them are placed in one
// step, choosing only whether to make a triplet, so each split is found once.
void extractSets(TileCounts& counts, size_t i, std::vector<Block>& blocks, std::vector<Reading>& out) {
    while (i < counts.size() && counts[i] == 0) ++i;
    if (i == counts.size()) {
        out.push_back({HandForm::Regular, blocks});
        return;
    }

    const uint8_t n = counts[i];
    const int maxTriplets = n >= 3 ? 1 : 0;  // at most 4 tiles of a kind
    for (int triplets = 0; triplets <= maxTriplets; ++triplets) {
        const int sequences = n - 3 * triplets;
        if (sequences > 0 && !(canStartSequence(i) && counts[i + 1] >= sequences && counts[i + 2] >= sequences)) {
            continue;
        }

        const size_t blocksBefore = blocks.size();
        counts[i] = 0;
        if (sequences > 0) {  // i + 1 / i + 2 may be past the end for honors
            counts[i + 1] = static_cast<uint8_t>(counts[i + 1] - sequences);
            counts[i + 2] = static_cast<uint8_t>(counts[i + 2] - sequences);
        }
        if (triplets == 1) blocks.push_back({BlockType::Triplet, kindAt(i)});
        for (int s = 0; s < sequences; ++s) {
            blocks.push_back({BlockType::Sequence, kindAt(i)});
        }

        extractSets(counts, i + 1, blocks, out);

        counts[i] = n;
        if (sequences > 0) {
            counts[i + 1] = static_cast<uint8_t>(counts[i + 1] + sequences);
            counts[i + 2] = static_cast<uint8_t>(counts[i + 2] + sequences);
        }

        blocks.resize(blocksBefore);
    }
}

bool isChiitoitsu(const TileCounts& counts) {
    int pairs = 0;
    for (uint8_t n : counts) {
        if (n == 2) ++pairs;
        else if (n != 0) return false;
    }
    return pairs == 7;
}

bool isKokushi(const TileCounts& counts) {
    int total = 0;
    for (uint8_t id : kTerminalsAndHonors) {
        if (counts[id] == 0) return false;
        total += counts[id];
    }
    return total == 14 && totalTiles(counts) == 14;
}

// The wait the winning tile completed if it is part of this concealed block.
std::optional<WaitType> waitIn(const Block& block, Tile winTile) {
    switch (block.type) {
        case BlockType::Pair:
            if (block.first.sameKind(winTile)) return WaitType::Tanki;
            return std::nullopt;
        case BlockType::Triplet:
            if (block.first.sameKind(winTile)) return WaitType::Shanpon;
            return std::nullopt;
        case BlockType::Sequence: {
            const int position = winTile.kind() - block.first.kind();
            if (winTile.isHonor() || position < 0 || position > 2) return std::nullopt;
            if (position == 1) return WaitType::Kanchan;                                   // 4 in 3-4-5
            if (position == 0) return block.first.number() == 7 ? WaitType::Penchan      // 7 in 7-8-9
                                                                : WaitType::Ryanmen;
            return block.first.number() == 1 ? WaitType::Penchan : WaitType::Ryanmen;     // 3 in 1-2-3
        }
        case BlockType::Kan:
            return std::nullopt;  // only from melds, which never hold the winning tile
    }
    return std::nullopt;
}

}  // namespace

std::vector<Reading> readings(const Hand& hand, Tile winTile, bool ron) {
    std::vector<Reading> result;
    auto add = [&result](const Reading& reading) {
        if (std::find(result.begin(), result.end(), reading) == result.end()) result.push_back(reading);
    };

    for (const Reading& base : decompose(hand)) {
        if (base.form != HandForm::Regular) {  // chiitoitsu and kokushi: always a single-tile wait
            Reading reading = base;
            reading.wait = WaitType::Tanki;
            add(reading);
            continue;
        }
        // decompose() puts the called melds after the concealed blocks.
        const size_t concealedBlocks = base.blocks.size() - hand.melds().size();
        for (size_t i = 0; i < concealedBlocks; ++i) {
            std::optional<WaitType> wait = waitIn(base.blocks[i], winTile);
            if (!wait) continue;
            Reading reading = base;
            reading.wait = wait;
            if (ron && *wait == WaitType::Shanpon) reading.blocks[i].open = true;
            add(reading);
        }
    }
    return result;
}

std::vector<Reading> decompose(const Hand& hand) {
    std::vector<Block> meldBlocks;
    for (const Meld& meld : hand.melds()) {
        Tile first(meld.baseTile().kind());  // drop the red flag
        switch (meld.type) {
            case MeldType::Chi: meldBlocks.push_back({BlockType::Sequence, first, true}); break;
            case MeldType::Pon: meldBlocks.push_back({BlockType::Triplet, first, true}); break;
            case MeldType::OpenKan: meldBlocks.push_back({BlockType::Kan, first, true}); break;
            case MeldType::ClosedKan: meldBlocks.push_back({BlockType::Kan, first, false}); break;
        }
    }

    std::vector<Reading> result = decomposeConcealed(hand.counts());
    for (Reading& d : result) {
        d.blocks.insert(d.blocks.end(), meldBlocks.begin(), meldBlocks.end());
        d.handOpen = !hand.isClosed();
    }
    return result;
}

std::vector<Reading> decomposeConcealed(const TileCounts& concealed) {
    std::vector<Reading> result;
    if (totalTiles(concealed) % 3 != 2) return result;

    // Kokushi cannot be read any other way.
    if (isKokushi(concealed)) {
        result.push_back({HandForm::Kokushi, {}});
        return result;
    }

    // A chiitoitsu hand may also be a regular hand (ryanpeikou), which then
    // scores higher, so the regular decompositions are still added below.
    if (isChiitoitsu(concealed)) {
        Reading d{HandForm::Chiitoitsu, {}};
        for (size_t i = 0; i < concealed.size(); ++i) {
            if (concealed[i] == 2) d.blocks.push_back({BlockType::Pair, kindAt(i)});
        }
        result.push_back(std::move(d));
    }

    TileCounts counts = concealed;
    std::vector<Block> blocks;
    for (size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] < 2) continue;
        counts[i] -= 2;
        blocks.push_back({BlockType::Pair, kindAt(i)});
        extractSets(counts, 0, blocks, result);
        blocks.pop_back();
        counts[i] += 2;
    }
    return result;
}

bool isComplete(const TileCounts& concealed) {
    return !decomposeConcealed(concealed).empty();
}

bool isTenpai(const TileCounts& concealed) {
    return !waits(concealed).empty();
}

bool isTenpai(const Hand& hand) {
    return isTenpai(hand.counts());
}

std::vector<Tile> waits(const TileCounts& concealed) {
    std::vector<Tile> result;
    if (totalTiles(concealed) % 3 != 1) return result;

    TileCounts counts = concealed;
    for (size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] >= 4) continue;
        ++counts[i];
        if (isComplete(counts)) result.push_back(kindAt(i));
        --counts[i];
    }
    return result;
}

}  // namespace mj
