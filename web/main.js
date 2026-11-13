// Runs the game: passes the engine's steps to the table and the human's
// choices back to the engine.
import createEngine from "./engine.js";
import { clearActions, showActions } from "./actions.js";
import { autoChoice } from "./options.js";
import { hideResult, showGameResult, showRoundResult } from "./results.js";
import { loadTiles, showStep } from "./table.js";

const page = {
    status: document.getElementById("status"),
    form: document.getElementById("new-game"),
    seed: document.getElementById("seed"),
    table: document.getElementById("table"),
    log: document.getElementById("log"),
};

// The engine and all tile images load together, before a game can start.
let engine;
try {
    [engine] = await Promise.all([createEngine(), loadTiles()]);
} catch (error) {
    page.status.textContent = `Loading failed: ${error.message}`;
    throw error;
}
let game = null;

function randomSeed() {
    return crypto.getRandomValues(new Uint32Array(1))[0];
}

function log(line) {
    page.log.textContent += line + "\n";
    page.log.scrollTop = page.log.scrollHeight;
}

function describe(event) {
    const parts = [event.type];
    if (event.type !== "RoundEnd" && event.type !== "GameEnd") parts.push(`seat ${event.seat}`);
    if (event.tile) parts.push(event.tile);
    if (event.riichiDiscard) parts.push("(riichi)");
    if (event.meld) parts.push(event.meld.type);
    if (event.abortReason) parts.push(event.abortReason);
    for (const win of event.wins) {
        const yaku = win.yaku.map((source) => `${source.name} ${source.han}`).join(", ");
        parts.push(`seat ${win.seat}: ${win.han} han ${win.fu} fu [${yaku}]`);
    }
    if (event.type === "RoundEnd") parts.push(event.scoreChanges.join(" "));
    return parts.join(" ");
}

// Pause in milliseconds before a step is shown, so the AIs' turns can be
// followed. The human's own actions show at once.
const PAUSE = {
    Draw: 350,            // between one turn and the next
    Discard: 450,         // an AI thinking
    call: 450,            // Chi, Pon, Kan
    DoraRevealed: 300,
    roundEnd: 400,        // Tsumo, Ron, Abort, ExhaustiveDraw
    RoundEnd: 3000,       // time to look at the revealed hands before the result screen
    auto: 400,            // before an action the options choose for the human
};

const ROUND_ENDINGS = ["Tsumo", "Ron", "Abort", "ExhaustiveDraw"];

function pauseBefore(event, humanSeat) {
    const ownAction = event.seat === humanSeat;
    switch (event.type) {
        case "Draw": return PAUSE.Draw;
        case "Discard": return ownAction ? 0 : PAUSE.Discard;
        case "Chi":
        case "Pon":
        case "Kan": return ownAction ? 0 : PAUSE.call;
        case "DoraRevealed": return PAUSE.DoraRevealed;
        case "Tsumo":
        case "Ron":
        case "Abort":
        case "ExhaustiveDraw": return PAUSE.roundEnd;
        case "RoundEnd": return PAUSE.RoundEnd;
        default: return 0;
    }
}

function sleep(milliseconds) {
    return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

// True while steps are being played; the human's clicks are ignored then.
let playing = false;
let lastView = null;  // the table after the latest step, for the options

function choose(actionIndex) {
    if (playing) return;
    game.submit(actionIndex);
    play();
}

// Plays the steps since the last call one by one, then offers the next
// decision or shows the result. A round's result screen waits for a click.
async function play() {
    const current = game;
    playing = true;
    clearActions();
    let roundEnding = null;  // how the round ended, for the result screen at RoundEnd
    for (const step of JSON.parse(game.takeSteps())) {
        const pause = pauseBefore(step.event, step.view.seat);
        if (pause > 0) await sleep(pause);
        if (game !== current) return;  // a new game was started meanwhile
        log(describe(step.event));
        showStep(step);
        lastView = step.view;

        if (ROUND_ENDINGS.includes(step.event.type)) roundEnding = step.event;
        if (step.event.type === "RoundEnd" && roundEnding) {
            await showRoundResult(roundEnding, step.view);
            if (game !== current) return;
        }
        if (step.event.type === "GameEnd") showGameResult(step.view);
    }
    playing = false;

    const decision = JSON.parse(game.pending());
    if (decision) {
        const auto = autoChoice(decision, lastView);
        if (auto === null) {
            showActions(decision, choose);
            return;
        }
        await sleep(PAUSE.auto);
        if (game !== current) return;
        choose(auto);
    } else {
        log(`Result: ${game.result()}`);
    }
}

function newGame(seed) {
    hideResult();
    game?.delete();
    game = new engine.WebGame(seed);
    page.seed.value = seed;
    page.status.textContent = `Seed ${seed}`;
    page.log.textContent = "";
    page.table.hidden = false;
    game.start();
    play();
}

page.form.addEventListener("submit", (event) => {
    event.preventDefault();
    newGame(page.seed.value === "" ? randomSeed() : Number(page.seed.value));
});

page.status.textContent = "Ready.";
page.form.querySelector("button").disabled = false;
