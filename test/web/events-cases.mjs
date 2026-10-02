// When the page reads the box again on its events.
//
// EventSource is replaced by a stand in.
import * as store from '../../data/ni-web/app/store.js';
import * as events from '../../data/ni-web/app/events.js';

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

}
globalThis.EventSource = /** @type {any} */ (FakeSource);

function settle() {
	return new Promise(function (resolve) {
		realSetTimeout(function () { realSetTimeout(resolve, 0); }, 0);
	});
}

function last() {
	return sources[sources.length - 1];
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
stopCurrent();

if (failed > 0) {
	process.stderr.write('events: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
console.log('events: ' + checked + ' cases');
