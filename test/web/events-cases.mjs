// When the page reads the box again: on its events, and after a gap in the stream.
//
// EventSource is replaced by a stand in, and timers of a second or more are held
// until a case runs them.
import * as store from '../../data/ni-web/app/store.js';
import * as events from '../../data/ni-web/app/events.js';
import * as session from '../../data/ni-web/app/session.js';
import { watchApplyFailed } from '../../data/ni-web/app/screens/settings/applyfailed.js';
import { setLanguage } from '../../data/ni-web/app/i18n.js';
import { readFileSync } from 'node:fs';
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
says.set('GET /api/v1/session', '{"authenticated":false,"level":"read"}');

/** What the box refuses, and with which status. */
/** @type {Map<string, number>} */
const refuses = new Map();

globalThis.fetch = function (url, init) {
	const key = (init && init.method ? init.method : 'GET') + ' ' + url;
	asked.push(key);
	const status = refuses.get(key) || 200;
	const body = status === 200 ? (says.get(key) || '{}') : '{"status":' + status + ',"title":"no"}';
	return Promise.resolve({
		ok: status === 200,
		status: status,
		text: function () { return Promise.resolve(body); },
		json: function () { return Promise.resolve(JSON.parse(body)); },
	});
};

// ------------------------------------------------------------ the browser

/** @type {Record<string, Array<(event: object) => void>>} */
const handlers = {};
/** Every delay a case asked the window to wait, in order. */
/** @type {Array<number>} */
const alarms = [];
globalThis.window = /** @type {any} */ ({
	addEventListener: function (/** @type {string} */ type, /** @type {(event: object) => void} */ fn) {
		(handlers[type] = handlers[type] || []).push(fn);
	},
	setTimeout: function (/** @type {() => void} */ fn, /** @type {number} */ ms) {
		alarms.push(ms);
		return alarms.length;
	},
	clearTimeout: function () {},
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
same(timesAsked('GET /api/v1/session'), 0, 'nor asks about the session');

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

// A playback starting, pausing or ending changes what plays.
const kPlayback = 'GET /api/v1/playback';
const stopPlayback = store.watch('GET', '/api/v1/playback', null, function () {});
const stopCurrentToo = store.watch('GET', '/api/v1/channels/current', null, function () {});
await settle();
asked = [];
last().emit('playback', '{"channel_id":"0","value":12,"text":"paused recording dd27cf8d43d5a28f"}');
await settle();
same([timesAsked(kPlayback), timesAsked(kCurrent)], [1, 1],
	'a playback event reads what plays and the running channel again');
stopPlayback();
stopCurrentToo();
// And the archive, whose mark says which recording plays on the TV.
const kArchive = 'GET /api/v1/recordings/archive';
const stopArchive = store.watch('GET', '/api/v1/recordings/archive', null, function () {});
await settle();
asked = [];
last().emit('playback', '{"channel_id":"0","value":0,"text":"stopped recording dd27cf8d43d5a28f"}');
await settle();
same(timesAsked(kArchive), 1, 'a playback event reads the archive again');
stopArchive();

// The box could not put a written setting in force: the keys and the reason reach a
// listener, and the settings are read again because they may not be what was written.
const kSettings = 'GET /api/v1/settings/schema';
const stopSchema = store.watch('GET', '/api/v1/settings/schema', null, function () {});
await settle();
asked = [];
/** @type {any[]} */
const failures = [];
const stopFailures = events.on('setting-apply-failed', function (event) { failures.push(event); });
last().emit('setting-apply-failed', '{"keys":["video_Mode","x",3],"status":500,"detail":"no"}');
await settle();
same(failures.map(function (e) { return [e.keys, e.status, e.detail]; }), [[['video_Mode', 'x'], 500, 'no']],
	'a setting-apply-failed event hands over its keys, status and detail');
same(timesAsked(kSettings), 1, 'and the declaration is read again');
last().emit('zap', '{"channel_id":"0","value":0}');
same(failures.length, 1, 'another type is not one');
stopFailures();
stopSchema();

// The front displays come and go with glcd_enable and the LCD4Linux settings.
const kDisplays = 'GET /api/v1/osd/displays';
const stopDisplays = store.watch('GET', '/api/v1/osd/displays', null, function () {});
await settle();
asked = [];
last().emit('settings-changed', '{"channel_id":"0","value":0,"text":"glcd_enable"}');
await settle();
same(timesAsked(kDisplays), 1, 'a settings change reads the list of displays again');
asked = [];
last().emit('setting-apply-failed', '{"keys":["glcd_enable"],"status":500,"detail":"no"}');
await settle();
same(timesAsked(kDisplays), 1, 'and so does a setting that could not be put in force');
stopDisplays();

// --------------------------------- the browser reconnects after one drop

asked = [];
last().drop();
await settle();
same(timesAsked(kVolume), 0, 'a drop on its own reads nothing');
is(events.streamStatus().reachable, 'and one drop does not count the box as away');
last().open();
await settle();
same(timesAsked(kVolume), 1, 'the stream the browser opened again by itself reads everything once');
same(timesAsked('GET /api/v1/session'), 1, 'and asks the box once whether the session survived the gap');
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
same(timesAsked('GET /api/v1/session'), 1, 'and asks about the session once');

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

const alarmsBefore = alarms.length;
says.set('GET /api/v1/session', '{"authenticated":true,"level":"system","expires_in":2592000}');
await session.refresh();
same(alarms.length, alarmsBefore + 1, 'a thirty day session sets one alarm');
same(alarms[alarms.length - 1], 2147483647, 'as long as the browser can hold');

// A sign in after a refusal.

const kGuides = 'GET /api/v1/ai/guides';
const kAllow = 'GET /api/v1/ai/allowlists';
const kMissing = 'GET /api/v1/channels/0';
says.set('GET /api/v1/session', '{"authenticated":false,"level":"read"}');
await session.refresh();
refuses.set(kGuides, 403);
refuses.set(kAllow, 403);
refuses.set(kMissing, 404);
/** @type {any} */
let guides = null;
/** @type {any} */
let missing = null;
const stopGuides = store.watch('GET', '/api/v1/ai/guides', null, function (shot) { guides = shot; });
const stopMissing = store.watch('GET', '/api/v1/channels/{id}', { params: { id: '0' } }, function (shot) { missing = shot; });
store.watch('GET', '/api/v1/ai/allowlists', null, function () {})();
await settle();
same([guides.state, missing.state, store.read('GET', '/api/v1/ai/allowlists').state], ['error', 'error', 'error'],
	'what the box refuses a reading caller is held as refused');
refuses.clear();
asked = [];
says.set('GET /api/v1/session', '{"authenticated":true,"level":"system"}');
await session.refresh();
same([guides.state, guides.phase, guides.error], ['loading', 'first', null],
	'signing in reads a refusal for want of rights again, from nothing held');
await settle();
same(timesAsked(kGuides), 1, 'once');
same(guides.state, 'ready', 'and draws its answer');
same(timesAsked(kMissing), 0, 'a refusal that is not about rights is not asked again');
same(missing.state, 'error', 'and stays as it was');
same([timesAsked(kAllow), store.read('GET', '/api/v1/ai/allowlists').state], [0, 'empty'],
	'one nobody is looking at is not asked, and is nothing known, so the next screen asks afresh');
refuses.set(kGuides, 403);
store.reload('GET', '/api/v1/ai/guides').catch(function () {});
await settle();
asked = [];
await session.refresh();
await settle();
same([timesAsked(kGuides), guides.state], [0, 'error'], 'the same session told again asks nothing again');
refuses.clear();
stopGuides();
stopMissing();

// ------------------------------ the apply failure reaches the page's toast
says.set('GET /api/v1/settings/schema', '{"items":[{"id":"lcd4l_support","type":"bool","section":"x","label":"LCD4Linux","conditions":[]}]}');
/** @type {any[]} */
const shown = [];
globalThis.document = /** @type {any} */ ({ documentElement: {} });
setLanguage('en');
const stopWatching = watchApplyFailed(function (message, kind) { shown.push([message, kind]); });
events.reopen();
last().open();
await settle();
last().emit('setting-apply-failed', '{"keys":["lcd4l_support","zz"],"status":500,"detail":"script failed"}');
await settle();
same(shown, [['The box stored LCD4Linux, zz but could not put it in force: script failed', 'bad']],
	'an apply failure reaches the toast with the settings named by their labels');
stopWatching();
last().emit('setting-apply-failed', '{"keys":["lcd4l_support"],"status":500,"detail":"again"}');
await settle();
same(shown.length, 1, 'and stops when the watching stops');
is(/watchApplyFailed\(toast\)/.test(readFileSync(new URL('../../data/ni-web/app/main.js', import.meta.url), 'utf8')),
	'the page starts the watching with its toast');

// ----------------------------- the stream follows who is signed in
// The box sends some events only to the stream of the session that wrote, and tags a stream
// with the session it was opened under. One opened before signing in, or under a session
// that ran out, would never get them.
says.set('GET /api/v1/session', '{"authenticated":false,"level":"read"}');
await session.refresh();
await settle();
events.reopen();
last().open();
await settle();
let streams = sources.length;
same(openStreams(), 1, 'a stream is open for a caller holding nothing');
says.set('GET /api/v1/session', '{"authenticated":true,"level":"system","user":"root","csrf":"one"}');
await session.refresh();
await settle();
same([sources.length, openStreams()], [streams + 1, 1], 'signing in opens a new stream and closes the old');
last().open();
streams = sources.length;
await session.refresh();
await settle();
same(sources.length, streams, 'the same session told again keeps its stream');
says.set('GET /api/v1/session', '{"authenticated":true,"level":"system","user":"root","csrf":"two"}');
await session.refresh();
await settle();
same([sources.length, openStreams()], [streams + 1, 1], 'a renewed session, same user and level, opens a new stream');
last().open();
streams = sources.length;
says.set('GET /api/v1/session', '{"authenticated":false,"level":"read"}');
await session.refresh();
await settle();
same([sources.length, openStreams()], [streams + 1, 1], 'and so does signing out');

if (failed > 0) {
	process.stderr.write('events: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
console.log('events: ' + checked + ' cases');
