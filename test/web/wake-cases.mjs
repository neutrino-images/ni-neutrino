// What the page does when the box refuses a zap, a mode change or a play for want of leave:
// in standby, while a file plays, while a recording holds the tuner.
//
// The runtime is stubbed and the module is not, as in decide-cases.mjs. The stub
// runs an effect at once and records every value the question is drawn with.
import * as loader from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join } from 'node:path';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('wake-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const here = dirname(fileURLToPath(import.meta.url));
const web = join(here, '..', '..', 'data', 'ni-web');

/** Every question shown, oldest first, null for a closed one. */
globalThis.__wakeShown = [];

const kStubs = {
	'/vendor/preact.module.js':
		'export function h() { return null; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	// What a template was last drawn with, so the words of the question can be read back.
	'/vendor/htm.module.js':
		'export default { bind: function () { return function () {\n' +
		'\tglobalThis.__wakeDrawn = Array.prototype.slice.call(arguments, 1);\n' +
		'\t(globalThis.__wakeDrawnAll = globalThis.__wakeDrawnAll || []).push(globalThis.__wakeDrawn); return null; }; } };\n',
	'/vendor/hooks.module.js':
		'export function useState(v) {\n' +
		'\tconst k = globalThis.__wakeSlots;\n' +
		'\tif (!k) return [v, function (n) { globalThis.__wakeShown.push(n); }];\n' +
		'\tconst i = k.at++;\n' +
		'\tif (!(i in k.vals)) k.vals[i] = v;\n' +
		'\treturn [k.vals[i], function (n) { k.vals[i] = n; globalThis.__wakeShown.push(n); }];\n' +
		'}\n' +
		'export function useEffect(fn) { fn(); }\n' +
		['useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId']
			.map(function (name) { return 'export function ' + name + '() {}\n'; }).join(''),
	'/vendor/preact-router.module.js':
		'export default function Router() { return null; }\n' +
		'export function Link() { return null; }\n' +
		'export function route() {}\n' +
		'export function getCurrentUrl() { return ""; }\n',
};

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (Object.prototype.hasOwnProperty.call(kStubs, spec)) {
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		}
		if (spec.indexOf('/vendor/') === 0) {
			throw new Error('wake-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

// --------------------------------------------------------------- the box

/** Every request that left: method, address and the body as sent. */
let sent = [];
/** What the box answers next, one per request, and 202 once they run out. */
let answers = [];

/**
 * @param {string} code
 * @returns {{ status: number, body: string }}
 */
function refusal(code) {
	return {
		status: 409,
		body: JSON.stringify({ type: '/errors/' + code, title: 'Conflict', status: 409, detail: code }),
	};
}

globalThis.fetch = function (url, init) {
	sent.push({
		method: init && init.method ? init.method : 'GET',
		url: String(url),
		body: init && typeof init.body === 'string' ? JSON.parse(init.body) : null,
	});
	const said = answers.length ? answers.shift() : { status: 202, body: '' };
	return Promise.resolve({
		ok: said.status >= 200 && said.status < 300,
		status: said.status,
		headers: { get: function () { return null; } },
		text: function () { return Promise.resolve(said.body); },
		json: function () { return Promise.resolve(said.body === '' ? null : JSON.parse(said.body)); },
	});
};

const wake = await import(pathToFileURL(join(web, 'app', 'ui', 'wake.js')).href);

// The component subscribes once, the way the page mounts it once.
wake.WakeQuestion();

// --------------------------------------------------------------- the tally

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
		process.stderr.write('wake: ' + what + '\n');
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

function settle() {
	return new Promise(function (resolve) {
		setTimeout(function () { setTimeout(resolve, 0); }, 0);
	});
}

/** Whether a question was open, for every time it was drawn. */
function shown() {
	return globalThis.__wakeShown.map(Boolean);
}

/** The topic of the question shown last. */
function topic() {
	const last = globalThis.__wakeShown[globalThis.__wakeShown.length - 1];
	return last ? last.topic : '';
}

function fresh() {
	sent = [];
	answers = [];
	globalThis.__wakeShown.length = 0;
}

/** The posts that left, as path and body. */
function posts() {
	return sent.filter(function (one) { return one.method === 'POST'; }).map(function (one) {
		return [one.url.replace(/^https?:\/\/[^/]+/, '').replace(/\?.*$/, ''), one.body];
	});
}

/**
 * Where a promise stands after everything in flight has run.
 *
 * @param {Promise<unknown>} p
 * @returns {{ state: string, value?: unknown, error?: unknown }}
 */
function watch(p) {
	const seat = { state: 'pending' };
	p.then(function (v) { seat.state = 'resolved'; seat.value = v; },
		function (e) { seat.state = 'rejected'; seat.error = e; });
	return seat;
}

// ------------------------------------------------------- an awake box

fresh();
let r = watch(wake.zap('ffffffffbe692dd5'));
await settle();
same(posts(), [['/api/v1/zap', { channel_id: 'ffffffffbe692dd5', wake: false }]],
	'an awake box is sent the zap once, without leave to wake');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');
same(shown(), [], 'and nobody is asked anything');

// ------------------------------------------------ a box in standby, yes

fresh();
answers = [refusal('box-in-standby')];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(shown(), [true], 'a box in standby opens the question');
is(r.state === 'pending', 'and the caller waits for the answer');
same(posts().length, 1, 'with nothing sent a second time before it');
wake.answer(true);
await settle();
same(posts(), [
	['/api/v1/zap', { channel_id: 'ffffffff48deb591', wake: false }],
	['/api/v1/zap', { channel_id: 'ffffffff48deb591', wake: true }],
], 'a yes sends the same zap again, now with leave to wake');
same(shown(), [true, false], 'and closes the question');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

// ------------------------------------------------- a box in standby, no

fresh();
answers = [refusal('box-in-standby')];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
wake.answer(false);
await settle();
same(posts().length, 1, 'a no sends nothing more');
same(r.state + ':' + r.value, 'resolved:false', 'and tells the caller nothing was sent, which is not a failure');
same(shown(), [true, false], 'and closes the question');

// ------------------------------------------------- any other refusal

fresh();
answers = [refusal('no-such-channel')];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(shown(), [], 'a refusal for any other reason asks nothing');
same(posts().length, 1, 'and sends nothing more');
is(r.state === 'rejected', 'and is handed back to the caller');
same(r.error && r.error.problem ? r.error.problem.type : '', '/errors/no-such-channel',
	'as the box wrote it');

// ------------------------------------------------- two at once

fresh();
answers = [refusal('box-in-standby'), refusal('box-in-standby')];
const first = watch(wake.zap('ffffffffbe692dd5'));
const second = watch(wake.zap('ffffffff48deb591'));
await settle();
same(shown(), [true, true], 'two refusals at once keep the one question open');
wake.answer(true);
await settle();
same(posts().slice(2), [
	['/api/v1/zap', { channel_id: 'ffffffffbe692dd5', wake: true }],
	['/api/v1/zap', { channel_id: 'ffffffff48deb591', wake: true }],
], 'and its one answer sends both again');
is(first.state === 'resolved' && second.state === 'resolved', 'and both callers are told');

// ------------------------------------------------- the mode

fresh();
r = watch(wake.switchMode('radio'));
await settle();
same(posts(), [['/api/v1/mode', { mode: 'radio', wake: false }]],
	'an awake box is sent the mode once, without leave to wake');

fresh();
answers = [refusal('box-in-standby')];
r = watch(wake.switchMode('tv'));
await settle();
wake.answer(true);
await settle();
same(posts(), [
	['/api/v1/mode', { mode: 'tv', wake: false }],
	['/api/v1/mode', { mode: 'tv', wake: true }],
], 'a mode change in standby asks the same question and sends again on a yes');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

// A recording on the TV.

const kPlay = '/api/v1/recordings/archive/dd27cf8d43d5a28f/play';

/**
 * @param {boolean} wakeIt
 * @param {boolean} stop
 */
function play(wakeIt, stop) {
	const body = stop ? { wake: wakeIt, stop_playback: true } : { wake: wakeIt };
	return fetch(kPlay, { method: 'POST', body: JSON.stringify(body) }).then(function (res) {
		return res.ok ? null : res.json().then(function (problem) { throw { problem: problem }; });
	});
}

fresh();
r = watch(wake.waking(play, 'play'));
await settle();
same(posts(), [[kPlay, { wake: false }]], 'an awake box is sent the play once, without leave to wake');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

fresh();
answers = [refusal('box-in-standby')];
r = watch(wake.waking(play, 'play'));
await settle();
same(shown(), [true], 'a play refused in standby opens the same question');
wake.answer(true);
await settle();
same(posts(), [[kPlay, { wake: false }], [kPlay, { wake: true }]],
	'and a yes sends the play again, now with leave to wake');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

fresh();
answers = [refusal('box-in-standby')];
r = watch(wake.waking(play, 'play'));
await settle();
wake.answer(false);
await settle();
same(posts().length, 1, 'a no to the play sends nothing more');
same(r.state + ':' + r.value, 'resolved:false', 'and tells the caller nothing was sent');

// A file playing

fresh();
answers = [refusal('playback-running')];
r = watch(wake.zap('ffffffffbe692dd5'));
await settle();
same(shown(), [true], 'a file playing opens a question');
same(topic(), 'playback', 'about ending the playback');
is(r.state === 'pending', 'and the caller waits for the answer');
wake.answer(true);
await settle();
same(posts(), [
	['/api/v1/zap', { channel_id: 'ffffffffbe692dd5', wake: false }],
	['/api/v1/zap', { channel_id: 'ffffffffbe692dd5', wake: false, stop_playback: true }],
], 'a yes sends the same zap again, now with leave to end the playback');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

fresh();
answers = [refusal('playback-running')];
r = watch(wake.zap('ffffffffbe692dd5'));
await settle();
wake.answer(false);
await settle();
same(posts().length, 1, 'a no to ending the playback sends nothing more');
same(r.state + ':' + r.value, 'resolved:false', 'and tells the caller nothing was sent');

fresh();
answers = [refusal('playback-running'), refusal('box-in-standby')];
r = watch(wake.zap('ffffffffbe692dd5'));
await settle();
wake.answer(true);
await settle();
same(topic(), 'wake', 'a box that sleeps as well asks the second question after the first');
wake.answer(true);
await settle();
same(posts().slice(2), [['/api/v1/zap', { channel_id: 'ffffffffbe692dd5', wake: true, stop_playback: true }]],
	'and sends once more with both leaves');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

fresh();
answers = [refusal('playback-running'), refusal('playback-running')];
r = watch(wake.zap('ffffffffbe692dd5'));
await settle();
wake.answer(true);
await settle();
same(posts().length, 2, 'a playback refused again after the leave is not asked about twice');
is(r.state === 'rejected', 'and is handed back to the caller');

fresh();
answers = [refusal('playback-running')];
r = watch(wake.waking(play, 'play'));
await settle();
same(topic(), 'playback', 'a play while a file plays asks about ending the playback');
wake.answer(true);
await settle();
same(posts(), [[kPlay, { wake: false }], [kPlay, { wake: false, stop_playback: true }]],
	'and a yes sends the play again with leave to end it');

fresh();
r = watch(wake.switchMode('tv'));
await settle();
same(posts(), [['/api/v1/mode', { mode: 'tv', wake: false }]], 'a mode change never asks to end a playback');

// A recording on the tuner

/**
 * @param {number} id
 * @param {string} title
 */
function recording(id, title) {
	return { id: id, channel_id: 'ffffffffbe692dd5', title: title, start: 1790000000, path: '/r/' + id + '.ts',
		timeshift: false, started_by: 'timer' };
}

/** @param {object[]} items */
function listing(items) {
	return { status: 200, body: JSON.stringify({ items: items }) };
}

/** Every request that left, as method and path. */
function calls() {
	return sent.map(function (one) {
		return one.method + ' ' + one.url.replace(/^https?:\/\/[^/]+/, '').replace(/\?.*$/, '');
	});
}

fresh();
answers = [refusal('recording-holds-tuner'), listing([recording(7, 'Tatort')]), { status: 202, body: '' },
	listing([])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(topic(), 'recording', 'a recording on the tuner opens the question about ending it');
same(globalThis.__wakeShown[0].recordings.map(function (one) { return one.id; }), [7],
	'with the one recording running');
wake.answer(true, 7);
await settle();
await settle();
same(calls(), ['POST /api/v1/zap', 'GET /api/v1/recordings', 'DELETE /api/v1/recordings/7',
	'GET /api/v1/recordings', 'POST /api/v1/zap'], 'a yes ends it, waits for it to be gone and sends again');
same(posts()[1], ['/api/v1/zap', { channel_id: 'ffffffff48deb591', wake: false }], 'the same zap');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

fresh();
answers = [refusal('recording-holds-tuner'), listing([recording(7, 'Tatort'), recording(9, 'Lanz')]),
	{ status: 202, body: '' }, listing([recording(7, 'Tatort')])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(globalThis.__wakeShown[0].recordings.map(function (one) { return one.id; }), [7, 9],
	'several running are all offered');
globalThis.__wakeDrawnAll = [];
wake.WakeQuestion();
const which = globalThis.__wakeDrawnAll.filter(function (values) {
	return values.some(function (v) {
		return v && typeof v === 'object' && Array.isArray(/** @type {{ recordings?: unknown }} */ (v).recordings);
	});
});
is(which.length === 1, 'and drawn as a choice');
wake.answer(true, 9);
await settle();
await settle();
same(calls().slice(2), ['DELETE /api/v1/recordings/9', 'GET /api/v1/recordings', 'POST /api/v1/zap'],
	'the one chosen is ended');

fresh();
answers = [refusal('recording-holds-tuner'), listing([recording(7, 'Tatort')])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
wake.answer(false);
await settle();
same(calls(), ['POST /api/v1/zap', 'GET /api/v1/recordings'], 'a no ends nothing and sends nothing more');
same(r.state + ':' + r.value, 'resolved:false', 'and tells the caller nothing was sent');

// A recording numbered 0 is a recording like any other.
fresh();
answers = [refusal('recording-holds-tuner'), listing([recording(0, 'Tatort')]), { status: 202, body: '' },
	listing([])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
wake.answer(true, 0);
await settle();
await settle();
same(calls(), ['POST /api/v1/zap', 'GET /api/v1/recordings', 'DELETE /api/v1/recordings/0',
	'GET /api/v1/recordings', 'POST /api/v1/zap'], 'a yes for recording 0 ends that one');
same(r.state + ':' + r.value, 'resolved:true', 'and the caller is told the box has it');

// With 0 among several, the first is still the one offered.
fresh();
answers = [refusal('recording-holds-tuner'), listing([recording(5, 'Lanz'), recording(0, 'Tatort')])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
globalThis.__wakeDrawnAll = [];
wake.WakeQuestion();
const offered = globalThis.__wakeDrawnAll.filter(function (values) {
	return values.some(function (v) {
		return v && typeof v === 'object' && Array.isArray(/** @type {{ recordings?: unknown }} */ (v).recordings);
	});
})[0] || [];
const at = offered.findIndex(function (v) {
	return v && typeof v === 'object' && Array.isArray(/** @type {{ recordings?: unknown }} */ (v).recordings);
});
same(offered[at + 1], 5, 'the first recording is chosen until another is');
wake.answer(false);
await settle();

// A choice belongs to its question; the next one opens on its first again.
/** @returns {unknown[]} what the choice was drawn with, rendered with kept state */
function drawnChoice() {
	globalThis.__wakeSlots.at = 0;
	globalThis.__wakeDrawnAll = [];
	wake.WakeQuestion();
	const values = globalThis.__wakeDrawnAll.filter(function (all) {
		return all.some(function (v) {
			return v && typeof v === 'object' && Array.isArray(/** @type {{ recordings?: unknown }} */ (v).recordings);
		});
	})[0] || [];
	const from = values.findIndex(function (v) {
		return v && typeof v === 'object' && Array.isArray(/** @type {{ recordings?: unknown }} */ (v).recordings);
	});
	return from === -1 ? [] : values.slice(from + 1);
}
fresh();
globalThis.__wakeSlots = { at: 0, vals: [] };
answers = [refusal('recording-holds-tuner'), listing([recording(7, 'Tatort'), recording(9, 'Lanz')])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
const choose = /** @type {(id: number) => void} */ (drawnChoice()[1]);
choose(9);
same(drawnChoice()[0], 9, 'a choice is drawn while its question is up');
wake.answer(false);
await settle();
answers = [refusal('recording-holds-tuner'), listing([recording(7, 'Tatort'), recording(9, 'Lanz')])];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(drawnChoice()[0], 7, 'the next question opens on its first recording');
wake.answer(false);
await settle();
globalThis.__wakeSlots = null;

// ------------------------------------------------- the words

const words = (await import(pathToFileURL(join(web, 'app', 'ui', 'wake.text.js')).href)).default;

/**
 * The question as drawn while one asker of that kind waits.
 *
 * @param {'switch' | 'play'} kind
 * @returns {Promise<unknown[]>}
 */
async function drawnFor(kind) {
	answers = [refusal('box-in-standby')];
	const one = watch(wake.waking(play, kind));
	await settle();
	wake.WakeQuestion();
	const drawn = globalThis.__wakeDrawn || [];
	wake.answer(false);
	await settle();
	is(one.state === 'resolved', 'the question for ' + kind + ' is answered');
	return drawn;
}

fresh();
is((await drawnFor('play')).indexOf(words.de['wake.ask.play']) !== -1,
	'a play asks whether to switch on and play');
is((await drawnFor('switch')).indexOf(words.de['wake.ask']) !== -1,
	'a zap or a mode change still asks whether to switch on and switch over');
same(words.de['wake.ask.play'], 'Die Box ist im Standby. Einschalten und abspielen?',
	'the play question is the one agreed on');

same(words.de['wake.ask'], 'Die Box ist im Standby. Einschalten und umschalten?',
	'the question is the one agreed on');
same(words.de['wake.playback.ask'], 'Wiedergabe beenden und umschalten?', 'the playback questions are the ones agreed on');
same(words.de['wake.playback.ask.play'], 'Wiedergabe beenden und abspielen?', 'for a play as well');
same(words.de['wake.recording.ask'],
	'Eine Aufnahme belegt den Tuner für diesen Sender. Aufnahme beenden und umschalten?',
	'the recording question is the one agreed on');

/**
 * The words drawn for one refusal and kind.
 *
 * @param {string} code
 * @param {'switch' | 'play'} kind
 * @returns {Promise<unknown[]>}
 */
async function drawnOn(code, kind) {
	answers = [refusal(code)];
	const one = watch(wake.waking(play, kind));
	await settle();
	wake.WakeQuestion();
	const drawn = globalThis.__wakeDrawn || [];
	wake.answer(false);
	await settle();
	is(one.state === 'resolved', 'the question on ' + code + ' is answered');
	return drawn;
}

is((await drawnOn('playback-running', 'switch')).indexOf(words.de['wake.playback.ask']) !== -1,
	'a zap asks whether to end the playback and switch over');
is((await drawnOn('playback-running', 'play')).indexOf(words.de['wake.playback.ask.play']) !== -1,
	'a play asks whether to end the playback and play');

if (checked === 0) {
	process.stderr.write('wake-cases.mjs: nothing was checked\n');
	process.exit(1);
}
if (failed) {
	process.stderr.write('wake-cases.mjs: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('wake-cases.mjs: ' + checked + ' checked\n');
