#pragma once

#include <vector>
#include <optional>

#include "game/Action.hpp"
#include "game/RoundState.hpp"
#include "rules/RuleSet.hpp"
#include "rules/Scorer.hpp"
#include "rules/WinContext.hpp"

namespace mj {

// TODO concealed Kan can be robbed by 13 Orphans
enum class ClaimSource { Discard, KanFromPon };

// Actions the current player (state.current) can take on their turn.
std::vector<Action> legalTurnActions(const RoundState& state, const RuleSet& rules, const Scorer& scorer);

// Reactions `seat` can make to `tile`, given away by `fromSeat`. Includes Pass
// when there is anything else; empty if the seat cannot react at all.
std::vector<Action> legalReactions(const RoundState& state, int seat, Tile tile, int fromSeat, ClaimSource source,
                                   const RuleSet& rules, const Scorer& scorer);

// Tile kinds that may not be discarded right after this chi / pon (kuikae).
std::vector<Tile> kuikaeForbidden(const Meld& call);

// Win contexts for the current player's tsumo, and for `seat` winning on `tile`.
WinContext tsumoContext(const RoundState& state);
WinContext ronContext(const RoundState& state, int seat, Tile tile, ClaimSource source);

}  // namespace mj
