// The box's clock as the page reads it off the Date header, and the record button
// on the Now screen judged by it. The device here is three minutes behind the box.
import * as loader from 'node:module';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('boxclock-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const kStubs = {
	'/vendor/preact.module.js':
		'export function h() { return null; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	'/vendor/htm.module.js':
		'export default { bind: function () { return function () { return null; }; } };\n',
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId']
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
			throw new Error('boxclock-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

globalThis.document = /** @type {any} */ ({ documentElement: {} });

const clock = await import('../../data/ni-web/app/boxclock.js');
const { api } = await import('../../data/ni-web/app/api.js');
const { programmeEnd, recordNowBody } = await import('../../data/ni-web/app/screens/now/overview.js');

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
		process.stderr.write('boxclock: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

/**
 * @param {boolean} ok
 * @param {string} what
 */
function is(ok, what) {
	same(ok, true, what);
}

const BEHIND_MS = 180000;

/**
 * The header the box writes at a moment of its own clock.
 * @param {number} ms
 * @returns {string}
 */
function header(ms) {
	return new Date(ms).toUTCString();
}

// ------------------------------------------------------------ the estimate

{
	clock.forgetBoxDate();
	same(clock.boxOffsetMs(), 0, 'with no answer read the device clock stands for the box');

	const sent = Date.now();
	const got = sent + 40;
	// Written at the middle of the trip, at the start of its second.
	const boxMs = Math.floor((sent + 20 + BEHIND_MS) / 1000) * 1000;
	is(clock.noteBoxDate(header(boxMs), sent, got), true, 'a Date header is read');
	is(Math.abs(clock.boxOffsetMs() - BEHIND_MS) <= 1000, 'and the box is three minutes ahead to within a second');
	is(Math.abs(clock.boxNowSeconds() - Math.floor((Date.now() + BEHIND_MS) / 1000)) <= 1,
		'boxNowSeconds counts the box\'s time');

	const was = clock.boxOffsetMs();
	is(clock.noteBoxDate(header(boxMs + 1000), sent, got), true, 'a reading a second later is read');
	is(clock.boxOffsetMs() > was && clock.boxOffsetMs() < was + 1000, 'and moves the estimate only part of the way');

	same(clock.noteBoxDate(header(Date.UTC(1970, 0, 1, 0, 2)), sent, got), false,
		'a box that has not set its clock is not believed');
	same(clock.noteBoxDate('not a date', sent, got), false, 'nor a header that is no date');
	same(clock.noteBoxDate(null, sent, got), false, 'nor an answer without one');
	same(clock.noteBoxDate(header(boxMs + 600000), sent, sent + 9000), false, 'nor one that took seconds to arrive');
	is(Math.abs(clock.boxOffsetMs() - BEHIND_MS) <= 1000, 'and none of them moved the estimate');

	is(clock.noteBoxDate(header(sent + 20 - 3600000), sent, got), true, 'a device clock set anew is read');
	is(Math.abs(clock.boxOffsetMs() + 3600000) <= 1000, 'and taken at once rather than crept towards');
}

// ------------------------------------------------- the answers the page reads

{
	clock.forgetBoxDate();
	/** @type {Array<{ method: string, url: string, body: unknown }>} */
	const sent = [];
	globalThis.fetch = /** @type {any} */ (function (/** @type {string} */ url, /** @type {RequestInit} */ init) {
		sent.push({ method: init && init.method ? init.method : 'GET', url: String(url),
			body: init && typeof init.body === 'string' ? JSON.parse(init.body) : null });
		const date = header(Date.now() + BEHIND_MS);
		return Promise.resolve({
			ok: true,
			status: 200,
			headers: { get: function (/** @type {string} */ name) { return name === 'Date' ? date : null; } },
			text: function () { return Promise.resolve('{"items":[]}'); },
			json: function () { return Promise.resolve({ items: [] }); },
		});
	});
	await api('GET', '/api/v1/timers');
	is(Math.abs(clock.boxOffsetMs() - BEHIND_MS) <= 1500, 'any answer through api() sets the box clock');
}

// ------------------------------------- the record button on the Now screen

{
	const boxNow = clock.boxNowSeconds();
	const device = Math.floor(Date.now() / 1000);
	const playing = { id: 'b4cc03fd00016eac', name: 'Das Erste HD' };

	// Over by the box's clock, still on by the device's.
	const over = { id: '40200016dcb1000', title: 'Tagesschau', start: boxNow - 900, duration: 900 - 60 };
	is(over.start + over.duration > device, 'the programme looks on air to the device');
	same(programmeEnd(over, boxNow), 0, 'and is no end to record to by the box');
	const fallback = boxNow + 4 * 3600;
	const body = recordNowBody(playing, over, boxNow, fallback);
	same(body.start, boxNow, 'the start is the box\'s time of the press');
	same(body.stop, fallback, 'the end is the box\'s own length');
	same(body.epg_start, undefined, 'and no guide start makes the box keep a past end');
	same(body.epg_id, undefined, 'nor a guide event that is over');

	const on = { id: '40200016dcb1001', title: 'Tatort', start: boxNow - 600, duration: 5400 };
	const end = programmeEnd(on, boxNow);
	same(end, on.start + on.duration, 'a programme still on ends the recording');
	const kept = recordNowBody(playing, on, boxNow, end);
	same([kept.kind, kept.start, kept.stop, kept.epg_id, kept.epg_start, kept.title],
		['immediate-record', boxNow, end, '40200016dcb1001', on.start, 'Tatort'],
		'and goes with its guide event and start, so the box keeps that end');

	same(recordNowBody(playing, null, boxNow, fallback).title, 'Das Erste HD',
		'without a programme the channel names the recording');
}

const FLOOR = 23;
if (checked < FLOOR) {
	process.stderr.write('boxclock-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('boxclock-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-boxclock.sh: ' + checked + ' assertions over the box\'s clock and the record button judged by it\n');
