// The options left of the board. Each one takes effect at once, also during a game.

const board = document.getElementById("board");
const option = {
    grayTsumogiri: document.getElementById("gray-tsumogiri"),
    autoWin: document.getElementById("auto-win"),
    autoPass: document.getElementById("auto-pass"),
    autoDiscard: document.getElementById("auto-discard"),
};

// Tsumogiri tiles in the rivers are grayed out through a class on the board,
// so switching it changes the tiles already on the table too.
function applyGrayTsumogiri() {
    board.classList.toggle("gray-tsumogiri", option.grayTsumogiri.checked);
}

option.grayTsumogiri.addEventListener("change", applyGrayTsumogiri);
applyGrayTsumogiri();

// Calls on a discard that auto-pass declines; a Ron always asks.
const CALLS_ON_DISCARD = ["Chi", "Pon", "OpenKan"];

// The index of the action the options take for the human in this decision, or
// null to ask. `view` is the table at the decision. In this order:
// - auto-win: Tsumo or Ron, whatever else is offered;
// - auto-pass: Pass, when the only other options are chi, pon or open kan;
// - auto-discard: the discard, in riichi, when it is the only action.
export function autoChoice(decision, view) {
    const legal = decision.legal;
    const indexOf = (types) => legal.findIndex((action) => types.includes(action.type));

    if (option.autoWin.checked) {
        const win = indexOf(["Tsumo", "Ron"]);
        if (win >= 0) return win;
    }
    if (option.autoPass.checked) {
        const pass = indexOf(["Pass"]);
        const onlyCalls = legal.every((action) => action.type === "Pass" || CALLS_ON_DISCARD.includes(action.type));
        if (pass >= 0 && onlyCalls) return pass;
    }
    if (option.autoDiscard.checked) {
        const inRiichi = view?.seats[decision.seat].riichi;
        if (inRiichi && legal.length === 1 && legal[0].type === "Discard") return 0;
    }
    return null;
}
