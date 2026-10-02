/* The programme on air, read again when it ends: a web channel has no broadcast
   for the box to announce a new programme from. */

import * as store from './store.js';

const kPath = '/api/v1/epg/current';

// The box still answers a programme in the second it ends.
const kPastEndMs = 1500;

// An answer that is already over means the two clocks disagree.
const kFirstRetryMs = 2000;
const kLastRetryMs = 60000;

// A timer stands still while the machine sleeps; the wall clock does not.
const kLookMs = 60000;

/**
 * When to read again after one answer, on the wall clock, and nought for
 * never.
 *
 * @param {Web.Snapshot<Api.Result<'GET /api/v1/epg/current'>>} shot
 * @param {number} now milliseconds since the epoch
 * @param {number} misses answers in a row that gave nothing to wait for
 * @returns {{ due: number, misses: number }}
 */
export function nextRead(shot, now, misses) {
	if (shot.state === store.FAILED) {
		const status = shot.error ? shot.error.status : 0;
		// Nothing on, or not allowed: only an event changes that.
		if (status >= 400 && status < 500) {
			return { due: 0, misses: 0 };
		}
		return retry(now, misses + 1);
	}
	const event = shot.data;
	if (!event) {
		return { due: 0, misses: 0 };
	}
	const due = (event.start + event.duration) * 1000 + kPastEndMs;
	if (due > now) {
		return { due: due, misses: 0 };
	}
	return retry(now, misses + 1);
}

/**
 * @param {number} now
 * @param {number} misses
 * @returns {{ due: number, misses: number }}
 */
function retry(now, misses) {
	return { due: now + Math.min(kFirstRetryMs * Math.pow(2, misses - 1), kLastRetryMs), misses: misses };
}

/**
 * The programme on air on one channel, as store.watch hands it, read again
 * whenever the one held has ended.
 *
 * @param {string} channel the identifier the guide files it under
 * @param {(snapshot: Web.Snapshot<Api.Result<'GET /api/v1/epg/current'>>) => void} watcher
 * @returns {() => void} stops the watching
 */
export function watchOnAir(channel, watcher) {
	const options = { query: { channel: channel } };
	let due = 0;
	let misses = 0;
	let timer = 0;
	/** @type {unknown} */
	let judged = null;

	function look() {
		clearTimeout(timer);
		timer = 0;
		if (due === 0) {
			return;
		}
		const left = due - Date.now();
		if (left > 0) {
			timer = setTimeout(look, Math.min(left, kLookMs));
			return;
		}
		store.reload('GET', kPath, options).catch(function () { });
	}

	// A hidden page's timers run late, so it catches up as it comes back.
	function shown() {
		if (document.visibilityState === 'visible') {
			look();
		}
	}

	const stop = store.watch('GET', kPath, options, function (shot) {
		watcher(shot);
		const answer = shot.state === store.READY ? shot.data : (shot.state === store.FAILED ? shot.error : null);
		if (answer === null || answer === judged) {
			return;
		}
		judged = answer;
		const next = nextRead(shot, Date.now(), misses);
		due = next.due;
		misses = next.misses;
		look();
	});
	document.addEventListener('visibilitychange', shown);

	return function () {
		stop();
		clearTimeout(timer);
		document.removeEventListener('visibilitychange', shown);
	};
}
