#pragma once

#include <array>
#include <optional>
#include <vector>

#include "game/Action.hpp"
#include "game/LegalActions.hpp"
#include "game/Player.hpp"
#include "game/RoundState.hpp"
#include "rules/RuleSet.hpp"
#include "rules/Scorer.hpp"

namespace mj {

enum class RoundEnd { Tsumo, Ron, ExhaustiveDraw, Aborted };

enum class AbortReason {
    KyuushuKyuuhai,  // nine terminals and honors
    SuufonRenda,     // four identical winds
    SuuchaRiichi,    // four riichi
    Suukaikan,       // four kans by more than one player
    Sanchahou,       // three rons
};

struct Win {
    int seat;
    int fromSeat;  // the discarder; equal to seat for tsumo
    ScoreResult score;
};

struct RoundResult {
    RoundEnd end;
    std::vector<Win> wins;
    std::array<bool, 4> tenpai{};             // only filled at an exhaustive draw
    std::optional<AbortReason> abortReason{};  // only filled when aborted
};

// Runs one round: deals, asks the players for their actions, resolves calls
// and wins, and applies everything to its RoundState. Payments are not done yet.
class RoundController {
public:
    // The rules are the scorer's rules.
    RoundController(RoundState state, std::array<IPlayer*, 4> players, const Scorer& scorer);

    // Deals and plays the round to its end. Call once.
    // Throws std::logic_error if a player picks an action that is not legal.
    RoundResult play();

    const RoundState& state() const { return state_; }
    const RuleSet& rules() const { return scorer_.rules(); }

private:
    using Choices = std::array<std::optional<Action>, 4>;

    void deal();
    void draw(); // uses tileDraw and rinshan to decide what to do
    Action ask(int seat, const std::vector<Action>& legal);

    RoundResult tsumo() const;
    void closedKan(const Action& action);
    std::optional<RoundResult> kanFromPon(const Action& action);
    std::optional<RoundResult> discard(const Action& action);

    // Asks every other seat that can react; marks furiten for passed rons.
    Choices askReactions(int fromSeat, Tile tile, ClaimSource source);
    std::optional<RoundResult> rons(const Choices& choices, int fromSeat, Tile tile, ClaimSource source) const;
    void call(int seat, const Action& action, int fromSeat);
    void afterKan();
    void breakIppatsu();

    RoundState state_;
    std::array<IPlayer*, 4> players_;
    const Scorer& scorer_;
};

}  // namespace mj
