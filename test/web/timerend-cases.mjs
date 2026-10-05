// The end of a recording as a moment, kept in step with its minutes.
//
// Local time is pinned to a zone with a summer time change, so the two nights
// a year that are not twenty four hours long are tested wherever this runs.
process.env.TZ = 'Europe/Berlin';
import * as loader from 'node:module';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('timerend-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
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
			throw new Error('timerend-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

const { emptyDraft, draftOf, draftProblems, createBody, endInput, minutesUntil, momentSeconds, canPickEnd } =
	await import('../../data/ni-web/app/screens/timers/list.model.js');

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
		process.stderr.write('timerend: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

const NOW = momentSeconds('2026-10-04T12:00');

/**
 * @param {string} start
 * @param {string} minutes
 */
function record(start, minutes) {
	const draft = emptyDraft('record');
	draft.channel_id = 'b4cc03fd00016eac';
	draft.start = start;
	draft.minutes = minutes;
	return draft;
}

same(new Date(NOW * 1000).getTimezoneOffset(), -120, 'the zone is pinned');

{
	const draft = record('2026-10-04T20:15', '90');
	same(endInput(draft, NOW), '2026-10-04T21:45', 'the end is the start plus the minutes');
	same(minutesUntil(draft, '2026-10-04T21:45', NOW), 90, 'and the minutes are the end less the start');
	same(minutesUntil(draft, '2026-10-04T21:00', NOW), 45, 'an earlier end is fewer minutes');
	draft.start = '2026-10-04T21:00';
	same(endInput(draft, NOW), '2026-10-04T22:30', 'a later start moves the end and keeps the minutes');
	same(minutesUntil(draft, '2026-10-04T21:00', NOW), 0, 'an end at the start is no duration');
	same(minutesUntil(draft, '2026-10-04T20:00', NOW), 0, 'nor is one before it');
	same(minutesUntil(draft, '', NOW), 0, 'nor is no end at all');
	same(minutesUntil(draft, '2026-10-0', NOW), 0, 'nor a half typed one');
	draft.minutes = '';
	same(endInput(draft, NOW), '', 'no minutes is no end');
	same(draftProblems(draft, NOW), ['form.bad.duration'], 'and nothing is sent');
	draft.minutes = '90';
	draft.start = '';
	same(endInput(draft, NOW), '', 'no start is no end');
}

{
	const draft = record('2026-10-04T23:30', '60');
	same(endInput(draft, NOW), '2026-10-05T00:30', 'an end past midnight is on the next day');
	same(minutesUntil(draft, '2026-10-05T01:15', NOW), 105, 'and counts the minutes across it');
}

{
	// Summer time ends at three in the night of 25 October 2026: the clock
	// shows two to three twice, so that night has an hour more.
	const draft = record('2026-10-25T01:30', '');
	same(minutesUntil(draft, '2026-10-25T03:30', NOW), 180, 'two hours on the clock across the change back are three');
	draft.minutes = '180';
	same(endInput(draft, NOW), '2026-10-25T03:30', 'and three hours end where the clock says half past three');
	// And it begins at two on 29 March 2026, which skips an hour.
	const spring = record('2026-03-29T01:30', '');
	same(minutesUntil(spring, '2026-03-29T03:30', NOW), 60, 'two hours on the clock across the change forward are one');
}

{
	// A running recording: its start is when it began, to the second.
	const began = NOW - 600 + 17;
	const timer = {
		id: 7, kind: 'record', channel_id: 'b4cc03fd00016eac', start: began, stop: began + 1800,
		title: 'Tatort', repeat: 0, repeat_count: 0, state: 2,
		announce: began - 180, epg_id: '0', epg_start: 0, standby_on: false,
		recording_dir: '/media/hdd/movies',
	};
	const draft = draftOf(timer);
	same(draft.running, true, 'the timer is running');
	same(endInput(draft, NOW), '2026-10-04T12:20', 'its end counts from when it began');
	same(minutesUntil(draft, '2026-10-04T13:00', NOW), 70, 'and a later end lengthens it from there');
}

same(canPickEnd({ HTMLInputElement: { prototype: { showPicker: function () {} } } }), true,
	'a browser with a picker offers the end');
same(canPickEnd({ HTMLInputElement: { prototype: {} } }), false, 'one without does not');
same(canPickEnd({}), false, 'nor does a scope without inputs');

{
	const draft = record('2026-10-04T20:15', '90');
	const body = createBody(draft, NOW);
	same(body.stop - body.start, 5400, 'what is sent is still the start and the minutes');
}

if (failed > 0) {
	process.stderr.write('timerend-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-timerend.sh: ' + checked + ' assertions over the end of a recording\n');
