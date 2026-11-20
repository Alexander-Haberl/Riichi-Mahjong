#pragma once

namespace mj {

enum class GameLength {
    Tonpuusen,  // East round only
    Hanchan,    // East and South rounds
};

// Rule options that differ between rule sets. Defaults follow common online rules.
struct RuleSet {
    int startingPoints = 25000;
    int redFivesPerSuit = 1;

    // Game flow
    GameLength gameLength = GameLength::Hanchan;
    // The dealer keeps the seat after an exhaustive draw in which they are tenpai.
    bool renchanOnDealerTenpai = true;
    // The dealer keeps the seat after an abortive draw.
    bool renchanOnAbort = true;
    // The game ends as soon as a player has fewer than 0 points (tobi).
    bool endOnBust = true;

    // Scoring
    // 13 or more han from regular yaku and dora count as a yakuman (kazoe
    // yakuman); otherwise they are sanbaiman.
    bool kazoeYakuman = true;
    // Yakuman that some rule sets count double (e.g. suuankou tanki, 13-sided
    // kokushi) are worth two; otherwise every yakuman is worth one.
    bool doubleYakuman = true;
    // Several yakuman in one hand add up; otherwise only the highest counts.
    bool stackYakuman = true;
    // 4 han 30 fu and 3 han 60 fu (1920 basic points) are rounded up to mangan.
    bool kiriageMangan = false;
    // Tanyao also counts on an open hand (kuitan).
    bool kuitan = true;

    // Allow discarding the called kind (or the other end of a chi) right after a call.
    bool allowKuikae = false;
    // Riichi needs at least 1000 points.
    bool riichiNeeds1000Points = true;
    // Several players can ron the same tile; otherwise only the first in turn order wins.
    bool multipleRon = true;
    // Three rons on the same tile abort the round (sanchahou), whatever multipleRon says.
    bool tripleRonAborts = true;
    // Nagashi mangan: at an exhaustive draw, a player whose discards are all
    // terminals and honors, none of them called, is paid a mangan tsumo
    // instead of the tenpai payments. Honba and riichi sticks stay on the table.
    bool nagashiMangan = true;

    // Abortive draws
    // Nine kinds of terminals and honors on the first turn (the player chooses).
    bool kyuushuKyuuhai = true;
    // All four first discards are the same wind, with no call in between.
    bool suufonRenda = true;
    // All four players are in riichi.
    bool suuchaRiichi = true;
    // A fourth kan by more than one player; the round ends after the next discard.
    bool suukaikan = true;
};

}  // namespace mj
