// Draws the table from the engine's steps. Seats are placed relative to the
// viewing seat: position 0 is the bottom, then in turn order 1 right, 2 top, 3 left.

const WIND_NAMES = { East: "東", South: "南", West: "西", North: "北" };
const RANK_NAMES = ["1st", "2nd", "3rd", "4th"];
const MAX_DORA_INDICATORS = 5;
// A river has rows of 6 tiles; the third row takes all the rest.
const RIVER_ROW_LENGTH = 6;
const RIVER_ROWS = 3;

const seatElements = [...document.querySelectorAll("#board .seat")];  // by position
const center = {
    roundWind: document.getElementById("round-wind"),
    counters: document.getElementById("counters"),
    wall: document.getElementById("wall"),
    dora: document.getElementById("dora"),
};

// How long a call stays announced at the caller's seat.
const ANNOUNCE_MILLISECONDS = 1200;

// What the page remembers between steps, because a view alone does not say it.
const justDrew = [false, false, false, false];  // by seat: the last tile of the hand was just drawn
const revealed = [null, null, null, null];     // by seat: tiles shown face up at the end of a round
let furiten = false;
let latestDiscarder = null;  // the seat whose newest river tile is the latest discard
let lastView = null;
const announceTimers = [];   // by position

// Shows one step: remembers what the event says, then draws the view.
export function showStep(step) {
    remember(step.event, step.view);
    render(step.view);
    announce(step.event, step.view.seat);
}

function remember(event, view) {
    switch (event.type) {
        case "RoundStart":
            justDrew.fill(false);
            revealed.fill(null);
            latestDiscarder = null;
            break;
        case "Draw":
            justDrew[event.seat] = true;
            break;
        case "Discard":
            justDrew[event.seat] = false;
            latestDiscarder = event.seat;
            break;
        case "Chi":
        case "Pon":
        case "Kan":
            justDrew[event.seat] = false;
            break;
        case "Tsumo":
        case "Ron":
        case "Abort":
        case "ExhaustiveDraw":
            reveal(event);
            break;
    }
    lastView = view;
    // A hand that has just drawn or called has no waits: keep the value until its discard.
    if (view.hand.length % 3 === 1) furiten = view.furiten;
}

// The hands an event shows: winners, tenpai hands, or the hand of nine
// terminals. A ron winner's hand gets the ron tile, set apart like a drawn tile.
function reveal(event) {
    event.revealedHands.forEach((hand, seat) => {
        if (!hand) return;
        revealed[seat] = [...hand.concealed];
        if (event.type === "Ron" && event.wins.some((win) => win.seat === seat)) {
            revealed[seat].push(event.tile);
            justDrew[seat] = true;
        }
    });
}

// A call is announced at the caller's seat for a moment.
function announce(event, viewSeat) {
    const calls = [];
    if (["Chi", "Pon", "Kan", "Tsumo"].includes(event.type)) calls.push({ seat: event.seat, text: event.type });
    if (event.type === "Discard" && event.riichiDiscard) calls.push({ seat: event.seat, text: "Riichi" });
    if (event.type === "Ron") calls.push(...event.wins.map((win) => ({ seat: win.seat, text: "Ron" })));

    for (const { seat, text } of calls) {
        const position = (seat - viewSeat + 4) % 4;
        const element = seatElements[position].querySelector(".announce");
        element.textContent = text;
        element.hidden = false;
        clearTimeout(announceTimers[position]);
        announceTimers[position] = setTimeout(() => { element.hidden = true; }, ANNOUNCE_MILLISECONDS);
    }
}

function render(view) {
    const ranks = placements(view.seats);
    view.seats.forEach((seat, index) => {
        const element = seatElements[(index - view.seat + 4) % 4];
        renderInfo(element.querySelector(".info"), seat, ranks[index], index === view.currentSeat);

        const tiles = revealed[index]
            ?? (index === view.seat ? view.hand : Array(seat.concealedCount).fill(null));
        renderHand(element.querySelector(".hand"), tiles, justDrew[index]);
        renderRiver(element.querySelector(".river"), seat.river, index === latestDiscarder);
        element.querySelector(".melds").replaceChildren(...seat.melds.map((meld) => meldElement(meld, index)));
    });
    seatElements[0].querySelector(".furiten").hidden = !furiten;

    center.roundWind.textContent = `${WIND_NAMES[view.roundWind]} ${view.dealer + 1}`;
    center.counters.textContent = `Honba ${view.honba} · Riichi sticks ${view.riichiSticks}`;
    center.wall.textContent = `Wall ${view.wallRemaining}`;
    const hidden = Array(MAX_DORA_INDICATORS - view.doraIndicators.length).fill(null);
    center.dora.replaceChildren(...[...view.doraIndicators, ...hidden].map(tileImage));
}

// Lets the human discard by clicking a tile of their hand. The tiles named in
// `tiles` rise on hover, and a click calls onDiscard with the tile's name.
// Lasts until the hand is drawn again by the next step.
export function enableDiscards(tiles, onDiscard) {
    for (const image of seatElements[0].querySelectorAll(".hand .tile")) {
        if (!tiles.includes(image.dataset.tile)) continue;
        image.classList.add("discardable");
        image.addEventListener("click", () => onDiscard(image.dataset.tile));
    }
}

// Ends enableDiscards: the tiles no longer rise or react to clicks.
export function disableDiscards() {
    for (const image of seatElements[0].querySelectorAll(".hand .discardable")) {
        image.replaceWith(tileImage(image.dataset.tile));
    }
}

function renderInfo(element, seat, rank, current) {
    element.querySelector(".stick").hidden = !seat.riichi;
    element.querySelector(".wind").textContent = WIND_NAMES[seat.seatWind];
    element.querySelector(".score").textContent = seat.score;
    element.querySelector(".rank").textContent = RANK_NAMES[rank];
    element.classList.toggle("current", current);
}

// `tiles` are tile names, or null for tiles turned face down. A drawn tile
// stays last and apart; the others are sorted.
function renderHand(element, tiles, drew) {
    const drawn = drew && tiles.length % 3 === 2 ? tiles.at(-1) : undefined;
    const rest = drawn === undefined ? tiles : tiles.slice(0, -1);

    const images = sortTiles(rest).map(tileImage);
    if (drawn !== undefined) {
        const image = tileImage(drawn);
        image.classList.add("drawn");
        images.push(image);
    }
    element.replaceChildren(...images);
}

// Every tile image, by tile name ("tileBack" for the back), filled by loadTiles.
const tileImages = new Map();

// Fetches all tile images at once. Resolves when they can be shown, and
// rejects if one could not be loaded.
export async function loadTiles() {
    const names = ["tileBack"];
    for (const suit of "mps") {
        for (let number = 0; number <= 9; ++number) names.push(`${number}${suit}`);
    }
    for (let number = 1; number <= 7; ++number) names.push(`${number}z`);

    await Promise.all(names.map((name) => {
        const image = new Image();
        image.src = `tiles/pngs/${name}.png`;
        image.className = "tile";
        image.dataset.tile = name;
        image.alt = name === "tileBack" ? "hidden tile" : name;
        tileImages.set(name, image);
        return image.decode();
    }));
}

// Tiles taken by a call are left out. The riichi tile lies sideways; if it was
// called, the next tile lies sideways instead. With `latest`, the newest tile
// is the latest discard of the round and is highlighted.
function renderRiver(element, river, latest) {
    const tiles = [];
    let sidewaysNext = false;
    for (const [index, discard] of river.entries()) {
        if (discard.calledAway) {
            if (discard.riichi) sidewaysNext = true;
            continue;
        }
        const image = tileImage(discard.tile);
        if (latest && index === river.length - 1) image.classList.add("latest");
        if (discard.riichi || sidewaysNext) {
            tiles.push(sideways(image));
            sidewaysNext = false;
        } else {
            tiles.push(image);
        }
    }

    const rows = [];
    for (let row = 0; row < RIVER_ROWS; ++row) {
        const last = row === RIVER_ROWS - 1;
        const start = row * RIVER_ROW_LENGTH;
        const rowTiles = last ? tiles.slice(start) : tiles.slice(start, start + RIVER_ROW_LENGTH);
        if (rowTiles.length === 0) break;
        const rowElement = document.createElement("div");
        rowElement.className = "river-row";
        rowElement.replaceChildren(...rowTiles);
        rows.push(rowElement);
    }
    element.replaceChildren(...rows);
}

// Wraps a tile image in a slot as wide as the tile is high, in which it lies
// turned by 90 degrees. Turning the image alone would not change the space it takes.
function sideways(image) {
    const slot = document.createElement("span");
    slot.className = "sideways";
    slot.append(image);
    return slot;
}

// Two sideways tiles on top of each other, `upper` further from the player.
function stacked(lower, upper) {
    const slot = sideways(upper);
    slot.classList.add("stacked");
    slot.append(lower);
    return slot;
}

// An <img> of a tile, or of a tile back for null. A copy of the loaded image,
// so the browser takes it from memory instead of fetching it again.
export function tileImage(tile) {
    return tileImages.get(tile ?? "tileBack").cloneNode();
}

// Man, pin, sou, honors; a red five right after the normal fives.
function sortTiles(tiles) {
    const key = (tile) => {
        if (tile === null) return 0;
        const number = tile[0] === "0" ? 5.5 : Number(tile[0]);
        return "mpsz".indexOf(tile[1]) * 10 + number;
    };
    return [...tiles].sort((a, b) => key(a) - key(b));
}

// The meld an action would make, drawn like the melds on the table, or null
// for an action that makes none. For the options of a call.
export function actionMeld(action) {
    const owner = lastView.seat;
    switch (action.type) {
        case "Chi":
        case "Pon":
        case "OpenKan": {
            // While the others react to a discard, the discarder stays the current seat.
            const meld = { type: action.type, handTiles: action.handTiles, calledTile: action.tile,
                           fromSeat: lastView.currentSeat, kanFromPon: false };
            return meldElement(meld, owner);
        }
        case "ClosedKan":
            return meldElement({ type: "ClosedKan", handTiles: action.handTiles }, owner);
        case "KanFromPon": {
            const pon = lastView.seats[owner].melds.find(
                (meld) => meld.type === "Pon" && sameKind(meld.calledTile, action.tile));
            const kan = { ...pon, type: "OpenKan", handTiles: [...pon.handTiles, action.tile], kanFromPon: true };
            return meldElement(kan, owner);
        }
        default:
            return null;
    }
}

// Same tile apart from the red flag: "0m" and "5m" are the same kind.
function sameKind(a, b) {
    return a.replace("0", "5") === b.replace("0", "5");
}

// A meld as it lies on the table. The claimed tile lies sideways on the side
// of the player it came from: left, middle (across) or right. A tile added to
// a pon lies on top of the claimed tile; a closed kan shows its outer tiles face down.
export function meldElement(meld, owner) {
    const element = document.createElement("div");
    element.className = "meld";

    if (meld.type === "ClosedKan") {
        // Sorted, a red five comes last: show the last two face up.
        const [, , third, fourth] = sortTiles(meld.handTiles);
        element.append(tileImage(null), tileImage(third), tileImage(fourth), tileImage(null));
        return element;
    }

    const own = meld.kanFromPon ? meld.handTiles.slice(0, -1) : meld.handTiles;
    const claimed = meld.kanFromPon
        ? stacked(tileImage(meld.calledTile), tileImage(meld.handTiles.at(-1)))
        : sideways(tileImage(meld.calledTile));

    const tiles = sortTiles(own).map(tileImage);
    const from = (meld.fromSeat - owner + 4) % 4;  // 1 right, 2 across, 3 left
    const position = from === 3 ? 0 : from === 2 ? 1 : tiles.length;
    tiles.splice(position, 0, claimed);
    element.append(...tiles);
    return element;
}

// Placement by score. A tie goes to the seat closer to the first dealer
// (seat 0), as in the engine's final result.
export function placements(seats) {
    const order = [0, 1, 2, 3].sort((a, b) => seats[b].score - seats[a].score || a - b);
    const ranks = [];
    order.forEach((seat, rank) => { ranks[seat] = rank; });
    return ranks;
}
