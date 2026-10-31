#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>

#include "TestHelpers.hpp"
#include "game/RoundController.hpp"

using namespace mj;
using namespace mj::test;

namespace {

// Always picks Tsumo, which is never legal for a non-winning hand.
class CheatingPlayer : public IPlayer {
public:
    Action chooseAction(const PlayerView&, const std::vector<Action>&) override { return {ActionType::Tsumo}; }
};

// Only a ron on a kan from pon has a yaku, so other wins are impossible.
const Yaku kChankanOnly{"ChankanOnly", [](const Reading&, const WinContext& ctx) { return ctx.chankan ? 1 : 0; }};

// Thirteen unconnected tiles: never tenpai, no calls on the filler 8p.
constexpr const char* kJunk = "147m259p369s4567z";

const std::array<int, 4> kScores{25000, 25000, 25000, 25000};

// Wall that deals the given 13-tile hands (seat 0 is the dealer and gets the
// first hand), then gives out `draws` in order. Everything after that, the
// dead wall included, is 8p.
Wall stackedWall(const std::array<const char*, 4>& hands, const char* draws) {
    std::vector<Tile> tiles;
    for (const char* hand : hands) {
        std::vector<Tile> dealt = parseTiles(hand);
        REQUIRE(dealt.size() == 13);
        tiles.insert(tiles.end(), dealt.begin(), dealt.end());
    }
    std::vector<Tile> drawn = parseTiles(draws);
    tiles.insert(tiles.end(), drawn.begin(), drawn.end());
    tiles.resize(kNumTiles, tile("8p"));
    return Wall(std::move(tiles));
}

template <typename P>
std::array<IPlayer*, 4> seats(std::array<P, 4>& players) {
    return {&players[0], &players[1], &players[2], &players[3]};
}

size_t totalDiscards(const RoundState& state) {
    return std::accumulate(state.players.begin(), state.players.end(), size_t{0},
                           [](size_t sum, const PlayerState& p) { return sum + p.river.size(); });
}

}  // namespace

TEST_CASE("A round without wins or calls ends in an exhaustive draw") {
    std::mt19937 rng(3);
    std::array<TestPlayer, 4> players;
    Scorer scorer = noYaku();
    RoundController round(RoundState(Wall(rng), Honor::East, 0, kScores), seats(players), scorer);

    RoundResult result = round.play();

    CHECK(result.end == RoundEnd::ExhaustiveDraw);
    CHECK(result.wins.empty());
    CHECK(round.state().wall.remaining() == 0);
    CHECK(totalDiscards(round.state()) == 70);    // 136 - 14 dead wall - 52 dealt
    CHECK(round.state().players[0].river.size() == 18);  // the dealer draws 1st, 5th, ..., 69th
    CHECK(round.state().players[3].river.size() == 17);
}

TEST_CASE("The dealer starts, whatever their seat") {
    std::mt19937 rng(5);
    std::array<TestPlayer, 4> players;
    Scorer scorer = noYaku();
    RoundController round(RoundState(Wall(rng), Honor::East, 2, kScores), seats(players), scorer);

    round.play();

    const RoundState& state = round.state();
    CHECK(state.players[2].seatWind == Honor::East);
    CHECK(state.players[3].seatWind == Honor::South);
    CHECK(state.players[0].seatWind == Honor::West);
    CHECK(state.players[1].seatWind == Honor::North);
    CHECK(state.players[2].river.size() == 18);
    CHECK(state.players[1].river.size() == 17);
}

TEST_CASE("Ron on a discard ends the round") {
    std::array<TestPlayer, 4> players;
    players[1].takeWins = true;
    Scorer scorer = alwaysYaku();
    RoundController round(RoundState(stackedWall({kJunk, "123m456p789s11z22z", kJunk, kJunk}, "1z"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    CHECK(result.end == RoundEnd::Ron);
    REQUIRE(result.wins.size() == 1);
    CHECK(result.wins[0].seat == 1);
    CHECK(result.wins[0].fromSeat == 0);
    CHECK(result.wins[0].score.han == 1);
    CHECK(round.state().players[0].river.size() == 1);
}

TEST_CASE("Tsumo on the drawn tile ends the round") {
    std::array<TestPlayer, 4> players;
    players[1].takeWins = true;
    Scorer scorer = alwaysYaku();
    RoundController round(RoundState(stackedWall({kJunk, "123m456p789s11z22z", kJunk, kJunk}, "9m1z"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    CHECK(result.end == RoundEnd::Tsumo);
    REQUIRE(result.wins.size() == 1);
    CHECK(result.wins[0].seat == 1);
    CHECK(result.wins[0].fromSeat == 1);
    CHECK(round.state().players[1].river.empty());
}

TEST_CASE("Double ron, or only the first player in turn order") {
    std::array<TestPlayer, 4> players;
    players[1].takeWins = true;
    players[2].takeWins = true;
    RuleSet rules = testRules();
    auto play = [&] {
        Scorer scorer = alwaysYaku(rules);
        RoundController round(RoundState(stackedWall({kJunk, "123m456p789s11z22z", "123p456s789m11z33z", kJunk}, "1z"),
                                         Honor::East, 0, kScores),
                              seats(players), scorer);
        return round.play();
    };

    SECTION("double ron") {
        rules.multipleRon = true;
        RoundResult result = play();
        REQUIRE(result.wins.size() == 2);
        CHECK(result.wins[0].seat == 1);
        CHECK(result.wins[1].seat == 2);
    }
    SECTION("first in turn order") {
        rules.multipleRon = false;
        RoundResult result = play();
        REQUIRE(result.wins.size() == 1);
        CHECK(result.wins[0].seat == 1);
    }
}

TEST_CASE("Several rons are listed in turn order from the discarder, not by seat") {
    // Seat 2 draws and discards the 1z that seats 3 and 0 wait on. After seat 2
    // comes seat 3, then seat 0; sorting by seat would put seat 0 first.
    std::array<TestPlayer, 4> players;
    players[0].takeWins = true;
    players[3].takeWins = true;
    RuleSet rules = testRules();
    auto play = [&] {
        Scorer scorer = alwaysYaku(rules);
        RoundController round(RoundState(stackedWall({"123m456p789s11z22z", kJunk, kJunk, "123p456s789m11z33z"},
                                                     "8p8p1z"),
                                         Honor::East, 0, kScores),
                              seats(players), scorer);
        return round.play();
    };

    SECTION("multiple ron") {
        rules.multipleRon = true;
        RoundResult result = play();
        REQUIRE(result.wins.size() == 2);
        CHECK(result.wins[0].seat == 3);
        CHECK(result.wins[1].seat == 0);
        CHECK(result.wins[0].fromSeat == 2);
        CHECK(result.wins[1].fromSeat == 2);
    }
    SECTION("only the first winner") {
        rules.multipleRon = false;
        RoundResult result = play();
        REQUIRE(result.wins.size() == 1);
        CHECK(result.wins[0].seat == 3);
    }
}

TEST_CASE("Pon moves the turn to the caller") {
    std::array<TestPlayer, 4> players;
    players[2].callPon = true;
    Scorer scorer = noYaku();
    RoundController round(RoundState(stackedWall({kJunk, kJunk, "147m259p369s11z56z", kJunk}, "1z"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    const RoundState& state = round.state();
    CHECK(result.end == RoundEnd::ExhaustiveDraw);
    REQUIRE(state.players[2].hand.melds().size() == 1);
    const Meld& pon = state.players[2].hand.melds()[0];
    CHECK(pon.type == MeldType::Pon);
    CHECK(pon.fromSeat == 0);
    CHECK(pon.calledTile == tile("1z"));
    CHECK(state.players[0].river[0].calledAway);
    CHECK(state.players[2].river[0].tile == tile("6z"));  // the caller discarded without drawing
    CHECK(state.players[1].river.size() == 17);            // seat 1 was skipped once
    CHECK(totalDiscards(state) == 71);                     // 70 draws plus the discard after the pon
}

TEST_CASE("Open kan gives the caller a replacement draw") {
    std::array<TestPlayer, 4> players;
    players[2].callOpenKan = true;
    Scorer scorer = noYaku();
    RoundController round(RoundState(stackedWall({kJunk, kJunk, "147m259p369s111z5z", kJunk}, "1z"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    round.play();

    const RoundState& state = round.state();
    REQUIRE(state.players[2].hand.melds().size() == 1);
    CHECK(state.players[2].hand.melds()[0].type == MeldType::OpenKan);
    CHECK(state.players[2].hand.melds()[0].fromSeat == 0);
    CHECK(state.wall.rinshanRemaining() == 3);
    CHECK(state.wall.doraIndicators().size() == 2);
    CHECK(state.players[2].river[0].tile == tile("8p"));  // the replacement tile, discarded
    CHECK(totalDiscards(state) == 70);  // 69 live draws (the kan shortened the wall) plus the rinshan turn
}

TEST_CASE("Closed kan draws a replacement tile and reveals a dora") {
    std::array<TestPlayer, 4> players;
    players[0].declareClosedKan = true;
    Scorer scorer = noYaku();
    RoundController round(RoundState(stackedWall({"1111m259p369s567z", kJunk, kJunk, kJunk}, ""),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    round.play();

    const RoundState& state = round.state();
    REQUIRE(state.players[0].hand.melds().size() == 1);
    const Meld& kan = state.players[0].hand.melds()[0];
    CHECK(kan.type == MeldType::ClosedKan);
    CHECK(kan.fromSeat == 0);
    CHECK_FALSE(kan.calledTile.has_value());
    CHECK(state.players[0].hand.isClosed());
    CHECK(state.wall.rinshanRemaining() == 3);
    CHECK(state.wall.doraIndicators().size() == 2);
    CHECK(totalDiscards(state) == 69);  // one discard per live draw; the kan turn had two draws
}

TEST_CASE("A kan from pon can be robbed") {
    // Seat 2 pons the dealer's 5s and later draws the fourth 5s; seat 1 waits on 5s.
    std::array<TestPlayer, 4> players;
    players[1].takeWins = true;
    players[2].callPon = true;
    players[2].declareKanFromPon = true;
    Scorer scorer = makeScorer({kChankanOnly});  // seat 1 can only win by chankan
    RoundController round(RoundState(stackedWall({kJunk, "123m456p789m11z46s", "55s147m259p369m56z", kJunk},
                                                 "5s8p8p8p5s"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    CHECK(result.end == RoundEnd::Ron);
    REQUIRE(result.wins.size() == 1);
    CHECK(result.wins[0].seat == 1);
    CHECK(result.wins[0].fromSeat == 2);
    CHECK(round.state().players[2].hand.melds()[0].type == MeldType::Pon);  // the kan never happened
}

TEST_CASE("Riichi costs a stick and locks the hand") {
    std::array<TestPlayer, 4> players;
    players[0].declareRiichi = true;
    RuleSet rules = testRules();
    rules.riichiNeeds1000Points = true;
    Scorer scorer = noYaku(rules);
    RoundController round(RoundState(stackedWall({"123m456p789s111z2z", kJunk, kJunk, kJunk}, "3z"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    const RoundState& state = round.state();
    const PlayerState& dealer = state.players[0];
    CHECK(result.end == RoundEnd::ExhaustiveDraw);
    CHECK(result.tenpai == std::array<bool, 4>{true, false, false, false});
    CHECK(dealer.riichi);
    CHECK(dealer.doubleRiichi);  // first discard, no call before
    CHECK_FALSE(dealer.ippatsu);  // gone with the next own discard
    CHECK(dealer.score == 24000);
    CHECK(state.riichiSticks == 1);

    REQUIRE(dealer.river.size() == 18);
    CHECK(dealer.river[0].riichi);
    CHECK(dealer.river[0].tile == tile("2z"));  // the first riichi discard offered
    CHECK_FALSE(dealer.river[0].tsumogiri);     // 2z came from the hand, 3z was drawn
    CHECK(std::all_of(dealer.river.begin() + 1, dealer.river.end(),
                      [](const RiverTile& t) { return t.tile == tile("8p") && t.tsumogiri && !t.riichi; }));
}

TEST_CASE("Kyuushu kyuuhai ends the round on the first turn") {
    std::array<TestPlayer, 4> players;
    players[0].declareKyuushu = true;
    RuleSet rules = testRules();
    rules.kyuushuKyuuhai = true;
    Scorer scorer = noYaku(rules);
    RoundController round(RoundState(stackedWall({"19m19p19s1234z234m", kJunk, kJunk, kJunk}, "5m"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    CHECK(result.end == RoundEnd::Aborted);
    CHECK(result.abortReason == AbortReason::KyuushuKyuuhai);
    CHECK(result.wins.empty());
    CHECK(round.state().players[0].river.empty());
}

TEST_CASE("Triple ron aborts the round, unless the rules say otherwise") {
    std::array<TestPlayer, 4> players;
    for (int seat : {1, 2, 3}) players[static_cast<size_t>(seat)].takeWins = true;
    RuleSet rules = testRules();
    auto play = [&] {
        // Seats 1 to 3 all wait on 1z, which the dealer draws and discards.
        Scorer scorer = alwaysYaku(rules);
        RoundController round(RoundState(stackedWall({kJunk, "123m456p789s11z22z", "123p456s789m11z33z",
                                                      "123s456m789p11z44z"},
                                                     "1z"),
                                         Honor::East, 0, kScores),
                              seats(players), scorer);
        return round.play();
    };

    SECTION("aborted") {
        rules.tripleRonAborts = true;
        RoundResult result = play();
        CHECK(result.end == RoundEnd::Aborted);
        CHECK(result.abortReason == AbortReason::Sanchahou);
        CHECK(result.wins.empty());
    }
    SECTION("all three win") {
        rules.tripleRonAborts = false;
        rules.multipleRon = true;
        RoundResult result = play();
        CHECK(result.end == RoundEnd::Ron);
        CHECK(result.wins.size() == 3);
    }
    SECTION("only the first wins") {
        rules.tripleRonAborts = false;
        rules.multipleRon = false;
        RoundResult result = play();
        REQUIRE(result.wins.size() == 1);
        CHECK(result.wins[0].seat == 1);
    }
}

TEST_CASE("Four identical winds as first discards abort the round") {
    std::array<TestPlayer, 4> players;
    RuleSet rules = testRules();
    auto play = [&](const char* draws) {
        // Everyone discards the tile they draw, so the draws are the first four discards.
        Scorer scorer = noYaku(rules);
        RoundController round(RoundState(stackedWall({kJunk, kJunk, kJunk, kJunk}, draws), Honor::East, 0, kScores),
                              seats(players), scorer);
        return round.play();
    };

    SECTION("four East") {
        rules.suufonRenda = true;
        RoundResult result = play("1z1z1z1z");
        CHECK(result.end == RoundEnd::Aborted);
        CHECK(result.abortReason == AbortReason::SuufonRenda);
    }
    SECTION("not with one different wind") {
        rules.suufonRenda = true;
        CHECK(play("1z1z1z2z").end == RoundEnd::ExhaustiveDraw);
    }
    SECTION("not when the rules turn it off") {
        rules.suufonRenda = false;
        CHECK(play("1z1z1z1z").end == RoundEnd::ExhaustiveDraw);
    }
}

TEST_CASE("Four riichi abort the round") {
    std::array<TestPlayer, 4> players;
    for (TestPlayer& p : players) p.declareRiichi = true;
    RuleSet rules = testRules();
    rules.riichiNeeds1000Points = true;
    rules.suuchaRiichi = true;
    // Every player draws 3z and declares riichi by discarding 2z.
    const char* tenpai = "123m456p789s111z2z";
    Scorer scorer = noYaku(rules);
    RoundController round(RoundState(stackedWall({tenpai, tenpai, tenpai, tenpai}, "3z3z3z3z"),
                                     Honor::East, 0, kScores),
                          seats(players), scorer);

    RoundResult result = round.play();

    CHECK(result.end == RoundEnd::Aborted);
    CHECK(result.abortReason == AbortReason::SuuchaRiichi);
    CHECK(round.state().riichiSticks == 4);  // the fourth riichi stands before the abort
    for (const PlayerState& p : round.state().players) CHECK(p.score == 24000);
}

TEST_CASE("Four kans by different players abort the round") {
    std::array<TestPlayer, 4> players;
    for (TestPlayer& p : players) p.declareClosedKan = true;
    RuleSet rules = testRules();
    rules.suukaikan = true;

    SECTION("two players with two kans each") {
        // The dealer kans 1m and 2m, then seat 1 kans 3p and 4p.
        Scorer scorer = noYaku(rules);
        RoundController round(RoundState(stackedWall({"1111m2222m369s56z", "3333p4444p369s56z", kJunk, kJunk}, ""),
                                         Honor::East, 0, kScores),
                              seats(players), scorer);
        RoundResult result = round.play();

        CHECK(result.end == RoundEnd::Aborted);
        CHECK(result.abortReason == AbortReason::Suukaikan);
        CHECK(round.state().player(1).river.size() == 1);  // after the discard that followed the fourth kan
    }
    SECTION("not when one player made all four") {
        // Three kans from the hand; the replacement tiles are 8p filler and make the fourth.
        Scorer scorer = noYaku(rules);
        RoundController round(RoundState(stackedWall({"1111m2222m3333m4z", kJunk, kJunk, kJunk}, ""),
                                         Honor::East, 0, kScores),
                              seats(players), scorer);
        RoundResult result = round.play();

        CHECK(result.end == RoundEnd::ExhaustiveDraw);
        CHECK(round.state().player(0).hand.melds().size() == 4);
        CHECK(round.state().wall.rinshanRemaining() == 0);
    }
}

TEST_CASE("Illegal choices are rejected") {
    std::mt19937 rng(1);
    std::array<CheatingPlayer, 4> players;
    Scorer scorer = noYaku();
    RoundController round(RoundState(Wall(rng), Honor::East, 0, kScores), seats(players), scorer);

    CHECK_THROWS_AS(round.play(), std::logic_error);
}
