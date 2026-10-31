#include "game/RoundController.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "rules/Decomposer.hpp"

namespace mj {

namespace {

RoundResult aborted(AbortReason reason) {
    return RoundResult{RoundEnd::Aborted, {}, {}, reason};
}

MeldType meldTypeOf(ActionType type) {
    switch (type) {
        case ActionType::Chi: return MeldType::Chi;
        case ActionType::Pon: return MeldType::Pon;
        case ActionType::OpenKan: return MeldType::OpenKan;
        case ActionType::ClosedKan: return MeldType::ClosedKan;
        default: throw std::logic_error("this action makes no meld");
    }
}

EventType eventTypeOf(MeldType type) {
    switch (type) {
        case MeldType::Chi: return EventType::Chi;
        case MeldType::Pon: return EventType::Pon;
        case MeldType::OpenKan:
        case MeldType::ClosedKan: return EventType::Kan;
    }
    throw std::logic_error("unknown meld type");
}

bool allInRiichi(const RoundState& state) {
    return std::all_of(state.players.begin(), state.players.end(), [](const PlayerState& p) { return p.riichi; });
}

// All four kans are made, and not all by the same player.
bool fourKansByDifferentPlayers(const RoundState& state) {
    if (state.wall.rinshanRemaining() > 0) return false;
    auto hasKan = [](const PlayerState& p) {
        const auto& melds = p.hand.melds();
        return std::any_of(melds.begin(), melds.end(), [](const Meld& m) { return m.isKan(); });
    };
    return std::count_if(state.players.begin(), state.players.end(), hasKan) > 1;
}

// Every player's only discard is the same wind, and nobody called in between.
bool fourSameWinds(const RoundState& state) {
    if (state.anyCall) return false;
    const Tile first = state.player(0).river.empty() ? Tile() : state.player(0).river[0].tile;
    return first.isWind() && std::all_of(state.players.begin(), state.players.end(), [first](const PlayerState& p) {
               return p.river.size() == 1 && p.river[0].tile.sameKind(first);
           });
}

}  // namespace

RoundController::RoundController(RoundState state, const Scorer& scorer, std::optional<int> viewerSeat)
    : state_(std::move(state)), scorer_(scorer), viewerSeat_(viewerSeat) {}

void RoundController::start() {
    deal();
    record({EventType::RoundStart});
    state_.current = state_.dealer;
    state_.tileDraw = true;  // the dealer's first turn starts with a draw
    state_.rinshan = false;
    beginTurn();
}

void RoundController::submit(Action action) {
    if (!pending_) throw std::logic_error("no decision is pending");
    const Decision decision = *pending_;
    if (std::find(decision.legal.begin(), decision.legal.end(), action) == decision.legal.end()) {
        throw std::logic_error("player " + std::to_string(decision.seat) + " chose an illegal action");
    }
    pending_.reset();

    if (claim_) {
        recordReaction(decision.seat, action, decision.legal);
        askNextReaction();
    } else {
        applyTurnAction(action);
    }
}

std::vector<GameStep> RoundController::takeSteps() {
    std::vector<GameStep> steps = std::move(steps_);
    steps_.clear();
    return steps;
}

RoundResult RoundController::play(const std::array<IPlayer*, 4>& players) {
    start();
    while (pending_) {
        const int seat = pending_->seat;
        submit(players[static_cast<size_t>(seat)]->chooseAction(PlayerView(state_, seat), pending_->legal));
    }
    return *result_;
}

void RoundController::deal() {
    for (int i = 0; i < 4; ++i) {
        PlayerState& player = state_.player(seatAdd(state_.dealer, i));
        for (int t = 0; t < 13; ++t) player.hand.add(state_.wall.draw());
    }
}

void RoundController::draw() {
    if (!state_.tileDraw) return;  // after a chi / pon the turn starts with the discard
    Tile tile = state_.rinshan ? state_.wall.drawRinshan() : state_.wall.draw();
    state_.currentPlayer().hand.add(tile);
    state_.lastCall.reset();

    GameEvent event{EventType::Draw, state_.current};
    event.tile = tile;
    record(std::move(event));
}

void RoundController::beginTurn() {
    draw();
    pending_ = Decision{state_.current, legalTurnActions(state_, rules(), scorer_)};
}

void RoundController::applyTurnAction(const Action& action) {
    switch (action.type) {
        case ActionType::Tsumo:
            finishTsumo();
            break;
        case ActionType::KyuushuKyuuhai:
            finishAbort(AbortReason::KyuushuKyuuhai, state_.current);
            break;
        case ActionType::ClosedKan:
            closedKan(action);
            break;
        case ActionType::KanFromPon:
            // The other players may rob the added tile (chankan) before the kan happens.
            claim_ = Claim{action.tile, state_.current, ClaimSource::KanFromPon};
            askNextReaction();
            break;
        case ActionType::Discard:
        case ActionType::Riichi:
            discard(action);
            break;
        default:
            // submit() only lets through legal actions, and those have no reactions here.
            throw std::logic_error("reaction chosen on the player's own turn");
    }
}

void RoundController::closedKan(const Action& action) {
    Meld meld{meldTypeOf(action.type), action.handTiles, std::nullopt, state_.current};
    state_.currentPlayer().hand.addMeld(meld);

    GameEvent event{eventTypeOf(meld.type), state_.current};
    event.meld = meld;
    record(std::move(event));

    afterKan();
    beginTurn();
}

void RoundController::discard(const Action& action) {
    PlayerState& player = state_.currentPlayer();
    const bool declaringRiichi = action.type == ActionType::Riichi;
    // TODO add tsumogiri when added to Action: with an identical tile elsewhere
    // in the hand, this cannot tell which of the two was discarded.
    const bool tsumogiri = state_.tileDraw && action.tile == player.tileDrawn();

    if (declaringRiichi) {
        player.declareRiichi(state_.anyCall);
    } else {
        player.ippatsu = false;
    }
    player.discard(action.tile, tsumogiri, declaringRiichi);
    player.temporaryFuriten = false;

    GameEvent event{EventType::Discard, state_.current};
    event.tile = action.tile;
    event.riichiDiscard = declaringRiichi;
    record(std::move(event));

    claim_ = Claim{action.tile, state_.current, ClaimSource::Discard, declaringRiichi};
    askNextReaction();
}

// Stops at the next seat that can react; when there is none, the reactions are complete.
void RoundController::askNextReaction() {
    Claim& claim = *claim_;
    while (claim.nextOffset < 4) {
        const int seat = seatAdd(claim.fromSeat, claim.nextOffset);
        ++claim.nextOffset;
        std::vector<Action> legal =
            legalReactions(state_, seat, claim.tile, claim.fromSeat, claim.source, rules(), scorer_);
        if (!legal.empty()) {
            pending_ = Decision{seat, std::move(legal)};
            return;
        }
    }
    resolveReactions();
}

void RoundController::recordReaction(int seat, const Action& action, const std::vector<Action>& legal) {
    const bool couldRon =
        std::any_of(legal.begin(), legal.end(), [](const Action& a) { return a.type == ActionType::Ron; });
    if (couldRon && action.type != ActionType::Ron) state_.player(seat).passedOnRon();
    claim_->choices[static_cast<size_t>(seat)] = action;
}

void RoundController::resolveReactions() {
    const Claim claim = std::move(*claim_);
    claim_.reset();

    if (std::optional<RoundResult> roned = rons(claim)) {
        if (roned->abortReason) finishAbort(*roned->abortReason, claim.fromSeat);
        else finishRon(std::move(*roned), claim);
        return;
    }

    if (claim.source == ClaimSource::KanFromPon) {
        resolveKan(claim);
        return;
    }

    // The riichi stands once its discard was not ronned.
    if (claim.declaringRiichi) {
        state_.player(claim.fromSeat).score -= 1000;
        ++state_.riichiSticks;
        if (rules().suuchaRiichi && allInRiichi(state_)) {
            finishAbort(AbortReason::SuuchaRiichi, claim.fromSeat);
            return;
        }
    }

    // The discard after the fourth kan was not ronned.
    if (rules().suukaikan && fourKansByDifferentPlayers(state_)) {
        finishAbort(AbortReason::Suukaikan, claim.fromSeat);
        return;
    }

    if (resolveCalls(claim)) return;

    if (rules().suufonRenda && fourSameWinds(state_)) {
        finishAbort(AbortReason::SuufonRenda, claim.fromSeat);
        return;
    }

    if (state_.wall.remaining() == 0) {
        finishExhaustiveDraw();
        return;
    }

    // setup next turn
    state_.current = seatAdd(claim.fromSeat, 1);
    state_.tileDraw = true;
    state_.rinshan = false;
    state_.lastCall.reset();
    beginTurn();
}

// Nobody robbed the tile added to the pon: the kan happens.
void RoundController::resolveKan(const Claim& claim) {
    Hand& hand = state_.currentPlayer().hand;
    hand.addKanToPon(claim.tile);

    GameEvent event{EventType::Kan, state_.current};
    event.tile = claim.tile;
    for (const Meld& meld : hand.melds()) {
        if (meld.kanFromPon && meld.baseTile().sameKind(claim.tile)) event.meld = meld;
    }
    record(std::move(event));

    afterKan();
    beginTurn();
}

bool RoundController::resolveCalls(const Claim& claim) {
    // Pon and open kan go before chi.
    for (int seat = 0; seat < 4; ++seat) {
        const auto& choice = claim.choices[static_cast<size_t>(seat)];
        if (choice && (choice->type == ActionType::Pon || choice->type == ActionType::OpenKan)) {
            call(seat, *choice, claim.fromSeat);
            beginTurn();
            return true;
        }
    }

    // Only the following player can chi.
    const int nextSeat = seatAdd(claim.fromSeat, 1);
    const auto& choice = claim.choices[static_cast<size_t>(nextSeat)];
    if (choice && choice->type == ActionType::Chi) {
        call(nextSeat, *choice, claim.fromSeat);
        beginTurn();
        return true;
    }
    return false;
}

std::optional<RoundResult> RoundController::rons(const Claim& claim) const {
    // In turn order from the discarder, so without multiple ron the first one wins.
    std::vector<int> winners;
    for (int offset = 1; offset < 4; ++offset) {
        const int seat = seatAdd(claim.fromSeat, offset);
        const auto& choice = claim.choices[static_cast<size_t>(seat)];
        if (choice && choice->type == ActionType::Ron) winners.push_back(seat);
    }
    if (winners.empty()) return std::nullopt;
    if (winners.size() == 3 && rules().tripleRonAborts) return aborted(AbortReason::Sanchahou);
    if (!rules().multipleRon) winners.resize(1);

    RoundResult result{RoundEnd::Ron, {}, {}};
    for (int seat : winners) {
        const WinContext ctx = ronContext(state_, seat, claim.tile, claim.source);
        result.wins.push_back({seat, claim.fromSeat, *scorer_.score(ctx)});
    }
    return result;
}

void RoundController::call(int seat, const Action& action, int fromSeat) {
    Meld meld{meldTypeOf(action.type), action.handTiles, action.tile, fromSeat};
    state_.player(seat).hand.addMeld(meld);
    state_.player(fromSeat).lastRiverTileCalled();
    state_.current = seat;

    GameEvent event{eventTypeOf(meld.type), seat};
    event.tile = action.tile;
    event.meld = meld;
    record(std::move(event));

    if (meld.type == MeldType::OpenKan) {
        afterKan();
        return;
    }
    // Chi / pon: the caller's turn starts without a draw, with kuikae applied.
    state_.anyCall = true;
    breakIppatsu();
    state_.tileDraw = false;
    state_.rinshan = false;
    state_.lastCall = meld;
}

// The current player made a kan: their next turn starts with a draw from the dead wall.
void RoundController::afterKan() {
    state_.anyCall = true;
    breakIppatsu();
    state_.wall.revealNextDora();
    state_.tileDraw = true;
    state_.rinshan = true;
    state_.lastCall.reset();
    record({EventType::DoraRevealed, state_.current});
}

void RoundController::breakIppatsu() {
    for (PlayerState& player : state_.players) player.ippatsu = false;
}

void RoundController::finishTsumo() {
    const int seat = state_.current;
    const WinContext ctx = tsumoContext(state_);

    GameEvent event{EventType::Tsumo, seat};
    event.tile = ctx.winTile;
    event.wins.push_back({seat, seat, *scorer_.score(ctx)});
    event.revealedHands[static_cast<size_t>(seat)] = state_.player(seat).hand;
    event.uraDoraIndicators = ctx.uraDoraIndicators;

    RoundResult result{RoundEnd::Tsumo, event.wins, {}};
    finish(std::move(result), std::move(event));
}

void RoundController::finishRon(RoundResult result, const Claim& claim) {
    GameEvent event{EventType::Ron, claim.fromSeat};
    event.tile = claim.tile;
    event.wins = result.wins;
    for (const Win& win : result.wins) {
        const PlayerState& winner = state_.player(win.seat);
        event.revealedHands[static_cast<size_t>(win.seat)] = winner.hand;
        if (winner.riichi) event.uraDoraIndicators = state_.wall.uraDoraIndicators();
    }
    finish(std::move(result), std::move(event));
}

void RoundController::finishAbort(AbortReason reason, int seat) {
    GameEvent event{EventType::Abort, seat};
    event.abortReason = reason;
    if (reason == AbortReason::KyuushuKyuuhai) {
        event.revealedHands[static_cast<size_t>(seat)] = state_.player(seat).hand;
    }
    finish(aborted(reason), std::move(event));
}

void RoundController::finishExhaustiveDraw() {
    RoundResult result{RoundEnd::ExhaustiveDraw, {}, {}};
    GameEvent event{EventType::ExhaustiveDraw};
    for (int seat = 0; seat < 4; ++seat) {
        const Hand& hand = state_.player(seat).hand;
        if (!isTenpai(hand)) continue;
        result.tenpai[static_cast<size_t>(seat)] = true;
        event.revealedHands[static_cast<size_t>(seat)] = hand;
    }
    finish(std::move(result), std::move(event));
}

void RoundController::finish(RoundResult result, GameEvent event) {
    result_ = std::move(result);
    record(std::move(event));
}

void RoundController::record(GameEvent event) {
    if (!viewerSeat_) return;
    // Nobody sees what another player draws.
    if (event.type == EventType::Draw && event.seat != *viewerSeat_) event.tile.reset();
    steps_.push_back({std::move(event), PlayerView(state_, *viewerSeat_)});
}

}  // namespace mj
