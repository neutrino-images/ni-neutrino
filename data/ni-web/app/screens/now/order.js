// The order of the cards on the overview, while this browser has chosen to move
// them (app/ui/arrange.js).
//
// A list of names and nothing else, kept in this browser's localStorage as one line.
// What it is read into is always all eight cards exactly once: a name this page no
// longer draws is dropped, and a card added after the list was written is put where
// it stands in the page's own order rather than left out, because a card somebody
// never sees is a card they never learn exists.
//
// Nothing in this file reaches the document, so the arrangement can be checked
// without a browser (test/web/now-cases.mjs).
import { placed } from '../channels/drag.js';

const KEY = 'ni-web.now.order';

/* The page's own order, which is the overview as it is drawn with the grips off.
   Change this and the card list in overview.js together. */
export const CARDS = Object.freeze([
	'playing', 'box', 'sound', 'message', 'recordings', 'storage', 'reception', 'quick'
]);

/**
 * All the cards, in the order a stored line names them.
 *
 * A card the line does not name goes in behind the card it follows in the page's
 * own order, or at the front where that is none of them.
 *
 * @param {string | null | undefined} saved
 * @returns {readonly string[]}
 */
export function orderOf(saved) {
	/** @type {string[]} */
	const out = [];
	for (const one of String(saved || '').split(',')) {
		const id = one.trim();
		if (CARDS.indexOf(id) !== -1 && out.indexOf(id) === -1)
			out.push(id);
	}
	if (out.length === 0)
		return CARDS;
	CARDS.forEach(function (id, at) {
		if (out.indexOf(id) !== -1)
			return;
		const before = at > 0 ? CARDS[at - 1] : undefined;
		const where = before === undefined ? -1 : out.indexOf(before);
		out.splice(where + 1, 0, id);
	});
	return isOwn(out) ? CARDS : out;
}

/**
 * @param {readonly string[]} list
 * @returns {boolean} whether this is the page's own order
 */
export function isOwn(list) {
	if (list.length !== CARDS.length)
		return false;
	for (let i = 0; i < list.length; i++) {
		if (list[i] !== CARDS[i])
			return false;
	}
	return true;
}

/**
 * One card moved by a number of places, and the list as it was where that goes
 * past either end.
 *
 * @param {readonly string[]} list
 * @param {string} id
 * @param {number} by
 * @returns {readonly string[]}
 */
export function shifted(list, id, by) {
	const from = list.indexOf(id);
	const to = from + by;
	if (from === -1 || by === 0 || to < 0 || to >= list.length)
		return list;
	const out = list.slice();
	out.splice(from, 1);
	out.splice(to, 0, id);
	return out;
}

/**
 * One card put in front of or behind another, by the same rule the bouquet screen
 * moves a row by.
 *
 * @param {readonly string[]} list
 * @param {string} id
 * @param {string} target
 * @param {boolean} after
 * @returns {readonly string[]}
 */
export function dropped(list, id, target, after) {
	return placed(list, id, target, after);
}

/**
 * Whether a pointer over a card means behind it rather than in front of it.
 *
 * A card that has the row to itself is a line in a column, so the lower half is
 * behind it. One that shares its row is a place in a line read from the left, so
 * the right half is.
 *
 * @param {{ left: number, top: number, width: number, height: number }} box
 * @param {number} x
 * @param {number} y
 * @param {boolean} alone whether the card has its row to itself
 * @returns {boolean}
 */
export function behind(box, x, y, alone) {
	return alone ? y > box.top + box.height / 2 : x > box.left + box.width / 2;
}

/**
 * @returns {readonly string[]} the order this browser last left
 */
export function keptOrder() {
	try {
		return orderOf(window.localStorage.getItem(KEY));
	} catch (e) {
		// No site data: the page's own order.
		return CARDS;
	}
}

/**
 * @param {readonly string[]} list
 * @returns {void}
 */
export function keepOrder(list) {
	try {
		if (isOwn(list))
			window.localStorage.removeItem(KEY);
		else
			window.localStorage.setItem(KEY, list.join(','));
	} catch (e) {
		// Holds until the page is loaded again.
	}
}
