/* What the bouquet screen works out without asking the box.

   A BOUQUET IS ADDRESSED BY ITS NAME AND NEVER BY THE NUMBER IT IS LISTED WITH.
   The number is the place the channel list holds it at counted from one
   (src/coreapi/box/channelsource_real.cpp), so deleting, hiding or moving anything
   above it renumbers it while a screen is still holding the old number. Every
   writing route takes the name. The number is worked out again from a fresh
   listing at the one place that needs one, which is the query that asks for a
   bouquet's members.

   Nothing in this file reaches the network or holds state. */

/* The most channels one half of one bouquet takes. The box states the same
   number and refuses a longer body with it (src/httpd/ep/ep_channels.cpp,
   kMaxChannelsInBouquet), and it is repeated here so that a list too long is
   turned away before it costs a round trip and so that the sentence on screen
   carries the real figure rather than a rounder one somebody liked better. */
export const MAX_IN_HALF = 4096;

/* And the longest a name may be, from the same tables (kMaxBouquetName). The
   box counts bytes there and not characters, which is why the check below
   encodes before it counts: a name of two hundred umlauts is four hundred
   bytes and would be refused by a box a screen had told nothing. */
export const MAX_NAME_BYTES = 255;

/**
 * The bouquets out of what the listing answered, and the empty list for an answer
 * that is not there yet. A screen draws by state and never by whether data
 * happens to have arrived.
 *
 * @param {Api.BouquetList | null} data
 * @returns {Api.Bouquet[]}
 */
export function rowsOf(data) {
	return (data && data.items) ? data.items : [];
}

/**
 * The bouquet of that name, and null for a name the listing no longer carries.
 * Null is an ordinary answer and not a fault: somebody at the television may have
 * renamed or deleted it a moment ago.
 *
 * @param {Api.Bouquet[]} rows
 * @param {string} name
 * @returns {Api.Bouquet | null}
 */
export function byName(rows, name) {
	for (const row of rows) {
		if (row.name === name)
			return row;
	}
	return null;
}

/**
 * One entry of a list moved to another place, as a new list.
 *
 * A new list and not the one handed in, because what is drawn and what is being
 * edited would otherwise be the same array and nothing would see that it changed.
 * A move that goes nowhere hands the list back as it is, so a caller can compare
 * the two.
 *
 * @template T
 * @param {T[]} list
 * @param {number} from
 * @param {number} to
 * @returns {T[]}
 */
export function moved(list, from, to) {
	if (from === to || from < 0 || to < 0 || from >= list.length || to >= list.length)
		return list;
	const out = list.slice();
	const taken = out.splice(from, 1);
	// Walked rather than indexed: one element came out and the loop says so
	// without asking the reader to trust that splice answered a list of one.
	for (const one of taken)
		out.splice(to, 0, one);
	return out;
}

/**
 * The identifiers of a list of channels, in its order. This is the body the box
 * takes for one half of a bouquet, and the order in it is the order that will
 * apply.
 *
 * @param {Api.Channel[]} list
 * @returns {string[]}
 */
export function idsOf(list) {
	/** @type {string[]} */
	const out = [];
	for (const one of list)
		out.push(one.id);
	return out;
}

/**
 * Whether two lists of channels are the same channels in the same sequence.
 * Compared as a sequence and not as a set, because the order is half of what the
 * editor saves.
 *
 * @param {Api.Channel[]} a
 * @param {Api.Channel[]} b
 * @returns {boolean}
 */
export function sameOrder(a, b) {
	return a.length === b.length && idsOf(a).join(' ') === idsOf(b).join(' ');
}

/**
 * Whether a list already holds that channel. A bouquet holding one channel twice
 * is one the box reduces to one when it writes it out (src/httpd/ep/ep_channels.cpp),
 * so adding it a second time would be a row that disappears at the next reading.
 *
 * @param {Api.Channel[]} list
 * @param {string} id
 * @returns {boolean}
 */
export function holds(list, id) {
	for (const one of list) {
		if (one.id === id)
			return true;
	}
	return false;
}

/**
 * How many bytes a name costs the box, which is what the box bounds and not
 * how many characters somebody typed.
 *
 * @param {string} name
 * @returns {number}
 */
export function nameBytes(name) {
	return new TextEncoder().encode(name).length;
}

/**
 * What is wrong with a name somebody typed, as a key of this stream's own
 * catalogue, and the empty string for one worth sending.
 *
 * Read here as well as on the box because the box answers a refusal that costs a
 * round trip and names the rule rather than the field, and because the one fault
 * it cannot see at all is the name that is already taken: the box would take it
 * and the listing would then carry two bouquets one name addresses, where every
 * writing route means the first.
 *
 * @param {string} name
 * @param {Api.Bouquet[]} rows
 * @param {string} keeping  the name being renamed, which may keep its own
 * @returns {string}
 */
export function nameFault(name, rows, keeping) {
	const wanted = name.trim();
	if (wanted === '')
		return 'bouquets.name.empty';
	if (nameBytes(wanted) > MAX_NAME_BYTES)
		return 'bouquets.name.long';
	if (wanted !== keeping && byName(rows, wanted) !== null)
		return 'bouquets.name.taken';
	return '';
}

/**
 * What a control now holds, out of the event it raised.
 *
 * Here rather than at each of the six places that read one: an event names its
 * target as something that can receive events, and only the element knows it has
 * a value. A control without one answers the empty string rather than throwing.
 *
 * @param {Event} event
 * @returns {string}
 */
export function readValue(event) {
	const target = /** @type {{ value?: unknown } | null} */ (event.currentTarget);
	return (target && typeof target.value === 'string') ? target.value : '';
}

/**
 * And whether a switch is now on, read the same way and for the same reason.
 *
 * @param {Event} event
 * @returns {boolean}
 */
export function readChecked(event) {
	const target = /** @type {{ checked?: unknown } | null} */ (event.currentTarget);
	return !!(target && target.checked === true);
}

/**
 * Where a channel of that identifier sits in a list, counted from nought, and
 * minus one for one that is not in it.
 *
 * @param {Api.Channel[]} list
 * @param {string} id
 * @returns {number}
 */
export function placeOfId(list, id) {
	let at = 0;
	for (const one of list) {
		if (one.id === id)
			return at;
		at++;
	}
	return -1;
}

/**
 * The channels of a list whose name carries that text, compared without case.
 *
 * The filter is over what is loaded and never over what the box holds: there is
 * no route that searches channel names. That is said on the screen beside the
 * field rather than only here.
 *
 * @param {Api.Channel[]} list
 * @param {string} wanted
 * @returns {Api.Channel[]}
 */
export function matching(list, wanted) {
	const needle = wanted.trim().toLowerCase();
	if (needle === '')
		return list;
	/** @type {Api.Channel[]} */
	const out = [];
	for (const one of list) {
		if (one.name.toLowerCase().indexOf(needle) !== -1)
			out.push(one);
	}
	return out;
}
