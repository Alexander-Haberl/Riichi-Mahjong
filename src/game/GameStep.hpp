#pragma once

#include <array>
#include <optional>
#include <vector>

#include "core/Hand.hpp"
#include "core/Tile.hpp"
#include "game/Action.hpp"
#include "game/Player.hpp"
#include "rules/Scorer.hpp"

// What the engine hands to a UI: one GameStep per thing that happened, and a
// Decision when it needs an answer before it can go on.
namespace mj {

enum class AbortReason {
    KyuushuKyuuhai,  // nine terminals and honors
    SuufonRenda,     // four identical winds
    SuuchaRiichi,    // four riichi
    Suukaikan,       // four kans by more than one player
    Sanchahou,       // three rons
};

struct Win {
    int seat;
    int fromSeat;  // the discarder; equal to seat for tsumo
    ScoreResult score;
    int points = 0;  // what the winner gains from the hand alone, without honba and riichi sticks
};

enum class EventType {
    RoundStart,      // hands dealt; the view shows round wind, dealer and honba
    Draw,            // also the replacement draw after a kan
    Discard,
    Chi,
    Pon,
    Kan,             // closed, open or added to a pon: see meld
    DoraRevealed,
    Tsumo,
    Ron,             // one event for all winners of a tile
    Abort,
    ExhaustiveDraw,
    RoundEnd,        // the payments of the round
    GameEnd,
};

// What just happened. Which fields are set depends on the type. Most things
// the UI shows come from the view of the step; an event only carries what the
// view does not have, like the tile that moved or hands that become public.
struct GameEvent {
    EventType type;
    // Who acted; for Ron the discarder (the winners are in `wins`). Unused for
    // RoundStart, ExhaustiveDraw, RoundEnd and GameEnd.
    int seat = 0;

    std::optional<Tile> tile{};  // the tile that moved. Draw: empty for other players' draws
    bool riichiDiscard = false;  // Discard: this tile declares riichi
    std::optional<Meld> meld{};  // Chi, Pon, Kan

    std::vector<Win> wins{};                             // Tsumo, Ron
    std::array<std::optional<Hand>, 4> revealedHands{};  // hands shown at the end of a round
    std::vector<Tile> uraDoraIndicators{};               // Tsumo, Ron: for a riichi win
    std::optional<AbortReason> abortReason{};            // Abort
    std::array<int, 4> scoreChanges{};                   // RoundEnd: by seat
};

// One thing for the UI to show: play `event`, then show `view`.
struct GameStep {
    GameEvent event;
    PlayerView view;  // the table right after this event, as the viewing seat sees it
};

// The engine waits for this seat to pick one of `legal`.
struct Decision {
    int seat;
    std::vector<Action> legal;
};

}  // namespace mj
