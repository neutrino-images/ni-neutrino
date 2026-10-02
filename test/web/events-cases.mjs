// When the page reads the box again: on its events, and after a gap in the stream.
//
// EventSource is replaced by a stand in, and timers of a second or more are held
// until a case runs them.
import * as store from '../../data/ni-web/app/store.js';
import * as events from '../../data/ni-web/app/events.js';
import * as session from '../../data/ni-web/app/session.js';
import { refreshAll } from '../../data/ni-web/app/refresh.js';

let checked = 0;
let failed = 0;

/**
 * @param {boolean} ok
 * @param {string} what
 */
function is(ok, what) {
	checked++;
	if (!ok) {
		failed++;
		process.stderr.write('events: ' + what + '\n');
	}
}

/**
 * @param {unknown} got
 * @param {unknown} want
 * @param {string} what
 */
function same(got, want, what) {
	is(JSON.stringify(got) === JSON.stringify(want),
		what + ': got ' + JSON.stringify(got) + ', wanted ' + JSON.stringify(want));
}

// --------------------------------------------------------------- the box

/** @type {string[]} */
let asked = [];
/** @type {Map<string, string>} */
const says = new Map();

globalThis.fetch = function (url, init) {
	const key = (init && init.method ? init.method : 'GET') + ' ' + url;
	asked.push(key);
	const body = says.get(key) || '{}';
	return Promise.resolve({
		ok: true,
		status: 200,
		text: function () { return Promise.resolve(body); },
		json: function () { return Promise.resolve(JSON.parse(body)); },
	});
};

// ------------------------------------------------------------ the browser

/** @type {Record<string, Array<(event: object) => void>>} */
const handlers = {};
globalThis.window = /** @type {any} */ ({
	addEventListener: function (/** @type {string} */ type, /** @type {(event: object) => void} */ fn) {
		(handlers[type] = handlers[type] || []).push(fn);
	},
});

/** @param {string} type @param {object} event */
function windowSays(type, event) {
	for (const fn of handlers[type] || []) {
		fn(event);
	}
}

/** Every stream the page opened, in order. */
/** @type {FakeSource[]} */
const sources = [];

class FakeSource {
	/** @param {string} url */
	constructor(url) {
		this.url = url;
		this.readyState = 0;
		/** @type {Record<string, Array<(message: { data: string }) => void>>} */
		this.typed = {};
		/** @type {null | (() => void)} */
		this.onopen = null;
		/** @type {null | (() => void)} */
		this.onerror = null;
		sources.push(this);
	}

	/**
	 * @param {string} type
	 * @param {(message: { data: string }) => void} fn
	 */
	addEventListener(type, fn) {
		(this.typed[type] = this.typed[type] || []).push(fn);
	}

	/**
	 * One event the box names, as the browser hands it over.
	 *
	 * @param {string} type
	 * @param {string} data
	 */
	emit(type, data) {
		for (const fn of this.typed[type] || []) {
			fn({ data: data });
		}
	}

	close() {
		this.readyState = 2;
	}

	// The connection is up, the first time or after the browser's own retry.
	open() {
		this.readyState = 1;
		if (this.onopen) {
			this.onopen();
		}
	}

	// Lost, and the browser will try again by itself.
	drop() {
		this.readyState = 0;
		if (this.onerror) {
			this.onerror();
		}
	}

	// Refused, and the browser will not.
	refuse() {
		this.readyState = 2;
		if (this.onerror) {
			this.onerror();
		}
	}
}
globalThis.EventSource = /** @type {any} */ (FakeSource);

// The page waits seconds before it opens again; anything shorter is the store's own.
const realSetTimeout = globalThis.setTimeout;
const realClearTimeout = globalThis.clearTimeout;
/** @type {Array<{ fn: () => void, ms: number }>} */
let held = [];
globalThis.setTimeout = /** @type {any} */ (function (/** @type {() => void} */ fn, /** @type {number} */ ms) {
	if (ms >= 1000) {
		const one = { fn: fn, ms: ms };
		held.push(one);
		return one;
	}
	return realSetTimeout(fn, ms);
});
globalThis.clearTimeout = /** @type {any} */ (function (/** @type {any} */ id) {
	const at = held.indexOf(id);
	if (at !== -1) {
		held.splice(at, 1);
		return;
	}
	realClearTimeout(id);
});

function runHeld() {
	const now = held;
	held = [];
	for (const one of now) {
		one.fn();
	}
}

function settle() {
	return new Promise(function (resolve) {
		realSetTimeout(function () { realSetTimeout(resolve, 0); }, 0);
	});
}

function last() {
	return sources[sources.length - 1];
}

function openStreams() {
	return sources.filter(function (one) { return one.readyState !== 2; }).length;
}

const kVolume = 'GET /api/v1/osd/volume';

/** @param {string} key */
function timesAsked(key) {
	return asked.filter(function (one) { return one === key; }).length;
}

// --------------------------------------------------------------- the page

store.watch('GET', '/api/v1/osd/volume', null, function () {});
await settle();
same(timesAsked(kVolume), 1, 'a screen reads what it shows once');

events.start();
same(sources.length, 1, 'the page opens one stream');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'and the first open reads nothing again');

// ------------------------------------------- standby and what is playing

// Standby changes what plays without a zap, both ways.
const kCurrent = 'GET /api/v1/channels/current';
const stopCurrent = store.watch('GET', '/api/v1/channels/current', null, function () {});
await settle();
asked = [];
last().emit('standby', '{"value":1}');
await settle();
same(timesAsked(kCurrent), 1, 'going into standby reads the running channel again');
asked = [];
last().emit('standby', '{"value":0}');
await settle();
same(timesAsked(kCurrent), 1, 'and so does leaving it');
// Leaving it the box says standby and zap at once, and the zap finds the read the
// standby started still on its way.
asked = [];
last().emit('standby', '{"value":0}');
last().emit('zap', '{"channel_id":"ffffffffbe692dd5","value":0}');
await settle();
same(timesAsked(kCurrent), 2, 'a zap right behind the standby event reads the running channel once more');
stopCurrent();

// --------------------------------- the browser reconnects after one drop

asked = [];
last().drop();
await settle();
same(timesAsked(kVolume), 0, 'a drop on its own reads nothing');
is(events.streamStatus().reachable, 'and one drop does not count the box as away');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'the stream the browser opened again by itself reads everything once');
same(sources.length, 1, 'on the same stream, with no second one opened');

// ------------------------------ the browser reconnects after several drops

asked = [];
last().drop();
last().drop();
last().drop();
is(!events.streamStatus().reachable, 'several drops count the box as away');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'and coming back from that reads everything once, not once per drop');

// ------------------------------ the box refuses, and the page opens again

asked = [];
last().refuse();
same(openStreams(), 0, 'a refused stream is closed');
same(held.length, 1, 'and the page waits before it asks again');
runHeld();
same(sources.length, 2, 'then opens a new one');
last().refuse();
runHeld();
last().refuse();
runHeld();
await settle();
same(timesAsked(kVolume), 0, 'attempts that are refused read nothing');
same(openStreams(), 1, 'and leave one stream at most');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'the stream the page opened again reads everything once');

// --------------------------------------------------------- pull to refresh

asked = [];
last().drop();
const before = sources.length;
refreshAll();
same(sources.length, before + 1, 'a refresh opens the stream anew');
same(openStreams(), 1, 'and closes the one it replaces');
await settle();
same(timesAsked(kVolume), 1, 'a refresh reads everything');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'and its stream opening does not read it all a second time');

// --------------------------------------- a document kept and shown again

asked = [];
last().drop();
windowSays('pagehide', {});
same(openStreams(), 0, 'a document put aside closes its stream');
same(held.length, 0, 'and waits for nothing');
windowSays('pageshow', { persisted: true });
await settle();
same(timesAsked(kVolume), 1, 'shown again it reads everything');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'and its stream opening does not read it all a second time');

// ----------------------------------------- a caller that may not read

says.set('GET /api/v1/session', '{"authenticated":false,"level":"public"}');
await session.refresh();
asked = [];
const count = sources.length;
last().refuse();
is(events.streamStatus().denied, 'a refusal to a caller below read says so');
same(held.length, 0, 'and is not tried again');
runHeld();
same(sources.length, count, 'so no stream is opened');
same(timesAsked(kVolume), 0, 'and nothing is read');

if (failed > 0) {
	process.stderr.write('events: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
console.log('events: ' + checked + ' cases');
