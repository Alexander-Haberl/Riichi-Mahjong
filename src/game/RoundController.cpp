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

RoundController::RoundController(RoundState state, std::array<IPlayer*, 4> players, const Scorer& scorer)
    : state_(std::move(state)), players_(players), scorer_(scorer) {}

RoundResult RoundController::play() {
    deal();
    state_.current = state_.dealer;
    state_.tileDraw = true;  // the dealer's first turn starts with a draw
    state_.rinshan = false;

    // One iteration per action on someone's own turn. draw() takes the tile the
    // turn starts with (none after a chi / pon). The action then either ends the
    // round, or sets current / tileDraw / rinshan for the next iteration.
    while (true) {
        draw();
        Action action = ask(state_.current, legalTurnActions(state_, rules(), scorer_));

        std::optional<RoundResult> result;
        switch (action.type) {
            case ActionType::Tsumo:
                result = tsumo();
                break;
            case ActionType::KyuushuKyuuhai:
                result = aborted(AbortReason::KyuushuKyuuhai);
                break;
            case ActionType::ClosedKan:
                closedKan(action);
                break;
            case ActionType::KanFromPon:
                result = kanFromPon(action);  // may be robbed (chankan)
                break;
            case ActionType::Discard:
            case ActionType::Riichi:
                result = discard(action);  // reactions, calls, next turn or exhaustive draw
                break;
            default:
                // ask() only returns actions from the legal list, which has no reactions.
                throw std::logic_error("reaction chosen on the player's own turn");
        }
        if (result) return *result;
    }
}

void RoundController::deal() {
    for (int i = 0; i < 4; ++i) {
        PlayerState& player = state_.player((state_.dealer + i) % 4);
        for (int t = 0; t < 13; ++t) player.hand.add(state_.wall.draw());
    }
}

void RoundController::draw() {
    if (!state_.tileDraw) return;  // after a chi / pon the turn starts with the discard
    Tile tile = state_.rinshan ? state_.wall.drawRinshan() : state_.wall.draw();
    state_.currentPlayer().hand.add(tile);
    state_.lastCall.reset();
}

Action RoundController::ask(int seat, const std::vector<Action>& legal) {
    Action choice = players_[static_cast<size_t>(seat)]->chooseAction(PlayerView(state_, seat), legal);
    if (std::find(legal.begin(), legal.end(), choice) == legal.end()) {
        throw std::logic_error("player " + std::to_string(seat) + " chose an illegal action");
    }
    return choice;
}

RoundResult RoundController::tsumo() const {
    const int seat = state_.current;
    return RoundResult{RoundEnd::Tsumo, {{seat, seat, *scorer_.score(tsumoContext(state_))}}, {}};
}

void RoundController::closedKan(const Action& action) {
    state_.currentPlayer().hand.addMeld({MeldType::ClosedKan, action.handTiles, std::nullopt, state_.current});
    afterKan();
}

std::optional<RoundResult> RoundController::kanFromPon(const Action& action) {
    // The other players may rob the added tile (chankan).
    Choices reactions = askReactions(state_.current, action.tile, ClaimSource::KanFromPon);
    std::optional<RoundResult> roned = rons(reactions, state_.current, action.tile, ClaimSource::KanFromPon);
    if (roned) return roned;

    state_.currentPlayer().hand.addKanToPon(action.tile);
    afterKan();
    return std::nullopt;
}

RoundController::Choices RoundController::askReactions(int fromSeat, Tile tile, ClaimSource source){
    Choices reactions;
    for (int i = 0; i < 4; i++){
        std::vector<Action> actions = legalReactions(state_, i, tile, fromSeat, source, rules(), scorer_);
        if (actions.empty()){
            continue;
        }
        reactions[i] = ask(i, actions);
        bool couldRon = std::any_of(actions.begin(), actions.end(),
                                    [](const Action& a) { return a.type == ActionType::Ron; });
        if (reactions[i]->type != ActionType::Ron && couldRon){
            state_.player(i).passedOnRon();
        }
    }
    return reactions;
}

std::optional<RoundResult> RoundController::discard(const Action& action){
    PlayerState& player = state_.currentPlayer();
    const bool declaringRiichi = action.type == ActionType::Riichi;
    // TODO add tsumogiri when added to Action: with an identical tile elsewhere
    // in the hand, this cannot tell which of the two was discarded.
    const bool tsumogiri = state_.tileDraw && action.tile == player.tileDrawn();

    if (declaringRiichi){
        player.declareRiichi(state_.anyCall);
    } else {
        player.ippatsu = false;
    }
    player.discard(action.tile, tsumogiri, declaringRiichi);
    player.temporaryFuriten = false;


    Choices reactions = askReactions(state_.current, action.tile, ClaimSource::Discard);

    std::optional<RoundResult> roned = rons(reactions, state_.current, action.tile, ClaimSource::Discard);
    if (roned) return roned;

    // The riichi stands once its discard was not ronned.
    if (declaringRiichi){
        player.score -= 1000;
        ++state_.riichiSticks;
        if (rules().suuchaRiichi && allInRiichi(state_)) return aborted(AbortReason::SuuchaRiichi);
    }

    // The discard after the fourth kan was not ronned.
    if (rules().suukaikan && fourKansByDifferentPlayers(state_)) return aborted(AbortReason::Suukaikan);

    // check for Pon or Kan
    for (int i = 0; i < 4; i++){
        std::optional<Action> a = reactions[i];
        if (a && (a.value().type == ActionType::Pon || a.value().type == ActionType::OpenKan)){
            call(i, a.value(), state_.current);
            return std::nullopt;
        }
    }

    // check for Chi (could compute the next player and only check that one)
    for (int i = 0; i < 4; i++){
        std::optional<Action> a = reactions[i];
        if (a && (a.value().type == ActionType::Chi)){
            call(i, a.value(), state_.current);
            return std::nullopt;
        }
    }

    if (rules().suufonRenda && fourSameWinds(state_)) return aborted(AbortReason::SuufonRenda);

    if (state_.wall.remaining() == 0){
        // check tenpai and return result
        std::array<bool, 4> tenpai;
        for (int i = 0; i < 4; i++){
            tenpai[i] = isTenpai(state_.player(i).hand);
        }
        return RoundResult{RoundEnd::ExhaustiveDraw, {}, tenpai};
    }

    // setup next turn
    state_.current = (state_.current + 1) % 4;
    state_.tileDraw = true;
    state_.rinshan = false;
    state_.lastCall = std::nullopt;

    return std::nullopt;

}

std::optional<RoundResult> RoundController::rons(const Choices& choices, int fromSeat, Tile tile,
                                                 ClaimSource source) const {
    // In turn order from the discarder, so without multiple ron the first one wins.
    std::vector<int> winners;
    for (int offset = 1; offset < 4; ++offset) {
        const int seat = (fromSeat + offset) % 4;
        const auto& choice = choices[static_cast<size_t>(seat)];
        if (choice && choice->type == ActionType::Ron) winners.push_back(seat);
    }
    if (winners.empty()) return std::nullopt;
    if (winners.size() == 3 && rules().tripleRonAborts) return aborted(AbortReason::Sanchahou);
    if (!rules().multipleRon) winners.resize(1);

    RoundResult result{RoundEnd::Ron, {}, {}};
    for (int seat : winners) {
        result.wins.push_back({seat, fromSeat, *scorer_.score(ronContext(state_, seat, tile, source))});
    }
    return result;
}

void RoundController::call(int seat, const Action& action, int fromSeat) {
    MeldType type = action.type == ActionType::Chi  ? MeldType::Chi
                    : action.type == ActionType::Pon ? MeldType::Pon
                                                     : MeldType::OpenKan;
    Meld meld{type, action.handTiles, action.tile, fromSeat};
    state_.player(seat).hand.addMeld(meld);
    state_.player(fromSeat).lastRiverTileCalled();
    state_.current = seat;

    if (type == MeldType::OpenKan) {
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
}

void RoundController::breakIppatsu() {
    for (PlayerState& player : state_.players) player.ippatsu = false;
}

}  // namespace mj
