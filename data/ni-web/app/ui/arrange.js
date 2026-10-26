// Whether the cards of the overview can be moved, which is this browser's business
// like the theme.
//
// Off is the default and writes the key away, and off is the overview exactly as it
// was before this existed: the same cards in the same order with nothing added to
// them. On, every card carries a grip and the order is the one this browser last
// left (app/screens/now/order.js). Switching it off again keeps that order, so
// switching it back on finds it where it was.
//
// Kept in this browser's localStorage and nowhere else, for the same reasons as the
// text size: a phone and the laptop in the kitchen may want the cards differently,
// and the box is never told. Both halves are wrapped because a browser told to keep
// no site data throws.
//
// The switch is in the header's menu and the cards are on one screen, so the screen
// is told when it changes rather than reading it once: switched on while the
// overview is open, the grips appear without a reload.

const KEY = 'ni-web.now.arrange';

/** @type {Set<(on: boolean) => void>} */
const told = new Set();

/**
 * @returns {boolean} whether this browser last chose to move the cards
 */
export function chosenArrange() {
	try {
		return window.localStorage.getItem(KEY) === 'yes';
	} catch (e) {
		// No site data: the default applies.
		return false;
	}
}

/**
 * @param {boolean} on
 * @returns {void}
 */
export function chooseArrange(on) {
	try {
		if (on)
			window.localStorage.setItem(KEY, 'yes');
		else
			window.localStorage.removeItem(KEY);
	} catch (e) {
		// Applies until the page is loaded again.
	}
	told.forEach(function (one) { one(on); });
}

/**
 * Who is told when the choice changes.
 *
 * @param {(on: boolean) => void} one
 * @returns {() => void} what stops telling it
 */
export function onArrange(one) {
	told.add(one);
	return function () { told.delete(one); };
}
