// How big the writing is, which is this browser's business and not the box's.
//
// A box is watched by whoever is in the room, and one of them needs the words larger
// than the next one does. A setting on the box would be one household arguing over one
// number, and it would travel to a phone that already draws text the way its owner
// wants it. So it is remembered here, and nothing about it ever reaches the box.
//
// localStorage and not the session store beside it: this is the one thing on these
// pages worth keeping after the tab is closed. Both halves are wrapped, because a
// browser told to keep no site data throws on the read rather than answering nothing.
//
// The size is one number on the root element and every rule on every screen is a
// fraction of it (app/css/tokens.css), so this file sets one property and touches no
// other.

const KEY = 'ni-web.text-scale';
const PROPERTY = '--text-scale';

/* Three and not a slider. A slider over a range invites a value somebody cannot
   undo by eye, and these pages are read at arm's length from a sofa.

   The largest is a third again and no more. The frame around the words is in pixels
   and does not grow with them, so past this the controls of the narrowest phone
   start to touch. Measured at 390 pixels. */
const NORMAL = { id: 'normal', scale: 1 };

export const SIZES = [
	NORMAL,
	{ id: 'large', scale: 1.15 },
	{ id: 'largest', scale: 1.3 }
];

/**
 * @returns {string} the identifier this browser last chose, or the first
 *          of the three where it has never chosen or cannot remember
 */
export function chosen() {
	let saved = '';
	try {
		saved = window.localStorage.getItem(KEY) || '';
	} catch (e) {
		// A browser that keeps no site data draws the size the page ships
		// with. Nothing else here depends on the answer.
	}
	for (const size of SIZES) {
		if (size.id === saved)
			return size.id;
	}
	return NORMAL.id;
}

/**
 * Draws the page at one of the three and remembers which.
 *
 * Written straight onto the root element rather than into a stylesheet: a style
 * attribute is refused by the policy this document carries, and a property set
 * through the object model is not.
 *
 * @param {string} id one of SIZES
 * @returns {void}
 */
export function choose(id) {
	let picked = NORMAL;
	for (const size of SIZES) {
		if (size.id === id)
			picked = size;
	}

	document.documentElement.style.setProperty(PROPERTY, String(picked.scale));

	try {
		if (picked === NORMAL) {
			window.localStorage.removeItem(KEY);
		} else {
			window.localStorage.setItem(KEY, picked.id);
		}
	} catch (e) {
		// The page is the size that was asked for until it is loaded again.
	}
}

/**
 * The size this browser last chose, drawn before anything else is.
 *
 * Called once at the start so a reader who needs the larger size never sees the
 * smaller one first. The first of the three writes nothing, because the stylesheet
 * already says it.
 *
 * @returns {void}
 */
export function restore() {
	const id = chosen();
	if (id !== NORMAL.id)
		choose(id);
}
