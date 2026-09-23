/* Walking a route that answers a page and a cursor.

   The rule this file exists for: a page shorter than the one that was asked for is not on
   that account the last one. The channel route fills a page up to the limit and stops early
   wherever its own ceiling comes first, and the guide's grid does the same once the events
   of a page reach theirs, so a walk that stopped on a short page would stop in the middle of
   a channel list and look exactly like a box that holds nothing more. What says a walk is
   over is the absence of the cursor and nothing else. */

/**
 * @template T
 * @typedef {object} Walk
 * @property {T[]} items what has arrived so far, in the order it arrived
 * @property {string} cursor what to hand back for the page after this one
 * @property {boolean} more whether the box said there is a page after this one
 * @property {boolean} busy whether a page is on its way
 * @property {import('../../problem.js').ApiError | null} error
 */

/**
 * A walk that has not begun. more is true before anything has been asked,
 * because the first page is the page that is still to come.
 *
 * @template T
 * @returns {Walk<T>}
 */
export function emptyWalk() {
	return { items: [], cursor: '', more: true, busy: false, error: null };
}

/**
 * The cursor an answer carries, as the empty string for an answer carrying none.
 *
 * The route leaves the member out on the last page rather than answering it empty: an empty
 * value is an absent one to every parameter this server reads, so an empty cursor handed
 * back would ask for the first page again.
 *
 * @param {{ next_cursor?: string }} page
 * @returns {string}
 */
export function cursorOf(page) {
	if (!page || typeof page.next_cursor !== 'string') {
		return '';
	}
	return page.next_cursor;
}

/**
 * What one answer adds to a walk.
 *
 * @template T
 * @param {Walk<T>} walk
 * @param {T[]} items
 * @param {{ next_cursor?: string }} page
 * @returns {Walk<T>}
 */
export function extend(walk, items, page) {
	const cursor = cursorOf(page);
	return {
		items: walk.items.concat(items),
		cursor: cursor,
		more: cursor !== '',
		busy: false,
		error: null,
	};
}

/**
 * @template T
 * @param {Walk<T>} walk
 * @returns {Walk<T>}
 */
export function beginPage(walk) {
	return {
		items: walk.items,
		cursor: walk.cursor,
		more: walk.more,
		busy: true,
		error: null,
	};
}

/**
 * A walk that stopped on a refusal. more stays as it was, so that asking again
 * is asking for the page that did not arrive and not for the first one.
 *
 * @template T
 * @param {Walk<T>} walk
 * @param {import('../../problem.js').ApiError} error
 * @returns {Walk<T>}
 */
export function failPage(walk, error) {
	return {
		items: walk.items,
		cursor: walk.cursor,
		more: walk.more,
		busy: false,
		error: error,
	};
}

/**
 * Whether the next page is worth asking for now.
 *
 * @template T
 * @param {Walk<T>} walk
 * @returns {boolean}
 */
export function wants(walk) {
	return walk.more && !walk.busy && walk.error === null;
}

/* The text filter of the channel list, and the one thing it has to be honest about.

   There is no route that searches names, so this searches what has been walked so far and
   nothing else. A screen that draws the result without saying how much of the list it
   stands for answers "no such channel" for a channel that is simply on a page nobody has
   asked for yet. Matched without regard to case in the language the page is drawn in. */

/**
 * @param {Api.Channel[]} rows
 * @param {string} needle
 * @param {string} lang
 * @returns {Api.Channel[]}
 */
export function byName(rows, needle, lang) {
	const want = needle.trim().toLocaleLowerCase(lang);
	if (want === '') {
		return rows;
	}
	const out = [];
	for (const row of rows) {
		if (String(row.name).toLocaleLowerCase(lang).indexOf(want) !== -1) {
			out.push(row);
		}
	}
	return out;
}

/**
 * Whether a channel is one the browser fetches for itself.
 *
 * Read off the address and not off the kind: the route fills url in for the two web kinds
 * alone and leaves it empty for everything that is tuned (src/httpd/ep/ep_channels.cpp).
 *
 * @param {Api.Channel} channel
 * @returns {boolean}
 */
export function isWeb(channel) {
	return !!channel && typeof channel.url === 'string' && channel.url !== '';
}
