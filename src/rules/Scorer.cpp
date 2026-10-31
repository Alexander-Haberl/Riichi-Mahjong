#include "rules/Scorer.hpp"

#include <algorithm>
#include <tuple>

namespace mj {

namespace {

// Sequences give nothing. Triplets: 2 for an open triplet of simples, doubled
// when closed, doubled again for terminals and honors, times four for a kan.
// Pairs: 2 each for a dragon, the seat wind and the round wind.
int blockFu(const Block& block, const WinContext& ctx) {
    const Tile t = block.first;
    switch (block.type) {
        case BlockType::Sequence:
            return 0;
        case BlockType::Pair:
            if (t.isDragon()) return 2;
            if (!t.isWind()) return 0;
            return (t.asHonor() == ctx.roundWind ? 2 : 0) + (t.asHonor() == ctx.seatWind ? 2 : 0);
        case BlockType::Triplet:
        case BlockType::Kan:
            break;
    }
    int fu = block.open ? 2 : 4;
    if (t.isTerminalOrHonor()) fu *= 2;
    if (block.type == BlockType::Kan) fu *= 4;
    return fu;
}

int waitFu(std::optional<WaitType> wait) {
    return wait == WaitType::Kanchan || wait == WaitType::Penchan || wait == WaitType::Tanki ? 2 : 0;
}

}  // namespace

DoraCount countDora(const WinContext& ctx) {
    DoraCount result;
    TileCounts counts{};
    auto add = [&](Tile t) {
        ++counts[t.kind()];
        if (t.isRed()) ++result.akaDora;  // the counts cannot carry the red flag
    };
    for (Tile t : ctx.hand.concealed()) add(t);
    for (const Meld& meld : ctx.hand.melds()) {
        for (Tile t : meld.tiles()) add(t);
    }

    auto doraFor = [&counts](const std::vector<Tile>& indicators) {
        int dora = 0;
        for (Tile indicator : indicators) dora += counts[indicator.doraFromIndicator().kind()];
        return dora;
    };
    result.dora = doraFor(ctx.doraIndicators);
    if (ctx.riichi || ctx.doubleRiichi) result.uraDora = doraFor(ctx.uraDoraIndicators);
    return result;
}

BasicPoints basicPoints(int han, int fu, bool yakuman, const RuleSet& rules) {
    if (yakuman) return {8000 * (han / kYakumanHan), ScoreName::Yakuman};
    if (han >= 13 && rules.kazoeYakuman) return BasicPoints{8000, ScoreName::KazoeYakuman};
    if (han >= 11) return {6000, ScoreName::Sanbaiman};
    if (han >= 8) return {4000, ScoreName::Baiman};
    if (han >= 6) return {3000, ScoreName::Haneman};
    if (han >= 5) return {2000, ScoreName::Mangan};

    const int points = fu * (1 << (2 + han));
    if (points >= 2000 || (rules.kiriageMangan && points == 1920)) return {2000, ScoreName::Mangan};
    return {points, ScoreName::None};
}

int unroundedFu(const Reading& reading, const WinContext& ctx) {
    if (reading.form == HandForm::Chiitoitsu) return 25;
    if (reading.form == HandForm::Kokushi) return 0;

    int fu = 20 + waitFu(reading.wait);
    for (const Block& block : reading.blocks) fu += blockFu(block, ctx);

    const bool closed = !reading.handOpen;
    if (ctx.tsumo) {
        if (!(closed && fu == 20)) fu += 2;  // a closed hand without fu is pinfu: no fu for the tsumo
    } else if (closed) {
        fu += 10;
    }
    return fu;
}

int calculateFu(const Reading& reading, const WinContext& ctx) {
    const int fu = unroundedFu(reading, ctx);
    if (reading.form != HandForm::Regular) return fu;
    if (fu == 20 && reading.handOpen) return 30;  // open "pinfu" ron
    return (fu + 9) / 10 * 10;
}

Scorer standardScorer(const RuleSet& rules) {
    return Scorer(standardYaku(rules), rules);
}

Scorer::Scorer(std::vector<Yaku> yaku, const RuleSet& rules)
    : yaku_(std::move(yaku)), rules_(rules) {}

std::optional<ScoreResult> Scorer::score(const WinContext& ctx) const {
    const DoraCount dora = countDora(ctx);  // the same for every reading
    const std::vector<HanSource> doraSources = {
        {"Dora", dora.dora}, {"Ura Dora", dora.uraDora}, {"Aka Dora", dora.akaDora}};

    std::optional<ScoreResult> best;
    for (Reading& reading : readings(ctx.hand, ctx.winTile, !ctx.tsumo)) {
        ScoreResult result{std::move(reading)};
        std::vector<HanSource> yakuman;
        for (const Yaku& yaku : yaku_) {
            const int han = yaku.han(result.reading, ctx);
            if (han <= 0) continue;
            if (han >= kYakumanHan) {
                yakuman.push_back({yaku.name, rules_.doubleYakuman ? han : kYakumanHan});
            } else {
                result.hanSources.push_back({yaku.name, han});
                result.han += han;
            }
        }
        if (yakuman.empty() && result.hanSources.empty()) continue;  // no yaku: no win

        const bool isYakuman = !yakuman.empty();
        if (isYakuman) {
            if (!rules_.stackYakuman) {
                auto highest = std::max_element(yakuman.begin(), yakuman.end(),
                                                [](const HanSource& a, const HanSource& b) { return a.han < b.han; });
                yakuman = {*highest};
            }
            result.hanSources = std::move(yakuman);
            result.han = 0;
            for (const HanSource& source : result.hanSources) result.han += source.han;
        } else {
            for (const HanSource& source : doraSources) {
                if (source.han <= 0) continue;
                result.hanSources.push_back(source);
                result.han += source.han;
            }
            if (rules_.kazoeYakuman && result.han > kYakumanHan) result.han = kYakumanHan;
        }

        result.fu = calculateFu(result.reading, ctx);
        const BasicPoints points = basicPoints(result.han, result.fu, isYakuman, rules_);
        result.basicPoints = points.points;
        result.name = points.name;

        if (!best || std::tie(result.basicPoints, result.han, result.fu) >
                         std::tie(best->basicPoints, best->han, best->fu)) {
            best = std::move(result);
        }
    }
    return best;
}

}  // namespace mj
