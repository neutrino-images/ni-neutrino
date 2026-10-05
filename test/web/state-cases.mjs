// What the shared widget draws for a refusal, and for one being read again, run without a browser.
import * as loader from 'node:module';
import { existsSync } from 'node:fs';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('state-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const kHtm = new URL('./node_modules/htm/dist/htm.module.js', import.meta.url);
if (!existsSync(kHtm)) {
	process.stderr.write('state-cases.mjs: no ' + kHtm.pathname + '; sh test/web/fetch-types.sh puts it there\n');
	process.exit(1);
}

const kStubs = {
	'/vendor/preact.module.js':
		'export function h(type, props) { return { type: type, props: props || {}, children: Array.prototype.slice.call(arguments, 2) }; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId']
			.map(function (name) { return 'export function ' + name + '(a, b) { return globalThis.stateHooks.' + name + '(a, b); }\n'; }).join(''),
	'/vendor/preact-router.module.js':
		'export default function Router() { return null; }\n' +
		'export function Link() { return null; }\n' +
		'export function route() {}\n' +
		'export function getCurrentUrl() { return ""; }\n',
};

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (spec === '/vendor/htm.module.js')
			return { url: kHtm.href, shortCircuit: true };
		if (Object.prototype.hasOwnProperty.call(kStubs, spec))
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		if (spec.indexOf('/vendor/') === 0)
			throw new Error('state-cases.mjs: no stub for the runtime module ' + spec);
		return next(spec, context);
	},
});

// Hooks by call position for the one widget drawn at a time; mounted() starts a fresh one.
const hooks = { slots: /** @type {any[]} */ ([]), at: 0 };
/** @param {() => unknown} fn @param {unknown[] | undefined} deps */
function effect(fn, deps) {
	const i = hooks.at++;
	const was = hooks.slots[i];
	if (was && deps && was.deps && deps.every(function (d, k) { return Object.is(d, was.deps[k]); }))
		return;
	hooks.slots[i] = { deps: deps };
	fn();
}
globalThis.stateHooks = {
	useState: function (/** @type {unknown} */ init) {
		const i = hooks.at++;
		if (!(i in hooks.slots))
			hooks.slots[i] = { v: init };
		const slot = hooks.slots[i];
		return [slot.v, function (/** @type {unknown} */ next) { slot.v = next; }];
	},
	useEffect: effect,
	useLayoutEffect: effect,
	useRef: function (/** @type {unknown} */ init) {
		const i = hooks.at++;
		if (!(i in hooks.slots))
			hooks.slots[i] = { current: init };
		return hooks.slots[i];
	},
	useMemo: function (/** @type {() => unknown} */ fn) { hooks.at++; return fn(); },
	useCallback: function (/** @type {unknown} */ fn) { hooks.at++; return fn; },
	useId: function () { return ':h' + (hooks.at++); },
};
function mounted() {
	hooks.slots = [];
}
/** @param {Record<string, unknown>} props @returns {unknown} drawn, as the same widget drawn again */
function draw(props) {
	hooks.at = 0;
	return State(props);
}

const { State } = await import('../../data/ni-web/app/ui/state.js');
const { Button } = await import('../../data/ni-web/app/ui/button.js');
const { t } = await import('../../data/ni-web/app/i18n.js');
const text = (await import('../../data/ni-web/app/shell.text.js')).default;

let checked = 0;
let failed = 0;

/** @param {unknown} got @param {unknown} want @param {string} what */
function same(got, want, what) {
	checked++;
	if (JSON.stringify(got) !== JSON.stringify(want)) {
		failed++;
		process.stderr.write('state: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

/** @typedef {{ type: unknown, props: Record<string, unknown>, children: unknown[] }} Node */

/** @param {unknown} node @param {(n: Node) => boolean} match @returns {Node[]} */
function nodes(node, match) {
	/** @type {Node[]} */
	const out = [];
	(function walk(/** @type {unknown} */ n) {
		if (Array.isArray(n)) {
			n.forEach(walk);
			return;
		}
		if (!n || typeof n !== 'object' || !('type' in n))
			return;
		const v = /** @type {Node} */ (n);
		if (match(v))
			out.push(v);
		walk(v.children);
	})(node);
	return out;
}

/** @param {unknown} node @returns {string} */
function words(node) {
	if (node === null || node === undefined || typeof node === 'boolean')
		return '';
	if (typeof node === 'string' || typeof node === 'number')
		return String(node);
	if (Array.isArray(node))
		return node.map(words).join('');
	return words(/** @type {Node} */ (node).children);
}

/** @param {unknown} tree @returns {Array<[boolean, string]>} every retry, whether it can be pressed and what it says */
function retries(tree) {
	return nodes(tree, function (n) { return n.type === Button; }).map(function (b) {
		return [!b.props.disabled, words(b.children).trim()];
	});
}

const problem = { title: 'Fehlgeschlagen', detail: 'Die Box hat nicht geantwortet', href: '' };
let retried = 0;
/** @returns {void} */
function retry() { retried++; }

/** @param {unknown} tree */
function press(tree) {
	const b = nodes(tree, function (n) { return n.type === Button; })[0];
	if (b && typeof b.props.onClick === 'function')
		/** @type {() => void} */ (b.props.onClick)();
}

{
	mounted();
	const tree = draw({ problem: problem, phase: '', onRetry: retry });
	same(retries(tree), [[true, t(text, 'shell.retry')]], 'a refusal offers to try again');
	same(nodes(tree, function (n) { return n.props.role === 'alert'; }).length, 1, 'and is said as an alert');
}

{
	mounted();
	draw({ problem: problem, phase: '', onRetry: retry });
	same(retries(draw({ problem: problem, phase: 'again', onRetry: retry })), [[true, t(text, 'shell.retry')]],
		'a refusal read again by itself, on a timer or an event, leaves its button as it was');
	same(retries(draw({ problem: problem, phase: '', onRetry: retry })), [[true, t(text, 'shell.retry')]],
		'and after that read too');
}

{
	mounted();
	retried = 0;
	press(draw({ problem: problem, phase: '', onRetry: retry }));
	same(retried, 1, 'pressed, it asks again');
	const tree = draw({ problem: problem, phase: 'again', onRetry: retry });
	same(retries(tree), [[false, t(text, 'shell.reloading')]], 'tried again, the button says so and cannot be pressed twice');
	same(words(tree).indexOf(problem.title) !== -1, true, 'and the refusal stays until the answer replaces it');
	draw({ problem: problem, phase: '', onRetry: retry });
	same(retries(draw({ problem: problem, phase: 'again', onRetry: retry })), [[true, t(text, 'shell.retry')]],
		'once that read is answered, the next one nobody asked for leaves the button alone');
}

{
	mounted();
	const tree = draw({ problem: problem, phase: 'first', onRetry: retry });
	same(retries(tree), [[true, t(text, 'shell.retry')]], 'a refusal is no first load, whatever the phase is');
}

{
	mounted();
	const tree = draw({ problem: null, phase: 'again', onRetry: retry, children: 'Liste' });
	same([retries(tree), words(tree)], [[], 'Liste'], 'a read again with nothing refused draws what is there and no mark');
}

if (checked < 5) {
	process.stderr.write('state-cases.mjs: only ' + checked + ' assertions ran\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('state-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-state.sh: ' + checked + ' assertions over a refusal and a refusal read again\n');
