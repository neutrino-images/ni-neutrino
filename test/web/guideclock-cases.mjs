// When the page asks again for the programme on air.
//
// The wall clock and the timers' clock are faked apart, as a hidden tab or a
// machine that slept has them. fetch is replaced as in store-cases.mjs.
import * as store from '../../data/ni-web/app/store.js';
import { buildUrl } from '../../data/ni-web/app/api.js';
import { watchOnAir, nextRead } from '../../data/ni-web/app/guideclock.js';

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
		process.stderr.write('guideclock: ' + what + '\n');
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

// ------------------------------------------------------------- the clocks

const realTimeout = globalThis.setTimeout;
const kStart = Date.UTC(2026, 9, 1, 12, 0, 0);
let wall = kStart;
let mono = 0;
/** @type {Array<{ id: number, at: number, fn: () => void, wait: number }>} */
let timers = [];
let nextTimer = 1;
/** Every wait the module asked for, longest first is what one case reads. */
let waits = [];

Date.now = function () { return wall; };
globalThis.setTimeout = /** @type {any} */ (function (/** @type {() => void} */ fn, /** @type {number} */ ms) {
	const wait = Math.max(0, Number(ms) || 0);
	const id = nextTimer++;
	timers.push({ id: id, at: mono + wait, fn: fn, wait: wait });
	waits.push(wait);
	return id;
});
globalThis.clearTimeout = /** @type {any} */ (function (/** @type {number} */ id) {
	timers = timers.filter(function (one) { return one.id !== id; });
});

/** A rule that asks without end shows as a hang, so it is ended here. */
function storm(/** @type {string} */ what) {
	process.stderr.write('guideclock: ' + what + ', which is a storm\n');
	process.exit(1);
}

/** Everything in flight, run out, on the real clock. */
function settle() {
	return new Promise(function (resolve) {
		realTimeout(function () { realTimeout(resolve, 0); }, 0);
	});
}

/**
 * Both clocks forward, firing every timer that comes due on the way.
 *
 * @param {number} ms
 */
async function advance(ms) {
	const until = mono + ms;
	let fired = 0;
	for (;;) {
		if (++fired > 1000) {
			storm('a thousand timers in one stretch');
		}
		timers.sort(function (a, b) { return a.at - b.at || a.id - b.id; });
		const due = timers[0];
		if (!due || due.at > until) {
			break;
		}
		timers.shift();
		wall += due.at - mono;
		mono = due.at;
		due.fn();
		await settle();
	}
	wall += until - mono;
	mono = until;
	await settle();
}

/**
 * The wall clock alone, which is a machine asleep or a tab whose timers the
 * browser holds back.
 *
 * @param {number} ms
 */
function asleep(ms) {
	wall += ms;
}

// ---------------------------------------------------------- the document

/** @type {Record<string, Array<() => void>>} */
let heard = {};
const page = {
	visibilityState: 'visible',
	/**
	 * @param {string} type
	 * @param {() => void} fn
	 */
	addEventListener: function (type, fn) { (heard[type] = heard[type] || []).push(fn); },
	/**
	 * @param {string} type
	 * @param {() => void} fn
	 */
	removeEventListener: function (type, fn) {
		heard[type] = (heard[type] || []).filter(function (one) { return one !== fn; });
	},
};
/** @type {any} */ (globalThis).document = page;

/** @param {'visible' | 'hidden'} state */
async function becomes(state) {
	page.visibilityState = state;
	for (const fn of (heard['visibilitychange'] || []).slice()) {
		fn();
	}
	await settle();
}

// ---------------------------------------------------------------- the box

const kChannel = 'c0ffee';
const kKey = 'GET ' + buildUrl('/api/v1/epg/current', null, { channel: kChannel });

/** Every request that left, as when on the wall clock and what. */
let asked = [];
/** @type {{ status: number, body: string }} */
let says = { status: 200, body: '' };
/** @type {Array<() => void>} */
let held = [];
let holding = false;

globalThis.fetch = /** @type {any} */ (function (/** @type {string} */ url, /** @type {{ method?: string }} */ init) {
	const key = (init && init.method ? init.method : 'GET') + ' ' + url;
	asked.push({ at: wall, key: key });
	if (asked.length > 1000) {
		storm('a thousand reads in one case');
	}
	// What the box says when the answer leaves it, which for a held one is later.
	function answer() {
		const said = key === kKey ? says : { status: 202, body: '' };
		return {
			ok: said.status >= 200 && said.status < 300,
			status: said.status,
			text: function () { return Promise.resolve(said.body); },
			json: function () { return Promise.resolve(said.body === '' ? null : JSON.parse(said.body)); },
		};
	}
	if (holding && key === kKey) {
		return new Promise(function (resolve) { held.push(function () { resolve(answer()); }); });
	}
	return Promise.resolve(answer());
});

async function release() {
	holding = false;
	for (const one of held.splice(0)) {
		one();
	}
	await settle();
}

/**
 * The box on air with one programme, in seconds as the box counts.
 *
 * @param {string} title
 * @param {number} start
 * @param {number} duration
 */
function onAir(title, start, duration) {
	says = {
		status: 200,
		body: JSON.stringify({ id: '1', channel_id: kChannel, title: title, description: '', start: start, duration: duration }),
	};
}

/** @param {number} status */
function refuses(status) {
	says = {
		status: status,
		body: JSON.stringify({ type: '', title: 'no', status: status, detail: '' }),
	};
}

/** When each read of the programme on air left, in ms after the given moment. */
function readsAfter(/** @type {number} */ from) {
	return asked.filter(function (one) { return one.key === kKey && one.at >= from; })
		.map(function (one) { return one.at - from; });
}

// ---------------------------------------------------------------- a page

/** @type {{ stop: () => void, shot: any } | null} */
let seat = null;

async function open() {
	/** @type {{ stop: () => void, shot: any }} */
	const one = { stop: function () { }, shot: null };
	one.stop = watchOnAir(kChannel, function (shot) { one.shot = shot; });
	seat = one;
	await settle();
	return one;
}

async function fresh() {
	if (seat) {
		seat.stop();
		seat = null;
	}
	store.clear();
	await settle();
	timers = [];
	waits = [];
	asked = [];
	held = [];
	holding = false;
	heard = {};
	page.visibilityState = 'visible';
	wall = kStart;
	mono = 0;
}

const s0 = kStart / 1000;

// ------------------------------------------------- the rule on its own

same(nextRead({ state: 'ready', phase: '', data: null, error: null, at: 0, url: '' }, kStart, 3),
	{ due: 0, misses: 0 }, 'an answer that names no programme gives nothing to wait for');

// ------------------------------------------------- the programme ends

await fresh();
onAir('Eins', s0 - 60, 180);
let tile = await open();
same(readsAfter(kStart), [0], 'the page reads the programme once as it opens');
await advance(120000 + 1499);
same(readsAfter(kStart), [0], 'and not again while it runs, nor in the second the box still counts as its own');
onAir('Zwei', s0 + 120, 180);
await advance(1);
same(readsAfter(kStart), [0, 121500], 'and once, a second and a half after it ended');
same(tile.shot.data && tile.shot.data.title, 'Zwei', 'which every watcher of the address is handed');
await advance(180000 - 1);
same(readsAfter(kStart).length, 2, 'the next one is waited out the same way');
await advance(1);
same(readsAfter(kStart), [0, 121500, 301500], 'and read again at its own end');

// ------------------------------------- the box answers what has ended

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await advance(121500);
same(readsAfter(kStart + 121500), [0], 'the read at the end');
await advance(600000);
same(readsAfter(kStart + 121500), [0, 2000, 6000, 14000, 30000, 62000, 122000, 182000, 242000, 302000, 362000, 422000, 482000, 542000],
	'a box still answering the programme that ended is asked again after a wait that doubles, and at most once a minute');
const backAt = wall;
const late = Math.floor(backAt / 1000) - 10;
const lateEnd = (late + 180) * 1000 + 1500 - backAt;
onAir('Zwei', late, 180);
await advance(lateEnd);
same(readsAfter(backAt), [2000, lateEnd], 'until it answers the next one, which is waited out to its end');

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await advance(121500 + 2000 + 4000);
same(readsAfter(kStart + 121500), [0, 2000, 6000], 'three misses');
onAir('Zwei', s0 + 125, 180);
await advance(8000);
same(readsAfter(kStart + 121500).length, 4, 'then an answer still running');
const zweiDue = (s0 + 305) * 1000 + 1500;
await advance(zweiDue - wall + 2000);
same(readsAfter(zweiDue), [0, 2000], 'and once that is over too, the wait starts short again');

// ------------------------------------------------------ nothing on

await fresh();
onAir('Eins', s0 - 60, 180);
tile = await open();
refuses(404);
await advance(121500);
same(readsAfter(kStart), [0, 121500], 'the end is read');
is(tile.shot.state === store.FAILED, 'and the box says nothing is on');
await advance(3600000);
same(readsAfter(kStart), [0, 121500], 'after which nothing is asked for an hour');
same(timers.length, 0, 'and no timer is left standing');

await fresh();
refuses(503);
await open();
await advance(10000);
same(readsAfter(kStart), [0, 2000, 6000], 'a box that fails to answer is asked again, waiting longer each time');

// --------------------------------------- a long programme, a long sleep

await fresh();
onAir('Film', s0, 7200);
await open();
same(Math.max.apply(null, waits), 60000, 'a long programme is looked at once a minute and not left to one timer');
asleep(7200000 + 10000);
await advance(60000);
same(readsAfter(kStart).length, 2, 'so a machine that slept past the end asks within a minute of waking');

// -------------------------------------------------- a page in the back

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await becomes('hidden');
asleep(130000);
same(readsAfter(kStart).length, 1, 'a hidden page whose timers were held back has not asked');
await becomes('hidden');
same(readsAfter(kStart).length, 1, 'and does not on a change that leaves it hidden');
onAir('Zwei', s0 + 120, 180);
await becomes('visible');
same(readsAfter(kStart), [0, 130000], 'it asks the moment it is looked at again');
await advance(1000);
same(readsAfter(kStart).length, 2, 'and once');

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await becomes('visible');
await becomes('visible');
await becomes('visible');
same(readsAfter(kStart).length, 1, 'a page shown before the end asks nothing for it');
same(timers.length, 1, 'and keeps one timer however often it is shown');

// ------------------------------------- the event and the clock at once

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
holding = true;
await advance(121500);
store.invalidate('/api/v1/epg');
await settle();
onAir('Zwei', s0 + 120, 180);
await release();
same(readsAfter(kStart), [0, 121500], 'an event arriving while the clock is asking costs no second read');

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await advance(121000);
holding = true;
onAir('Zwei', s0 + 120, 180);
store.invalidate('/api/v1/epg');
await settle();
await advance(500);
await release();
same(readsAfter(kStart), [0, 121000], 'and the clock coming due while the event is asking costs none either');
await advance(178000);
same(readsAfter(kStart).length, 2, 'what that read brought is waited out like any other');

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
onAir('Zwei', s0 + 120, 180);
await advance(121500);
await advance(3000);
store.invalidate('/api/v1/epg');
await settle();
await advance(176000);
same(readsAfter(kStart), [0, 121500, 124500], 'an event after the clock read is one more read');
await advance(1000);
same(readsAfter(kStart), [0, 121500, 124500, 301500], 'and moves nothing the clock had planned');

// ----------------------------------- reloads somebody else asked for

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await advance(30000);
store.clear();
await settle();
same(readsAfter(kStart), [0, 30000], 'pulling to refresh reads once');
await advance(91499);
same(readsAfter(kStart).length, 2, 'and the end is waited out as before');
await advance(1);
same(readsAfter(kStart).length, 3, 'to the same moment');

await fresh();
onAir('Eins', s0 - 60, 180);
await open();
await advance(121500 + 2000 + 4000);
same(readsAfter(kStart + 121500), [0, 2000, 6000], 'the third miss in a row');
await store.write('POST', '/api/v1/zap', { body: { channel_id: kChannel }, touches: ['/api/v1/epg/current'] });
await settle();
await advance(7999);
same(readsAfter(kStart + 121500).length, 3, 'a write marking the address is not an answer and resets no wait');
await advance(1);
same(readsAfter(kStart + 121500), [0, 2000, 6000, 14000], 'so the doubling goes on where it was');

// ----------------------------------------------------------- stopping

await fresh();
onAir('Eins', s0 - 60, 180);
tile = await open();
tile.stop();
seat = null;
await advance(3600000);
await becomes('visible');
same(readsAfter(kStart), [0], 'a page that stopped watching asks nothing at the end');
same((heard['visibilitychange'] || []).length, 0, 'and listens to nothing');

await fresh();

process.stdout.write('guideclock: ' + checked + ' checked, ' + failed + ' failed\n');
process.exit(failed === 0 ? 0 : 1);
