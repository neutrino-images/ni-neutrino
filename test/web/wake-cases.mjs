// What the page does when the box says it is in standby.
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

/** Every value the question was drawn with, oldest first. */
globalThis.__wakeShown = [];

const kStubs = {
	'/vendor/preact.module.js':
		'export function h() { return null; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	'/vendor/htm.module.js':
		'export default { bind: function () { return function () { return null; }; } };\n',
	'/vendor/hooks.module.js':
		'export function useState(v) { return [v, function (n) { globalThis.__wakeShown.push(n); }]; }\n' +
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
same(globalThis.__wakeShown, [], 'and nobody is asked anything');

// ------------------------------------------------ a box in standby, yes

fresh();
answers = [refusal('box-in-standby')];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(globalThis.__wakeShown, [true], 'a box in standby opens the question');
is(r.state === 'pending', 'and the caller waits for the answer');
same(posts().length, 1, 'with nothing sent a second time before it');
wake.answer(true);
await settle();
same(posts(), [
	['/api/v1/zap', { channel_id: 'ffffffff48deb591', wake: false }],
	['/api/v1/zap', { channel_id: 'ffffffff48deb591', wake: true }],
], 'a yes sends the same zap again, now with leave to wake');
same(globalThis.__wakeShown, [true, false], 'and closes the question');
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
same(globalThis.__wakeShown, [true, false], 'and closes the question');

// ------------------------------------------------- any other refusal

fresh();
answers = [refusal('recording-holds-tuner')];
r = watch(wake.zap('ffffffff48deb591'));
await settle();
same(globalThis.__wakeShown, [], 'a refusal for any other reason asks nothing');
same(posts().length, 1, 'and sends nothing more');
is(r.state === 'rejected', 'and is handed back to the caller');
same(r.error && r.error.problem ? r.error.problem.type : '', '/errors/recording-holds-tuner',
	'as the box wrote it');

// ------------------------------------------------- two at once

fresh();
answers = [refusal('box-in-standby'), refusal('box-in-standby')];
const first = watch(wake.zap('ffffffffbe692dd5'));
const second = watch(wake.zap('ffffffff48deb591'));
await settle();
same(globalThis.__wakeShown, [true, true], 'two refusals at once keep the one question open');
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

// ------------------------------------------------- the words

const words = (await import(pathToFileURL(join(web, 'app', 'ui', 'wake.text.js')).href)).default;
same(words.de['wake.ask'], 'Die Box ist im Standby. Einschalten und umschalten?',
	'the question is the one agreed on');

if (checked === 0) {
	process.stderr.write('wake-cases.mjs: nothing was checked\n');
	process.exit(1);
}
if (failed) {
	process.stderr.write('wake-cases.mjs: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('wake-cases.mjs: ' + checked + ' checked\n');
