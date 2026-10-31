#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>

#include "TestHelpers.hpp"
#include "web/Json.hpp"
#include "web/WebGame.hpp"

using namespace mj;
using namespace mj::test;
using nlohmann::json;

namespace {

RoundState emptyRound() {
    return RoundState(Wall(Wall::fullSet()), Honor::South, 1, {25000, 26000, 27000, 22000}, 2, 1);
}

GameStep stepOf(GameEvent event) {
    return {std::move(event), PlayerView(emptyRound(), 0)};
}

}  // namespace

TEST_CASE("Web JSON: a view shows the table of one seat") {
    RoundState state = emptyRound();
    state.players[0].hand = Hand(parseTiles("13m0p"));
    state.players[2].hand = Hand(parseTiles("99m1z"));
    state.players[2].hand.addMeld({MeldType::Pon, parseTiles("99m"), tile("9m"), 3});
    state.players[2].riichi = true;
    state.players[3].river.push_back({tile("9m"), true, false, true});

    const json view = toJson(GameStep{{EventType::Draw}, PlayerView(state, 0)})["view"];

    CHECK(view["seat"] == 0);
    CHECK(view["hand"] == json{"1m", "3m", "0p"});
    CHECK(view["furiten"] == false);
    CHECK(view["roundWind"] == "South");
    CHECK(view["dealer"] == 1);
    CHECK(view["honba"] == 2);
    CHECK(view["riichiSticks"] == 1);
    REQUIRE(view["seats"].size() == 4);
    CHECK(view["seats"][1]["seatWind"] == "East");
    CHECK(view["seats"][1]["score"] == 26000);

    const json& other = view["seats"][2];
    CHECK(other["concealedCount"] == 1);
    CHECK(other["riichi"] == true);
    CHECK_FALSE(other.contains("hand"));
    CHECK(other["melds"][0] == json{{"type", "Pon"},
                                    {"handTiles", {"9m", "9m"}},
                                    {"calledTile", "9m"},
                                    {"fromSeat", 3},
                                    {"kanFromPon", false}});

    CHECK(view["seats"][3]["river"][0] ==
          json{{"tile", "9m"}, {"tsumogiri", true}, {"riichi", false}, {"calledAway", true}});
}

TEST_CASE("Web JSON: empty optionals of an event are null") {
    const json event = toJson(stepOf({EventType::Draw, 2}))["event"];

    CHECK(event["type"] == "Draw");
    CHECK(event["seat"] == 2);
    CHECK(event["tile"].is_null());
    CHECK(event["meld"].is_null());
    CHECK(event["abortReason"].is_null());
    CHECK(event["revealedHands"] == json{nullptr, nullptr, nullptr, nullptr});
}

TEST_CASE("Web JSON: a closed kan has no called tile") {
    GameEvent kan{EventType::Kan, 1};
    kan.meld = Meld{MeldType::ClosedKan, parseTiles("1111z"), std::nullopt, 1};

    const json meld = toJson(stepOf(std::move(kan)))["event"]["meld"];
    CHECK(meld["type"] == "ClosedKan");
    CHECK(meld["calledTile"].is_null());
}

TEST_CASE("Web JSON: wins and hands at the end of a round") {
    ScoreResult score{Reading{HandForm::Regular, {}}};
    score.hanSources = {{"Riichi", 1}, {"Dora", 2}};
    score.han = 3;
    score.fu = 40;
    score.basicPoints = 1280;
    score.name = ScoreName::None;

    GameEvent ron{EventType::Ron, 3};
    ron.tile = tile("0s");
    ron.wins.push_back({1, 3, score});
    ron.revealedHands[1] = Hand(parseTiles("11z"));
    ron.uraDoraIndicators = parseTiles("4p");

    const json event = toJson(stepOf(std::move(ron)))["event"];

    CHECK(event["tile"] == "0s");
    CHECK(event["wins"][0] == json{{"seat", 1},
                                   {"fromSeat", 3},
                                   {"han", 3},
                                   {"fu", 40},
                                   {"basicPoints", 1280},
                                   {"name", "None"},
                                   {"yaku", {{{"name", "Riichi"}, {"han", 1}}, {{"name", "Dora"}, {"han", 2}}}}});
    CHECK(event["revealedHands"][0].is_null());
    CHECK(event["revealedHands"][1]["concealed"] == json{"1z", "1z"});
    CHECK(event["uraDoraIndicators"] == json{"4p"});
}

TEST_CASE("Web JSON: abort reason and score changes") {
    GameEvent abort{EventType::Abort, 0};
    abort.abortReason = AbortReason::Sanchahou;
    CHECK(toJson(stepOf(std::move(abort)))["event"]["abortReason"] == "Sanchahou");

    GameEvent roundEnd{EventType::RoundEnd};
    roundEnd.scoreChanges = {-1000, 3000, -1000, -1000};
    CHECK(toJson(stepOf(std::move(roundEnd)))["event"]["scoreChanges"] == json{-1000, 3000, -1000, -1000});
}

TEST_CASE("Web JSON: a decision lists the actions, without a tile where there is none") {
    const Decision decision{2,
                            {Action{ActionType::Riichi, tile("0p")},
                             Action{ActionType::Chi, tile("3m"), parseTiles("12m")},
                             Action{ActionType::Pass},
                             Action{ActionType::KyuushuKyuuhai}}};

    const json out = toJson(decision);

    CHECK(out["seat"] == 2);
    CHECK(out["legal"][0] == json{{"type", "Riichi"}, {"tile", "0p"}, {"handTiles", json::array()}});
    CHECK(out["legal"][1] == json{{"type", "Chi"}, {"tile", "3m"}, {"handTiles", {"1m", "2m"}}});
    CHECK(out["legal"][2]["tile"].is_null());
    CHECK(out["legal"][3]["tile"].is_null());
}

TEST_CASE("Web JSON: the result of a game") {
    const GameResult result{{31000, 25000, 24000, 20000}, {1, 2, 3, 4}, 1};

    CHECK(toJson(result) == json{{"scores", {31000, 25000, 24000, 20000}},
                                 {"ranks", {1, 2, 3, 4}},
                                 {"riichiSticksLeft", 1}});
}

TEST_CASE("WebGame: the loop of the UI plays a whole game") {
    WebGame game(12345);
    game.start();

    std::optional<int> viewerSeat;
    json lastStep;
    int decisions = 0;
    while (true) {
        for (const json& step : json::parse(game.takeSteps())) {
            if (!viewerSeat) {
                CHECK(step["event"]["type"] == "RoundStart");
                viewerSeat = step["view"]["seat"].get<int>();
            }
            CHECK(step["view"]["seat"] == *viewerSeat);
            lastStep = step;
        }

        const json decision = json::parse(game.pending());
        if (decision.is_null()) break;
        CHECK(decision["seat"] == *viewerSeat);
        REQUIRE_FALSE(decision["legal"].empty());
        game.submit(0);
        ++decisions;
    }

    CHECK(decisions > 0);
    CHECK(lastStep["event"]["type"] == "GameEnd");

    const json result = json::parse(game.result());
    CHECK(result["scores"].size() == 4);
    CHECK(result["ranks"].size() == 4);
}

TEST_CASE("WebGame: the same seed gives the same game") {
    WebGame first(777);
    WebGame second(777);
    first.start();
    second.start();

    CHECK(first.takeSteps() == second.takeSteps());
    CHECK(first.pending() == second.pending());
}

TEST_CASE("WebGame: submit only takes an index of the pending decision") {
    WebGame game(12345);

    SECTION("before the game has started") {
        CHECK_THROWS_AS(game.submit(0), std::logic_error);
    }
    SECTION("with an index out of range") {
        game.start();
        const int actions = static_cast<int>(json::parse(game.pending())["legal"].size());
        CHECK_THROWS_AS(game.submit(-1), std::logic_error);
        CHECK_THROWS_AS(game.submit(actions), std::logic_error);
    }
}
