#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "rules/Scorer.hpp"

using namespace mj;
using namespace mj::test;

namespace {

const Yaku kFakeYakuman{"Yakuman", [](const Reading&, const WinContext&) { return kYakumanHan; }};
const Yaku kFakeDoubleYakuman{"Double Yakuman", [](const Reading&, const WinContext&) { return 2 * kYakumanHan; }};
const Yaku kTwelveHan{"Twelve", [](const Reading&, const WinContext&) { return 12; }};

// Closed pinfu-shaped ron on 2p (30 fu), with the given dora indicators.
WinContext pinfuRon(const char* doraIndicators = "", const char* hand = "123m456s789s234p55s") {
    WinContext ctx;
    ctx.hand = Hand(parseTiles(hand));
    ctx.winTile = tile("2p");
    ctx.doraIndicators = parseTiles(doraIndicators);
    return ctx;
}

}  // namespace

TEST_CASE("Basic points below mangan are fu * 2^(2 + han)") {
    RuleSet rules = testRules();
    rules.kiriageMangan = false;

    CHECK(basicPoints(1, 30, false, rules).points == 240);
    CHECK(basicPoints(2, 25, false, rules).points == 400);  // chiitoitsu
    CHECK(basicPoints(3, 30, false, rules).points == 960);
    CHECK(basicPoints(4, 30, false, rules).points == 1920);
    CHECK(basicPoints(4, 30, false, rules).name == ScoreName::None);
    CHECK(basicPoints(3, 60, false, rules).points == 1920);
}

TEST_CASE("Mangan caps the basic points at 2000") {
    RuleSet rules = testRules();
    rules.kiriageMangan = false;

    CHECK(basicPoints(3, 70, false, rules).points == 2000);  // 2240
    CHECK(basicPoints(4, 40, false, rules).points == 2000);  // 2560
    CHECK(basicPoints(4, 40, false, rules).name == ScoreName::Mangan);

    rules.kiriageMangan = true;
    CHECK(basicPoints(4, 30, false, rules).points == 2000);
    CHECK(basicPoints(3, 60, false, rules).name == ScoreName::Mangan);
    CHECK(basicPoints(3, 50, false, rules).points == 1600);  // only exactly 1920 is rounded up
}

TEST_CASE("Limits from 5 han on, whatever the fu") {
    RuleSet rules = testRules();
    rules.kazoeYakuman = true;

    struct Limit {
        int han;
        int points;
        ScoreName name;
    };
    for (Limit l : {Limit{5, 2000, ScoreName::Mangan}, Limit{6, 3000, ScoreName::Haneman},
                    Limit{7, 3000, ScoreName::Haneman}, Limit{8, 4000, ScoreName::Baiman},
                    Limit{10, 4000, ScoreName::Baiman}, Limit{11, 6000, ScoreName::Sanbaiman},
                    Limit{12, 6000, ScoreName::Sanbaiman}, Limit{13, 8000, ScoreName::KazoeYakuman}}) {
        INFO(l.han << " han");
        CHECK(basicPoints(l.han, 20, false, rules).points == l.points);
        CHECK(basicPoints(l.han, 20, false, rules).name == l.name);
    }

    rules.kazoeYakuman = false;
    CHECK(basicPoints(13, 30, false, rules).points == 6000);
    CHECK(basicPoints(13, 30, false, rules).name == ScoreName::Sanbaiman);
}

TEST_CASE("Yakuman are 8000 basic points each") {
    CHECK(basicPoints(13, 0, true, testRules()).points == 8000);
    CHECK(basicPoints(26, 0, true, testRules()).points == 16000);
    CHECK(basicPoints(39, 0, true, testRules()).points == 24000);
    CHECK(basicPoints(13, 0, true, testRules()).name == ScoreName::Yakuman);
}

TEST_CASE("The score result carries han, fu, basic points and the name") {
    auto result = alwaysYaku().score(pinfuRon());
    REQUIRE(result.has_value());
    CHECK(result->han == 1);
    CHECK(result->fu == 30);
    CHECK(result->basicPoints == 240);
    CHECK(result->name == ScoreName::None);
}

TEST_CASE("Dora only count when the hand has a yaku") {
    WinContext ctx = pinfuRon("1m");  // 2m is dora

    CHECK_FALSE(noYaku().score(ctx).has_value());

    auto result = alwaysYaku().score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 2);
    REQUIRE(result->hanSources.size() == 2);
    CHECK(result->hanSources[0].name == "Always");
    CHECK(result->hanSources[1].name == "Dora");
    CHECK(result->hanSources[1].han == 1);
}

TEST_CASE("Ura and aka dora are sources of their own, empty ones are left out") {
    WinContext ctx = pinfuRon("", "123m406s789s234p55s");  // a red 5s
    ctx.riichi = true;
    ctx.uraDoraIndicators = parseTiles("1p");  // 2p is ura dora

    auto result = alwaysYaku().score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 3);
    REQUIRE(result->hanSources.size() == 3);  // no "Dora": none of the indicators hit
    CHECK(result->hanSources[1].name == "Ura Dora");
    CHECK(result->hanSources[2].name == "Aka Dora");
}

TEST_CASE("Yakuman replace all other han") {
    auto result = makeScorer({kAlways, kFakeYakuman}).score(pinfuRon("1m"));
    REQUIRE(result.has_value());
    REQUIRE(result->hanSources.size() == 1);  // neither the 1-han yaku nor the dora
    CHECK(result->hanSources[0].name == "Yakuman");
    CHECK(result->han == 13);
    CHECK(result->basicPoints == 8000);
    CHECK(result->name == ScoreName::Yakuman);
}

TEST_CASE("Double and stacked yakuman follow the rules") {
    RuleSet rules = testRules();
    auto score = [&rules] { return makeScorer({kFakeYakuman, kFakeDoubleYakuman}, rules).score(pinfuRon()); };

    SECTION("both count double and stack") {
        rules.doubleYakuman = true;
        rules.stackYakuman = true;
        auto result = score();
        REQUIRE(result.has_value());
        CHECK(result->han == 39);
        CHECK(result->basicPoints == 24000);
        CHECK(result->hanSources.size() == 2);
    }
    SECTION("no double yakuman") {
        rules.doubleYakuman = false;
        rules.stackYakuman = true;
        auto result = score();
        REQUIRE(result.has_value());
        CHECK(result->han == 26);
        CHECK(result->basicPoints == 16000);
    }
    SECTION("no stacking: only the highest yakuman counts") {
        rules.doubleYakuman = true;
        rules.stackYakuman = false;
        auto result = score();
        REQUIRE(result.has_value());
        REQUIRE(result->hanSources.size() == 1);
        CHECK(result->hanSources[0].name == "Double Yakuman");
        CHECK(result->han == 26);
    }
    SECTION("neither") {
        rules.doubleYakuman = false;
        rules.stackYakuman = false;
        auto result = score();
        REQUIRE(result.has_value());
        CHECK(result->han == 13);
        CHECK(result->basicPoints == 8000);
    }
}

TEST_CASE("13 or more han from yaku and dora are a kazoe yakuman") {
    RuleSet rules = testRules();
    WinContext ctx = pinfuRon("4s");  // the three 5s are dora: 15 han
    auto score = [&] { return makeScorer({kTwelveHan}, rules).score(ctx); };

    SECTION("with kazoe yakuman") {
        rules.kazoeYakuman = true;
        auto result = score();
        REQUIRE(result.has_value());
        CHECK(result->han == 13);
        CHECK(result->name == ScoreName::KazoeYakuman);
        CHECK(result->basicPoints == 8000);
        CHECK(result->hanSources.size() == 2);  // the yaku and the dora stay listed
    }
    SECTION("without") {
        rules.kazoeYakuman = false;
        auto result = score();
        REQUIRE(result.has_value());
        CHECK(result->han == 15);
        CHECK(result->name == ScoreName::Sanbaiman);
        CHECK(result->basicPoints == 6000);
    }
}

TEST_CASE("Win points: ron 4 or 6 times the basic points, tsumo split, rounded up per payment") {
    SECTION("mangan") {
        CHECK(winPoints(2000, false, false) == 8000);
        CHECK(winPoints(2000, true, false) == 12000);
        CHECK(winPoints(2000, false, true) == 8000);   // 4000 + 2 * 2000
        CHECK(winPoints(2000, true, true) == 12000);   // 3 * 4000
    }
    SECTION("each payment is rounded on its own") {
        // 3 han 30 fu: 960 basic points
        CHECK(winPoints(960, false, false) == 3900);   // 3840
        CHECK(winPoints(960, true, false) == 5800);    // 5760
        CHECK(winPoints(960, false, true) == 4000);    // 2000 + 2 * 1000
        CHECK(winPoints(960, true, true) == 6000);     // 3 * 2000
    }
}

