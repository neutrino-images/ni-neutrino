// The one source both foldings read. The bar and the sheet draw this, the router is
// derived from it, and the server's prefix list is held to the seven identifiers below.
// A destination states its whole table and this file names it: nothing registers
// itself, so nothing can be missing without this file saying so.
//
// A screen is named here before it is written, and the file it names exists from the
// first day.
import now from './screens/now/nav.js';
import channels from './screens/channels/nav.js';
import epg from './screens/epg/nav.js';
import timers from './screens/timers/nav.js';
import files from './screens/files/nav.js';
import system from './screens/system/nav.js';
import settings from './screens/settings/nav.js';
import dev from './screens/dev/nav.js';
import { t } from './i18n.js';
import text from './shell.text.js';

// Written out here rather than read off the eight tables, because this is the list
// the server's prefix list is compared against, and a list that exists only as the
// sum of eight files cannot be read without running the page.
//
// All eight, including the one a build may not offer. What the server answers with
// the page and what the page puts in front of somebody are two different questions:
// the first is about whether an address works when it is reloaded, and the second is
// visibleAreas below.
export const ids = ['now', 'channels', 'epg', 'timers', 'files', 'system', 'settings', 'dev'];

/** @type {Web.NavArea[]} */
export const areas = [now, channels, epg, timers, files, system, settings, dev];

// A table that disagrees with the list above would give the bar one order and
// the router another, and both would look right on screen. It is settled here,
// once, at load.
areas.forEach(function (area, i) {
	if (area.id !== ids[i])
		throw new Error('nav: area ' + i + ' is ' + area.id + ' and the list says ' + ids[i]);
});

/* WHICH OF THE EIGHT ARE OFFERED, which is not the same as which of them work. One
   of them is a way into the API documentation, and a build compiled without it
   carries neither the prose nor the reader page. The box says which kind of build it
   is and the answer is handed in here; before it has arrived nothing is hidden,
   because a bar whose contents jump once the first answer lands is worse than a bar
   that is right a moment late.

   Held to the tables rather than written out: a destination that is not always
   offered says what it needs under needs.
 */
/**
 * @param {readonly Web.NavArea[]} all
 * @param {{ apiDoc?: boolean } | null | undefined} build what the box said
 *        about its own build, and null while it has not said yet
 * @returns {Web.NavArea[]}
 */
export function visibleAreas(all, build) {
	return all.filter(function (area) {
		if (area.needs !== 'api-doc')
			return true;
		return !build || build.apiDoc !== false;
	});
}

/**
 * @param {string} id
 * @returns {Web.NavArea | null}
 */
export function areaById(id) {
	for (const area of areas) {
		if (area.id === id)
			return area;
	}
	return null;
}

// One spelling of an address for every part of the page, so the bar, the sheet
// and the router cannot disagree about where a screen lives.
/**
 * @param {string} areaId
 * @param {string} [entryId]
 * @param {string | number} [param]
 * @returns {string}
 */
export function hrefFor(areaId, entryId, param) {
	let path = '/' + areaId;
	if (entryId)
		path += '/' + entryId;
	if (param !== undefined && param !== null && param !== '')
		path += '/' + encodeURIComponent(param);
	return path;
}

// The catalogue first, what the box called the thing after it, the identifier last.
// The order matters for the sections of the settings: they are named by the running
// box, and a box that carries one this page has never heard of still gets a legible
// line rather than a key on screen.
/**
 * @param {Web.NavNode} node
 * @returns {string}
 */
export function labelOf(node) {
	if (node.text) {
		const said = t(text, node.text);
		if (said !== node.text)
			return said;
	}
	return node.label || node.id;
}

// Some second levels are not a list but a question for the box. Settings is the one:
// the tables name sixteen sections and which of them a build carries only the running
// box knows, so a list written here would be a second truth beside the box's own.
// What is resolved is kept, because the two foldings ask for it separately and twice
// would be two answers that could differ.
/** @type {Map<string, Web.NavEntry[]>} */
const resolved = new Map();
/** @type {Map<string, Promise<Web.NavEntry[]>>} */
const asking = new Map();

// Null while the answer is on its way, which is a state the caller draws and
// not a hole it has to guess at.
/**
 * @param {Web.NavArea | null | undefined} area
 * @returns {Web.NavEntry[] | null} null while the answer is on its way, which
 *          is a state the caller draws and not a hole it has to guess at
 */
export function secondLevel(area) {
	if (!area)
		return [];
	if (typeof area.items !== 'function')
		return area.items;
	return resolved.get(area.id) || null;
}

/**
 * @param {Web.NavArea | null | undefined} area
 * @param {Web.Context} ctx
 * @returns {Promise<Web.NavEntry[]>}
 */
export function loadSecondLevel(area, ctx) {
	if (!area || typeof area.items !== 'function')
		return Promise.resolve(secondLevel(area) || []);
	const already = resolved.get(area.id);
	if (already)
		return Promise.resolve(already);

	const running = asking.get(area.id);
	if (running)
		return running;

	const answer = Promise.resolve(area.items(ctx)).then(function (items) {
		const list = items || [];
		resolved.set(area.id, list);
		asking.delete(area.id);
		return list;
	}, function (failed) {
		// Not remembered: a box that was busy once answers the next time, and
		// an empty second level kept for the life of the page would be a
		// destination that never comes back.
		asking.delete(area.id);
		throw failed;
	});

	asking.set(area.id, answer);
	return answer;
}

// What a destination opens on when only the destination was asked for.
/**
 * @param {Web.NavArea | null | undefined} area
 * @returns {Web.NavEntry | null} what a destination opens on when only the
 *          destination was asked for
 */
export function firstEntry(area) {
	const items = secondLevel(area);
	return (items && items.length) ? (items[0] || null) : null;
}

/* AND UNDER A NAME IT USED TO HAVE. Two screens that become one leave an address
   behind, and somebody who wrote that address down should reach the screen. What a
   bar draws is still the identifier; the older names are only answered to.

   Asked for in two passes, so that a name which is one entry's own and another
   entry's former one cannot be taken by whichever comes first in the table. */
/**
 * @param {Web.NavArea | null | undefined} area
 * @param {string} entryId
 * @returns {Web.NavEntry | null}
 */
export function entryById(area, entryId) {
	const items = secondLevel(area) || [];
	for (const entry of items) {
		if (entry.id === entryId)
			return entry;
	}
	for (const entry of items) {
		if (entry.was && entry.was.indexOf(entryId) !== -1)
			return entry;
	}
	return null;
}
