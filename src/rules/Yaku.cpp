#include "rules/Yaku.hpp"

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

// --- Yakuman ----------------------------------------------------------------

int kokushi(const Reading& reading, const WinContext&) {
    return reading.form == HandForm::Kokushi ? kYakumanHan : 0;
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
        {"Kokushi Musou", kokushi},
    };
}

}  // namespace mj
