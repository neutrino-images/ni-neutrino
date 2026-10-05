/* What the archive routes answer, as plain values, and the addresses of one recording. */
import { buildUrl } from '../../api.js';

/**
 * @typedef {object} ArchiveItem
 * @property {string} id
 * @property {string} title
 * @property {string} channel
 * @property {number} start
 * @property {number} duration
 * @property {number} size
 * @property {boolean} playing
 */

const kId = /^[0-9a-f]{16}$/;

/**
 * @param {unknown} v
 * @returns {string}
 */
function str(v) { return typeof v === 'string' ? v : ''; }

/**
 * @param {unknown} v
 * @returns {number}
 */
function num(v) { return typeof v === 'number' && isFinite(v) && v >= 0 ? v : 0; }

/**
 * @param {unknown} one
 * @returns {ArchiveItem | null} null without a proper id
 */
function itemOf(one) {
	const r = /** @type {Record<string, unknown>} */ (one && typeof one === 'object' ? one : {});
	if (!kId.test(str(r.id))) {
		return null;
	}
	return { id: str(r.id), title: str(r.title), channel: str(r.channel), start: num(r.start),
		duration: num(r.duration), size: num(r.size), playing: r.playing === true };
}

/**
 * What the details route answers beyond the list; an empty text or 0 is a detail the box never noted.
 *
 * @typedef {object} ArchiveDetails
 * @property {ArchiveItem} item
 * @property {string} description
 * @property {string} longDescription
 * @property {number} genre
 * @property {string} series
 * @property {string} country
 * @property {number} year
 * @property {number} rating tenths
 * @property {number} quality stars
 * @property {number} age up to 18, or kAlwaysLocked
 * @property {string[]} audio
 * @property {boolean} cover
 */

// The movie browser's age for a recording that always asks for the PIN.
export const kAlwaysLocked = 99;

/**
 * @param {number} n
 * @param {number} max
 * @returns {number} n, or 0 (not noted) past max
 */
function upTo(n, max) {
	return n > max ? 0 : n;
}

/**
 * @param {unknown} answer
 * @returns {ArchiveDetails | null}
 */
export function readArchiveDetails(answer) {
	const item = itemOf(answer);
	if (!item) {
		return null;
	}
	const a = /** @type {Record<string, unknown>} */ (answer);
	const audio = Array.isArray(a.audio) ? a.audio.filter(function (/** @type {unknown} */ n) {
		return typeof n === 'string' && n !== '';
	}) : [];
	return { item: item, description: str(a.description), longDescription: str(a.long_description),
		genre: num(a.genre), series: str(a.series), country: str(a.country), year: num(a.year),
		rating: upTo(num(a.rating), 100), quality: upTo(num(a.quality), 3),
		age: num(a.age) === kAlwaysLocked ? kAlwaysLocked : upTo(num(a.age), 18), audio: audio, cover: a.cover === true };
}

/**
 * @param {string} text
 * @returns {string[]} its lines, blank ones dropped
 */
export function paragraphsOf(text) {
	return text.split(/\r?\n/).map(function (line) { return line.trim(); }).filter(function (line) { return line !== ''; });
}

/**
 * @param {unknown} answer
 * @returns {{ items: ArchiveItem[], total: number, next: number }}
 */
export function readArchive(answer) {
	const a = /** @type {Record<string, unknown>} */ (answer && typeof answer === 'object' ? answer : {});
	const rows = Array.isArray(a.items) ? a.items : [];
	/** @type {ArchiveItem[]} */
	const items = [];
	for (const one of rows) {
		const item = itemOf(one);
		if (item) {
			items.push(item);
		}
	}
	const next = typeof a.next_offset === 'number' && a.next_offset > 0 ? a.next_offset : -1;
	return { items: items, total: num(a.total), next: next };
}

/**
 * @param {string} id
 * @returns {string}
 */
export function archiveFileHref(id) {
	return buildUrl('/api/v1/recordings/archive/{id}/file', { id: id }, null);
}

/**
 * @param {string} id
 * @returns {string}
 */
export function archiveCoverHref(id) {
	return buildUrl('/api/v1/recordings/archive/{id}/cover', { id: id }, null);
}

/**
 * @param {string} id
 * @param {string} token empty for a caller the home network already grants read
 * @returns {string}
 */
export function archivePlaylistHref(id, token) {
	return buildUrl('/api/v1/recordings/archive/{id}/playlist.m3u', { id: id }, token === '' ? null : { token: token });
}

/** @type {Readonly<Record<string, string>>} what a refused play on the TV says, by refusal */
export const kTvRefusals = {
	'recording-playing': 'rec.archive.tv.busy',
	'mode-unavailable': 'rec.archive.tv.starting',
	'playback-running': 'rec.archive.tv.playing',
	'box-in-standby': 'rec.archive.standby',
};

/** @type {readonly string[]} the sort keys the list route takes, its own order first */
export const kArchiveSortKeys = ['start', 'title', 'channel', 'duration', 'size'];

/**
 * @param {string} key
 * @returns {'asc' | 'desc'} the order the route takes when none is named
 */
export function archiveFirstOrder(key) {
	return key === 'title' || key === 'channel' ? 'asc' : 'desc';
}

/**
 * @param {Web.Sort} sort
 * @param {string} key
 * @returns {Web.Sort}
 */
export function nextArchiveSort(sort, key) {
	if (sort.column !== key) {
		return { column: key, dir: archiveFirstOrder(key) };
	}
	return { column: key, dir: sort.dir === 'asc' ? 'desc' : 'asc' };
}

/**
 * @param {string} title
 * @param {number} offset
 * @param {string} [sort]
 * @param {'asc' | 'desc'} [order]
 * @returns {Record<string, string>} only what differs from the route's own choice
 */
export function archiveQuery(title, offset, sort, order) {
	/** @type {Record<string, string>} */
	const q = {};
	if (title.trim() !== '') {
		q.title = title.trim();
	}
	if (offset > 0) {
		q.offset = String(offset);
	}
	const key = sort || 'start';
	if (key !== 'start') {
		q.sort = key;
	}
	if (order && order !== archiveFirstOrder(key)) {
		q.sort = key;
		q.order = order;
	}
	return q;
}

/**
 * @template T
 * @param {Record<string, T>} held
 * @param {readonly string[]} keys
 * @returns {Record<string, T>} held with only these keys
 */
export function onlyKeys(held, keys) {
	/** @type {Record<string, T>} */
	const kept = {};
	for (const key of keys) {
		if (Object.prototype.hasOwnProperty.call(held, key)) {
			kept[key] = /** @type {T} */ (held[key]);
		}
	}
	return kept;
}

/**
 * @param {number} seconds
 * @returns {string}
 */
export function lengthWords(seconds) {
	if (!(seconds > 0)) {
		return '';
	}
	const minutes = Math.round(seconds / 60);
	if (minutes < 60) {
		return minutes + ' min';
	}
	const rest = minutes % 60;
	return Math.floor(minutes / 60) + ':' + (rest < 10 ? '0' : '') + rest + ' h';
}

/**
 * The words for a refusal the page has no sentence of its own for, by its status class:
 * the box writes its problems in English.
 *
 * @param {unknown} caught
 * @returns {string} a catalogue key
 */
export function refusalKey(caught) {
	const failure = /** @type {{ problem?: { status?: unknown } } | null} */ (caught);
	const status = failure && failure.problem && typeof failure.problem.status === 'number' ? failure.problem.status : 0;
	if (status === 401)
		return 'rec.refused.signin';
	if (status === 403)
		return 'rec.refused.forbidden';
	if (status === 409)
		return 'rec.refused.conflict';
	if (status >= 400 && status < 500)
		return 'rec.refused.other';
	if (status >= 500)
		return 'rec.refused.box';
	return 'rec.refused.none';
}
