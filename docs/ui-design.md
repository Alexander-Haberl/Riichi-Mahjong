# UI design

How the browser UI will work and how it talks to the engine. The engine side is done; the UI is not started.

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

## Still to do

1. Emscripten build of the engine (`emcmake cmake`). It needs **`-fexceptions`**, because catching C++ exceptions is off by default and the engine's `std::logic_error`s would otherwise abort the page.
2. A thin Embind layer that gives JavaScript `Game`, plain step and decision objects, and turns a chosen action back into an `Action`. `std::optional`, `std::array` and `Tile` probably need wrappers, or the step can cross as JSON.
3. The UI: table layout, tile images, animations, hand selection, call and riichi prompts, round and game result screens. Tile images will be sorted out by the user, ask about them if they are needed.
4. A fixed `RuleSet` for the game. The seed comes from the browser and should be shown, so a game can be replayed.

The AI is `ShantenPlayer` (`src/players/`), which is temporary and fast enough for the page's main thread. A stronger AI that thinks for seconds would need a Web Worker, because `submit()` would otherwise freeze the page.
