#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "TestHelpers.hpp"
#include "game/LegalActions.hpp"

using namespace mj;
using namespace mj::test;

namespace {

RoundState emptyState() {
    return RoundState(Wall(Wall::fullSet()), Honor::East, 0, {25000, 25000, 25000, 25000});
}

// Seat 0 to move, having just drawn the last tile of `hand`.
RoundState afterDraw(const char* hand) {
    RoundState state = emptyState();
    state.players[0].hand = Hand(parseTiles(hand));
    state.current = 0;
    state.tileDraw = true;
    return state;
}

// `seat` holds `hand`; another seat is about to give away a tile.
RoundState withHand(int seat, const char* hand) {
    RoundState state = emptyState();
    state.players[static_cast<size_t>(seat)].hand = Hand(parseTiles(hand));
    return state;
}

std::vector<Action> ofType(const std::vector<Action>& actions, ActionType type) {
    std::vector<Action> result;
    std::copy_if(actions.begin(), actions.end(), std::back_inserter(result),
                 [type](const Action& a) { return a.type == type; });
    return result;
}

std::vector<Tile> tilesOf(const std::vector<Action>& actions) {
    std::vector<Tile> result;
    for (const Action& a : actions) result.push_back(a.tile);
    return result;
}

void drawUntil(Wall& wall, int remaining) {
    while (wall.remaining() > remaining) wall.draw();
}

}  // namespace

// --- Own turn ---------------------------------------------------------------

TEST_CASE("Each distinct tile can be discarded once") {
    RoundState state = afterDraw("112m05p");
    auto discards = ofType(legalTurnActions(state, testRules(), noYaku()), ActionType::Discard);
    CHECK(tilesOf(discards) == parseTiles("12m05p"));  // red and normal five separately
}

TEST_CASE("The drawn tile is a tsumogiri discard, an identical copy in the hand a second one") {
    auto discardsOf = [](const RoundState& state, Tile t) {
        std::vector<Action> result;
        for (const Action& a : ofType(legalTurnActions(state, testRules(), noYaku()), ActionType::Discard)) {
            if (a.tile == t) result.push_back(a);
        }
        return result;
    };

    SECTION("no copy: one action") {
        RoundState state = afterDraw("123m456p789s11z2z3z");
        const auto actions = discardsOf(state, tile("3z"));
        REQUIRE(actions.size() == 1);
        CHECK(actions[0].tsumogiri);
        CHECK_FALSE(discardsOf(state, tile("2z"))[0].tsumogiri);
    }
    SECTION("a copy in the hand: the tsumogiri discard first, then the copy") {
        RoundState state = afterDraw("123m456p789s11z2z2z");
        const auto actions = discardsOf(state, tile("2z"));
        REQUIRE(actions.size() == 2);
        CHECK(actions[0].tsumogiri);
        CHECK_FALSE(actions[1].tsumogiri);
    }
    SECTION("a red five and a normal five are not copies") {
        RoundState state = afterDraw("123m456p789s11z05m");
        CHECK(discardsOf(state, tile("5m")).size() == 1);
        CHECK(discardsOf(state, tile("0m")).size() == 1);
        CHECK_FALSE(discardsOf(state, tile("0m"))[0].tsumogiri);
    }
    SECTION("after a call nothing is tsumogiri") {
        RoundState state = afterDraw("123m456p789s11z2z2z");
        state.tileDraw = false;
        const auto actions = discardsOf(state, tile("2z"));
        REQUIRE(actions.size() == 1);
        CHECK_FALSE(actions[0].tsumogiri);
    }
}

TEST_CASE("Riichi discards are tsumogiri the same way") {
    RoundState state = afterDraw("123m456p789s111z22z");  // tenpai after either 2z, among others
    std::vector<Action> riichi;
    for (const Action& a : ofType(legalTurnActions(state, testRules(), noYaku()), ActionType::Riichi)) {
        if (a.tile == tile("2z")) riichi.push_back(a);
    }
    REQUIRE(riichi.size() == 2);
    CHECK(riichi[0].tile == tile("2z"));
    CHECK(riichi[0].tsumogiri);
    CHECK_FALSE(riichi[1].tsumogiri);
}

TEST_CASE("Riichi is offered for discards that leave the hand tenpai") {
    RoundState state = afterDraw("123m456p789s111z2z3z");
    RuleSet rules = testRules();
    rules.riichiNeeds1000Points = true;
    auto riichi = [&] { return tilesOf(ofType(legalTurnActions(state, rules, noYaku()), ActionType::Riichi)); };

    CHECK(riichi() == parseTiles("23z"));

    SECTION("not with fewer than 1000 points, unless the rules allow it") {
        state.players[0].score = 900;
        CHECK(riichi().empty());
        rules.riichiNeeds1000Points = false;
        CHECK(riichi() == parseTiles("23z"));
    }
    SECTION("not with an open hand") {
        state.players[0].hand.addMeld({MeldType::Pon, parseTiles("11z"), tile("1z"), 2});
        CHECK(riichi().empty());
    }
    SECTION("not with fewer than four tiles left") {
        drawUntil(state.wall, 3);
        CHECK(riichi().empty());
    }
    SECTION("not when already in riichi") {
        state.players[0].riichi = true;
        CHECK(riichi().empty());
    }
}

TEST_CASE("In riichi only the drawn tile can be discarded") {
    RoundState state = afterDraw("123m456p789s111z2z5m");
    state.players[0].riichi = true;
    auto discards = ofType(legalTurnActions(state, testRules(), noYaku()), ActionType::Discard);
    CHECK(tilesOf(discards) == parseTiles("5m"));
    CHECK(discards[0].tsumogiri);
}

TEST_CASE("Tsumo needs a complete hand and a yaku") {
    RoundState complete = afterDraw("123m456p789s11z222z");
    auto tsumo = ofType(legalTurnActions(complete, testRules(), alwaysYaku()), ActionType::Tsumo);
    REQUIRE(tsumo.size() == 1);
    CHECK(tsumo[0].tile == tile("2z"));
    CHECK(ofType(legalTurnActions(complete, testRules(), noYaku()), ActionType::Tsumo).empty());

    RoundState incomplete = afterDraw("123m456p789s11z223z");
    CHECK(ofType(legalTurnActions(incomplete, testRules(), alwaysYaku()), ActionType::Tsumo).empty());
}

TEST_CASE("Closed kan of four concealed tiles") {
    RoundState state = afterDraw("1111m456p789s11z23m");
    auto kans = [&] { return ofType(legalTurnActions(state, testRules(), noYaku()), ActionType::ClosedKan); };

    REQUIRE(kans().size() == 1);
    CHECK(kans()[0].tile == tile("1m"));
    CHECK(kans()[0].handTiles == parseTiles("1111m"));

    SECTION("not after four kans") {
        for (int i = 0; i < 4; ++i) state.wall.drawRinshan();
        CHECK(kans().empty());
    }
    SECTION("not on the last tile") {
        drawUntil(state.wall, 0);
        CHECK(kans().empty());
    }
}

TEST_CASE("Closed kan in riichi must keep the waits") {
    // Waits on 1m, 4m and 1z; after a kan of 1m only 1m and 4m are left.
    RoundState changes = afterDraw("111m23m456p789s11z1m");
    changes.players[0].riichi = true;
    CHECK(ofType(legalTurnActions(changes, testRules(), noYaku()), ActionType::ClosedKan).empty());

    // 222m can only be a triplet, so the kan keeps the 3s / 6s wait.
    RoundState keeps = afterDraw("222m456p789s45s11z2m");
    keeps.players[0].riichi = true;
    CHECK(ofType(legalTurnActions(keeps, testRules(), noYaku()), ActionType::ClosedKan).size() == 1);

    // Same hand, but the fourth 2m was not the drawn tile.
    RoundState notDrawn = afterDraw("2222m456p789s45s1z1z");
    notDrawn.players[0].riichi = true;
    CHECK(ofType(legalTurnActions(notDrawn, testRules(), noYaku()), ActionType::ClosedKan).empty());
}

TEST_CASE("Kan from pon adds the fourth tile") {
    RoundState state = afterDraw("55s123m456p789s0s");
    state.players[0].hand.addMeld({MeldType::Pon, parseTiles("55s"), tile("5s"), 2});

    auto kans = ofType(legalTurnActions(state, testRules(), noYaku()), ActionType::KanFromPon);
    REQUIRE(kans.size() == 1);
    CHECK(kans[0].tile == tile("0s"));
}

TEST_CASE("After a call only discards, without the kuikae tiles") {
    RoundState state = afterDraw("45m36m7p");
    Meld chi{MeldType::Chi, parseTiles("45m"), tile("3m"), 3};
    state.players[0].hand.addMeld(chi);
    state.tileDraw = false;
    state.lastCall = chi;

    RuleSet rules = testRules();
    rules.allowKuikae = false;
    auto actions = legalTurnActions(state, rules, alwaysYaku());
    CHECK(actions.size() == ofType(actions, ActionType::Discard).size());  // nothing but discards
    CHECK(tilesOf(actions) == parseTiles("7p"));                           // 3m called, 6m the other end

    rules.allowKuikae = true;
    CHECK(tilesOf(legalTurnActions(state, rules, noYaku())) == parseTiles("3m6m7p"));
}

TEST_CASE("Kyuushu kyuuhai on the first turn") {
    RoundState state = afterDraw("19m19p19s1234z234m5m");
    RuleSet rules = testRules();
    rules.kyuushuKyuuhai = true;
    auto offered = [&] {
        return !ofType(legalTurnActions(state, rules, noYaku()), ActionType::KyuushuKyuuhai).empty();
    };

    CHECK(offered());

    SECTION("not after the player's first discard") {
        state.players[0].river.push_back({tile("9m")});
        CHECK_FALSE(offered());
    }
    SECTION("not after a call") {
        state.anyCall = true;
        CHECK_FALSE(offered());
    }
    SECTION("not when the rules turn it off") {
        rules.kyuushuKyuuhai = false;
        CHECK_FALSE(offered());
    }
    SECTION("not with only eight kinds") {
        state.players[0].hand = Hand(parseTiles("19m19p19s12z2345m5m"));
        CHECK_FALSE(offered());
    }
}

// --- Reactions --------------------------------------------------------------

TEST_CASE("No reaction to the own discard") {
    RoundState state = withHand(1, "123m456p789s11z22z");
    CHECK(legalReactions(state, 1, tile("1z"), 1, ClaimSource::Discard, testRules(), alwaysYaku()).empty());
}

TEST_CASE("No reactions without a legal call or ron") {
    RoundState state = withHand(3, "19m19p19s1234567z");  // kokushi, not waiting on 5p
    CHECK(legalReactions(state, 3, tile("5p"), 2, ClaimSource::Discard, testRules(), alwaysYaku()).empty());
}

TEST_CASE("Ron unless furiten") {
    RoundState state = withHand(1, "123m456p789s11z22z");  // waits on 1z and 2z
    auto canRon = [&](const Scorer& scorer) {
        auto reactions = legalReactions(state, 1, tile("1z"), 0, ClaimSource::Discard, testRules(), scorer);
        return !ofType(reactions, ActionType::Ron).empty();
    };

    CHECK(canRon(alwaysYaku()));
    CHECK_FALSE(canRon(noYaku()));

    SECTION("a wait in the own river") {
        state.players[1].river.push_back({tile("2z")});
        CHECK_FALSE(canRon(alwaysYaku()));
    }
    SECTION("passed on a ron since the own last discard") {
        state.players[1].temporaryFuriten = true;
        CHECK_FALSE(canRon(alwaysYaku()));
    }
    SECTION("passed on a ron after riichi") {
        state.players[1].riichiFuriten = true;
        CHECK_FALSE(canRon(alwaysYaku()));
    }
}

TEST_CASE("Pon and open kan from any seat, chi only from the left") {
    RoundState state = withHand(1, "333m45m19p19s1234z");
    auto reactions = [&](int fromSeat) {
        return legalReactions(state, 1, tile("3m"), fromSeat, ClaimSource::Discard, testRules(), noYaku());
    };

    for (int fromSeat : {0, 2, 3}) {
        auto r = reactions(fromSeat);
        CHECK(ofType(r, ActionType::Pon).size() == 1);
        CHECK(ofType(r, ActionType::OpenKan).size() == 1);
        CHECK(ofType(r, ActionType::Pass).size() == 1);
        CHECK(ofType(r, ActionType::Chi).size() == (fromSeat == 0 ? 1 : 0));
    }
}

TEST_CASE("Every chi shape is offered") {
    RoundState state = withHand(1, "3467m19p19s1234z");
    auto chis = ofType(legalReactions(state, 1, tile("5m"), 0, ClaimSource::Discard, testRules(), noYaku()),
                       ActionType::Chi);
    REQUIRE(chis.size() == 3);
    CHECK(chis[0].handTiles == parseTiles("34m"));
    CHECK(chis[1].handTiles == parseTiles("46m"));
    CHECK(chis[2].handTiles == parseTiles("67m"));
}

TEST_CASE("Chi with or without the red five") {
    RoundState state = withHand(1, "3056m19p19s1234z");
    auto chis = ofType(legalReactions(state, 1, tile("4m"), 0, ClaimSource::Discard, testRules(), noYaku()),
                       ActionType::Chi);
    REQUIRE(chis.size() == 4);  // 3-5 and 5-6, each with the red or the normal five
    CHECK(chis[0].handTiles == parseTiles("30m"));
    CHECK(chis[1].handTiles == parseTiles("35m"));
    CHECK(chis[2].handTiles == parseTiles("06m"));
    CHECK(chis[3].handTiles == parseTiles("56m"));
}

TEST_CASE("Pon and open kan choices with a red five") {
    RoundState state = withHand(2, "055p19s1234z");
    auto reactions = legalReactions(state, 2, tile("5p"), 0, ClaimSource::Discard, testRules(), noYaku());

    auto pons = ofType(reactions, ActionType::Pon);
    REQUIRE(pons.size() == 2);
    CHECK(pons[0].handTiles == parseTiles("05p"));
    CHECK(pons[1].handTiles == parseTiles("55p"));

    auto kans = ofType(reactions, ActionType::OpenKan);
    REQUIRE(kans.size() == 1);
    CHECK(kans[0].handTiles == parseTiles("055p"));
}

TEST_CASE("A red five alone is not a pon") {
    RoundState state = withHand(2, "05p19s1234z");
    auto pons = ofType(legalReactions(state, 2, tile("5p"), 0, ClaimSource::Discard, testRules(), noYaku()),
                       ActionType::Pon);
    REQUIRE(pons.size() == 1);
    CHECK(pons[0].handTiles == parseTiles("05p"));

    RoundState redOnly = withHand(2, "0p19s1234z");
    CHECK(legalReactions(redOnly, 2, tile("5p"), 0, ClaimSource::Discard, testRules(), noYaku()).empty());
}

TEST_CASE("No calls in riichi, on the last discard, or after four kans") {
    RoundState state = withHand(2, "555p19s1234z");
    auto reactions = [&] {
        return legalReactions(state, 2, tile("5p"), 1, ClaimSource::Discard, testRules(), noYaku());
    };
    CHECK_FALSE(reactions().empty());

    SECTION("riichi") {
        state.players[2].riichi = true;
        CHECK(reactions().empty());
    }
    SECTION("last discard") {
        drawUntil(state.wall, 0);
        CHECK(reactions().empty());
    }
    SECTION("four kans: pon still allowed, open kan not") {
        for (int i = 0; i < 4; ++i) state.wall.drawRinshan();
        CHECK(ofType(reactions(), ActionType::Pon).size() == 1);
        CHECK(ofType(reactions(), ActionType::OpenKan).empty());
    }
}

TEST_CASE("Only ron on a kan from pon") {
    RoundState state = withHand(1, "123m456p789s11z22z");
    auto reactions = legalReactions(state, 1, tile("1z"), 0, ClaimSource::KanFromPon, testRules(), alwaysYaku());
    REQUIRE(reactions.size() == 2);
    CHECK(reactions[0].type == ActionType::Ron);
    CHECK(reactions[1].type == ActionType::Pass);
}

TEST_CASE("No call that would leave only kuikae discards") {
    RoundState state = withHand(1, "456m");
    RuleSet rules = testRules();
    rules.allowKuikae = false;
    auto chis = [&] {
        return ofType(legalReactions(state, 1, tile("3m"), 0, ClaimSource::Discard, rules, noYaku()),
                      ActionType::Chi);
    };
    CHECK(chis().empty());  // only 6m would be left, the other end of 3-4-5

    rules.allowKuikae = true;
    CHECK(chis().size() == 1);
}

// --- Helpers ----------------------------------------------------------------

TEST_CASE("Kuikae forbidden kinds") {
    CHECK(kuikaeForbidden({MeldType::Pon, parseTiles("11z"), tile("1z"), 1}) == parseTiles("1z"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("45m"), tile("3m"), 3}) == parseTiles("36m"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("45m"), tile("6m"), 3}) == parseTiles("63m"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("46m"), tile("5m"), 3}) == parseTiles("5m"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("23m"), tile("1m"), 3}) == parseTiles("14m"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("89m"), tile("7m"), 3}) == parseTiles("7m"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("78m"), tile("9m"), 3}) == parseTiles("96m"));
    CHECK(kuikaeForbidden({MeldType::Chi, parseTiles("46m"), tile("0m"), 3}) == parseTiles("5m"));  // never red
}

TEST_CASE("Win contexts") {
    RoundState state = afterDraw("123m456p789s11z222z");
    state.players[0].riichi = true;
    state.rinshan = true;

    WinContext tsumo = tsumoContext(state);
    CHECK(tsumo.tsumo);
    CHECK(tsumo.winTile == tile("2z"));
    CHECK(tsumo.riichi);
    CHECK(tsumo.rinshan);
    CHECK_FALSE(tsumo.haitei);
    CHECK(tsumo.uraDoraIndicators.size() == 1);

    WinContext ron = ronContext(state, 1, tile("1z"), ClaimSource::KanFromPon);
    CHECK_FALSE(ron.tsumo);
    CHECK(ron.chankan);
    CHECK(ron.uraDoraIndicators.empty());  // seat 1 is not in riichi
    CHECK(ron.seatWind == Honor::South);

    drawUntil(state.wall, 0);
    CHECK(ronContext(state, 1, tile("1z"), ClaimSource::Discard).houtei);
    state.rinshan = false;
    CHECK(tsumoContext(state).haitei);
}
