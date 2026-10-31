// A plain test page: it plays through the engine and shows everything as text.
import createEngine from "./engine.js";

const page = {
    status: document.getElementById("status"),
    form: document.getElementById("new-game"),
    seed: document.getElementById("seed"),
    table: document.getElementById("table"),
    round: document.getElementById("round"),
    scores: document.getElementById("scores"),
    hand: document.getElementById("hand"),
    furiten: document.getElementById("furiten"),
    actions: document.getElementById("actions"),
    log: document.getElementById("log"),
};

const engine = await createEngine();
let game = null;
let furiten = false;

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

function showTable(view) {
    const wind = view.roundWind;
    page.round.textContent = `${wind} round, dealer seat ${view.dealer}, honba ${view.honba}, `
        + `sticks ${view.riichiSticks}, wall ${view.wallRemaining}, dora indicators ${view.doraIndicators.join(" ")}`;
    page.scores.textContent = view.seats
        .map((seat, index) => `${index === view.seat ? "you" : `seat ${index}`} (${seat.seatWind}): ${seat.score}`)
        .join(" | ");
    page.hand.textContent = view.hand.join(" ");
    // A hand that has just drawn or called has no waits: keep the value until its discard.
    if (view.hand.length % 3 === 1) furiten = view.furiten;
    page.furiten.hidden = !furiten;
}

function describeAction(action) {
    return [action.type, action.tile, ...action.handTiles].filter(Boolean).join(" ");
}

function showActions(decision) {
    page.actions.replaceChildren(...decision.legal.map((action, index) => {
        const button = document.createElement("button");
        button.textContent = describeAction(action);
        button.addEventListener("click", () => {
            game.submit(index);
            update();
        });
        return button;
    }));
}

// Shows everything that happened, then the next decision or the result.
function update() {
    for (const step of JSON.parse(game.takeSteps())) {
        log(describe(step.event));
        showTable(step.view);
    }

    const decision = JSON.parse(game.pending());
    if (decision) {
        showActions(decision);
    } else {
        page.actions.replaceChildren();
        log(`Result: ${game.result()}`);
    }
}

function newGame(seed) {
    game?.delete();
    game = new engine.WebGame(seed);
    page.seed.value = seed;
    page.status.textContent = `Seed ${seed}`;
    page.log.textContent = "";
    page.table.hidden = false;
    game.start();
    update();
}

page.form.addEventListener("submit", (event) => {
    event.preventDefault();
    newGame(page.seed.value === "" ? randomSeed() : Number(page.seed.value));
});

page.status.textContent = "Engine ready.";
page.form.querySelector("button").disabled = false;
