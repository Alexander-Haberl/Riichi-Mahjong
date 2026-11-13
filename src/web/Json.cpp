#include "web/Json.hpp"

#include <optional>
#include <string>

using nlohmann::json;

// These to_json functions are found by nlohmann::json through the type's
// namespace, which also makes it convert vectors and arrays of these types.
namespace mj {

NLOHMANN_JSON_SERIALIZE_ENUM(Honor, {
    {Honor::East, "East"},
    {Honor::South, "South"},
    {Honor::West, "West"},
    {Honor::North, "North"},
    {Honor::White, "White"},
    {Honor::Green, "Green"},
    {Honor::Red, "Red"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(MeldType, {
    {MeldType::Chi, "Chi"},
    {MeldType::Pon, "Pon"},
    {MeldType::OpenKan, "OpenKan"},
    {MeldType::ClosedKan, "ClosedKan"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ActionType, {
    {ActionType::Discard, "Discard"},
    {ActionType::Riichi, "Riichi"},
    {ActionType::Tsumo, "Tsumo"},
    {ActionType::ClosedKan, "ClosedKan"},
    {ActionType::KanFromPon, "KanFromPon"},
    {ActionType::KyuushuKyuuhai, "KyuushuKyuuhai"},
    {ActionType::Ron, "Ron"},
    {ActionType::Chi, "Chi"},
    {ActionType::Pon, "Pon"},
    {ActionType::OpenKan, "OpenKan"},
    {ActionType::Pass, "Pass"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
    {EventType::RoundStart, "RoundStart"},
    {EventType::Draw, "Draw"},
    {EventType::Discard, "Discard"},
    {EventType::Chi, "Chi"},
    {EventType::Pon, "Pon"},
    {EventType::Kan, "Kan"},
    {EventType::DoraRevealed, "DoraRevealed"},
    {EventType::Tsumo, "Tsumo"},
    {EventType::Ron, "Ron"},
    {EventType::Abort, "Abort"},
    {EventType::ExhaustiveDraw, "ExhaustiveDraw"},
    {EventType::RoundEnd, "RoundEnd"},
    {EventType::GameEnd, "GameEnd"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(AbortReason, {
    {AbortReason::KyuushuKyuuhai, "KyuushuKyuuhai"},
    {AbortReason::SuufonRenda, "SuufonRenda"},
    {AbortReason::SuuchaRiichi, "SuuchaRiichi"},
    {AbortReason::Suukaikan, "Suukaikan"},
    {AbortReason::Sanchahou, "Sanchahou"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ScoreName, {
    {ScoreName::None, "None"},
    {ScoreName::Mangan, "Mangan"},
    {ScoreName::Haneman, "Haneman"},
    {ScoreName::Baiman, "Baiman"},
    {ScoreName::Sanbaiman, "Sanbaiman"},
    {ScoreName::KazoeYakuman, "KazoeYakuman"},
    {ScoreName::Yakuman, "Yakuman"},
})

void to_json(json& out, const Tile& tile) {
    out = tile.toString();
}

void to_json(json& out, const RiverTile& discard) {
    out = {{"tile", discard.tile},
           {"tsumogiri", discard.tsumogiri},
           {"riichi", discard.riichi},
           {"calledAway", discard.calledAway}};
}

template <typename T>
static json orNull(const std::optional<T>& value) {
    return value ? json(*value) : json(nullptr);
}

void to_json(json& out, const Meld& meld) {
    out = {{"type", meld.type},
           {"handTiles", meld.handTiles},
           {"calledTile", orNull(meld.calledTile)},
           {"fromSeat", meld.fromSeat},
           {"kanFromPon", meld.kanFromPon}};
}

void to_json(json& out, const Hand& hand) {
    out = {{"concealed", hand.concealed()}, {"melds", hand.melds()}};
}

void to_json(json& out, const HanSource& source) {
    out = {{"name", std::string(source.name)}, {"han", source.han}};
}

void to_json(json& out, const Win& win) {
    out = {{"seat", win.seat},
           {"fromSeat", win.fromSeat},
           {"han", win.score.han},
           {"fu", win.score.fu},
           {"basicPoints", win.score.basicPoints},
           {"name", win.score.name},
           {"yaku", win.score.hanSources},
           {"points", win.points},
           {"concealedBlocks", win.score.decomposition.concealedBlocks},
           {"melds", win.score.decomposition.melds},
           {"winningTile", win.score.decomposition.winningTile}};
}

void to_json(json& out, const Action& action) {
    // These two have no tile; the field only holds its default.
    const bool hasTile = action.type != ActionType::Pass && action.type != ActionType::KyuushuKyuuhai;
    out = {{"type", action.type},
           {"tile", hasTile ? json(action.tile) : json(nullptr)},
           {"handTiles", action.handTiles},
           {"tsumogiri", action.tsumogiri}};
}

void to_json(json& out, const GameEvent& event) {
    json revealedHands = json::array();
    for (const std::optional<Hand>& hand : event.revealedHands) revealedHands.push_back(orNull(hand));

    out = {{"type", event.type},
           {"seat", event.seat},
           {"tile", orNull(event.tile)},
           {"riichiDiscard", event.riichiDiscard},
           {"meld", orNull(event.meld)},
           {"wins", event.wins},
           {"revealedHands", std::move(revealedHands)},
           {"uraDoraIndicators", event.uraDoraIndicators},
           {"abortReason", orNull(event.abortReason)},
           {"scoreChanges", event.scoreChanges}};
}

void to_json(json& out, const PlayerView& view) {
    json seats = json::array();
    for (int who = 0; who < 4; ++who) {
        seats.push_back({{"river", view.river(who)},
                         {"melds", view.melds(who)},
                         {"concealedCount", view.concealedCount(who)},
                         {"score", view.score(who)},
                         {"riichi", view.riichi(who)},
                         {"seatWind", view.seatWind(who)}});
    }

    out = {{"seat", view.seat()},
           {"hand", view.hand().concealed()},
           {"furiten", view.furiten()},
           {"roundWind", view.roundWind()},
           {"dealer", view.dealer()},
           {"honba", view.honba()},
           {"riichiSticks", view.riichiSticks()},
           {"currentSeat", view.currentSeat()},
           {"wallRemaining", view.wallRemaining()},
           {"doraIndicators", view.doraIndicators()},
           {"seats", std::move(seats)}};
}

json toJson(const GameStep& step) {
    return {{"event", step.event}, {"view", step.view}};
}

json toJson(const Decision& decision) {
    return {{"seat", decision.seat}, {"legal", decision.legal}};
}

json toJson(const GameResult& result) {
    return {{"scores", result.scores}, {"ranks", result.ranks}, {"riichiSticksLeft", result.riichiSticksLeft}};
}

}  // namespace mj
