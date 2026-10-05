// The one source for both bars and the router.
import now from './screens/now/nav.js';
import channels from './screens/channels/nav.js';
import epg from './screens/epg/nav.js';
import timers from './screens/timers/nav.js';
import recordings from './screens/recordings/nav.js';
import files from './screens/files/nav.js';
import system from './screens/system/nav.js';
import settings from './screens/settings/nav.js';
import ai from './screens/ai/nav.js';
import dev from './screens/dev/nav.js';
import { t } from './i18n.js';
import text from './shell.text.js';

// Written out, since the server's prefix list is compared against it.
export const ids = ['now', 'channels', 'epg', 'timers', 'recordings', 'files', 'system', 'settings', 'ai', 'dev'];

/** @type {Web.NavArea[]} */
export const areas = [now, channels, epg, timers, recordings, files, system, settings, ai, dev];

areas.forEach(function (area, i) {
	if (area.id !== ids[i])
		throw new Error('nav: area ' + i + ' is ' + area.id + ' and the list says ' + ids[i]);
});

// Before the box has answered, the documentation shows and the AI area waits.
/**
 * @param {readonly Web.NavArea[]} all
 * @param {{ apiDoc?: boolean, mcp?: boolean } | null | undefined} build what the box said
 *        about its own build, and null while it has not said yet
 * @returns {Web.NavArea[]}
 */
export function visibleAreas(all, build) {
	return all.filter(function (area) {
		if (area.needs === 'api-doc')
			return !build || build.apiDoc !== false;
		if (area.needs === 'mcp')
			return !!build && build.mcp === true;
		return true;
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

// Catalogue first, then the box's name, then the identifier.
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

// Settings sections come from the box; kept, since both bars ask.
/** @type {Map<string, Web.NavEntry[]>} */
const resolved = new Map();
/** @type {Map<string, Promise<Web.NavEntry[]>>} */
const asking = new Map();

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
		// Not remembered: a busy box answers next time.
		asking.delete(area.id);
		throw failed;
	});

	asking.set(area.id, answer);
	return answer;
}

/**
 * @param {Web.NavArea | null | undefined} area
 * @returns {Web.NavEntry | null} what a destination opens on when only the
 *          destination was asked for
 */
export function firstEntry(area) {
	const items = secondLevel(area);
	return (items && items.length) ? (items[0] || null) : null;
}

// Former names, in a second pass so an own name wins.
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
