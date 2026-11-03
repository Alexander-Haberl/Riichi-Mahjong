// The result screen after a round, in front of the board: for each win the
// hand, the yaku and dora, han and fu, and the points; otherwise the kind of draw.
import { meldElement, placements, tileImage } from "./table.js";

const SCORE_NAMES = {
    None: "",
    Mangan: "Mangan",
    Haneman: "Haneman",
    Baiman: "Baiman",
    Sanbaiman: "Sanbaiman",
    KazoeYakuman: "Kazoe yakuman",
    Yakuman: "Yakuman",
};

const ABORT_REASONS = {
    KyuushuKyuuhai: "Nine terminals",
    SuufonRenda: "Four winds",
    SuuchaRiichi: "Four riichi",
    Suukaikan: "Four kans",
    Sanchahou: "Three rons",
};

const YAKUMAN_HAN = 13;
const MAX_DORA_INDICATORS = 5;
const RANK_NAMES = ["1st", "2nd", "3rd", "4th"];
// The players' names: the human, and the AI every other seat has.
const HUMAN_NAME = "You";
const AI_NAME = "ShantenAI";

const result = {
    element: document.getElementById("result"),
    content: document.querySelector("#result .content"),
    continue: document.querySelector("#result .continue"),
};
let closed = null;  // resolves the promise of the screen that is open

result.continue.addEventListener("click", hideResult);

// Shows how the round ended: `endEvent` is its Tsumo, Ron, Abort or
// ExhaustiveDraw, `view` the table afterwards. Resolves when the screen is closed.
export function showRoundResult(endEvent, view) {
    return open(sections(endEvent, view), "Continue");
}

// Shows the final placements from the view at the game end: placement, name
// and score of each seat. Resolves when the screen is closed.
export function showGameResult(view) {
    const ranks = placements(view.seats);
    const order = [0, 1, 2, 3].sort((a, b) => ranks[a] - ranks[b]);

    const table = document.createElement("table");
    table.className = "standings";
    for (const seat of order) {
        const row = table.insertRow();
        if (seat === view.seat) row.className = "own";
        row.insertCell().textContent = RANK_NAMES[ranks[seat]];
        row.insertCell().textContent = seat === view.seat ? HUMAN_NAME : AI_NAME;
        row.insertCell().textContent = view.seats[seat].score;
    }
    return open([section(heading("Final scores"), table)], "Close");
}

function open(children, buttonText) {
    hideResult();
    result.content.replaceChildren(...children);
    result.continue.textContent = buttonText;
    result.element.hidden = false;
    return new Promise((resolve) => { closed = resolve; });
}

// Closes the screen, if it is open.
export function hideResult() {
    result.element.hidden = true;
    result.content.replaceChildren();
    closed?.();
    closed = null;
}

function sections(event, view) {
    switch (event.type) {
        case "Tsumo":
        case "Ron":
            return event.wins.map((win) => winSection(win, event, view));
        case "ExhaustiveDraw": {
            const tenpai = event.revealedHands.flatMap((hand, seat) => (hand ? [name(seat, view)] : []));
            return [section(heading("Exhaustive draw"),
                            text(tenpai.length > 0 ? `Tenpai: ${tenpai.join(", ")}` : "Nobody is tenpai"))];
        }
        case "Abort":
            return [section(heading("Abortive draw"), text(ABORT_REASONS[event.abortReason]))];
        default:
            return [];
    }
}

function winSection(win, event, view) {
    const title = event.type === "Tsumo"
        ? `Tsumo: ${name(win.seat, view)}`
        : `Ron: ${name(win.seat, view)} from ${name(win.fromSeat, view).toLowerCase()}`;

    const yaku = document.createElement("table");
    for (const source of win.yaku) {
        const row = yaku.insertRow();
        row.insertCell().textContent = source.name;
        row.insertCell().textContent = hanText(source.han);
    }

    // Ura dora only count for a winner in riichi.
    const ura = view.seats[win.seat].riichi ? event.uraDoraIndicators : null;
    return section(heading(title), handElement(win), indicatorsElement(view.doraIndicators, ura), yaku,
                   text(totalText(win), "total"), text(`${win.points} points`, "points"));
}

// The complete blocks, then the melds, then the block the winning tile
// completed, and the winning tile a little apart.
function handElement(win) {
    const block = (tiles) => {
        const element = document.createElement("div");
        element.className = "block";
        element.append(...tiles.map(tileImage));
        return element;
    };
    const complete = win.concealedBlocks.slice(0, -1).map(block);
    const melds = win.melds.map((meld) => meldElement(meld, win.seat));
    const incomplete = block(win.concealedBlocks.at(-1) ?? []);
    const winning = tileImage(win.winningTile);
    winning.classList.add("winning");

    const hand = document.createElement("div");
    hand.className = "result-hand";
    hand.append(...complete, ...melds, incomplete, winning);
    return hand;
}

// The dora indicators, and the ura dora indicators when they count: each a
// row of five tiles, the indicators face up and the rest face down, with a
// label on the left.
function indicatorsElement(dora, ura) {
    const element = document.createElement("div");
    element.className = "indicators";
    const row = (label, indicators) => {
        const tiles = document.createElement("div");
        tiles.className = "block";
        const hidden = Array(MAX_DORA_INDICATORS - indicators.length).fill(null);
        tiles.append(...[...indicators, ...hidden].map(tileImage));
        element.append(text(label, "label"), tiles);
    };
    row("Dora", dora);
    if (ura) row("Ura dora", ura);
    return element;
}

function hanText(han) {
    if (han < YAKUMAN_HAN) return `${han} han`;
    return han === YAKUMAN_HAN ? "Yakuman" : `${han / YAKUMAN_HAN}× yakuman`;
}

// "3 han 40 fu", "4 han 30 fu Mangan", or "Yakuman" (fu do not count there).
function totalText(win) {
    if (win.name === "Yakuman") return hanText(win.han);
    return [`${win.han} han`, `${win.fu} fu`, SCORE_NAMES[win.name]].filter(Boolean).join(" ");
}

function name(seat, view) {
    return seat === view.seat ? "You" : view.seats[seat].seatWind;
}

function section(...children) {
    const element = document.createElement("section");
    element.append(...children);
    return element;
}

function heading(content) {
    const element = document.createElement("h2");
    element.textContent = content;
    return element;
}

function text(content, className = "") {
    const element = document.createElement("div");
    element.textContent = content;
    element.className = className;
    return element;
}
