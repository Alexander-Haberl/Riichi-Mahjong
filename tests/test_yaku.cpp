#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string_view>
#include <vector>

#include "TestHelpers.hpp"
#include "game/LegalActions.hpp"
#include "scoring/Scorer.hpp"
#include "scoring/Yaku.hpp"

using namespace mj;
using namespace mj::test;

namespace {

// Han of the named standard yaku: the best over all readings of the hand.
int han(std::string_view name, const WinContext& ctx, const RuleSet& rules = testRules()) {
    const auto yaku = standardYaku(rules);
    const auto it = std::find_if(yaku.begin(), yaku.end(), [name](const Yaku& y) { return y.name == name; });
    REQUIRE(it != yaku.end());

    const auto rs = readings(ctx.hand, ctx.winTile, !ctx.tsumo);
    REQUIRE_FALSE(rs.empty());
    int best = 0;
    for (const Reading& reading : rs) best = std::max(best, it->han(reading, ctx));
    return best;
}

Meld chi(const char* own, const char* called) { return {MeldType::Chi, parseTiles(own), tile(called), 3}; }
Meld pon(const char* own, const char* called) { return {MeldType::Pon, parseTiles(own), tile(called), 2}; }
Meld openKan(const char* own, const char* called) { return {MeldType::OpenKan, parseTiles(own), tile(called), 2}; }
Meld closedKan(const char* tiles) { return {MeldType::ClosedKan, parseTiles(tiles), std::nullopt, 0}; }

// Four sequences with a plain pair and a ryanmen wait on 2p: pinfu on ron.
constexpr const char* kPlain = "123m456s789s234p55s";
// An open version: the chi 789s is called.
WinContext openHand(bool tsumo = false) {
    return win("123m456m234p55s89s", "2p", tsumo, {chi("89s", "7s")});
}

bool hasSource(const ScoreResult& result, std::string_view name) {
    return std::any_of(result.hanSources.begin(), result.hanSources.end(),
                       [name](const HanSource& source) { return source.name == name; });
}

}  // namespace

TEST_CASE("Yaku names are unique") {
    std::set<std::string_view> names;
    const auto yaku = standardYaku(testRules());
    for (const Yaku& y : yaku) names.insert(y.name);
    CHECK(names.size() == yaku.size());
    CHECK(yaku.size() == 42);
}

TEST_CASE("Kokushi is the only yaku of a kokushi hand") {
    const WinContext ctx = win("19m19p19s12345677z", "1z", false);  // single wait
    for (const Yaku& yaku : standardYaku(testRules())) {
        INFO(yaku.name);
        CHECK(han(yaku.name, ctx) == (yaku.name == "Kokushi Musou" ? kYakumanHan : 0));
    }
    CHECK(han("Kokushi Musou", win(kPlain, "2p", false)) == 0);
}

// --- Situation yaku ---------------------------------------------------------

TEST_CASE("Riichi and double riichi exclude each other") {
    WinContext ctx = win(kPlain, "2p", false);
    CHECK(han("Riichi", ctx) == 0);
    CHECK(han("Double Riichi", ctx) == 0);

    ctx.riichi = true;
    CHECK(han("Riichi", ctx) == 1);
    CHECK(han("Double Riichi", ctx) == 0);

    ctx.doubleRiichi = true;  // declareRiichi sets both flags
    CHECK(han("Riichi", ctx) == 0);
    CHECK(han("Double Riichi", ctx) == 2);
}

TEST_CASE("Ippatsu yaku") {
    WinContext ctx = win(kPlain, "2p", false);
    CHECK(han("Ippatsu", ctx) == 0);
    ctx.ippatsu = true;
    CHECK(han("Ippatsu", ctx) == 1);
}

TEST_CASE("Menzen tsumo needs a closed hand and a tsumo") {
    CHECK(han("Menzen Tsumo", win(kPlain, "2p", true)) == 1);
    CHECK(han("Menzen Tsumo", win(kPlain, "2p", false)) == 0);
    CHECK(han("Menzen Tsumo", openHand(true)) == 0);
    // A closed kan keeps the hand closed.
    CHECK(han("Menzen Tsumo", win("1111m456s789s234p55s", "2p", true, {closedKan("1111m")})) == 1);
}

TEST_CASE("Haitei, houtei, rinshan and chankan follow the context, open or closed") {
    struct Flag {
        const char* name;
        bool WinContext::*member;
    };
    for (Flag flag : {Flag{"Haitei", &WinContext::haitei}, Flag{"Houtei", &WinContext::houtei},
                      Flag{"Rinshan Kaihou", &WinContext::rinshan}, Flag{"Chankan", &WinContext::chankan}}) {
        INFO(flag.name);
        WinContext closed = win(kPlain, "2p", false);
        CHECK(han(flag.name, closed) == 0);
        closed.*flag.member = true;
        CHECK(han(flag.name, closed) == 1);

        WinContext open = openHand();
        open.*flag.member = true;
        CHECK(han(flag.name, open) == 1);  // the same on an open hand
    }
}

// --- Tanyao, pinfu, yakuhai -------------------------------------------------

TEST_CASE("Tanyao yaku") {
    CHECK(han("Tanyao", win("234m345p456s678s22m", "4m", false)) == 1);
    CHECK(han("Tanyao", win("123m345p456s678s22m", "4p", false)) == 0);  // 1m
    CHECK(han("Tanyao", win("234m345p456s678s99m", "4m", false)) == 0);  // a terminal pair
    CHECK(han("Tanyao", win("234m345p456s666z22m", "4m", false)) == 0);  // an honor triplet
    CHECK(han("Tanyao", win("234m345p456s789s22m", "4m", false)) == 0);  // 789s
    CHECK(han("Tanyao", win("223388m4455p6677s", "8m", false)) == 1);     // chiitoitsu
}

TEST_CASE("Tanyao on an open hand depends on the kuitan rule") {
    const WinContext ctx = win("234m22m456s678s34p", "4m", false, {chi("34p", "5p")});
    RuleSet rules = testRules();

    rules.kuitan = true;
    CHECK(han("Tanyao", ctx, rules) == 1);
    rules.kuitan = false;
    CHECK(han("Tanyao", ctx, rules) == 0);
    CHECK(han("Tanyao", win("234m345p456s678s22m", "4m", false), rules) == 1);  // closed: always
}

TEST_CASE("Pinfu yaku") {
    CHECK(han("Pinfu", win(kPlain, "2p", false)) == 1);
    CHECK(han("Pinfu", win(kPlain, "2p", true)) == 1);
    CHECK(han("Pinfu", win(kPlain, "3p", false)) == 0);                           // kanchan
    CHECK(han("Pinfu", win("123m456s789s123p55s", "3p", false)) == 0);            // penchan
    CHECK(han("Pinfu", win("123m456s789s234p55z", "2p", false)) == 0);            // dragon pair
    CHECK(han("Pinfu", win("111m456s789s234p55s", "2p", false)) == 0);            // a triplet
    CHECK(han("Pinfu", openHand()) == 0);                                         // open
}

TEST_CASE("Pinfu and the seat and round wind pairs") {
    WinContext east = win("123m456s789s234p11z", "2p", false);
    CHECK(han("Pinfu", east) == 0);  // East is the seat and the round wind

    WinContext south = win("123m456s789s234p22z", "2p", false);
    CHECK(han("Pinfu", south) == 1);  // South is neither
    south.seatWind = Honor::South;
    CHECK(han("Pinfu", south) == 0);
    south.seatWind = Honor::East;
    south.roundWind = Honor::South;
    CHECK(han("Pinfu", south) == 0);
}

TEST_CASE("Dragon yakuhai") {
    const WinContext red = win("123m456p789s55s777z", "5s", false);
    CHECK(han("Red Dragon", red) == 1);
    CHECK(han("White Dragon", red) == 0);
    CHECK(han("Green Dragon", red) == 0);

    CHECK(han("White Dragon", win("123m456p789s55s555z", "5s", false)) == 1);
    CHECK(han("Green Dragon", win("123m456p789s55s666z", "5s", false)) == 1);
}

TEST_CASE("A pair is not yakuhai") {
    const WinContext pair = win("123m456p789s111z77z", "2m", false);
    CHECK(han("Red Dragon", pair) == 0);
}

TEST_CASE("Wind yakuhai: seat wind, round wind, or both") {
    WinContext ctx = win("123m456p789s111z77z", "2m", false);  // a triplet of East

    SECTION("seat and round wind are East") {
        CHECK(han("Seat Wind", ctx) == 1);
        CHECK(han("Round Wind", ctx) == 1);  // 2 han in total
    }
    SECTION("only the round wind") {
        ctx.seatWind = Honor::South;
        CHECK(han("Seat Wind", ctx) == 0);
        CHECK(han("Round Wind", ctx) == 1);
    }
    SECTION("only the seat wind") {
        ctx.roundWind = Honor::South;
        CHECK(han("Seat Wind", ctx) == 1);
        CHECK(han("Round Wind", ctx) == 0);
    }
    SECTION("neither") {
        ctx.seatWind = Honor::South;
        ctx.roundWind = Honor::West;
        CHECK(han("Seat Wind", ctx) == 0);
        CHECK(han("Round Wind", ctx) == 0);
    }
}

TEST_CASE("Kans and open pons count as yakuhai") {
    CHECK(han("White Dragon", win("5555z123m456p789s55s", "2m", false, {closedKan("5555z")})) == 1);
    CHECK(han("Green Dragon", win("66z123m456p789s55s", "2m", false, {pon("66z", "6z")})) == 1);  // open: still 1
}

// --- Identical sequences and chiitoitsu -------------------------------------

TEST_CASE("Iipeikou yaku") {
    CHECK(han("Iipeikou", win("112233m456p789s55z", "5z", false)) == 1);
    CHECK(han("Ryanpeikou", win("112233m456p789s55z", "5z", false)) == 0);
    CHECK(han("Iipeikou", win(kPlain, "2p", false)) == 0);  // no identical sequences
}

TEST_CASE("Iipeikou needs a closed hand") {
    CHECK(han("Iipeikou", win("112233m456p55z78s", "5z", false, {chi("78s", "9s")})) == 0);
}

TEST_CASE("Three identical sequences make one pair") {
    const WinContext ctx = win("111222333m456p55z", "5z", false);
    CHECK(han("Iipeikou", ctx) == 1);
    CHECK(han("Ryanpeikou", ctx) == 0);
}

TEST_CASE("Ryanpeikou excludes iipeikou") {
    const WinContext ctx = win("112233m445566p77z", "7z", false);
    CHECK(han("Ryanpeikou", ctx) == 3);
    CHECK(han("Iipeikou", ctx) == 0);
    CHECK(han("Chiitoitsu", ctx) == 2);  // the same hand also has a chiitoitsu reading
}

TEST_CASE("Ryanpeikou needs a closed hand") {
    const WinContext ctx = win("112233m456p77z45p", "7z", false, {chi("45p", "6p")});
    CHECK(han("Ryanpeikou", ctx) == 0);
    CHECK(han("Iipeikou", ctx) == 0);
}

TEST_CASE("Four identical sequences are ryanpeikou") {
    const WinContext ctx = win("111122223333m55z", "5z", false);
    CHECK(han("Ryanpeikou", ctx) == 3);
}

TEST_CASE("Chiitoitsu yaku") {
    CHECK(han("Chiitoitsu", win("1133557799m1155p", "5p", false)) == 2);
    CHECK(han("Chiitoitsu", win(kPlain, "2p", false)) == 0);
}

// --- Triplets ---------------------------------------------------------------

TEST_CASE("Toitoi yaku") {
    CHECK(han("Toitoi", win("111m222p333s444s55z", "5z", false)) == 2);
    CHECK(han("Toitoi", win("123m222p333s444s55z", "5z", false)) == 0);
    CHECK(han("Toitoi", win("11m222p333s444s55z", "5z", false, {pon("11m", "1m")})) == 2);  // open: still 2
    CHECK(han("Toitoi", win("1111m222p333s444s55z", "5z", false, {closedKan("1111m")})) == 2);
}

TEST_CASE("Sanankou yaku") {
    const char* hand = "111m222p333s456s55z";  // three closed triplets
    CHECK(han("Sanankou", win(hand, "6s", false)) == 2);
    CHECK(han("Sanankou", win(hand, "1m", true)) == 2);
    CHECK(han("Sanankou", win("123m222p333s456s55z", "6s", false)) == 0);  // only two
}

TEST_CASE("A triplet completed by ron is not concealed for sanankou") {
    CHECK(han("Sanankou", win("111m222p333s456s55z", "1m", false)) == 0);
}

TEST_CASE("Sanankou also works on an open hand, and counts closed kans") {
    CHECK(han("Sanankou", win("44s111m222p333s55z", "5z", false, {pon("44s", "4s")})) == 2);
    CHECK(han("Sanankou", win("1111m222p333s456s55z", "6s", false, {closedKan("1111m")})) == 2);
}

TEST_CASE("Open melds are not concealed for sanankou") {
    CHECK(han("Sanankou", win("22p111m333s456s55z", "6s", false, {pon("22p", "2p")})) == 0);
    CHECK(han("Sanankou", win("222p111m333s456s55z", "6s", false, {openKan("222p", "2p")})) == 0);
}

// --- Sequences across suits -------------------------------------------------

TEST_CASE("Sanshoku yaku") {
    CHECK(han("Sanshoku", win("123m123p123s789s55z", "5z", false)) == 2);
    CHECK(han("Sanshoku", win("789m789p789s123s55z", "5z", false)) == 2);  // the highest number
    CHECK(han("Sanshoku", win("123m123p234s789s55z", "5z", false)) == 0);
    CHECK(han("Sanshoku", win("123m123p23s789s55z", "5z", false, {chi("23s", "1s")})) == 1);  // open
}

TEST_CASE("Ittsu yaku") {
    CHECK(han("Ittsu", win("123456789m123p55z", "5z", false)) == 2);
    CHECK(han("Ittsu", win("123456789p123m55z", "5z", false)) == 2);
    CHECK(han("Ittsu", win("123456789s123m55z", "5z", false)) == 2);
    CHECK(han("Ittsu", win("123m456m789p123p55z", "5z", false)) == 0);  // split over two suits
    CHECK(han("Ittsu", win("123456m123p55z89m", "5z", false, {chi("89m", "7m")})) == 1);  // open
}

// --- Terminals and honors, and suits ----------------------------------------

TEST_CASE("Chanta yaku") {
    CHECK(han("Chanta", win("123m789p789s111z55z", "5z", false)) == 2);
    CHECK(han("Chanta", win("234m789p789s111z55z", "5z", false)) == 0);  // 234m has no terminal
    CHECK(han("Chanta", win("123m789p789s111z55s", "5s", false)) == 0);  // a simple pair
    CHECK(han("Chanta", win("123m789p555s111z99s", "9s", false)) == 0);  // a simple triplet
    CHECK(han("Chanta", win("23m789p789s111z55z", "5z", false, {chi("23m", "1m")})) == 1);  // open
}

TEST_CASE("Chanta needs a sequence and an honor") {
    CHECK(han("Chanta", win("123m789p789s111p99s", "9s", false)) == 0);  // no honor: junchan
    CHECK(han("Chanta", win("111m999p111s111z55z", "5z", false)) == 0);  // no sequence: honroutou
}

TEST_CASE("Honitsu yaku") {
    CHECK(han("Honitsu", win("111m234m567m999m22z", "2z", false)) == 3);
    CHECK(han("Honitsu", win("111m234m567m999m22m", "2m", false)) == 0);  // no honor: chinitsu
    CHECK(han("Honitsu", win("111m234p567s999m22z", "2z", false)) == 0);  // three suits
    CHECK(han("Honitsu", win("99m111m234m567m22z", "2z", false, {pon("99m", "9m")})) == 2);  // open
    CHECK(han("Honitsu", win("1133557799m22z44z", "2z", false)) == 3);  // chiitoitsu
}

TEST_CASE("Chinitsu yaku") {
    CHECK(han("Chinitsu", win("111m234m567m999m22m", "2m", false)) == 6);
    CHECK(han("Chinitsu", win("111m234m567m999m22z", "2z", false)) == 0);  // an honor: honitsu
    CHECK(han("Chinitsu", win("111m234m567m999p22p", "2p", false)) == 0);  // two suits
    CHECK(han("Chinitsu", win("99m111m234m567m22m", "2m", false, {pon("99m", "9m")})) == 5);  // open
    CHECK(han("Chinitsu", win("11223344557799m", "9m", false)) == 6);  // chiitoitsu
}

// --- Through the scorer -----------------------------------------------------

TEST_CASE("A riichi tsumo pinfu adds up through the standard scorer") {
    WinContext ctx = win(kPlain, "2p", true);
    ctx.riichi = true;

    auto result = standardScorer(testRules()).score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 3);
    CHECK(hasSource(*result, "Riichi"));
    CHECK(hasSource(*result, "Menzen Tsumo"));
    CHECK(hasSource(*result, "Pinfu"));
    CHECK(result->fu == 20);  // pinfu tsumo
    CHECK(result->basicPoints == 640);
}

TEST_CASE("A hand without yaku does not win") {
    // An open hand with nothing but sequences and ordinary tiles.
    const WinContext ctx = win("123m456m234p55s89s", "2p", false, {chi("89s", "7s")});
    CHECK_FALSE(standardScorer(testRules()).score(ctx).has_value());
}

TEST_CASE("The scorer prefers ryanpeikou to chiitoitsu") {
    const WinContext ctx = win("112233m445566p77z", "7z", false);

    auto result = standardScorer(testRules()).score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 3);
    REQUIRE(result->hanSources.size() == 1);
    CHECK(result->hanSources[0].name == "Ryanpeikou");
    CHECK(result->basicPoints == 1280);  // 40 fu; chiitoitsu would be 25 fu and 2 han: 400
}

TEST_CASE("Kokushi is worth a yakuman, whatever else is there") {
    WinContext ctx = win("19m19p19s12345677z", "1z", true);  // single wait
    ctx.riichi = true;

    auto result = standardScorer(testRules()).score(ctx);
    REQUIRE(result.has_value());
    REQUIRE(result->hanSources.size() == 1);
    CHECK(result->hanSources[0].name == "Kokushi Musou");
    CHECK(result->name == ScoreName::Yakuman);
    CHECK(result->basicPoints == 8000);
}

TEST_CASE("The kuitan rule decides whether an open tanyao wins") {
    const WinContext ctx = win("234m22m456s678s34p", "4m", false, {chi("34p", "5p")});
    RuleSet rules = testRules();

    rules.kuitan = true;
    CHECK(standardScorer(rules).score(ctx).has_value());
    rules.kuitan = false;
    CHECK_FALSE(standardScorer(rules).score(ctx).has_value());
}

// --- Remaining regular yaku -------------------------------------------------

TEST_CASE("Junchan: terminals in every block, a sequence, no honor") {
    CHECK(han("Junchan", win("123m789p789s123s99m", "9m", false)) == 3);
    CHECK(han("Junchan", win("123m789p123s99m89s", "9m", false, {chi("89s", "7s")})) == 2);
    CHECK(han("Junchan", win("123m789p789s111z99m", "9m", false)) == 0);  // an honor: chanta
    CHECK(han("Chanta", win("123m789p789s123s99m", "9m", false)) == 0);
    CHECK(han("Junchan", win("111m999p111s999s99m", "9m", false)) == 0);  // no sequence
    CHECK(han("Junchan", win("123m123m789m999m11m", "1m", false)) == 3);  // with chinitsu
}

TEST_CASE("Honroutou: terminals and honors only, both of them") {
    CHECK(han("Honroutou", win("111m999p111s111z99s", "9s", false)) == 2);
    CHECK(han("Honroutou", win("1199m1199p11s1122z", "2z", false)) == 2);  // chiitoitsu
    CHECK(han("Honroutou", win("111m999p111s999s99m", "9m", false)) == 0);  // no honor: chinroutou
    CHECK(han("Honroutou", win("123m999p111s111z99s", "9s", false)) == 0);  // a sequence
}

TEST_CASE("Shousangen: two dragon triplets and a dragon pair, besides the yakuhai") {
    const WinContext ctx = win("555z666z77z123m456p", "4p", false);
    CHECK(han("Shousangen", ctx) == 2);
    CHECK(han("White Dragon", ctx) == 1);
    CHECK(han("Shousangen", win("555z666z777z123m11p", "1p", false)) == 0);  // daisangen
    CHECK(han("Shousangen", win("555z66z123m456p789s", "9s", false)) == 0);
}

TEST_CASE("Sanshoku doukou: triplets of one number in all suits") {
    CHECK(han("Sanshoku Doukou", win("222m222p222s456s11z", "6s", false)) == 2);
    CHECK(han("Sanshoku Doukou", win("222m222p222s456s11z", "6s", false, {openKan("222s", "2s")})) == 2);
    CHECK(han("Sanshoku Doukou", win("222m222p333s456s11z", "6s", false)) == 0);
}

TEST_CASE("Sankantsu: exactly three kans") {
    const std::vector<Meld> threeKans = {closedKan("1111m"), openKan("999p", "9p"), closedKan("2222s")};
    CHECK(han("Sankantsu", win("1111m999p2222s456m11z", "6m", false, threeKans)) == 2);
    CHECK(han("Sankantsu", win("1111m999p456m222s11z", "6m", false, {closedKan("1111m"), openKan("999p", "9p")})) == 0);
}

// --- Yakuman ----------------------------------------------------------------

TEST_CASE("Kokushi counts double on a 13-sided wait") {
    CHECK(han("Kokushi Musou", win("19m19p19s11234567z", "7z", false)) == kYakumanHan);
    CHECK(han("Kokushi Musou", win("19m19p19s11234567z", "1z", false)) == 2 * kYakumanHan);
}

TEST_CASE("Suuankou: four concealed triplets, double on a tanki wait") {
    const char* hand = "111m222p333s444s55z";
    CHECK(han("Suuankou", win(hand, "2p", true)) == kYakumanHan);
    CHECK(han("Suuankou", win(hand, "2p", false)) == 0);  // the ronned triplet is open
    CHECK(han("Sanankou", win(hand, "2p", false)) == 2);
    CHECK(han("Suuankou", win(hand, "5z", false)) == 2 * kYakumanHan);
    CHECK(han("Suuankou", win("111m222p333s4444s55z", "5z", true, {closedKan("4444s")})) == 2 * kYakumanHan);
    CHECK(han("Suuankou", win("111m222p333s44s55z", "5z", true, {pon("44s", "4s")})) == 0);
}

TEST_CASE("Dragon and wind yakuman") {
    CHECK(han("Daisangen", win("555z666z777z123m11p", "1p", false)) == kYakumanHan);
    CHECK(han("Daisangen", win("555z666z77z123m456p", "4p", false)) == 0);
    CHECK(han("Shousuushii", win("111z222z333z44z123m", "3m", false)) == kYakumanHan);
    CHECK(han("Shousuushii", win("111z222z333z444z11m", "1m", false)) == 0);
    CHECK(han("Daisuushii", win("111z222z333z444z11m", "1m", false)) == 2 * kYakumanHan);
    CHECK(han("Daisuushii", win("111z222z333z44z123m", "3m", false)) == 0);
}

TEST_CASE("Tsuuiisou, chinroutou and ryuuiisou") {
    CHECK(han("Tsuuiisou", win("111z222z555z666z77z", "7z", false)) == kYakumanHan);
    CHECK(han("Tsuuiisou", win("11223344556677z", "7z", false)) == kYakumanHan);  // chiitoitsu
    CHECK(han("Tsuuiisou", win("111z222z555z666z11m", "1m", false)) == 0);
    CHECK(han("Chinroutou", win("111m999m111p999s11s", "1s", false)) == kYakumanHan);
    CHECK(han("Chinroutou", win("111m999p111s111z99s", "9s", false)) == 0);  // an honor
    CHECK(han("Ryuuiisou", win("234s234s666s888s66z", "6z", false)) == kYakumanHan);
    CHECK(han("Ryuuiisou", win("234s234s666s888s22s", "2s", false)) == kYakumanHan);  // no green dragon
    CHECK(han("Ryuuiisou", win("234s234s666s888s55z", "5z", false)) == 0);
    CHECK(han("Ryuuiisou", win("345s234s666s888s66z", "6z", false)) == 0);  // a 5s
}

TEST_CASE("Chuuren poutou: closed, one suit, double on the 9-sided wait") {
    const char* hand = "11123455678999m";
    CHECK(han("Chuuren Poutou", win(hand, "9m", false)) == kYakumanHan);
    CHECK(han("Chuuren Poutou", win(hand, "5m", false)) == 2 * kYakumanHan);
    CHECK(han("Chuuren Poutou", win("11123456789999m", "9m", true)) == 2 * kYakumanHan);
    CHECK(han("Chuuren Poutou", win("11122345678999m", "2m", false)) == 2 * kYakumanHan);
    CHECK(han("Chuuren Poutou", win("11122233345699m", "9m", false)) == 0);  // chinitsu, not the shape
    CHECK(han("Chuuren Poutou", win("1112345678999m", "9m", false, {chi("23m", "1m")})) == 0);
}

TEST_CASE("Suukantsu: four kans") {
    const std::vector<Meld> kans = {closedKan("1111m"), openKan("999p", "9p"), closedKan("2222s"),
                                    openKan("777z", "7z")};
    CHECK(han("Suukantsu", win("1111m999p2222s777z11z", "1z", true, kans)) == kYakumanHan);
    CHECK(han("Sankantsu", win("1111m999p2222s777z11z", "1z", true, kans)) == 0);
}

TEST_CASE("Tenhou and chiihou: tsumo on the first draw") {
    WinContext ctx = win(kPlain, "2p", true);
    CHECK(han("Tenhou", ctx) == 0);
    ctx.firstDraw = true;
    ctx.seatWind = Honor::East;
    CHECK(han("Tenhou", ctx) == kYakumanHan);
    CHECK(han("Chiihou", ctx) == 0);
    ctx.seatWind = Honor::South;
    CHECK(han("Tenhou", ctx) == 0);
    CHECK(han("Chiihou", ctx) == kYakumanHan);
}

TEST_CASE("Double yakuman and stacking follow the rules") {
    const WinContext ctx = win("111z222z333z444z55z", "5z", true);  // daisuushii, tsuuiisou, suuankou tanki
    RuleSet rules = testRules();

    rules.doubleYakuman = true;
    rules.stackYakuman = true;
    auto result = standardScorer(rules).score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 2 * kYakumanHan + kYakumanHan + 2 * kYakumanHan);

    rules.doubleYakuman = false;
    result = standardScorer(rules).score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == 3 * kYakumanHan);

    rules.stackYakuman = false;
    result = standardScorer(rules).score(ctx);
    REQUIRE(result.has_value());
    CHECK(result->han == kYakumanHan);
}

TEST_CASE("The first draw flag: tsumo on the own first draw, before any call") {
    RoundState state(Wall(Wall::fullSet()), Honor::East, 0, {25000, 25000, 25000, 25000});
    state.players[1].hand = Hand(parseTiles(kPlain));
    state.current = 1;
    state.tileDraw = true;
    CHECK(tsumoContext(state).firstDraw);

    state.anyCall = true;
    CHECK_FALSE(tsumoContext(state).firstDraw);
    state.anyCall = false;
    state.players[1].river.push_back({tile("9m")});
    CHECK_FALSE(tsumoContext(state).firstDraw);
}

