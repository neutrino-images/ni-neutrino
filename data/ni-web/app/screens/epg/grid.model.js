/* The arithmetic of the grid, apart from the drawing of it.

   Everything here is a function of its arguments: no clock, no network, no
   element, so the two things this screen can get wrong can be read without a
   browser.

   THE FIRST IS THE WALK. A page of the grid ends at the number of channels that
   were asked for or at the ceiling the layer below puts on the events of one
   page, whichever comes first (src/coreapi/epg.h), and the second ends it early
   without dropping anything. So a page holding fewer rows than were asked for is
   not on that account the last one, and the cursor is the only thing that says
   whether a walk is over. A screen that counted rows instead would stop in the
   middle of the bouquet on precisely the days the guide is full.

   THE SECOND IS WHAT IS DRAWN. Two thousand six hundred channels over three
   hours is more nodes than a browser draws, so what is drawn is the rows the
   vertical window holds and, inside each, the events the horizontal one does. */

/** The width of the window this screen draws, in minutes. */
export const WINDOW_MINUTES = 180;

/** How wide one minute is drawn before the box has been measured. */
export const MINUTE_PX = 6;

/* And the narrowest it is ever drawn. Under this a half hour programme is
   fifty pixels and holds nothing but its clock, so the axis stops shrinking
   and the box scrolls instead of squeezing. */
export const MIN_MINUTE_PX = 2;

/** The mark on the axis, and the boundary the window is snapped to. */
export const TICK_SECONDS = 1800;

/** How tall one channel is drawn. Every row is the same height, which is what
    lets the rows above and below the window be two spacers rather than rows. */
export const ROW_HEIGHT = 44;

/* How tall the row of hours is. Stated here and handed to the stylesheet as a
   custom property rather than written in both, because the height of the grid
   is worked out from it and two numbers meaning one thing drift. */
export const HEAD_HEIGHT = 28;

/* Narrow enough that an event of a few minutes is still a target somebody can
   hit, and a label that does not fit is hidden by the stylesheet rather than
   made to fit. */
export const MIN_EVENT_PX = 10;

/* How many channels one ask of the guide names at once.

   Under both ceilings the route states, and deliberately not at either: one
   request answers for at most a hundred channels and names at most two hundred,
   while what this screen needs is the rows of one window and the few above and
   below it. Asking for a hundred would buy rows nobody is looking at with the
   one thing this costs the box, which is a read of the guide per channel. */
export const CHANNELS_PER_ASK = 40;

/** How many rows above and below the window are drawn anyway. */
export const OVERSCAN_ROWS = 4;

/** And how much of the time axis, in pixels, on either side of what is shown. */
export const OVERSCAN_PX = 240;

/* How close to the end of the channels in hand the window may come before the
   next page of them is asked for. Half a screen, so the page is on its way
   before the scrollbar arrives at the bottom. */
export const AHEAD_ROWS = 10;

/* The same rule on the other axis, for the strip the folded grid is chosen
   from. In pixels because a strip has no rows: about two chips, so the next
   page is asked for while there are still two under the thumb. */
export const AHEAD_PX = 220;

/* How many channels the picker draws at once. A bouquet on this box runs to
   two thousand six hundred, and two thousand six hundred buttons in a sheet is
   a page a browser spends a second laying out for a list nobody reads past the
   tenth entry of. What is over the ceiling is said rather than dropped in
   silence, and typing more of the name is what gets at it. */
export const PICK_AT_ONCE = 40;

/**
 * @typedef {object} Row
 * @property {Api.Channel} channel what this row is for, as the listing named it
 * @property {Api.Event[] | null} events its events inside the window, and null
 *   while the guide has not been asked for them
 * @property {boolean} truncated whether the guide held more of this channel
 *   inside the window than one row of a grid carries
 */

/**
 * @typedef {object} Block
 * @property {Api.Event} event
 * @property {number} left where it begins, in pixels from the left of the axis
 * @property {number} width how wide it is drawn, never below MIN_EVENT_PX
 */

/**
 * @typedef {object} Band
 * @property {number} first the first row drawn
 * @property {number} count how many are drawn
 * @property {number} padTop the height of the rows above them
 * @property {number} padBottom and of the rows below
 */

/**
 * The half hour a moment falls in, which is where a window opens.
 *
 * Snapped rather than taken as it is, so the marks on the axis are the half
 * hours a programme actually starts on and two people looking at the same guide
 * see the same picture.
 *
 * @param {number} seconds
 * @returns {number}
 */
export function windowStart(seconds) {
	const whole = Math.floor(seconds);
	return whole - (((whole % TICK_SECONDS) + TICK_SECONDS) % TICK_SECONDS);
}

/**
 * @param {number} from
 * @returns {number}
 */
export function windowEnd(from) {
	return from + WINDOW_MINUTES * 60;
}

/**
 * HOW WIDE ONE MINUTE IS DRAWN, OUT OF THE ROOM THE BOX ACTUALLY HAS.
 *
 * Measured and not fixed, because a fixed figure is a fixed axis, and a fixed
 * axis on a tablet is a grid pushed sideways under a finger. The window stays
 * the same three hours at every width.
 *
 * The widest figure is what an unmeasured box draws at, so the first paint shows
 * the evening rather than a strip of it.
 *
 * @param {number} boxWidth what the scrolling box measured, in pixels
 * @param {number} nameWidth and how much of it the column of names takes
 * @returns {number}
 */
export function minutePx(boxWidth, nameWidth) {
	const room = boxWidth - nameWidth;
	if (!(room > 0)) {
		return MINUTE_PX;
	}
	const each = room / WINDOW_MINUTES;
	return each < MIN_MINUTE_PX ? MIN_MINUTE_PX : each;
}

/** How wide the whole axis is drawn, in pixels.
 * @param {number} from
 * @param {number} to
 * @param {number} each how wide one minute is drawn
 * @returns {number}
 */
export function axisWidth(from, to, each) {
	return ((to - from) / 60) * each;
}

/**
 * The marks of the axis, each with where it is drawn.
 *
 * @param {number} from
 * @param {number} to
 * @param {number} each
 * @returns {{ at: number, left: number }[]}
 */
export function ticks(from, to, each) {
	/** @type {{ at: number, left: number }[]} */
	const out = [];
	for (let at = from; at < to; at += TICK_SECONDS) {
		out.push({ at: at, left: ((at - from) / 60) * each });
	}
	return out;
}

/**
 * Where the moment sits on the axis, in pixels from its left edge, and below
 * nought where the window does not hold that moment at all.
 *
 * The one number the mark of the moment and the bar on a running programme are
 * both drawn from, so the bar cannot end anywhere but at the mark.
 *
 * @param {number} at
 * @param {number} from
 * @param {number} to
 * @param {number} each
 * @returns {number}
 */
export function atPixel(at, from, to, each) {
	if (at < from || at >= to) {
		return -1;
	}
	return ((at - from) / 60) * each;
}

/**
 * Where one event sits in the window, clipped to both of its ends.
 *
 * Clipped and not left out, because an event that began before this window and
 * runs into it is what the first column of a grid mostly consists of. Null for
 * one that lies wholly outside.
 *
 * @param {Api.Event} event
 * @param {number} from
 * @param {number} to
 * @param {number} each
 * @returns {{ left: number, width: number } | null}
 */
export function place(event, from, to, each) {
	const begins = event.start;
	const ends = event.start + event.duration;
	if (ends <= from || begins >= to) {
		return null;
	}
	const left = Math.max(0, (begins - from) / 60) * each;
	const right = Math.min((to - from) / 60, (ends - from) / 60) * each;
	const width = right - left;
	return { left: left, width: width < MIN_EVENT_PX ? MIN_EVENT_PX : width };
}

/**
 * Whether one event is the one on air at a given moment.
 *
 * The half open interval, so the event beginning at the moment another ends is
 * the one on air and never both of them.
 *
 * @param {Api.Event} event
 * @param {number} at
 * @returns {boolean}
 */
export function isOnAir(event, at) {
	return event.start <= at && at < event.start + event.duration;
}

/**
 * The events of one row that the horizontal window holds, already placed.
 *
 * @param {Api.Event[]} events
 * @param {number} from
 * @param {number} to
 * @param {number} leftPx the left edge of what is shown, in pixels
 * @param {number} rightPx and its right edge
 * @param {number} each
 * @returns {Block[]}
 */
export function blocks(events, from, to, leftPx, rightPx, each) {
	/** @type {Block[]} */
	const out = [];
	for (const event of events) {
		const at = place(event, from, to, each);
		if (at === null) {
			continue;
		}
		if (at.left + at.width <= leftPx || at.left >= rightPx) {
			continue;
		}
		out.push({ event: event, left: at.left, width: at.width });
	}
	return out;
}

/**
 * Which rows the vertical window holds, and how tall the rest of them are.
 *
 * The total is the rows in hand and not the rows that exist: with a cursor the
 * second number is unknown, and a height worked out from a guess jumps the
 * moment the guess is wrong. The one extra row below where there is more to come
 * is what keeps the scrollbar inviting.
 *
 * @param {{ total: number, viewport: number, scrollTop: number, hasMore: boolean, rowHeight: number }} opts
 * @returns {Band}
 */
export function band(opts) {
	const total = opts.total > 0 ? opts.total : 0;
	const height = opts.rowHeight > 0 ? opts.rowHeight : ROW_HEIGHT;
	const visible = Math.ceil(Math.max(0, opts.viewport) / height);

	let first = Math.floor(Math.max(0, opts.scrollTop) / height) - OVERSCAN_ROWS;
	if (first < 0) {
		first = 0;
	}
	if (first > total) {
		first = total;
	}

	let count = visible + OVERSCAN_ROWS * 2;
	if (first + count > total) {
		count = total - first;
	}
	if (count < 0) {
		count = 0;
	}

	return {
		first: first,
		count: count,
		padTop: first * height,
		padBottom: (total - first - count) * height + (opts.hasMore ? height : 0)
	};
}

/**
 * Whether the next page of channels is to be asked for.
 *
 * Asked of the window and not of the scroll position, so a viewport tall enough
 * to hold everything in hand asks for the next page without anybody scrolling.
 *
 * @param {Band} view
 * @param {number} total
 * @param {boolean} hasMore
 * @returns {boolean}
 */
export function wantsMoreChannels(view, total, hasMore) {
	return hasMore && view.first + view.count + AHEAD_ROWS >= total;
}

/**
 * THE CURSOR OF THE PAGE AFTER THIS ONE, AND EMPTY WHERE THIS ONE IS THE LAST.
 *
 * This is the only thing that says a walk is over. The number of rows never
 * says it: a page ends at the channels asked for or at the ceiling on the events
 * of one page, whichever comes first, so a page holding fewer rows than were
 * asked for is not on that account the last one.
 *
 * @param {Api.EpgGrid} page
 * @returns {string}
 */
export function nextCursor(page) {
	const cursor = page.next_cursor;
	return typeof cursor === 'string' ? cursor : '';
}

/**
 * The same, for the channel listing, which answers its own cursor the same way.
 *
 * @param {Api.ChannelPage} page
 * @returns {string}
 */
export function nextChannelCursor(page) {
	const cursor = page.next_cursor;
	return typeof cursor === 'string' ? cursor : '';
}

/**
 * A row per channel, with nothing of the guide in it yet.
 *
 * @param {Api.Channel[]} channels
 * @returns {Row[]}
 */
export function rowsFor(channels) {
	return channels.map(function (channel) {
		return { channel: channel, events: null, truncated: false };
	});
}

/**
 * The channels of the window whose guide has not arrived, at most as many as one
 * ask names.
 *
 * The window and not the whole list: the guide costs the box a read per channel
 * of the answer.
 *
 * @param {Row[]} rows
 * @param {number} first
 * @param {number} count
 * @returns {string[]}
 */
export function unfilled(rows, first, count) {
	/** @type {string[]} */
	const out = [];
	const stop = Math.min(rows.length, first + count);
	for (let i = Math.max(0, first); i < stop && out.length < CHANNELS_PER_ASK; i++) {
		const row = rows[i];
		if (row !== undefined && row.events === null) {
			out.push(row.channel.id);
		}
	}
	return out;
}

/**
 * Every channel of the window, whether or not its guide has arrived, at most as
 * many as one ask names. What is asked for again when the box says the guide
 * moved under the page.
 *
 * @param {Row[]} rows
 * @param {number} first
 * @param {number} count
 * @returns {string[]}
 */
export function inBand(rows, first, count) {
	/** @type {string[]} */
	const out = [];
	const stop = Math.min(rows.length, first + count);
	for (let i = Math.max(0, first); i < stop && out.length < CHANNELS_PER_ASK; i++) {
		const row = rows[i];
		if (row !== undefined) {
			out.push(row.channel.id);
		}
	}
	return out;
}

/**
 * The rows with one page of the guide written into them.
 *
 * By identifier and not by position: the answer names the channel of every row
 * it carries, and a page that ended early carries fewer rows than were asked
 * for, so counting along would write one channel's evening under another's name.
 *
 * @param {Row[]} rows
 * @param {Api.ChannelSchedule[]} answered
 * @returns {Row[]}
 */
export function absorb(rows, answered) {
	/** @type {Map<string, Api.ChannelSchedule>} */
	const byChannel = new Map();
	for (const one of answered) {
		byChannel.set(one.channel_id, one);
	}
	return rows.map(function (row) {
		const one = byChannel.get(row.channel.id);
		if (one === undefined) {
			return row;
		}
		return { channel: row.channel, events: one.events, truncated: one.truncated };
	});
}

/**
 * Every row's guide forgotten, the channels kept.
 *
 * The rows are what the listing cost, and keeping them is what keeps the screen
 * still while the new answer is on its way.
 *
 * @param {Row[]} rows
 * @returns {Row[]}
 */
export function forgetGuide(rows) {
	return rows.map(function (row) {
		return { channel: row.channel, events: null, truncated: false };
	});
}

/**
 * The channels of a bouquet whose name carries what was typed.
 *
 * Folded over case and nothing cleverer: what somebody types into a picker is a
 * piece of the name they are looking at. The empty query matches everything, so
 * the picker opens on the list rather than on nothing.
 *
 * @param {Row[]} rows
 * @param {string} query
 * @param {number} limit
 * @returns {{ items: Row[], more: boolean }} more says the ceiling cut the
 *          list, which is a thing to tell somebody and not to hide
 */
export function matching(rows, query, limit) {
	const wanted = query.trim().toLowerCase();
	/** @type {Row[]} */
	const out = [];
	let more = false;
	for (const row of rows) {
		if (wanted !== '' && row.channel.name.toLowerCase().indexOf(wanted) < 0) {
			continue;
		}
		if (out.length >= limit) {
			more = true;
			break;
		}
		out.push(row);
	}
	return { items: out, more: more };
}

/**
 * Whether a strip pushed this far is close enough to its end to ask for the next
 * page of channels.
 *
 * The horizontal twin of wantsMoreChannels: the page is on its way before the
 * last chip is under the thumb. A strip that does not run off the side asks for
 * nothing.
 *
 * @param {{ scrollLeft: number, clientWidth: number, scrollWidth: number }} strip
 * @param {boolean} hasMore
 * @returns {boolean}
 */
export function wantsMoreAlong(strip, hasMore) {
	if (!hasMore) {
		return false;
	}
	return strip.scrollWidth - strip.scrollLeft - strip.clientWidth <= AHEAD_PX;
}

/**
 * THE ONE ROW A TELEPHONE IS LOOKING AT, IN THE SHAPE THE WIDE WINDOW HAS.
 *
 * Folded, the grid is one channel's evening under a strip of channels, so the
 * window over the rows is one row wide. Answered as a band so everything behind
 * it is the same code at both foldings.
 *
 * The first row where nothing is chosen, and again where what was chosen is not
 * in the list any more, which is what a bouquet somebody has just switched to
 * looks like for one draw.
 *
 * @param {Row[]} rows
 * @param {string} chosen the channel identifier, or the empty string for none
 * @returns {Band}
 */
export function only(rows, chosen) {
	const empty = { first: 0, count: 0, padTop: 0, padBottom: 0 };
	if (rows.length === 0) {
		return empty;
	}
	let at = 0;
	if (chosen !== '') {
		at = -1;
		for (let i = 0; i < rows.length; i++) {
			const row = rows[i];
			if (row !== undefined && row.channel.id === chosen) {
				at = i;
				break;
			}
		}
		if (at < 0) {
			at = 0;
		}
	}
	return { first: at, count: 1, padTop: 0, padBottom: 0 };
}
