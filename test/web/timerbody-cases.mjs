// What a new timer is sent as, driven without a browser.
//
// createBody is pure bookkeeping over a draft, so the whole of it runs here. The
// part it is here for is the pair of flags a recording carries, because nothing
// about them is visible on the screen that sends them or in the answer that comes
// back: the box writes neither into the timer it reports (src/httpd/ep/ep_timers.cpp
// keeps them out of the read schema for that reason), so a flag that stopped being
// sent would show up as a recording that begins a few minutes late, months later,
// reported from a living room.
//
// THE RULE IS A COPY OF ONE THE BOX KEEPS, which is the whole reason for pinning it.
// Times typed into the box's own timer screen are taken as they stand
// (src/gui/timerlist.cpp sets both to false); pressing record on a guide entry asks
// for both (src/gui/epgview.cpp). This page has to draw the same line, and the line
// is invisible from either side.
//
// THE RUNTIME IS STUBBED AND THE MODEL IS NOT, the way the checks beside this one
// do it: the model names the runtime at the top of the file, the runtime names its
// four modules root absolute because the document is served under several paths,
// and those four belong to the build rather than to this tree. Nothing under test
// here calls any of them.
import * as loader from 'node:module';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('timerbody-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
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
			throw new Error('timerbody-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

const { createBody, emptyDraft } = await import('../../data/ni-web/app/screens/timers/list.model.js');

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
		process.stderr.write('timerbody: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

// A moment with nothing round about it, so that a case reads as the arithmetic it
// is rather than as whatever the clock said while it ran.
const NOW = 1800000000;
const HOUR = 3600;

/**
 * A draft of a recording, with the guide entry written in where one is named.
 * @param {number} epgStart
 * @param {number} startsIn
 */
function recording(epgStart, startsIn) {
	const draft = emptyDraft('record', NOW);
	draft.channel_id = 'b4cc03fd00016eac';
	draft.start = momentOf(NOW + startsIn);
	draft.minutes = '60';
	draft.title = 'Tagesschau';
	if (epgStart > 0) {
		draft.epg_id = 'b4cc03fd00016eac0000';
		draft.epg_start = epgStart;
	}
	return draft;
}

/* The control's own spelling, which is what the draft holds: a local moment
   without a zone, cut to the minute. Taken from the same Date the model reads it
   back with, so a machine in another zone reads its own. */
function momentOf(at) {
	const d = new Date(at * 1000);
	const pad = (n) => String(n).padStart(2, '0');
	return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()) +
		'T' + pad(d.getHours()) + ':' + pad(d.getMinutes());
}

// ------------------------------------------- a recording picked out of the guide

{
	const body = createBody(recording(NOW + 2 * HOUR, 2 * HOUR), NOW);
	same(body.recording_safety, true, 'a recording out of the guide asks for the margins');
	same(body.auto_adjust, true, 'and asks to follow a programme that has not begun');
	same(body.epg_id, 'b4cc03fd00016eac0000', 'and carries the entry it came from');
}

// --------------------------------------------- a recording somebody typed times for

{
	const body = createBody(recording(0, 2 * HOUR), NOW);
	same(body.recording_safety, undefined, 'a recording of typed times asks for no margins');
	same(body.auto_adjust, undefined, 'and does not ask to be moved onto anything');
	same(body.epg_id, undefined, 'and names no guide entry');
}

// ------------------------------------------------ a programme that is already on

{
	const body = createBody(recording(NOW - HOUR, 0), NOW);
	same(body.recording_safety, true, 'a running programme still asks for the margins');
	same(body.auto_adjust, false, 'but not to be followed, there being nothing ahead to follow');
}

// ------------------------------------------------------- every other kind of timer
//
// The daemon reads neither flag on anything but a recording, and an immediate
// recording begins now, where a margin in front of it would be a start already
// behind us.

for (const kind of ['zapto', 'remind', 'standby', 'shutdown', 'immediate-record']) {
	const draft = emptyDraft(kind, NOW);
	draft.channel_id = 'b4cc03fd00016eac';
	draft.epg_id = 'b4cc03fd00016eac0000';
	draft.epg_start = NOW + 2 * HOUR;
	const body = createBody(draft, NOW);
	same(body.recording_safety, undefined, 'a ' + kind + ' timer asks for no margins');
	same(body.auto_adjust, undefined, 'a ' + kind + ' timer does not ask to be moved');
}

// ------------------------------------------------------------------------ verdict

const FLOOR = 16;
if (checked < FLOOR) {
	process.stderr.write('timerbody-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('timerbody-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-timerbody.sh: ' + checked + ' assertions over which timers ask for a margin and which ask to be followed\n');
