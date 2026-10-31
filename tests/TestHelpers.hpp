#pragma once

// Helpers shared by the test files.

#include <algorithm>
#include <string_view>
#include <vector>

#include "core/Tile.hpp"
#include "game/Player.hpp"
#include "rules/RuleSet.hpp"
#include "rules/Scorer.hpp"
#include "rules/Shanten.hpp"

namespace mj::test {

// The first tile of a tile string, e.g. tile("0m") is the red 5m.
inline Tile tile(const char* s) {
    return parseTiles(s).front();
}

// A winning hand. `tiles` are all the player's own tiles, including those that
// go into `melds` (the called tile of a meld is not part of them). Seat and
// round wind are East.
inline WinContext win(const char* tiles, const char* winTile, bool tsumo, const std::vector<Meld>& melds = {}) {
    WinContext ctx;
    ctx.hand = Hand(parseTiles(tiles));
    for (const Meld& meld : melds) ctx.hand.addMeld(meld);
    ctx.winTile = tile(winTile);
    ctx.tsumo = tsumo;
    return ctx;
}

// Rule set with every option given explicitly, so tests do not depend on
// RuleSet's defaults. Tests about one option set that option again themselves.
// Add new RuleSet options here too.
inline RuleSet testRules() {
    RuleSet rules;
    rules.startingPoints = 25000;
    rules.redFivesPerSuit = 1;
    rules.gameLength = GameLength::Hanchan;
    rules.renchanOnDealerTenpai = true;
    rules.renchanOnAbort = true;
    rules.endOnBust = true;
    rules.kazoeYakuman = true;
    rules.doubleYakuman = true;
    rules.stackYakuman = true;
    rules.kiriageMangan = false;
    rules.kuitan = true;
    rules.allowKuikae = false;
    rules.riichiNeeds1000Points = true;
    rules.multipleRon = true;
    rules.tripleRonAborts = true;
    rules.kyuushuKyuuhai = true;
    rules.suufonRenda = true;
    rules.suuchaRiichi = true;
    rules.suukaikan = true;
    return rules;
}

// --- Stand-in yaku, for testing the scorer without real hands -----------------

// Present in every hand.
inline const Yaku kAlways{"Always", [](const Reading&, const WinContext&) { return 1; }};

inline int allTripletsHan(const Reading& reading, const WinContext&) {
    if (reading.form != HandForm::Regular) return 0;
    const bool all = std::none_of(reading.blocks.begin(), reading.blocks.end(),
                                  [](const Block& b) { return b.type == BlockType::Sequence; });
    return all ? 2 : 0;
}

// 2 han for a regular hand without sequences.
inline const Yaku kAllTriplets{"AllTriplets", allTripletsHan};

// 1 han in riichi.
inline const Yaku kRiichiOnly{"RiichiOnly", [](const Reading&, const WinContext& ctx) { return ctx.riichi ? 1 : 0; }};

inline Scorer makeScorer(std::vector<Yaku> yaku, const RuleSet& rules = testRules()) {
    return Scorer(std::move(yaku), rules);
}

// No hand can win.
inline Scorer noYaku(const RuleSet& rules = testRules()) {
    return makeScorer({}, rules);
}

// Every complete hand can win.
inline Scorer alwaysYaku(const RuleSet& rules = testRules()) {
    return makeScorer({kAlways}, rules);
}

// --- Players ----------------------------------------------------------------

// Plays by a few fixed habits. By default it passes on everything and
// discards the drawn tile; the flags make it take specific actions instead.
class TestPlayer : public IPlayer {
public:
    bool takeWins = false;  // tsumo and ron
    bool callPon = false;
    bool callOpenKan = false;
    bool declareRiichi = false;
    bool declareClosedKan = false;
    bool declareKanFromPon = false;
    bool declareKyuushu = false;
    // Discard the tile that leaves the lowest shanten instead of the drawn tile,
    // so the hand improves and the player gets tenpai and wins now and then.
    bool minimizeShanten = false;

    int seat = -1;  // the seat this player was last asked for

    Action chooseAction(const PlayerView& view, const std::vector<Action>& legal) override {
        seat = view.seat();

        std::vector<ActionType> wanted;
        if (takeWins) wanted.insert(wanted.end(), {ActionType::Tsumo, ActionType::Ron});
        if (callPon) wanted.push_back(ActionType::Pon);
        if (callOpenKan) wanted.push_back(ActionType::OpenKan);
        if (declareRiichi) wanted.push_back(ActionType::Riichi);
        if (declareClosedKan) wanted.push_back(ActionType::ClosedKan);
        if (declareKanFromPon) wanted.push_back(ActionType::KanFromPon);
        if (declareKyuushu) wanted.push_back(ActionType::KyuushuKyuuhai);
        wanted.push_back(ActionType::Pass);

        for (ActionType type : wanted) {
            auto it = std::find_if(legal.begin(), legal.end(), [type](const Action& a) { return a.type == type; });
            if (it != legal.end()) return *it;
        }
        if (minimizeShanten) {
            const Action* best = nullptr;
            int bestShanten = 99;
            for (const Action& a : legal) {
                if (a.type != ActionType::Discard) continue;
                TileCounts counts = view.hand().counts();
                --counts[a.tile.kind()];
                const int s = shanten::calc(counts, view.hand().meldCount());
                if (s < bestShanten) {
                    bestShanten = s;
                    best = &a;
                }
            }
            if (best) return *best;
        }
        for (const Action& a : legal) {
            if (a.type == ActionType::Discard && a.tile == view.hand().lastTile()) return a;
        }
        return *std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.type == ActionType::Discard; });
    }
};

}  // namespace mj::test
