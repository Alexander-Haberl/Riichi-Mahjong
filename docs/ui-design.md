# UI design

How the browser UI works and how it talks to the engine. The engine and the interface to the page are done; the page itself is only a plain text test page so far.

## Running it

`./serve.sh [port]` builds into `build-web` and serves `build-web/site` on `http://127.0.0.1:8000`, reachable from this machine only. It needs Emscripten: either `emcmake` on the `PATH` (after `source <emsdk>/emsdk_env.sh`), or `EMSDK_DIR` set to the emsdk folder, e.g. `EMSDK_DIR=~/emsdk ./serve.sh`. The server tells the browser not to cache, so after a rebuild a normal reload shows the new version. After a change, run `./serve.sh` again, or `cmake --build build-web` in a shell with the emsdk active, and reload the page.

The page's sources are in `web/`, and the build copies them next to the generated `engine.js` and `engine.wasm`.

## Decisions

- **Browser UI, no server.** The C++ engine is compiled to WebAssembly with Emscripten and runs inside the page. The UI is HTML, CSS and JavaScript. Hosting is static files. For local testing it still needs a small web server, because browsers usually refuse to load WebAssembly from `file://`.
- **Step-based engine.** `Game` and `RoundController` never block. They run until a human has to decide, then return. This is why the browser needs neither Asyncify nor threads. The older blocking version is the git tag `blocking-controller`.
- **The UI has no game logic.** Everything it shows comes from the engine, and all rules stay in C++.
- **One human, three AIs.** A `nullptr` in the players array marks a human seat. `Game` answers the AIs itself, so the UI only deals with the human. AIs answer inside `submit()`, so nothing shows when an AI passes on a call.

## How the UI talks to the engine

```js
game.start();
while (true) {
    for (const step of game.takeSteps()) { await animate(step.event); table = step.view; }
    const decision = game.pending();
    if (!decision) break;                      // game over: show game.result()
    game.submit(await humanChoice(decision));
}
```

The API (`src/game/Game.hpp`):

| Call | Meaning |
|---|---|
| `Game(players, scorer, seed, viewerPlayer)` | `viewerPlayer` is the player whose view the steps show |
| `start()` | runs to the first human decision |
| `takeSteps()` | everything that happened since the last call, in order |
| `pending()` | the human's `Decision` (seat and legal actions); empty when the game is over |
| `submit(action)` | answers it, with an action from `legal`, and runs to the next one |
| `result()` | final scores and ranks, by player |

The scorer comes from `standardScorer(rules)`. `Game` owns it and can't be copied or moved, so keep it in one place.

## Steps

A `GameStep` (`src/game/GameStep.hpp`) is `{GameEvent event; PlayerView view}`: **play `event`, then show `view`.**

- `view` is a copy of the table right after the event, as the viewing seat sees it. The UI keeps only the latest one. The tile that is moving has to be hidden during its animation, because the old view still shows it.
- `event` only carries what the view doesn't have: the tile that moved, the meld, and information that becomes public at that moment (`wins`, `revealedHands`, `uraDoraIndicators`, `scoreChanges`).
- Events: `RoundStart, Draw, Discard, Chi, Pon, Kan, DoraRevealed, Tsumo, Ron, Abort, ExhaustiveDraw, RoundEnd, GameEnd`.
- Details: a riichi is a `Discard` with `riichiDiscard`. Its stick simply appears in the next view. `Ron` is one event for all winners (`wins` has one entry each), with `seat` being the discarder. A `Draw` by another player has no `tile`.
- `RoundEnd` is the place for the score screen. The game has already moved on internally, so the UI can simply wait for a click before playing the next step.
- During a round, read scores from the views: `Game::scores()` only changes between rounds.

## JSON between engine and page

`src/web/WebGame.hpp` is the class JavaScript sees. Steps, decisions and the result leave C++ as JSON strings, and the answer comes back as an index into `legal`. So the page never builds an `Action`, Embind only has to expose one class, and everything except `Bindings.cpp` can be tested natively with Catch2.

Tiles are Tenhou strings (`"5m"`, `"0p"`, `"7z"`), which can also name the tile images. Enums are their names, and an empty optional is `null`. Every event has all fields. Seats are absolute: the UI places them around the table relative to `view.seat`.

```js
// Step
{
  event: {
    type: "Discard",                  // an EventType name
    seat: 2,
    tile: "5m",                       // or null
    riichiDiscard: false,
    meld: Meld,                       // or null
    wins: [{ seat: 1, fromSeat: 2, han: 4, fu: 30, basicPoints: 1920,
             name: "None",            // a ScoreName
             yaku: [{ name: "Riichi", han: 1 }],
             points: 7700,            // gained from the hand alone, without honba and riichi sticks
             concealedBlocks: [["1m", "2m", "3m"], ..., ["4p", "5p"]],  // the last lacks the winning tile
             melds: [Meld],
             winningTile: "6p" }],
    revealedHands: [null, { concealed: ["1m", ...], melds: [Meld] }, null, null],  // by seat
    uraDoraIndicators: ["3p"],
    abortReason: "SuufonRenda",       // or null
    nagashi: [false, true, false, false],  // ExhaustiveDraw: nagashi mangan, by seat
    scoreChanges: [-3900, 3900, 0, 0]
  },
  view: {
    seat: 0,
    hand: ["1m", ...],                // in engine order: a drawn tile is last
    furiten: false,                   // own seat only, see below
    roundWind: "East", dealer: 0, honba: 0, riichiSticks: 0,
    currentSeat: 2, wallRemaining: 69, doraIndicators: ["2s"],
    seats: [{                         // by seat
      river: [{ tile: "9p", tsumogiri: false, riichi: false, calledAway: false }],
      melds: [Meld],
      concealedCount: 13, score: 25000, riichi: false, seatWind: "East"
    }, ...]
  }
}

// Meld
{ type: "Pon", handTiles: ["5m", "0m"], calledTile: "5m", fromSeat: 3, kanFromPon: false }

// Decision: submit(i) answers with legal[i]. Pass and KyuushuKyuuhai have tile: null
{ seat: 0, legal: [{ type: "Riichi", tile: "9p", handTiles: [], tsumogiri: true }, ...] }
// tsumogiri: the drawn tile; with an identical copy in the hand, a second action discards the copy

// Result: by player, the human is player 0
{ scores: [31000, 25000, 24000, 20000], ranks: [1, 2, 3, 4], riichiSticksLeft: 0 }
```

`furiten` is only meaningful for a hand with waits. After a draw or call (`hand.length % 3 == 2`) it only reports a ron passed on, so a marker should keep its value until the discard.

## The page so far

`web/`: `main.js` runs the game, `table.js` draws the board from the steps, `actions.js` offers the human's choices, `results.js` shows the result of a round. The board shows the hands (the human's face up), rivers with sideways riichi tiles, melds at the right end of each seat, and the middle square with winds, scores, placements, riichi sticks, round, honba, wall and dora. Discards are clicks on the hand; calls are buttons above the hand, and a call with several options opens a chooser in front of the board. Tile images are in `web/tiles/pngs/`, named by Tenhou string, `tileBack.png` for the back.

## Test setups

To reach rare situations on purpose, a game can start from a scenario (`src/web/Scenarios.cpp`), chosen in the "Setup" select. A scenario gives the 13-tile hands and the first draws by position (you, right, across, left); `scenarioWallStart` turns them into the start of the first round's wall for the human's seat (`Wall::stacked`, `Game::stackFirstWall`). Later rounds are random. The scenarios are written for `CallingPlayer` opponents (`src/players/`), which take every call and otherwise discard what they draw, so their draws are their discards; choosing a scenario switches the "Opponents" select to them. A normal game with CallingAI opponents shows many melds.

## Still to do

Done: pacing (`play()` in `main.js` pauses before each step, clicks are ignored meanwhile), the latest discard highlighted, calls announced at the caller's seat, the winning or tenpai hands revealed on the board, the round result screen (`results.js`), which waits for a click, the final scores at the game end, non-discardable hand tiles grayed out, and the options left of the board (`options.js`): gray out tsumogiri discards, auto-call win, auto-pass on chi, pon and open kan (ron always asks), and auto-discard in riichi when the drawn tile is the only discard.

The rules are `webRules()` in `src/web/WebGame.hpp`, the standard rules for now. The seed comes from the browser and is shown, so a game can be replayed.

The AI is `ShantenPlayer` (`src/ai/`), which is temporary and fast enough for the page's main thread. A stronger AI that thinks for seconds would need a Web Worker, because `submit()` would otherwise freeze the page. Replacing the AI also means changing its name: the final scores screen calls the AIs "ShantenAI", a fixed string (`AI_NAME` in `web/results.js`). Better then: `WebGame` sends each seat's name in the JSON.
