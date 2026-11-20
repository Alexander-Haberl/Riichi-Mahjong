#include "scoring/Yaku.hpp"

#include <algorithm>
#include <array>

namespace mj {

namespace {

// --- Helpers ----------------------------------------------------------------

bool isRegular(const Reading& reading) { return reading.form == HandForm::Regular; }

bool isTripletOrKan(const Block& block) {
    return block.type == BlockType::Triplet || block.type == BlockType::Kan;
}

// Han of a yaku that is worth less on an open hand.
int byOpenness(const Reading& reading, int closed, int open) { return reading.handOpen ? open : closed; }

// A pair that gives fu and rules out pinfu: dragons, the seat wind and the round wind.
bool isYakuhaiPair(Tile pair, const WinContext& ctx) {
    return pair.isDragon() || pair.sameKind(Tile::honor(ctx.seatWind)) || pair.sameKind(Tile::honor(ctx.roundWind));
}

// Pairs of identical sequences: 0, 1 or 2 (three identical sequences make one pair).
int identicalSequencePairs(const Reading& reading) {
    std::array<int, kNumTileKinds> count{};
    for (const Block& block : reading.blocks) {
        if (block.type == BlockType::Sequence) ++count[block.first.kind()];
    }
    int pairs = 0;
    for (int n : count) pairs += n / 2;
    return pairs;
}

// starts[suit][number]: the reading has a sequence starting at this number.
using SequenceStarts = std::array<std::array<bool, 10>, 3>;

SequenceStarts sequenceStarts(const Reading& reading) {
    SequenceStarts starts{};
    for (const Block& block : reading.blocks) {
        if (block.type == BlockType::Sequence) {
            starts[static_cast<size_t>(block.first.suit())][static_cast<size_t>(block.first.number())] = true;
        }
    }
    return starts;
}

// Which suits and whether honors appear in the blocks.
struct Composition {
    std::array<bool, 3> suits{};
    bool honors = false;

    int suitCount() const { return static_cast<int>(std::count(suits.begin(), suits.end(), true)); }
};

Composition compositionOf(const Reading& reading) {
    Composition composition;
    for (const Block& block : reading.blocks) {
        if (block.first.isHonor()) composition.honors = true;
        else composition.suits[static_cast<size_t>(block.first.suit())] = true;
    }
    return composition;
}

// Blocks of the reading that match.
template <typename Predicate>
int countBlocks(const Reading& reading, Predicate matches) {
    return static_cast<int>(std::count_if(reading.blocks.begin(), reading.blocks.end(), matches));
}

int kanCount(const Reading& reading) {
    return countBlocks(reading, [](const Block& block) { return block.type == BlockType::Kan; });
}

template <typename Predicate>
bool allBlocks(const Reading& reading, Predicate matches) {
    return std::all_of(reading.blocks.begin(), reading.blocks.end(), matches);
}

bool hasSequence(const Reading& reading) {
    return countBlocks(reading, [](const Block& block) { return block.type == BlockType::Sequence; }) > 0;
}

// Triplets or kans, and pairs, of tiles that pass the test.
int tripletsOf(const Reading& reading, bool (*test)(Tile)) {
    return countBlocks(reading, [test](const Block& block) { return isTripletOrKan(block) && test(block.first); });
}

int pairsOf(const Reading& reading, bool (*test)(Tile)) {
    return countBlocks(reading, [test](const Block& block) { return block.type == BlockType::Pair && test(block.first); });
}

bool isDragonTile(Tile t) { return t.isDragon(); }
bool isWindTile(Tile t) { return t.isWind(); }

// Number of concealed tiles of each kind in the hand before the winning tile.
TileCounts countsBeforeWin(const WinContext& ctx) {
    TileCounts counts = ctx.hand.counts();
    --counts[ctx.winTile.kind()];
    return counts;
}

// A triplet or kan of this tile.
int yakuhai(const Reading& reading, Tile tile) {
    if (!isRegular(reading)) return 0;
    const bool found = std::any_of(reading.blocks.begin(), reading.blocks.end(), [tile](const Block& block) {
        return isTripletOrKan(block) && block.first.sameKind(tile);
    });
    return found ? 1 : 0;
}

// --- Situation yaku: only the flags of the win ------------------------------

int riichi(const Reading&, const WinContext& ctx) { return ctx.riichi && !ctx.doubleRiichi ? 1 : 0; }
int doubleRiichi(const Reading&, const WinContext& ctx) { return ctx.doubleRiichi ? 2 : 0; }
int ippatsu(const Reading&, const WinContext& ctx) { return ctx.ippatsu ? 1 : 0; }
int menzenTsumo(const Reading& reading, const WinContext& ctx) { return ctx.tsumo && !reading.handOpen ? 1 : 0; }
int haitei(const Reading&, const WinContext& ctx) { return ctx.haitei ? 1 : 0; }
int houtei(const Reading&, const WinContext& ctx) { return ctx.houtei ? 1 : 0; }
int rinshanKaihou(const Reading&, const WinContext& ctx) { return ctx.rinshan ? 1 : 0; }
int chankan(const Reading&, const WinContext& ctx) { return ctx.chankan ? 1 : 0; }

// --- Hand shape yaku --------------------------------------------------------

HanFunction tanyao(bool openAllowed) {
    return [openAllowed](const Reading& reading, const WinContext&) {
        if (reading.form == HandForm::Kokushi) return 0;
        if (reading.handOpen && !openAllowed) return 0;
        for (const Block& block : reading.blocks) {
            const int number = block.first.number();
            const bool simple = block.type == BlockType::Sequence ? number >= 2 && number <= 6 : block.first.isSimple();
            if (!simple) return 0;
        }
        return 1;
    };
}

int pinfu(const Reading& reading, const WinContext& ctx) {
    if (!isRegular(reading) || reading.handOpen || reading.wait != WaitType::Ryanmen) return 0;
    for (const Block& block : reading.blocks) {
        if (isTripletOrKan(block)) return 0;
        if (block.type == BlockType::Pair && isYakuhaiPair(block.first, ctx)) return 0;
    }
    return 1;
}

HanFunction dragon(Honor honor) {
    return [honor](const Reading& reading, const WinContext&) { return yakuhai(reading, Tile::honor(honor)); };
}

int seatWind(const Reading& reading, const WinContext& ctx) { return yakuhai(reading, Tile::honor(ctx.seatWind)); }
int roundWind(const Reading& reading, const WinContext& ctx) { return yakuhai(reading, Tile::honor(ctx.roundWind)); }

int iipeikou(const Reading& reading, const WinContext&) {
    return isRegular(reading) && !reading.handOpen && identicalSequencePairs(reading) == 1 ? 1 : 0;
}

int ryanpeikou(const Reading& reading, const WinContext&) {
    return isRegular(reading) && !reading.handOpen && identicalSequencePairs(reading) == 2 ? 3 : 0;
}

int chiitoitsu(const Reading& reading, const WinContext&) { return reading.form == HandForm::Chiitoitsu ? 2 : 0; }

int toitoi(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const bool anySequence = std::any_of(reading.blocks.begin(), reading.blocks.end(),
                                         [](const Block& block) { return block.type == BlockType::Sequence; });
    return anySequence ? 0 : 2;
}

// Three concealed triplets or kans. A triplet completed by ron is open.
int sanankou(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const auto closedTriplets = std::count_if(reading.blocks.begin(), reading.blocks.end(), [](const Block& block) {
        return isTripletOrKan(block) && !block.open;
    });
    return closedTriplets >= 3 ? 2 : 0;
}

int sanshoku(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const SequenceStarts starts = sequenceStarts(reading);
    for (size_t number = 1; number <= 7; ++number) {
        if (starts[0][number] && starts[1][number] && starts[2][number]) return byOpenness(reading, 2, 1);
    }
    return 0;
}

int ittsu(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const SequenceStarts starts = sequenceStarts(reading);
    for (const auto& suit : starts) {
        if (suit[1] && suit[4] && suit[7]) return byOpenness(reading, 2, 1);
    }
    return 0;
}

// Every block has a terminal or honor, with at least one sequence and one
// honor (without honors it is junchan, without sequences honroutou).
int chanta(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    bool sequence = false;
    bool honor = false;
    for (const Block& block : reading.blocks) {
        if (block.type == BlockType::Sequence) {
            sequence = true;
            if (block.first.number() != 1 && block.first.number() != 7) return 0;
        } else {
            if (!block.first.isTerminalOrHonor()) return 0;
            if (block.first.isHonor()) honor = true;
        }
    }
    return sequence && honor ? byOpenness(reading, 2, 1) : 0;
}

// One suit and honors.
int honitsu(const Reading& reading, const WinContext&) {
    if (reading.form == HandForm::Kokushi) return 0;
    const Composition composition = compositionOf(reading);
    return composition.suitCount() == 1 && composition.honors ? byOpenness(reading, 3, 2) : 0;
}

// One suit only.
int chinitsu(const Reading& reading, const WinContext&) {
    if (reading.form == HandForm::Kokushi) return 0;
    const Composition composition = compositionOf(reading);
    return composition.suitCount() == 1 && !composition.honors ? byOpenness(reading, 6, 5) : 0;
}

// Every block has a terminal, with at least one sequence and no honor.
int junchan(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    for (const Block& block : reading.blocks) {
        if (block.type == BlockType::Sequence) {
            if (block.first.number() != 1 && block.first.number() != 7) return 0;
        } else if (!block.first.isTerminal()) {
            return 0;
        }
    }
    return hasSequence(reading) ? byOpenness(reading, 3, 2) : 0;
}

// Only terminals and honors, both of them, and no sequence.
int honroutou(const Reading& reading, const WinContext&) {
    if (reading.form == HandForm::Kokushi || hasSequence(reading)) return 0;
    if (!allBlocks(reading, [](const Block& block) { return block.first.isTerminalOrHonor(); })) return 0;
    const int honorBlocks = countBlocks(reading, [](const Block& block) { return block.first.isHonor(); });
    const bool both = honorBlocks > 0 && honorBlocks < static_cast<int>(reading.blocks.size());
    return both ? 2 : 0;
}

// Two dragon triplets and a dragon pair.
int shousangen(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    return tripletsOf(reading, isDragonTile) == 2 && pairsOf(reading, isDragonTile) == 1 ? 2 : 0;
}

// Triplets or kans of the same number in all three suits.
int sanshokuDoukou(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    std::array<std::array<bool, 10>, 3> triplets{};
    for (const Block& block : reading.blocks) {
        if (isTripletOrKan(block) && !block.first.isHonor()) {
            triplets[static_cast<size_t>(block.first.suit())][static_cast<size_t>(block.first.number())] = true;
        }
    }
    for (size_t number = 1; number <= 9; ++number) {
        if (triplets[0][number] && triplets[1][number] && triplets[2][number]) return 2;
    }
    return 0;
}

int sankantsu(const Reading& reading, const WinContext&) {
    return isRegular(reading) && kanCount(reading) == 3 ? 2 : 0;
}

// --- Yakuman ----------------------------------------------------------------

// Double with a 13-sided wait: all 13 kinds once before the winning tile.
int kokushi(const Reading& reading, const WinContext& ctx) {
    if (reading.form != HandForm::Kokushi) return 0;
    const TileCounts before = countsBeforeWin(ctx);
    const bool thirteenSided = std::none_of(before.begin(), before.end(), [](uint8_t n) { return n > 1; });
    return thirteenSided ? 2 * kYakumanHan : kYakumanHan;
}

// Four concealed triplets or kans; a triplet completed by ron is open. Double on a tanki wait.
int suuankou(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const int closed = countBlocks(reading, [](const Block& block) { return isTripletOrKan(block) && !block.open; });
    if (closed != 4) return 0;
    return reading.wait == WaitType::Tanki ? 2 * kYakumanHan : kYakumanHan;
}

int daisangen(const Reading& reading, const WinContext&) {
    return isRegular(reading) && tripletsOf(reading, isDragonTile) == 3 ? kYakumanHan : 0;
}

int shousuushii(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    return tripletsOf(reading, isWindTile) == 3 && pairsOf(reading, isWindTile) == 1 ? kYakumanHan : 0;
}

int daisuushii(const Reading& reading, const WinContext&) {
    return isRegular(reading) && tripletsOf(reading, isWindTile) == 4 ? 2 * kYakumanHan : 0;
}

int tsuuiisou(const Reading& reading, const WinContext&) {
    if (reading.form == HandForm::Kokushi) return 0;
    return allBlocks(reading, [](const Block& block) { return block.first.isHonor(); }) ? kYakumanHan : 0;
}

int chinroutou(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const bool terminals = allBlocks(reading, [](const Block& block) {
        return block.type != BlockType::Sequence && block.first.isTerminal();
    });
    return terminals ? kYakumanHan : 0;
}

// Only 2s 3s 4s 6s 8s and the green dragon.
int ryuuiisou(const Reading& reading, const WinContext&) {
    if (!isRegular(reading)) return 0;
    const Tile twoSou = Tile::make(Suit::Sou, 2);
    const auto green = [](Tile t) {
        if (t.sameKind(Tile::honor(Honor::Green))) return true;
        const int n = t.number();
        return t.suit() == Suit::Sou && (n == 2 || n == 3 || n == 4 || n == 6 || n == 8);
    };
    const bool allGreen = allBlocks(reading, [&](const Block& block) {
        return block.type == BlockType::Sequence ? block.first.sameKind(twoSou) : green(block.first);
    });
    return allGreen ? kYakumanHan : 0;
}

// Closed, one suit, 1112345678999 plus one tile of the suit. Double when the
// hand before the winning tile was exactly 1112345678999 (a 9-sided wait).
int chuurenPoutou(const Reading& reading, const WinContext& ctx) {
    if (!isRegular(reading) || ctx.hand.meldCount() > 0) return 0;
    const Suit suit = ctx.winTile.suit();
    if (suit == Suit::Honor) return 0;
    const TileCounts counts = ctx.hand.counts();
    const size_t first = static_cast<size_t>(suit) * 9;
    constexpr std::array<int, 9> kBase = {3, 1, 1, 1, 1, 1, 1, 1, 3};

    int inSuit = 0;
    for (size_t i = 0; i < 9; ++i) {
        if (counts[first + i] < kBase[i]) return 0;
        inSuit += counts[first + i];
    }
    if (inSuit != 14) return 0;  // a tile outside the suit

    const TileCounts before = countsBeforeWin(ctx);
    for (size_t i = 0; i < 9; ++i) {
        if (before[first + i] != kBase[i]) return kYakumanHan;
    }
    return 2 * kYakumanHan;
}

int suukantsu(const Reading& reading, const WinContext&) {
    return isRegular(reading) && kanCount(reading) == 4 ? kYakumanHan : 0;
}

int tenhou(const Reading&, const WinContext& ctx) {
    return ctx.firstDraw && ctx.seatWind == Honor::East ? kYakumanHan : 0;
}

int chiihou(const Reading&, const WinContext& ctx) {
    return ctx.firstDraw && ctx.seatWind != Honor::East ? kYakumanHan : 0;
}

}  // namespace

std::vector<Yaku> standardYaku(const RuleSet& rules) {
    return {
        {"Riichi", riichi},
        {"Double Riichi", doubleRiichi},
        {"Ippatsu", ippatsu},
        {"Menzen Tsumo", menzenTsumo},
        {"Haitei", haitei},
        {"Houtei", houtei},
        {"Rinshan Kaihou", rinshanKaihou},
        {"Chankan", chankan},
        {"Tanyao", tanyao(rules.kuitan)},
        {"Pinfu", pinfu},
        {"White Dragon", dragon(Honor::White)},
        {"Green Dragon", dragon(Honor::Green)},
        {"Red Dragon", dragon(Honor::Red)},
        {"Seat Wind", seatWind},
        {"Round Wind", roundWind},
        {"Iipeikou", iipeikou},
        {"Ryanpeikou", ryanpeikou},
        {"Chiitoitsu", chiitoitsu},
        {"Toitoi", toitoi},
        {"Sanankou", sanankou},
        {"Sanshoku", sanshoku},
        {"Ittsu", ittsu},
        {"Chanta", chanta},
        {"Honitsu", honitsu},
        {"Chinitsu", chinitsu},
        {"Junchan", junchan},
        {"Honroutou", honroutou},
        {"Shousangen", shousangen},
        {"Sanshoku Doukou", sanshokuDoukou},
        {"Sankantsu", sankantsu},
        {"Kokushi Musou", kokushi},
        {"Suuankou", suuankou},
        {"Daisangen", daisangen},
        {"Shousuushii", shousuushii},
        {"Daisuushii", daisuushii},
        {"Tsuuiisou", tsuuiisou},
        {"Chinroutou", chinroutou},
        {"Ryuuiisou", ryuuiisou},
        {"Chuuren Poutou", chuurenPoutou},
        {"Suukantsu", suukantsu},
        {"Tenhou", tenhou},
        {"Chiihou", chiihou},
    };
}

}  // namespace mj
