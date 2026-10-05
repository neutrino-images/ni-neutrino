// What the television shows, read off GET /api/v1/playback, and what the tile offers for it.

// How often a playback is read again, between the events that say it changed.
export const kResyncMs = 15000;

/**
 * @typedef {object} Shown
 * @property {string} source channel, recording, file or none
 * @property {string} id the recording's archive id, else empty
 * @property {string} title
 * @property {string} channel the channel a recording was made from
 * @property {string} name the file's name
 * @property {number} position seconds
 * @property {number} duration seconds, 0 when the player does not know
 * @property {boolean} paused
 * @property {string} state
 * @property {string} returnsTo the live channel the box goes back to, else empty
 */

/**
 * @param {unknown} answer
 * @returns {Shown}
 */
export function readPlayback(answer) {
	const a = /** @type {Record<string, any>} */ (answer && typeof answer === 'object' ? answer : {});
	const rec = a.recording || {};
	const file = a.file || {};
	return {
		source: typeof a.source === 'string' ? a.source : 'none',
		id: typeof rec.id === 'string' ? rec.id : '',
		title: String(rec.title || file.title || ''),
		channel: String(rec.channel || ''),
		name: String(file.name || ''),
		position: Number(a.position) > 0 ? Number(a.position) : 0,
		duration: Number(a.duration) > 0 ? Number(a.duration) : 0,
		paused: a.paused === true,
		state: typeof a.state === 'string' ? a.state : '',
		returnsTo: a.returns_to && typeof a.returns_to.id === 'string' ? a.returns_to.id : '',
	};
}

/**
 * @param {Shown} shown
 * @returns {boolean} whether the player shows a recording or a file
 */
export function isPlayback(shown) {
	return shown.source === 'recording' || shown.source === 'file';
}

/**
 * Where the player stands now, moved on from the last answer while it plays.
 *
 * @param {Shown} shown
 * @param {number} sinceMs since that answer arrived
 * @returns {number} seconds
 */
export function positionAt(shown, sinceMs) {
	const moved = shown.state === 'playing' && sinceMs > 0 ? Math.floor(sinceMs / 1000) : 0;
	const at = shown.position + moved;
	return shown.duration > 0 && at > shown.duration ? shown.duration : at;
}

/**
 * A place in a playback as a clock reads it: 1:02:03, or 2:03 below an hour.
 *
 * @param {number} seconds
 * @returns {string}
 */
export function timeText(seconds) {
	const whole = Number.isFinite(seconds) && seconds > 0 ? Math.floor(seconds) : 0;
	const h = Math.floor(whole / 3600);
	const m = Math.floor((whole % 3600) / 60);
	const s = whole % 60;
	const two = function (/** @type {number} */ n) { return n < 10 ? '0' + n : String(n); };
	return h > 0 ? h + ':' + two(m) + ':' + two(s) : m + ':' + two(s);
}

/**
 * The actions a playback offers in place of the channel's, in the order drawn.
 *
 * @param {Shown} shown
 * @returns {string[]}
 */
export function tileActions(shown) {
	if (!isPlayback(shown)) {
		return [];
	}
	const out = shown.returnsTo !== '' ? ['stop'] : [];
	return shown.source === 'recording' && shown.id !== '' ? out.concat(['info', 'm3u', 'browser', 'archive']) : out;
}
