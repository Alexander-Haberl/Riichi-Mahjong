#pragma once

#include <array>
#include <optional>
#include <vector>

#include "game/Action.hpp"
#include "game/GameStep.hpp"
#include "game/LegalActions.hpp"
#include "game/Player.hpp"
#include "game/RoundState.hpp"
#include "rules/RuleSet.hpp"
#include "rules/Scorer.hpp"

namespace mj {

enum class RoundEnd { Tsumo, Ron, ExhaustiveDraw, Aborted };

struct RoundResult {
    RoundEnd end;
    std::vector<Win> wins;
    std::array<bool, 4> tenpai{};             // only filled at an exhaustive draw
    std::optional<AbortReason> abortReason{};  // only filled when aborted
};

// Runs one round step by step. It works until it needs a decision, then stops
// and says who has to decide what (pending). The caller answers with submit,
// whenever it is ready, and the round goes on to the next decision. Humans and
// AIs answer the same way; the controller never calls anyone.
// Payments are not done here: see Game.
class RoundController {
public:
    // The rules are the scorer's rules. With a viewerSeat, everything that
    // happens is recorded as GameSteps as this seat sees it; without one,
    // nothing is recorded. The scorer has to outlive the controller.
    RoundController(RoundState state, const Scorer& scorer, std::optional<int> viewerSeat = std::nullopt);

    // Deals and runs to the first decision. Call once.
    void start();

    // The decision the round is waiting for; empty once the round is over.
    const std::optional<Decision>& pending() const { return pending_; }

    // Answers the pending decision and runs to the next one.
    // Throws std::logic_error if nothing is pending or the action is not legal.
    void submit(Action action);

    // The end of the round; empty until then.
    const std::optional<RoundResult>& result() const { return result_; }

    // Everything recorded since the last call, in order.
    std::vector<GameStep> takeSteps();

    // Plays the whole round by asking these players for every decision.
    RoundResult play(const std::array<IPlayer*, 4>& players);

    const RoundState& state() const { return state_; }
    const RuleSet& rules() const { return scorer_.rules(); }

private:
    using Choices = std::array<std::optional<Action>, 4>;

    // A tile the other players may react to, while their answers come in.
    struct Claim {
        Tile tile;
        int fromSeat;
        ClaimSource source;
        bool declaringRiichi = false;
        Choices choices{};
        int nextOffset = 1;  // the next seat to ask, relative to fromSeat
    };

    void deal();
    void draw();       // uses tileDraw and rinshan to decide what to do
    void beginTurn();  // draws if needed, then waits for the current player

    void applyTurnAction(const Action& action);
    void closedKan(const Action& action);
    void discard(const Action& action);

    void askNextReaction();
    void recordReaction(int seat, const Action& action, const std::vector<Action>& legal);
    void resolveReactions();
    std::optional<RoundResult> rons(const Claim& claim) const;
    void resolveKan(const Claim& claim);
    // Applies a pon, open kan or chi if there is one, and returns whether there was.
    bool resolveCalls(const Claim& claim);
    void call(int seat, const Action& action, int fromSeat);
    void afterKan();
    void breakIppatsu();

    void finishTsumo();
    void finishRon(RoundResult result, const Claim& claim);
    void finishAbort(AbortReason reason, int seat);
    void finishExhaustiveDraw();
    void finish(RoundResult result, GameEvent event);

    void record(GameEvent event);

    RoundState state_;
    const Scorer& scorer_;
    std::optional<int> viewerSeat_;

    std::optional<Decision> pending_;
    std::optional<Claim> claim_;  // set while reactions are collected
    std::optional<RoundResult> result_;
    std::vector<GameStep> steps_;
};

}  // namespace mj
