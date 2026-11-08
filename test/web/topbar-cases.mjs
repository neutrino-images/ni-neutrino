// The name in the top left corner of the header, and where it leads, run without a browser.
import * as loader from 'node:module';
import { existsSync } from 'node:fs';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('topbar-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const kHtm = new URL('./node_modules/htm/dist/htm.module.js', import.meta.url);
if (!existsSync(kHtm)) {
	process.stderr.write('topbar-cases.mjs: no ' + kHtm.pathname + '; sh test/web/fetch-types.sh puts it there\n');
	process.exit(1);
}

const kStubs = {
	'/vendor/preact.module.js':
		'export function h(type, props) { return { type: type, props: props || {}, children: Array.prototype.slice.call(arguments, 2) }; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n' +
		'export function createContext(v) { return { Provider: function () { return null; }, value: v }; }\n',
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId', 'useContext', 'useReducer']
			.map(function (name) { return 'export function ' + name + '(a, b) { return globalThis.topbarHooks.' + name + '(a, b); }\n'; }).join(''),
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
			throw new Error('topbar-cases.mjs: no stub for the runtime module ' + spec);
		return next(spec, context);
	},
});

// Only the header itself is called; what it draws inside are elements, not calls.
globalThis.topbarHooks = {
	useState: function (/** @type {unknown} */ init) { return [typeof init === 'function' ? init() : init, function () {}]; },
	useEffect: function () {},
	useLayoutEffect: function () {},
	useRef: function (/** @type {unknown} */ init) { return { current: init }; },
	useMemo: function (/** @type {() => unknown} */ fn) { return fn(); },
	useCallback: function (/** @type {unknown} */ fn) { return fn; },
	useId: function () { return ':t'; },
	useContext: function (/** @type {{ value: unknown }} */ c) { return c ? c.value : undefined; },
	useReducer: function (/** @type {unknown} */ _r, /** @type {unknown} */ init) { return [init, function () {}]; },
};

const { Topbar } = await import('../../data/ni-web/app/ui/topbar.js');
const { Link } = await import('/vendor/preact-router.module.js');
const { t } = await import('../../data/ni-web/app/i18n.js');
const text = (await import('../../data/ni-web/app/shell.text.js')).default;

let checked = 0;
let failed = 0;

/** @param {unknown} got @param {unknown} want @param {string} what */
function same(got, want, what) {
	checked++;
	if (JSON.stringify(got) !== JSON.stringify(want)) {
		failed++;
		process.stderr.write('topbar: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
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

for (const status of [null, { carrying: true, timers: 0 }]) {
	const tree = Topbar({ status: status });
	const what = status ? 'with a stream carrying' : 'before the box has answered';
	const brands = nodes(tree, function (n) { return n.props && n.props.class === 'brand'; });
	same(brands.length, 1, 'one brand in the header ' + what);
	const brand = brands[0];
	if (!brand)
		continue;
	same(brand.type === Link, true, 'the brand is a link the router follows ' + what);
	same(brand.props.href, '/', 'the brand leads to the main page ' + what);
	same(words(brand).trim(), t(text, 'shell.brand'), 'the brand still says the name ' + what);

	// The first thing in the first row, so it is the top left corner.
	const main = nodes(tree, function (n) { return n.props && n.props.class === 'topbar-main'; })[0];
	const first = main ? main.children.find(function (c) { return c && typeof c === 'object'; }) : null;
	same(first === brand, true, 'the brand opens the first row ' + what);
}

if (checked < 10) {
	process.stderr.write('topbar-cases.mjs: only ' + checked + ' assertions ran\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('topbar-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-topbar.sh: ' + checked + ' assertions over the name in the corner and where it leads\n');
