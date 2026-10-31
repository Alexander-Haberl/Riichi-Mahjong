#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "rules/Decomposer.hpp"
#include "rules/RuleSet.hpp"
#include "rules/WinContext.hpp"
#include "rules/Yaku.hpp"

namespace mj {

// What a hand is called because of its value.
enum class ScoreName {
    None,          // below mangan
    Mangan,        // 5 han, or fewer han with enough fu
    Haneman,       // 6-7 han
    Baiman,        // 8-10 han
    Sanbaiman,     // 11-12 han (also 13+ without kazoe yakuman)
    KazoeYakuman,  // 13+ han from regular yaku and dora
    Yakuman,       // one or more yakuman
};

struct HanSource {
    std::string_view name;
    int han;
};

struct ScoreResult {
    Reading reading;  // the reading the score is based on
    // The yaku and dora, or only the yakuman if the hand has any.
    std::vector<HanSource> hanSources{};
    // Total han; 13 for a kazoe yakuman, the sum of the yakuman for yakuman.
    int han = 0;
    int fu = 0;  // rounded, see calculateFu
    int basicPoints = 0;
    ScoreName name = ScoreName::None;
};

// Fu of one reading of a winning hand before rounding: 20 base, plus bonuses
// for triplets and kans, the pair, the wait, tsumo and ron on a closed hand.
// A pinfu tsumo is a flat 20, chiitoitsu 25 and kokushi 0 (fu do not matter
// for yakuman). Blocks marked open count as open, including a triplet
// completed by ron.
int unroundedFu(const Reading& reading, const WinContext& ctx);

// The fu that count for scoring: rounded up to the next 10. Chiitoitsu stays
// 25, and an open hand without any fu bonus counts as 30.
int calculateFu(const Reading& reading, const WinContext& ctx);

struct DoraCount {
    int dora = 0;
    int uraDora = 0;  // only counted in riichi
    int akaDora = 0;  // red fives
};

// Dora in the whole hand (melds and the winning tile included); a kan counts
// all four of its tiles. Whether the hand has a yaku is not checked here.
DoraCount countDora(const WinContext& ctx);

struct BasicPoints {
    int points;
    ScoreName name;
};

// Basic points of a hand: fu * 2^(2 + han) below mangan, otherwise the limit.
// For yakuman, `han` is the sum of the yakuman (13 each, 26 for a double).
BasicPoints basicPoints(int han, int fu, bool yakuman, const RuleSet& rules);

// Scores a win: tries every reading of the hand and keeps the one with the
// most basic points (then han, then fu). Dora only count when the hand has a
// yaku; yakuman replace all other han.
class Scorer {
public:
    // Keeps its own copy of the rules.
    Scorer(std::vector<Yaku> yaku, const RuleSet& rules);

    // Empty if the hand is not complete or has no yaku (no valid win).
    std::optional<ScoreResult> score(const WinContext& ctx) const;

    const RuleSet& rules() const { return rules_; }

private:
    std::vector<Yaku> yaku_;
    RuleSet rules_;
};

// A scorer with all standard yaku (standardYaku) for these rules.
Scorer standardScorer(const RuleSet& rules);

}  // namespace mj
