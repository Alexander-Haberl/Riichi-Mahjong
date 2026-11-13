// The human's choices: discards are clicks on the hand's tiles, every other
// action is a call button above the hand. A call with several options opens
// the chooser in front of the board, which shows each option as its tiles.
import { actionMeld, disableDiscards, enableDiscards, tileImage } from "./table.js";

// One button per kind of call, in this order; all kinds of kan share one.
const CALLS = [
    { label: "Tsumo", types: ["Tsumo"] },
    { label: "Ron", types: ["Ron"] },
    { label: "Riichi", types: ["Riichi"] },
    { label: "Kan", types: ["ClosedKan", "KanFromPon", "OpenKan"] },
    { label: "Pon", types: ["Pon"] },
    { label: "Chi", types: ["Chi"] },
    { label: "Nine terminals", types: ["KyuushuKyuuhai"] },
    { label: "Pass", types: ["Pass"] },
];

const callButtons = document.getElementById("calls");
const chooser = {
    element: document.getElementById("chooser"),
    title: document.querySelector("#chooser h2"),
    options: document.querySelector("#chooser .options"),
    back: document.querySelector("#chooser .back"),
};

chooser.back.addEventListener("click", closeChooser);

// Offers the actions of a decision. onChoose gets the index of the chosen action.
export function showActions(decision, onChoose) {
    closeChooser();
    const actions = decision.legal.map((action, index) => ({ action, index }));

    enableDiscards(actions.filter(({ action }) => action.type === "Discard"), onChoose);

    const buttons = [];
    for (const { label, types } of CALLS) {
        const options = actions.filter(({ action }) => types.includes(action.type));
        if (options.length === 0) continue;

        const button = document.createElement("button");
        button.textContent = label;
        button.addEventListener("click", () => {
            if (options.length === 1) {
                onChoose(options[0].index);
            } else {
                openChooser(label, options, onChoose);
            }
        });
        buttons.push(button);
    }
    callButtons.replaceChildren(...buttons);
}

// Removes all choices: once one is made, or when nothing is left to decide.
export function clearActions() {
    closeChooser();
    callButtons.replaceChildren();
    disableDiscards();
}

function openChooser(label, options, onChoose) {
    chooser.title.textContent = label;
    chooser.options.replaceChildren(...options.map(({ action, index }) => {
        const button = document.createElement("button");
        button.className = "option";
        button.append(...optionTiles(action));
        button.addEventListener("click", () => {
            closeChooser();
            onChoose(index);
        });
        return button;
    }));
    chooser.element.hidden = false;
}

function closeChooser() {
    chooser.element.hidden = true;
    chooser.options.replaceChildren();
}

// What an option shows: the meld it makes, as it will lie on the table, or
// else its tile (the riichi discard, for example).
function optionTiles(action) {
    const meld = actionMeld(action);
    if (meld) return [meld];
    return action.tile ? [tileImage(action.tile)] : [];
}
