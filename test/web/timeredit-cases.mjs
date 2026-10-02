// What the form refuses and sends when a timer the box holds is changed.
//
// A running one-off recording can only be lengthened or cut short: its start is
// neither asked about nor sent, and its stop has to lie ahead. Every other timer
// is held as before. The runtime is stubbed as in timerbody-cases.mjs.
import * as loader from 'node:module';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('timeredit-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
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
			throw new Error('timeredit-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

const { draftOf, draftProblems, changeBody, createBody, startFixed, emptyDraft, momentSeconds } =
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
		process.stderr.write('timeredit: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

// A whole minute, so draft and box starts differ only where a case adds seconds.
const NOW = 1800000000;
const RUNNING = 2;
const SCHEDULED = 0;
const DAILY = 1;

/**
 * A timer as the listing answers it.
 * @param {string} kind
 * @param {number} state
 * @param {number} repeat
 * @param {number} start
 * @param {number} stop
 */
function held(kind, state, repeat, start, stop) {
	return {
		id: 7, kind: kind, channel_id: 'b4cc03fd00016eac', start: start, stop: stop,
		title: 'Tatort', repeat: repeat, repeat_count: 0, state: state,
		announce: start - 180, epg_id: '0', epg_start: 0, standby_on: false,
		recording_dir: '/media/hdd/movies',
	};
}

// --------------------------------------- a recording that runs once and is running

{
	// Begun ten minutes ago, at a moment with seconds in it, ending in twenty.
	const began = NOW - 600 + 17;
	const draft = draftOf(held('record', RUNNING, 0, began, began + 1800));
	same(draft.running, true, 'a running recording is read as running');
	same(startFixed(draft), true, 'and its start is fixed');
	same(draftProblems(draft, NOW), [], 'its start behind the clock is no reason to refuse it');

	draft.minutes = '90';
	same(draftProblems(draft, NOW), [], 'nor is lengthening it');
	const body = changeBody(draft, NOW);
	same(body.start, undefined, 'its start is not sent, so the box keeps its own');
	same(body.announce, undefined, 'nor is an announcement it is past');
	same(body.stop, began + 90 * 60, 'its stop is counted from when it began, to the second');
	same(body.repeat, 0, 'and it still runs once');

	draft.minutes = '5';
	same(draftProblems(draft, NOW), ['form.bad.ended'], 'an end already behind it is refused');
	same(draftProblems(draft, NOW).indexOf('form.bad.past'), -1, 'and not as a start in the past');
}

{
	// Begun on a whole minute, so a whole number of minutes reaches now exactly.
	const began = NOW - 600;
	const draft = draftOf(held('record', RUNNING, 0, began, began + 1800));
	draft.minutes = '10';
	same(draftProblems(draft, NOW), ['form.bad.ended'], 'an end on the very second of now is refused');
	same(draftProblems(draft, NOW - 1), [], 'and one a second ahead of now is not');
	draft.minutes = '';
	same(draftProblems(draft, NOW), ['form.bad.duration'], 'an empty duration is still the duration missing');
}

// -------------------------------- a running recording made the last of its series

{
	const began = NOW - 600 + 17;
	const draft = draftOf(held('record', RUNNING, DAILY, began, began + 1800));
	same(startFixed(draft), false, 'a running series keeps a start that can move');
	same(draftProblems(draft, NOW), [], 'and is not asked about the past, as before');
	same(changeBody(draft, NOW).start, momentSeconds(draft.start), 'and sends the start it shows, as before');

	draft.repeat = 'once';
	same(startFixed(draft), true, 'turned into a one-off it is one, and its start is fixed');
	same(draftProblems(draft, NOW), [], 'without a refusal for the start it began at');
	same(changeBody(draft, NOW).start, undefined, 'and without sending that start');
}

// ---------------------------------------- everything else, held as it was before

{
	const draft = draftOf(held('record', SCHEDULED, 0, NOW - 600, NOW + 1200));
	same(draft.running, false, 'a recording that has not begun is not running');
	same(draftProblems(draft, NOW), ['form.bad.past'], 'and a start of it in the past is refused');
	same(changeBody(draft, NOW).start, NOW - 600, 'and a start of it is sent');
}

{
	const draft = draftOf(held('record', SCHEDULED, 0, NOW + 600, NOW + 1200));
	same(draftProblems(draft, NOW), [], 'a recording ahead of the clock is taken');
	draft.minutes = '1';
	same(draftProblems(draft, NOW), [], 'however short, its end being ahead too');
}

{
	const draft = draftOf(held('record', SCHEDULED, DAILY, NOW - 600, NOW + 1200));
	same(draftProblems(draft, NOW), [], 'a series that has not begun may start in the past');
}

{
	// A state the daemon passes a zap timer through and does not keep it in.
	const draft = draftOf(held('zapto', RUNNING, 0, NOW - 600, 0));
	same(draft.running, false, 'a zap timer is never a running recording');
	same(draftProblems(draft, NOW), ['form.bad.past'], 'and its start in the past is refused');
	same(changeBody(draft, NOW).start, NOW - 600, 'and sent');
}

{
	const draft = emptyDraft('record', NOW);
	same(draft.running, false, 'a new timer is not running');
	draft.channel_id = 'b4cc03fd00016eac';
	draft.start = '';
	same(draftProblems(draft, NOW), ['form.bad.start'], 'and still needs its start');
}

// ------------------------------------------- what the daemon would drop on a change
//
// No title, standby flag or directory is sent for any kind, running or not.

for (const kind of ['record', 'zapto', 'remind', 'exec-plugin', 'standby']) {
	const timer = held(kind, SCHEDULED, 0, NOW + 600, kind === 'record' ? NOW + 1200 : 0);
	timer.standby_on = kind === 'standby';
	const draft = draftOf(timer);
	draft.title = 'changed';
	draft.standby_on = !draft.standby_on;
	draft.recording_dir = '/elsewhere';
	const body = changeBody(draft, NOW);
	same('title' in body, false, 'a change to a ' + kind + ' timer sends no title');
	same('standby_on' in body, false, 'nor a standby flag');
	same('recording_dir' in body, false, 'nor a directory');
}

// --------------------------------------------- the title, made and then changed
//
// A new timer sends the title for the kinds that have one; a change never does.

for (const [kind, carries] of [
	['record', true], ['immediate-record', true], ['zapto', true], ['remind', true],
	['exec-plugin', true], ['shutdown', false], ['standby', false], ['sleeptimer', false],
]) {
	const draft = emptyDraft(kind, NOW);
	draft.channel_id = 'b4cc03fd00016eac';
	draft.title = 'Tagesschau';
	same(createBody(draft, NOW).title, carries ? 'Tagesschau' : undefined,
		'a new ' + kind + ' timer ' + (carries ? 'carries its title' : 'carries no title'));
	same('title' in changeBody(draft, NOW), false, 'a change to a ' + kind + ' timer carries none');
}

// ------------------------------------------------------------------------ verdict

const FLOOR = 61;
if (checked < FLOOR) {
	process.stderr.write('timeredit-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('timeredit-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-timeredit.sh: ' + checked + ' assertions over what a change to a timer may and must not do\n');
