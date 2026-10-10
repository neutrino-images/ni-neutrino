// The one line a screen puts on a refusal, and two screens that put it in a toast, run without a browser.
import * as loader from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('refusal-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const here = dirname(fileURLToPath(import.meta.url));

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

// What a screen puts in a toast, kept rather than drawn.
const kToast =
	'export function toast(message, kind) { globalThis.__toasts.push([message, kind || ""]); return 0; }\n' +
	'export function dismiss() {}\n' +
	'export function Toasts() { return null; }\n';

/** @type {[string, string][]} */
globalThis.__toasts = [];

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (Object.prototype.hasOwnProperty.call(kStubs, spec)) {
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		}
		if (spec.indexOf('/vendor/') === 0) {
			throw new Error('refusal-cases.mjs: no stub for the runtime module ' + spec);
		}
		const found = next(spec, context);
		if (found.url.endsWith('/data/ni-web/app/ui/toast.js')) {
			return { url: 'data:text/javascript,' + encodeURIComponent(kToast), shortCircuit: true };
		}
		return found;
	},
});

// --------------------------------------------------------------- the box

/** What the box answers next, one per request other than the session, and 202 once they run out. */
let answers = [];

/**
 * @param {number} status
 * @param {string} code
 * @param {string} detail
 * @returns {{ status: number, body: string }}
 */
function refusal(status, code, detail) {
	return {
		status: status,
		body: JSON.stringify({ type: '/errors/' + code, title: 'Bad Request', status: status, detail: detail }),
	};
}

globalThis.fetch = function (url) {
	const said = String(url).indexOf('/api/v1/session') !== -1
		? { status: 200, body: JSON.stringify({ authenticated: true, level: 'system', user: 'root', csrf: 't', csrf_header: 'X-CSRF-Token' }) }
		: (answers.length ? answers.shift() : { status: 202, body: '' });
	return Promise.resolve({
		ok: said.status >= 200 && said.status < 300,
		status: said.status,
		headers: { get: function (/** @type {string} */ name) {
			return name.toLowerCase() === 'content-type' && said.body !== ''
				? (said.status < 300 ? 'application/json' : 'application/problem+json') : null;
		} },
		text: function () { return Promise.resolve(said.body); },
		json: function () { return Promise.resolve(said.body === '' ? null : JSON.parse(said.body)); },
	});
};
globalThis.document = /** @type {any} */ ({ documentElement: {} });

const problem = await import('../../data/ni-web/app/problem.js');
const words = (await import('../../data/ni-web/app/data.text.js')).text;
const i18n = await import('../../data/ni-web/app/i18n.js');
const session = await import('../../data/ni-web/app/session.js');
const event = await import('../../data/ni-web/app/ui/event.js');
const eventWords = (await import('../../data/ni-web/app/ui/event.text.js')).default;

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
		process.stderr.write('refusal: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

function settle() {
	return new Promise(function (resolve) {
		setTimeout(function () { setTimeout(resolve, 0); }, 0);
	});
}

const line = problem.refusalLine;
const kPast = { type: '/errors/timer-in-the-past', title: 'Bad Request', status: 400,
	detail: 'a timer that runs once cannot begin before now' };

// ------------------------------------------------------------ the one line

same(line(kPast), words.de['problem.code.timer-in-the-past'], 'a code with words is said in them');
i18n.setLanguage('en');
same(line(kPast), words.en['problem.code.timer-in-the-past'], 'and in the language of the page');
i18n.setLanguage('de');
same(line({ type: '/errors/bad-int', title: 'Bad Request', status: 400, detail: 'start is not a whole number' }),
	'start is not a whole number', 'a code without words is said as the box said it');
same(line({ type: '/errors/bad-int', title: 'Bad Request', status: 400, detail: '' }),
	'Bad Request', 'a refusal without a reason falls back to its title');
same(line({ type: 'https://example.org/errors/timer-in-the-past', title: 'Bad Request', status: 400, detail: 'elsewhere' }),
	'elsewhere', 'only the box spells a code, so an address of somebody else is no code');
same(line({ type: '', title: 'Bad Request', status: 400, detail: 'plain' }), 'plain', 'no type is no code');
same(line(problem.offlineError('').problem), words.de['problem.offline.detail'], 'a box that did not answer says so');
same([line(null), line(undefined), line('Conflict'), line(409), line({})], ['', '', '', '', ''],
	'what is no problem document has no line, and the caller says what failed');

// Every code with words is one the box sends, and has them in both languages.
const errors = readFileSync(join(here, '..', '..', 'src', 'coreapi', 'base', 'errors.h'), 'utf8');
const sent = new Set((errors.match(/return "[a-z0-9-]+";/g) || []).map(function (one) { return one.slice(8, -2); }));
const coded = Object.keys(words.de).filter(function (key) { return key.indexOf('problem.code.') === 0; });
same(coded.length > 30, true, 'the codes the screens meet have words');
for (const key of coded) {
	const code = key.slice('problem.code.'.length);
	same(sent.has(code), true, code + ' is a code the box sends');
	same(typeof words.en[key], 'string', key + ' is there in English too');
}
for (const code of ['timer-in-the-past', 'recording-without-duration', 'timer-exists', 'recording-holds-tuner',
	'box-in-standby', 'recording-running', 'no-running-channel', 'no-such-timer', 'no-such-channel']) {
	same(typeof words.de['problem.code.' + code], 'string', code + ' has words');
}

// ------------------------------------------------- the guide's event sheet

await session.refresh();
same(session.canWrite(), true, 'the session the box answered carries write');

// Recording now is judged again at the click, by the box's clock.
const began = Math.floor(Date.now() / 1000) - 600;
const onAir = /** @type {any} */ ({ id: '7', channel_id: 'b9b0040200016dcb', title: 'Tatort', start: began, duration: 5400 });
const record = event.eventActions({ event: onAir, at: began + 100 }).find(function (a) { return a.id === 'record'; });
const zap = event.eventActions({ event: onAir, at: began + 100 }).find(function (a) { return a.id === 'zap'; });

globalThis.__toasts = [];
answers = [refusal(400, 'timer-in-the-past', kPast.detail)];
record.onAct();
await settle();
same(globalThis.__toasts, [[words.de['problem.code.timer-in-the-past'], 'bad']],
	'recording now that the box turns down says why, not the name of the status');

globalThis.__toasts = [];
answers = [refusal(400, 'bad-int', 'stop is not a whole number')];
record.onAct();
await settle();
same(globalThis.__toasts, [['stop is not a whole number', 'bad']], 'a code without words is the box\'s own reason');

globalThis.__toasts = [];
answers = [refusal(404, 'no-such-channel', 'no channel with that id')];
zap.onAct();
await settle();
same(globalThis.__toasts, [[words.de['problem.code.no-such-channel'], 'bad']], 'a zap the box turns down says why');

globalThis.__toasts = [];
answers = [];
zap.onAct();
await settle();
same(globalThis.__toasts, [[i18n.t(eventWords, 'epg.event.zap.done', { title: onAir.title }), '']],
	'a zap the box took is said as done, so the cases above saw a refusal and not a stub');

// ------------------------------------------------------ the recordings

const archive = await import('../../data/ni-web/app/screens/recordings/archive.model.js');
same(archive.refusalReason({ problem: { type: '/errors/timeshift-running', title: 'Conflict', status: 409,
	detail: 'the box is already keeping a shift' } }), words.de['problem.code.timeshift-running'],
	'a timeshift the box will not start says why');
same(archive.refusalReason({ problem: { type: '/errors/no-timeshift', title: 'Conflict', status: 409, detail: '' } }),
	words.de['problem.code.no-timeshift'], 'and one it cannot stop');

// verdict

const FLOOR = 100;
if (checked < FLOOR) {
	process.stderr.write('refusal-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('refusal: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('refusal: ' + checked + ' checked\n');
