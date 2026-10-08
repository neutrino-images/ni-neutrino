// How the remote screen fetches the television picture, run without a browser.
//
// Images and timers are faked: an image is loaded or refused by hand, the way the
// box answers a capture with a picture or with 409 while another one runs.
import { readFileSync } from 'node:fs';
import { shotLoader, pictureForSaving, RETRY_MS, GIVE_UP_MS } from '../../data/ni-web/app/screens/now/shotloader.js';

let checked = 0;
let failed = 0;

/**
 * @param {unknown} got
 * @param {unknown} want
 * @param {string} what
 */
function same(got, want, what) {
	checked++;
	if (JSON.stringify(got) !== JSON.stringify(want)) {
		failed++;
		process.stderr.write('shot: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

/** @typedef {{ onload: (() => void) | null, onerror: (() => void) | null, src: string }} Img */

function rig() {
	/** @type {Img[]} */
	const images = [];
	/** @type {string[]} */
	const shown = [];
	let lost = 0;
	/** @type {Map<number, { fn: () => void, ms: number }>} */
	const timers = new Map();
	let id = 0;
	/** @param {number} i @param {'onload' | 'onerror'} how */
	function answer(i, how) {
		const img = images[i];
		same(!!img, true, 'capture ' + (i + 1) + ' was sent');
		const f = img ? img[how] : null;
		if (f)
			f();
	}
	const loader = shotLoader({
		address: function (n) { return '/shot?at=' + n; },
		shown: function (src) { shown.push(src); },
		lost: function () { lost++; },
		image: function () {
			/** @type {Img} */
			const img = { onload: null, onerror: null, src: '' };
			images.push(img);
			return img;
		},
		later: function (fn, ms) { id++; timers.set(id, { fn: fn, ms: ms }); return id; },
		cancel: function (t) { timers.delete(t); },
	});
	return {
		loader: loader,
		images: images,
		shown: shown,
		lost: function () { return lost; },
		timers: timers,
		/** @param {number} i */
		load: function (i) { answer(i, 'onload'); },
		/** @param {number} i */
		refuse: function (i) { answer(i, 'onerror'); },
		/** @param {number} ms the timers of this length run */
		fire: function (ms) {
			const due = Array.from(timers.entries()).filter(function (e) { return e[1].ms === ms; });
			for (const e of due)
				timers.delete(e[0]);
			for (const e of due)
				e[1].fn();
		},
		/** @param {number} ms @returns {number} timers of this length waiting */
		waiting: function (ms) {
			return Array.from(timers.values()).filter(function (t) { return t.ms === ms; }).length;
		},
	};
}

{
	// Presses while a capture runs are one capture after it, not one each.
	const r = rig();
	r.loader.want();
	r.loader.want();
	r.loader.want();
	r.loader.want();
	same(r.images.length, 1, 'asks while one runs send nothing more');
	r.load(0);
	same(r.shown, ['/shot?at=1'], 'the first picture is handed on once it arrived');
	same(r.images.length, 2, 'the asks that came meanwhile are one capture after it');
	same(r.images[1].src, '/shot?at=2', 'every capture has an address of its own');
	r.load(1);
	same(r.images.length, 2, 'nothing more is sent once the remembered ask is answered');
	same(r.shown, ['/shot?at=1', '/shot?at=2'], 'the second picture is handed on');
}

{
	// A refusal keeps what is on screen and is tried once more a little later.
	const r = rig();
	r.loader.want();
	r.load(0);
	r.loader.want();
	r.refuse(1);
	same(r.shown, ['/shot?at=1'], 'a refused capture hands nothing on, so the last picture stays');
	same(r.lost(), 0, 'a refusal with a picture on screen is not reported');
	same(r.waiting(RETRY_MS), 1, 'one retry is waiting');
	same(r.images.length, 2, 'the retry waits for its time');
	r.fire(RETRY_MS);
	same(r.images.length, 3, 'the retry is sent');
	r.refuse(2);
	same([r.images.length, r.waiting(RETRY_MS), r.lost()], [3, 0, 0], 'a refused retry is not retried, and the picture stays');
	r.loader.want();
	r.load(3);
	same(r.shown, ['/shot?at=1', '/shot?at=4'], 'the next ask brings a new picture');
}

{
	// Nothing ever arrived: only then is it said.
	const r = rig();
	r.loader.want();
	r.refuse(0);
	same(r.lost(), 0, 'the first refusal is retried before anything is said');
	r.fire(RETRY_MS);
	r.refuse(1);
	same(r.lost(), 1, 'with nothing to show and the retry refused, it is said once');
	r.loader.want();
	r.load(2);
	same(r.shown, ['/shot?at=3'], 'a later ask still brings a picture');
}

{
	// An ask while a retry waits goes now and the retry is dropped.
	const r = rig();
	r.loader.want();
	r.refuse(0);
	r.loader.want();
	same([r.images.length, r.waiting(RETRY_MS)], [2, 0], 'a fresh ask replaces the waiting retry');
	r.loader.want();
	r.refuse(1);
	same([r.images.length, r.waiting(RETRY_MS)], [3, 0], 'a refusal with an ask remembered sends that ask instead of retrying');
}

{
	// A screen that has gone hands nothing on and sends nothing more.
	const r = rig();
	r.loader.want();
	r.loader.want();
	r.loader.stop();
	r.load(0);
	same([r.shown.length, r.images.length], [0, 1], 'after stop nothing is handed on or sent');
}

{
	// A screen that has gone leaves no retry behind.
	const r = rig();
	r.loader.want();
	r.refuse(0);
	r.loader.stop();
	same(r.timers.size, 0, 'stop drops a waiting retry');
}

{
	// A capture that never answers counts as failed, so later asks are not held behind it.
	const r = rig();
	r.loader.want();
	r.load(0);
	r.loader.want();
	r.loader.want();
	same([r.images.length, r.waiting(GIVE_UP_MS)], [2, 1], 'a capture in flight is watched');
	r.fire(GIVE_UP_MS);
	same(r.images.length, 3, 'past the deadline the remembered ask is sent');
	r.load(1);
	same(r.shown, ['/shot?at=1'], 'the late answer of the given up capture is ignored');
	r.load(2);
	same([r.shown, r.waiting(GIVE_UP_MS)], [['/shot?at=1', '/shot?at=3'], 0], 'an answered capture is no longer watched');
	r.loader.want();
	r.fire(GIVE_UP_MS);
	same(r.waiting(RETRY_MS), 1, 'a capture given up on alone is retried like a refusal');
}

{
	// The element on the page is one for the life of the screen and only ever gets
	// an address that has arrived. A key on it would make every capture a new,
	// empty element, and an error handler on it would mean it still loads itself.
	const src = readFileSync(new URL('../../data/ni-web/app/screens/now/screenshot.js', import.meta.url), 'utf8');
	const live = src.slice(src.indexOf('export function useCapture'), src.indexOf('export function Display'));
	same(live.indexOf('shotLoader(') >= 0, true, 'the picture is fetched through the loader');
	const img = live.slice(live.indexOf('<img'), live.indexOf('/>', live.indexOf('<img')));
	same([/\bkey=/.test(img), /onError=|onLoad=/.test(img), /src=\$\{shot\.src\}/.test(img)], [false, false, true],
		'the live image has no key, loads nothing itself and shows what arrived');
}

// Saving the full size picture: tried once more when the box was busy.
{
	/** @type {Array<() => void>} */
	const timers = [];
	/** @type {number[]} */
	const delays = [];
	/** @param {() => void} fn @param {number} ms */
	const later = function (fn, ms) { timers.push(fn); delays.push(ms); };
	/** @param {unknown[]} answers */
	function box(answers) {
		let asked = 0;
		return {
			asked: function () { return asked; },
			get: function () {
				const a = answers[asked++];
				return a instanceof Error ? Promise.reject(a) : Promise.resolve(a);
			},
		};
	}
	/** @param {number} status */
	function refusal(status) { return Object.assign(new Error('refused'), { status: status }); }
	const settle = function () { return new Promise(function (r) { setTimeout(r, 0); }); };
	/** @param {Promise<unknown>} p @returns {Promise<unknown>} what it came to, or 'pending' */
	function outcome(p) {
		return Promise.race([p, settle().then(settle).then(function () { return 'pending'; })]);
	}
	/** @param {Promise<unknown>} p @returns {Promise<unknown>} the answer, or the status it failed with */
	function caught(p) {
		return p.then(function (v) { return v; }, function (/** @type {any} */ e) { return e && e.status; });
	}

	const once = box([refusal(409), 'png']);
	const got = caught(pictureForSaving(once.get, later));
	await settle();
	same([once.asked(), delays], [1, [RETRY_MS]], 'a busy box is asked again only after the retry delay');
	(timers.shift() || function () {})();
	same([await outcome(got), once.asked()], ['png', 2], 'the second answer is the picture saved');

	const twice = box([refusal(409), refusal(409), 'png']);
	const p2 = caught(pictureForSaving(twice.get, later));
	await settle();
	(timers.shift() || function () {})();
	same([await outcome(p2), twice.asked()], [409, 2], 'a refused retry is reported and not retried again');

	const other = box([refusal(500), 'png']);
	timers.length = 0;
	const p3 = caught(pictureForSaving(other.get, later));
	same([await outcome(p3), other.asked(), timers.length], [500, 1, 0], 'another failure is reported at once');
}

if (failed) {
	process.stderr.write('shot: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('shot: ' + checked + ' cases\n');
