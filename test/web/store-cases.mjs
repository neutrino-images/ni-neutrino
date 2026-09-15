// What the page believes about the box, driven without a browser.
//
// The store is the one module of this interface that is pure bookkeeping: it keys an
// address, counts who is watching it, and decides when the box is asked. Nothing in
// it reaches a document, so the whole of it runs here, and the thing it is here for
// is the half that has no visible shape at all: after a write, is what that write
// changed read again?
//
// The defect this was written for was reported from a living room. A card changed the
// picture mode, the box took it, and the card went on drawing the mode it had read
// before, because a write marked what it touched and never went back for it. It type
// checked, every screen check was green, and the only way to see it was to press the
// button.
//
// What is NOT checked here is the network below api.js. fetch is replaced, and what
// the fake is held to is the shape of a Response as this page reads one: ok, status,
// and text().
import * as store from '../../data/ni-web/app/store.js';

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
		process.stderr.write('store: ' + what + '\n');
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

/** Every request that left, in the order it left, as the store spelled it. */
let asked = [];
/** What the box answers, keyed the way asked is written. */
const says = new Map();

/**
 * @param {string} key method and address, one space between
 * @param {number} status
 * @param {string} [body]
 */
function boxSays(key, status, body) {
	says.set(key, { status: status, body: body === undefined ? '' : body });
}

globalThis.fetch = function (url, init) {
	const key = (init && init.method ? init.method : 'GET') + ' ' + url;
	asked.push(key);
	const said = says.get(key) || { status: 200, body: '{}' };
	return Promise.resolve({
		ok: said.status >= 200 && said.status < 300,
		status: said.status,
		text: function () { return Promise.resolve(said.body); },
		json: function () { return Promise.resolve(said.body === '' ? null : JSON.parse(said.body)); },
	});
};

/** Everything in flight, run out. Two turns, because a reload starts inside a promise. */
function settle() {
	return new Promise(function (resolve) {
		setTimeout(function () { setTimeout(resolve, 0); }, 0);
	});
}

/** @type {Array<() => void>} */
let watching = [];

/** A store holding nothing and a page nobody is looking at. */
function empty() {
	for (const stop of watching) {
		stop();
	}
	watching = [];
	store.clear();
	asked = [];
	says.clear();
}

/**
 * One screen looking at one address, and what it last saw.
 *
 * @param {string} method
 * @param {string} path
 * @param {object | null} options
 * @returns {{ shot: Web.Snapshot<unknown>, phases: string[] }}
 */
function looking(method, path, options) {
	/** @type {{ shot: any, phases: string[] }} */
	const seat = { shot: null, phases: [] };
	watching.push(store.watch(method, path, options, function (shot) {
		seat.shot = shot;
		seat.phases.push(shot.phase === '' ? shot.state : shot.phase);
	}));
	return seat;
}

/** @param {string} key */
function timesAsked(key) {
	let n = 0;
	for (const one of asked) {
		if (one === key) {
			n++;
		}
	}
	return n;
}

const kVideo = 'GET /api/v1/settings/video';
const kSchema = 'GET /api/v1/settings/schema';

/** One section of the settings, as the box writes one. */
function section(mode) {
	return '{"items":[{"id":"video_43mode","value":"' + mode + '"}]}';
}

// ------------------------------------------------ the defect this is for

empty();
boxSays(kVideo, 200, section('1'));
let card = looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
await settle();
same(asked, [kVideo], 'a card that opens asks its address once');
is(card.shot.state === 'ready', 'and holds what the box said');
same(card.shot.data.items[0].value, '1', 'which is letterbox');

// The box now says something else, which is what the write below makes true.
boxSays(kVideo, 200, section('2'));
await store.write('PATCH', '/api/v1/settings/{section}', {
	params: { section: 'video' },
	body: { video_43mode: '2' },
	touches: ['/api/v1/settings/'],
});
await settle();
is(timesAsked(kVideo) === 2, 'a write the box carried out is read back: ' + JSON.stringify(asked));
same(card.shot.data.items[0].value, '2', 'and the card draws full screen without anybody reloading the page');
is(card.shot.state === 'ready', 'and is settled again afterwards');

// The whole of what it touched and not only the address it wrote to. The
// declaration is under the same piece of the address, and a write can change
// what the box would offer.
empty();
boxSays(kVideo, 200, section('1'));
boxSays(kSchema, 200, '{"sections":[]}');
card = looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
const schema = looking('GET', '/api/v1/settings/schema', null);
const other = looking('GET', '/api/v1/system/info', null);
await settle();
same(asked.length, 3, 'three addresses, three requests');
await store.write('PATCH', '/api/v1/settings/{section}', {
	params: { section: 'video' },
	body: { video_43mode: '2' },
	touches: ['/api/v1/settings/'],
});
await settle();
is(timesAsked(kVideo) === 2, 'the section is asked again');
is(timesAsked(kSchema) === 2, 'and so is the declaration beside it, which the same prefix names');
is(timesAsked('GET /api/v1/system/info') === 1, 'and nothing under another prefix is');
is(other.shot.state === 'ready', 'which is left exactly as it was');

// ------------------------------------- what the box has only taken so far

empty();
boxSays('GET /api/v1/osd/volume', 200, '{"percent":30,"muted":false}');
const sound = looking('GET', '/api/v1/osd/volume', null);
await settle();
boxSays('PUT /api/v1/osd/volume', 202);
await store.write('PUT', '/api/v1/osd/volume', {
	body: { percent: 40 },
	touches: ['/api/v1/osd/volume'],
});
await settle();
is(timesAsked('GET /api/v1/osd/volume') === 1,
	'a write the box has only taken is not read back: ' + JSON.stringify(asked));
same(sound.shot.data.percent, 30, 'so the reading stands until the event says otherwise');
is(sound.shot.phase === '', 'and the mark that says a write is in flight is gone');

// And the same for the one that would read a channel list while the box is
// rebuilding it.
empty();
boxSays('GET /api/v1/channels', 200, '{"items":[]}');
looking('GET', '/api/v1/channels', null);
await settle();
boxSays('POST /api/v1/channels/reload', 202);
await store.write('POST', '/api/v1/channels/reload', {
	touches: ['/api/v1/channels', '/api/v1/bouquets'],
});
await settle();
is(timesAsked('GET /api/v1/channels') === 1, 'a channel list is not read while it is being rebuilt');

// --------------------------------------------- a write that did not happen

empty();
boxSays(kVideo, 200, section('1'));
card = looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
await settle();
boxSays('PATCH /api/v1/settings/video', 409, '{"title":"no"}');
let refused = false;
try {
	await store.write('PATCH', '/api/v1/settings/{section}', {
		params: { section: 'video' },
		body: { video_43mode: '2' },
		touches: ['/api/v1/settings/'],
	});
} catch (e) {
	refused = true;
}
await settle();
is(refused, 'a refusal reaches the caller');
is(timesAsked(kVideo) === 1, 'and reads nothing again, nothing having changed');
same(card.shot.data.items[0].value, '1', 'so the card still holds what the box holds');
is(card.shot.state === 'ready', 'and is not left marked as writing');

// ------------------------------------------- what nobody is looking at

// Dropped rather than kept, whichever of the two the write was: an entry
// nobody watches costs nothing to ask for again, and keeping it is keeping a
// picture of the box from before a change the box has taken.
empty();
boxSays('GET /api/v1/plugins', 200, '{"items":[]}');
await store.load('GET', '/api/v1/plugins');
is(store.read('GET', '/api/v1/plugins').state === 'ready', 'an address nobody watches can still be held');
boxSays('POST /api/v1/plugins/reload', 202);
await store.write('POST', '/api/v1/plugins/reload', { touches: ['/api/v1/plugins'] });
await settle();
is(store.read('GET', '/api/v1/plugins').state === 'empty',
	'a write the box took drops what nobody is watching');
is(timesAsked('GET /api/v1/plugins') === 1, 'without asking the box for it again');
await store.load('GET', '/api/v1/plugins');
is(timesAsked('GET /api/v1/plugins') === 2, 'so the next screen that wants it gets a fresh answer');

empty();
boxSays('GET /api/v1/timers', 200, '{"items":[]}');
await store.load('GET', '/api/v1/timers');
boxSays('DELETE /api/v1/timers/7', 204);
await store.write('DELETE', '/api/v1/timers/{id}', { params: { id: 7 }, touches: ['/api/v1/timers'] });
await settle();
is(store.read('GET', '/api/v1/timers').state === 'empty',
	'and a write the box carried out drops it too');
is(timesAsked('GET /api/v1/timers') === 1, 'also without asking for it');

// And a refusal drops nothing, because nothing stopped being true.
empty();
boxSays('GET /api/v1/timers', 200, '{"items":[]}');
await store.load('GET', '/api/v1/timers');
boxSays('DELETE /api/v1/timers/7', 409, '{"title":"no"}');
try {
	await store.write('DELETE', '/api/v1/timers/{id}', { params: { id: 7 }, touches: ['/api/v1/timers'] });
} catch (e) {
	// The refusal is the case; what it says is api.js's business.
}
await settle();
is(store.read('GET', '/api/v1/timers').state === 'ready', 'a refused write leaves what nobody watches alone');

// ------------------------------------------------- one question at a time

// A screen that asks again itself, which several of them do because the answer
// is worth waiting for, must not cost the box a second answer. The read the
// store starts is already in flight when the caller's own line runs, and
// reload joins it.
empty();
boxSays('GET /api/v1/bouquets', 200, '{"items":[]}');
looking('GET', '/api/v1/bouquets', null);
await settle();
boxSays('POST /api/v1/bouquets', 201, '{"id":3}');
await store.write('POST', '/api/v1/bouquets', {
	body: { name: 'Neu' },
	touches: ['/api/v1/bouquets'],
}).then(function () {
	return store.reload('GET', '/api/v1/bouquets');
});
await settle();
is(timesAsked('GET /api/v1/bouquets') === 2,
	'a screen that reloads after a write shares the read the write started: ' + JSON.stringify(asked));

// ------------------------------------------------------ what a screen sees

// The three states of one entry across a write, in order: it is ready, it is
// marked while the call is in flight, the mark goes, and the answer comes back.
// A screen greys what it is changing off the middle one.
empty();
boxSays(kVideo, 200, section('1'));
card = looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
await settle();
card.phases = [];
await store.write('PATCH', '/api/v1/settings/{section}', {
	params: { section: 'video' },
	body: { video_43mode: '2' },
	touches: ['/api/v1/settings/'],
});
await settle();
is(card.phases.indexOf('writing') === 0, 'the entry is marked as soon as the call goes: ' + JSON.stringify(card.phases));
is(card.phases.indexOf('again') > 0, 'and is asked again once it lands');
is(card.phases[card.phases.length - 1] === 'ready', 'and ends settled');

// A write that touches nothing anybody holds is a write and no more.
empty();
boxSays(kVideo, 200, section('1'));
looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
await settle();
boxSays('POST /api/v1/timers', 201, '{"id":1}');
await store.write('POST', '/api/v1/timers', { body: { kind: 'record' }, touches: ['/api/v1/timers'] });
await settle();
same(asked, [kVideo, 'POST /api/v1/timers'], 'a write whose prefixes match nothing reads nothing');

// A write that names no prefixes at all is the same thing said differently.
empty();
boxSays(kVideo, 200, section('1'));
looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
await settle();
boxSays('POST /api/v1/osd/message', 202);
await store.write('POST', '/api/v1/osd/message', { body: { text: 'hallo' } });
await settle();
same(asked, [kVideo, 'POST /api/v1/osd/message'], 'and a write that names none touches nothing');

// ------------------------------------------------ the door an event uses

// The same door, and it stays what it was: what somebody is looking at is
// asked again, what nobody is looking at is dropped.
empty();
boxSays(kVideo, 200, section('1'));
card = looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
boxSays('GET /api/v1/timers', 200, '{"items":[]}');
await store.load('GET', '/api/v1/timers');
await settle();
boxSays(kVideo, 200, section('3'));
store.invalidate('/api/v1/settings/');
store.invalidate('/api/v1/timers');
await settle();
is(timesAsked(kVideo) === 2, 'an event asks again for what is on screen');
same(card.shot.data.items[0].value, '3', 'and the screen has the new answer');
is(store.read('GET', '/api/v1/timers').state === 'empty', 'and drops what is not');

// A prefix is the beginning of an address and nothing cleverer.
empty();
boxSays(kVideo, 200, section('1'));
boxSays('GET /api/v1/settings/sections', 200, '{"items":[]}');
boxSays('GET /api/v1/system/info', 200, '{"model":"x"}');
looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
looking('GET', '/api/v1/settings/sections', null);
looking('GET', '/api/v1/system/info', null);
await settle();
store.invalidate('/api/v1/settings/');
await settle();
is(timesAsked(kVideo) === 2, 'every address under the prefix is named');
is(timesAsked('GET /api/v1/settings/sections') === 2, 'each of them');
is(timesAsked('GET /api/v1/system/info') === 1, 'and no address that merely looks like one');

// ------------------------------------------------------------- the query

// Two screens asking one address in two spellings arrive at one key, so a
// write that touches it touches both. The query is sorted by api.js; this is
// the store's half of that arrangement.
empty();
boxSays('GET /api/v1/epg?channel=7&limit=5', 200, '{"items":[]}');
looking('GET', '/api/v1/epg', { query: { channel: 7, limit: 5 } });
looking('GET', '/api/v1/epg', { query: { limit: 5, channel: 7 } });
await settle();
is(timesAsked('GET /api/v1/epg?channel=7&limit=5') === 1, 'one request for two spellings of one question');
boxSays('POST /api/v1/timers', 201, '{"id":2}');
await store.write('POST', '/api/v1/timers', { body: { kind: 'record' }, touches: ['/api/v1/epg'] });
await settle();
is(timesAsked('GET /api/v1/epg?channel=7&limit=5') === 2, 'and one read again for both of them');

// A screen that opened while the call was in flight is one of the screens the
// write touched, although it was not there when the marking was done.
empty();
boxSays('PATCH /api/v1/settings/video', 200, section('2'));
boxSays(kVideo, 200, section('2'));
const running = store.write('PATCH', '/api/v1/settings/{section}', {
	params: { section: 'video' },
	body: { video_43mode: '2' },
	touches: ['/api/v1/settings/'],
});
card = looking('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
await running;
await settle();
is(timesAsked(kVideo) >= 1, 'a screen that arrived mid write is answered');
same(card.shot.data.items[0].value, '2', 'with what the box says now');

// And what the box answered reaches the caller as it wrote it, whatever the
// store then does with the addresses around it.
empty();
boxSays('PUT /api/v1/storage/netfs/fstab/0', 200, '{"entries":[{"slot":0}]}');
const answer = await store.write('PUT', '/api/v1/storage/netfs/{table}/{slot}', {
	params: { table: 'fstab', slot: 0 },
	body: { host: 'nas' },
	touches: ['/api/v1/storage/netfs'],
});
same(answer, { entries: [{ slot: 0 }] }, 'the answer to a write is handed back untouched');

// ------------------------------------------------------------------ verdict

const FLOOR = 40;
if (checked < FLOOR) {
	process.stderr.write('store-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('store-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-store.sh: ' + checked + ' assertions over what a write leaves behind, what an event throws away and what the box is asked twice for\n');
