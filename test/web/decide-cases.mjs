// What the page decides about a channel before anything is fetched, driven without
// a browser.
//
// Four pure functions in app/ui/player.js answer the whole of it: which sound to ask
// the box for, whether this browser plays what the box would deliver, which of the
// four roads the channel takes, and what a media element is doing. Nothing in them
// reaches the network and nothing in them draws, so all four can be driven from
// here, and a road that changed is a failure here rather than a black rectangle
// somebody reports by telephone.
//
// THE ONE PART THAT REACHES THE DOCUMENT is browserPlays, which asks an audio
// element and MediaSource. Those two cannot be had here, so the fake below stands in
// for them. What the fake is held to is the shape the real pair answers with:
// canPlayType answers '', 'maybe' or 'probably', isTypeSupported answers a boolean.
// That decide() asks them at all is checked here; that they tell the truth about a
// real browser is not checkable anywhere, which is why the page asks rather than
// looks up a table.
//
// THE RUNTIME IS STUBBED AND THE MODULE IS NOT. player.js names the runtime at the
// top of the file, the runtime names its four modules root absolute because the
// document is served under several paths, and those four belong to the build rather
// than to this tree. So the four are answered with stubs and everything else,
// player.js included, is imported for real, because a rule rewritten there has to be
// a rule rewritten here. A runtime that grows an export the stub does not carry
// fails on the import rather than quietly in a case.
import * as loader from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join } from 'node:path';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('decide-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const here = dirname(fileURLToPath(import.meta.url));
const web = join(here, '..', '..', 'data', 'ni-web');

/* The runtime, as much of it as an import needs and no more: what these cases
   are about is the deciding, and none of the deciding calls any of this. */
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
			throw new Error('decide-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

// ------------------------------------------------------------- the browser

/** Every question the fake audio element was asked, newest last. */
const askedOfElement = [];
/** Every question the fake MediaSource was asked, newest last. */
const askedOfSource = [];

/* What this browser answers. A case that is about a browser refusing
   something puts its own answer here and everything() puts it back. */
const browser = {
	/** @type {(type: string) => string} */
	element: function () { return 'probably'; },
	/** @type {(type: string) => unknown} */
	source: function () { return true; },
	/** @type {unknown} */
	mediaSource: null,
};

const realSource = {
	isTypeSupported: function (type) {
		askedOfSource.push(type);
		return browser.source(type);
	},
};

function forget() {
	askedOfElement.length = 0;
	askedOfSource.length = 0;
}

/** A browser that plays all of it, which is what most cases want behind them. */
function everything() {
	browser.element = function () { return 'probably'; };
	browser.source = function () { return true; };
	browser.mediaSource = realSource;
	forget();
}

globalThis.document = {
	createElement: function (name) {
		if (name !== 'audio') {
			throw new Error('decide-cases.mjs: the page made a ' + name + ' element, and only an audio element was expected');
		}
		return {
			canPlayType: function (type) {
				askedOfElement.push(type);
				return browser.element(type);
			},
		};
	},
};

globalThis.window = {
	get MediaSource() { return browser.mediaSource; },
};

everything();

const player = await import(pathToFileURL(join(web, 'app', 'ui', 'player.js')).href);
const words = (await import(pathToFileURL(join(web, 'app', 'ui', 'player.text.js')).href)).default;
const { chosenSound, browserPlays, decide, phaseOf, faultOf } = player;

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
		process.stderr.write('decide: ' + what + '\n');
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

/**
 * One channel put to the page with nothing remembered from the case before.
 *
 * @param {object} channel
 */
function road(channel) {
	forget();
	return decide(channel);
}

// ---------------------------------------------------------- which sound

// A channel the box has never tuned names no sound at all.
same(chosenSound({ id: 'a', name: 'A' }), { codec: '', pid: 0 },
	'a channel with no track list names no sound');
same(chosenSound({ id: 'a', name: 'A', audio: [] }), { codec: '', pid: 0 },
	'and an empty list is the same answer');

// The one the box is on, where the box named one.
same(chosenSound({ audio: [
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: false },
	{ pid: 201, codec: 'ac3', description: 'Deutsch', selected: true },
] }), { codec: 'ac3', pid: 201 },
	'the track the box marked is the one that is taken');
same(chosenSound({ audio: [
	{ pid: 200, codec: 'ac3', description: 'Deutsch', selected: false },
	{ pid: 201, codec: 'mp2', description: 'Deutsch', selected: false },
] }), { codec: 'ac3', pid: 200 },
	'and where the box marked none, the first one it lists');

// Already AAC, which costs the box nothing.
same(chosenSound({ audio: [
	{ pid: 300, codec: 'aac', description: 'Deutsch', selected: true },
] }), { codec: 'aac', pid: 300 },
	'a chosen track that is already AAC is the answer');
same(chosenSound({ audio: [
	{ pid: 300, codec: 'aac', description: 'Deutsch', selected: true },
	{ pid: 301, codec: 'aac', description: 'English', selected: false },
] }), { codec: 'aac', pid: 300 },
	'and stays the answer beside a second AAC track');
same(chosenSound({ audio: [
	{ pid: 299, codec: 'aac', description: 'Deutsch', selected: false },
	{ pid: 300, codec: 'aac', description: 'Deutsch', selected: true },
] }), { codec: 'aac', pid: 300 },
	'and beside one the stream lists first and the box did not choose');
same(chosenSound({ audio: [
	{ codec: 'aac', description: 'Deutsch', selected: true },
] }), { codec: 'aac', pid: 0 },
	'a chosen AAC track with no pid named answers nought, which is no pid named');

// The step the whole function exists for: AAC over converting Layer II.
same(chosenSound({ audio: [
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
	{ pid: 201, codec: 'aac', description: 'Deutsch', selected: false },
] }), { codec: 'aac', pid: 201 },
	'an AAC track described the same way beats the Layer II the box is on');
same(chosenSound({ audio: [
	{ pid: 201, codec: 'aac', description: 'Deutsch', selected: false },
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
] }), { codec: 'aac', pid: 201 },
	'wherever in the list it stands');

// And the step above it: the language first.
same(chosenSound({ audio: [
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
	{ pid: 201, codec: 'aac', description: 'English', selected: false },
	{ pid: 202, codec: 'aac', description: 'Deutsch', selected: false },
] }), { codec: 'aac', pid: 202 },
	'the AAC track in the language the box chose beats the AAC track in another');
same(chosenSound({ audio: [
	{ pid: 202, codec: 'aac', description: 'Deutsch', selected: false },
	{ pid: 201, codec: 'aac', description: 'English', selected: false },
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
] }), { codec: 'aac', pid: 202 },
	'and beats it whichever of the two the stream lists first');
/* The language wins over the work saved: where the only AAC is in another
   language, the box converts its own track rather than hand over a
   commentary nobody asked for. */
same(chosenSound({ audio: [
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
	{ pid: 201, codec: 'aac', description: 'English', selected: false },
] }), { codec: 'mp2', pid: 200 },
	'an AAC track in another language does not displace the one the box chose');
same(chosenSound({ audio: [
	{ pid: 201, codec: 'aac', description: 'English', selected: false },
	{ pid: 202, codec: 'aac', description: 'Franc,ais', selected: false },
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
] }), { codec: 'mp2', pid: 200 },
	'and neither does a choice of them');

// No AAC anywhere: the chosen track, untouched.
same(chosenSound({ audio: [
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
	{ pid: 201, codec: 'ac3', description: 'Deutsch', selected: false },
] }), { codec: 'mp2', pid: 200 },
	'with no AAC anywhere the chosen track is the answer and not the other one');
same(chosenSound({ audio: [
	{ pid: 200, codec: 'aacplus', description: 'Deutsch', selected: true },
	{ pid: 201, codec: 'aac', description: 'Deutsch', selected: false },
] }), { codec: 'aac', pid: 201 },
	'AAC plus is not AAC, so the plain AAC track beside it is preferred');

// What a half written list does, because the channel route writes every member
// of a track and a caller of this function is not the channel route.
same(chosenSound({ audio: [
	null,
	{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
	{ pid: 201, codec: 'aac', description: 'Deutsch', selected: false },
] }), { codec: 'aac', pid: 201 },
	'a hole in the list is stepped over');
same(chosenSound({ audio: [
	{ pid: 200, description: 'Deutsch', selected: true },
] }), { codec: '', pid: 200 },
	'a track with no codec named answers no codec');
same(chosenSound({ audio: [
	{ pid: 200, codec: 'mp2', selected: true },
	{ pid: 201, codec: 'aac', description: 'English' },
	{ pid: 202, codec: 'aac' },
] }), { codec: 'aac', pid: 202 },
	'two tracks the stream describes with nothing are described the same way');

// ------------------------------------------------------- what this browser plays

everything();
is(browserPlays('') === true, 'radio plays where the audio element says it plays');
same(askedOfElement, ['audio/aac'],
	'and what it is asked about is ADTS, which is what the box sends for radio');
same(askedOfSource, [],
	'radio does not go through the demuxer, so MediaSource is not asked at all');

everything();
browser.element = function () { return ''; };
is(browserPlays('') === false, 'and does not where the element answers with nothing');

everything();
browser.element = function () { return 'maybe'; };
is(browserPlays('') === true, 'maybe is an answer and not a refusal');

everything();
is(browserPlays('h264') === true, 'H.264 plays where MediaSource says it plays');
same(askedOfSource, ['video/mp4; codecs="avc1.42E01E,mp4a.40.2"'],
	'and what it is asked about is the fragmented MP4 the demuxer hands over, at the mildest H.264 there is');
same(askedOfElement, [], 'a picture does not go past an audio element');

everything();
is(browserPlays('hevc') === true, 'HEVC the same way');
same(askedOfSource, ['video/mp4; codecs="hvc1.1.6.L93.B0,mp4a.40.2"'],
	'asked about at the mildest HEVC there is');

everything();
browser.source = function () { return false; };
is(browserPlays('hevc') === false, 'and refused where the browser refuses it, which is Firefox');

everything();
browser.source = function () { return 'probably'; };
is(browserPlays('h264') === false,
	'isTypeSupported answers a boolean, so anything else is not a yes');

everything();
is(browserPlays('mpeg2') === false, 'MPEG-2 is not a picture this asks about');
same(askedOfSource, [], 'and is refused without asking anybody');
is(browserPlays('cavs') === false, 'nor is AVS');
is(browserPlays('unknown') === false, 'nor is a picture nobody has read');
is(browserPlays('none') === false, 'nor the word radio is spelled with');

everything();
browser.mediaSource = undefined;
is(browserPlays('h264') === false, 'a browser with no MediaSource plays no picture here');

everything();
browser.mediaSource = {};
is(browserPlays('h264') === false, 'and neither does one whose MediaSource cannot be asked');

// -------------------------------------------------------------- the four roads

everything();

const tv = { id: 'c0123', name: 'Das Erste', kind: 'tv', streams_known: true };

let d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'aac', description: 'Deutsch', selected: true }],
}));
same([d.path, d.why, d.radio, d.video, d.audio, d.apid],
	['copy', '', false, 'h264', 'aac', 200],
	'a channel whose chosen sound is already AAC goes straight through');
is(d.src === '/api/v1/stream/browser/c0123?apid=200&audio=aac&video=h264',
	'and the address names the road, the pid and nothing else: ' + d.src);

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true }],
}));
same([d.path, d.why, d.video, d.audio, d.apid], ['convert-sound', '', 'h264', 'mp2', 200],
	'Layer II with no AAC beside it is converted');
is(d.src === '/api/v1/stream/browser/c0123?apid=200&audio=mp2&video=h264',
	'and the box is told which sound to convert: ' + d.src);

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'ac3', description: 'Deutsch', selected: true }],
}));
is(d.path === 'convert-sound', 'AC-3 the same way, which is the other half of German television');

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [
		{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true },
		{ pid: 201, codec: 'aac', description: 'Deutsch', selected: false },
	],
}));
same([d.path, d.audio, d.apid], ['copy', 'aac', 201],
	'a channel carrying both takes the AAC track and is copied rather than converted');

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [{ codec: 'aac', description: 'Deutsch', selected: true }],
}));
is(d.src === '/api/v1/stream/browser/c0123?audio=aac&video=h264',
	'a sound with no pid named leaves the pid out rather than sending nought: ' + d.src);

// Not offered, and the three different reasons.
d = road(Object.assign({}, tv, {
	video: { codec: 'mpeg2', pid: 100 },
	audio: [{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true }],
}));
same([d.path, d.why, d.video, d.src], ['not-offered', 'video', 'mpeg2', ''],
	'standard definition is MPEG-2, which no browser has decoded for years and this box cannot re-encode');

d = road(Object.assign({}, tv, {
	video: { codec: 'cavs', pid: 100 },
	audio: [{ pid: 200, codec: 'mp2', description: 'Deutsch', selected: true }],
}));
same([d.path, d.why], ['not-offered', 'video'], 'and AVS, which nothing plays either');

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'mpeg4-audio', description: 'Deutsch', selected: true }],
}));
same([d.path, d.why, d.audio, d.src], ['not-offered', 'audio', 'mpeg4-audio', ''],
	'a sound this box has no decoder for ends the channel whatever the picture is');
same(askedOfSource, [],
	'and ends it before the browser is asked, because what the box cannot send is not the browser`s question');

everything();
browser.source = function () { return false; };
d = road(Object.assign({}, tv, {
	video: { codec: 'hevc', pid: 100 },
	audio: [{ pid: 200, codec: 'ac3', description: 'Deutsch', selected: true }],
}));
same([d.path, d.why, d.video, d.audio, d.src], ['not-offered', 'browser', 'hevc', 'ac3', ''],
	'what the box would deliver and what the browser plays are two questions, and the second one is asked last');

everything();

// The ordinary road: a channel nobody has been on.
d = road(Object.assign({}, tv, {
	streams_known: false,
	video: { codec: 'unknown', pid: 0 },
	audio: [],
}));
same([d.path, d.why, d.video, d.audio, d.apid], ['blind', '', 'unknown', 'unknown', 0],
	'a channel the box knows nothing about is opened rather than refused');
is(d.src === '/api/v1/stream/browser/c0123?audio=unknown&video=unknown',
	'and the box is told so rather than guessed at: ' + d.src);
same(askedOfSource, [], 'a browser cannot be asked about a codec nobody has read');
same(askedOfElement, [], 'and neither can an audio element');

d = road(Object.assign({}, tv, {
	streams_known: false,
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'aac', description: 'Deutsch', selected: true }],
}));
is(d.path === 'blind',
	'a track list beside streams_known false is a list the box has not read, so it is not believed');

d = road(Object.assign({}, tv, {
	video: { codec: 'unknown', pid: 0 },
	audio: [{ pid: 200, codec: 'aac', description: 'Deutsch', selected: true }],
}));
same([d.path, d.video, d.audio], ['blind', 'unknown', 'aac'],
	'a picture nobody has read is nothing known, whatever is known about the sound');

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [],
}));
same([d.path, d.audio], ['blind', 'unknown'], 'and a sound nobody has read the same way');

d = road(Object.assign({}, tv, {
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'unknown', description: 'Deutsch', selected: true }],
}));
is(d.path === 'blind', 'including a track the box read and could not name');

d = road({ id: 'c0123', name: 'Das Erste', kind: 'tv',
	video: { codec: 'h264', pid: 100 },
	audio: [{ pid: 200, codec: 'aac', description: 'Deutsch', selected: true }] });
is(d.path === 'copy',
	'a channel that says nothing about whether its streams are known is read off the codecs it names');

// Radio, which is a channel with no picture at all.
for (const kind of ['radio', 'webradio']) {
	d = road({ id: 'r7', name: 'Bayern 2', kind: kind, streams_known: true,
		audio: [{ pid: 400, codec: 'aac', description: 'Deutsch', selected: true }] });
	same([d.path, d.radio, d.video, d.audio], ['copy', true, 'none', 'aac'],
		kind + ' whose sound is already AAC goes straight through');
	is(d.src === '/api/v1/stream/browser/r7?apid=400&audio=aac&video=none',
		'and the box is told there is no picture: ' + d.src);
	same(askedOfElement, ['audio/aac'],
		kind + ' is asked of an audio element and not of the demuxer');
	same(askedOfSource, [], 'which is what an ADTS stream needs and all it needs');

	d = road({ id: 'r7', name: 'Bayern 2', kind: kind, streams_known: true,
		audio: [{ pid: 400, codec: 'mp2', description: 'Deutsch', selected: true }] });
	same([d.path, d.radio, d.video], ['convert-sound', true, 'none'],
		kind + ' carrying Layer II is converted like anything else');
}

d = road({ id: 'r7', name: 'Bayern 2', kind: 'radio', streams_known: true,
	video: { codec: 'mpeg2', pid: 100 },
	audio: [{ pid: 400, codec: 'aac', description: 'Deutsch', selected: true }] });
same([d.path, d.video], ['copy', 'none'],
	'radio has no picture whatever a picture codec left in the answer says');

d = road({ id: 'r7', name: 'Bayern 2', kind: 'radio', streams_known: false, audio: [] });
same([d.path, d.video, d.audio], ['blind', 'none', 'unknown'],
	'radio nobody has been on is opened blind, and the box is told there is no picture');
is(d.src === '/api/v1/stream/browser/r7?audio=unknown&video=none',
	'which is a thing the box knows from the service type and not from the stream: ' + d.src);

everything();
browser.element = function () { return ''; };
d = road({ id: 'r7', name: 'Bayern 2', kind: 'radio', streams_known: true,
	audio: [{ pid: 400, codec: 'mp2', description: 'Deutsch', selected: true }] });
same([d.path, d.why, d.radio], ['not-offered', 'browser', true],
	'a browser that plays no AAC plays no radio from this box either');

everything();

/* The two invariants the module beside this one leans on: playing.js starts a
   channel when the address is not empty and explains it when it is, and it
   reads the reason only where there is one. */
const every = [
	{ id: 'c1', name: 'a', kind: 'tv', streams_known: true, video: { codec: 'h264' },
		audio: [{ pid: 1, codec: 'aac', selected: true }] },
	{ id: 'c2', name: 'b', kind: 'tv', streams_known: true, video: { codec: 'h264' },
		audio: [{ pid: 2, codec: 'ac3', selected: true }] },
	{ id: 'c3', name: 'c', kind: 'tv', streams_known: false },
	{ id: 'c4', name: 'd', kind: 'tv', streams_known: true, video: { codec: 'mpeg2' },
		audio: [{ pid: 4, codec: 'mp2', selected: true }] },
	{ id: 'c5', name: 'e', kind: 'tv', streams_known: true, video: { codec: 'h264' },
		audio: [{ pid: 5, codec: 'opus', selected: true }] },
	{ id: 'c6', name: 'f', kind: 'radio', streams_known: true,
		audio: [{ pid: 6, codec: 'mp2', selected: true }] },
];
const roads = every.map(function (one) { return road(one).path; });
same(roads, ['copy', 'convert-sound', 'blind', 'not-offered', 'not-offered', 'convert-sound'],
	'the four roads, one channel of each kind');
for (const one of every) {
	const got = road(one);
	is((got.src === '') === (got.path === 'not-offered'),
		one.id + ' has an address exactly where there is a road to walk');
	is((got.why === '') === (got.path !== 'not-offered'),
		one.id + ' says why exactly where there is no road');
	is(['copy', 'convert-sound', 'blind', 'not-offered'].indexOf(got.path) >= 0,
		one.id + ' takes one of the four roads and not a fifth');
	is(['', 'video', 'audio', 'browser'].indexOf(got.why) >= 0,
		one.id + ' fails for one of the three reasons and not a fourth');
}

// ------------------------------------------------------- what the element is doing

/**
 * A media element, as much of one as this reads.
 *
 * @param {object} state
 */
function element(state) {
	return Object.assign({ error: null, ended: false, paused: true, currentSrc: '', readyState: 0 }, state);
}

const phases = [];
/**
 * @param {object} state
 * @param {string} want
 * @param {string} what
 */
function phase(state, want, what) {
	const got = phaseOf(element(state));
	phases.push(want);
	is(got === want, what + ': got ' + got + ', wanted ' + want);
}

phase({}, 'idle', 'an element nothing has been put on has not started');
phase({ paused: true, currentSrc: 'http://box/api/v1/stream/browser/c1' }, 'paused',
	'one with an address on it that is not running is held');
phase({ paused: false, readyState: 4 }, 'playing', 'one that is running and has enough of the stream is playing');
phase({ paused: false, readyState: 3 }, 'playing', 'three being enough of it');
phase({ paused: false, readyState: 2 }, 'loading', 'and two not being');
phase({ paused: false, readyState: 0 }, 'loading', 'nor nought');
phase({ ended: true, paused: true, currentSrc: 'http://box/x' }, 'ended',
	'a stream that ran out is over and not merely held, which is what the browser leaves it as');
phase({ error: { code: 2, message: 'net' }, paused: false, readyState: 4 }, 'failed',
	'an element carrying a fault broke off whatever else it says about itself');
phase({ error: { code: 4, message: '' }, ended: true, paused: true }, 'failed',
	'and the fault comes before the ending');
phase({ paused: false, readyState: 4, currentSrc: '' }, 'playing',
	'a running element with no address is still running, the address being read only where it is held');

for (const one of phases) {
	is(typeof words.de['player.state.' + one] === 'string' && words.de['player.state.' + one] !== '',
		'the state ' + one + ' has a German word');
	is(typeof words.en['player.state.' + one] === 'string' && words.en['player.state.' + one] !== '',
		'the state ' + one + ' has an English word');
}

// ---------------------------------------------------- what the element said

is(faultOf(element({})) === null, 'an element with no fault says nothing about one');
same(faultOf(element({ error: { code: 1, message: 'stopped' } })), { why: 'aborted', said: 'stopped' },
	'the four names the standard gives, and the first is a fetch somebody stopped');
same(faultOf(element({ error: { code: 2, message: 'gone' } })), { why: 'network', said: 'gone' },
	'the second a source that could not be fetched');
same(faultOf(element({ error: { code: 3, message: 'bad' } })), { why: 'decode', said: 'bad' },
	'the third bytes that arrived and would not decode');
same(faultOf(element({ error: { code: 4, message: 'no' } })), { why: 'format', said: 'no' },
	'the fourth a format this browser will not play, which sends somebody somewhere else entirely');
same(faultOf(element({ error: { code: 99, message: 'what' } })), { why: 'unknown', said: 'what' },
	'a code the standard does not name lands on the last line and not on nothing');
same(faultOf(element({ error: { code: 2 } })), { why: 'network', said: '' },
	'an element that says nothing beside the code is carried through as nothing');

// ------------------------------------------------------------------ verdict

const FLOOR = 90;
if (checked < FLOOR) {
	process.stderr.write('decide-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('decide-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-decide.sh: ' + checked + ' assertions over the sound, the roads, the browser and the element\n');
