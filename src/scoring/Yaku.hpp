#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "core/Decomposer.hpp"
#include "scoring/RuleSet.hpp"
#include "scoring/WinContext.hpp"

namespace mj {

// A yaku worth this many han or more is a yakuman; a double yakuman is worth
// twice as much.
inline constexpr int kYakumanHan = 13;

// Han a yaku is worth for one reading of the hand, 0 if it is not present.
// Reduced han for open hands is up to the yaku (via reading.handOpen).
// Yakuman return kYakumanHan, or 2 * kYakumanHan where they can count double.
using HanFunction = std::function<int(const Reading&, const WinContext&)>;

// One yaku. The Scorer holds a list of them and adds up their han, so a yaku
// has to exclude the yaku it conflicts with in its own condition.
struct Yaku {
    std::string_view name;
    HanFunction han;
};

// The standard yaku of the game, see docs/yaku-spec.md. Their names are unique.
std::vector<Yaku> standardYaku(const RuleSet& rules);

}  // namespace mj
