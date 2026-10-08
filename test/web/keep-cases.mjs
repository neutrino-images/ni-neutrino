// What the viewer opened stays open when a screen is drawn anew by an automatic reload,
// run without a browser.
import * as loader from 'node:module';
import { existsSync } from 'node:fs';

/** @returns {any} */
function element() {
	return { style: {}, dataset: {}, setAttribute() {}, removeAttribute() {}, appendChild() {}, remove() {},
		addEventListener() {}, removeEventListener() {}, querySelector() { return null; }, querySelectorAll() { return []; } };
}
globalThis.document = /** @type {any} */ ({ documentElement: { lang: 'de' }, head: element(), body: element(),
	createElement: element, addEventListener() {}, removeEventListener() {}, querySelector() { return null; },
	querySelectorAll() { return []; }, activeElement: null, visibilityState: 'visible' });
/** @type {Map<string, string>} */
const tab = new Map();
const storage = { getItem(/** @type {string} */ k) { return tab.has(k) ? tab.get(k) : null; },
	setItem(/** @type {string} */ k, /** @type {string} */ v) { tab.set(k, String(v)); },
	removeItem(/** @type {string} */ k) { tab.delete(k); } };
globalThis.window = /** @type {any} */ ({ sessionStorage: storage, localStorage: storage,
	setTimeout: setTimeout, clearTimeout: clearTimeout, setInterval: function () { return 0; }, clearInterval: function () {},
	addEventListener() {}, removeEventListener() {},
	// The remote screen's picture loads into one of these; it never arrives here.
	Image: function () { return { onload: null, onerror: null, src: '' }; },
	matchMedia: function () { return { matches: false, addEventListener() {}, removeEventListener() {} }; } });
// A screen effect left running at the end is not what is asked here.
process.on('unhandledRejection', function () { });

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('keep-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const kHtm = new URL('./node_modules/htm/dist/htm.module.js', import.meta.url);
if (!existsSync(kHtm)) {
	process.stderr.write('keep-cases.mjs: no ' + kHtm.pathname + '; sh test/web/fetch-types.sh puts it there\n');
	process.exit(1);
}

const kStubs = {
	'/vendor/preact.module.js':
		'export function h(type, props) { return { type: type, props: props || {}, children: Array.prototype.slice.call(arguments, 2) }; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId']
			.map(function (name) { return 'export function ' + name + '(a, b) { return globalThis.keepHooks.' + name + '(a, b); }\n'; }).join(''),
	'/vendor/preact-router.module.js':
		'export default function Router() { return null; }\n' +
		'export function Link() { return null; }\n' +
		'export function route() {}\n' +
		'export function getCurrentUrl() { return globalThis.keepUrl || ""; }\n',
};

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (spec === '/vendor/htm.module.js')
			return { url: kHtm.href, shortCircuit: true };
		if (Object.prototype.hasOwnProperty.call(kStubs, spec))
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		if (spec.indexOf('/vendor/') === 0)
			throw new Error('keep-cases.mjs: no stub for the runtime module ' + spec);
		return next(spec, context);
	},
});

// Hooks by call position, each component's kept by its place in the tree, the way a
// browser keeps a component mounted while its place stays drawn.
const hooks = { slots: /** @type {any[]} */ ([]), at: 0 };
/** @param {unknown[] | undefined} was @param {unknown[] | undefined} now */
function sameDeps(was, now) {
	return !!was && !!now && was.length === now.length && now.every(function (d, i) { return Object.is(d, was[i]); });
}
/** @param {() => unknown} fn @param {unknown[] | undefined} deps */
function effect(fn, deps) {
	const i = hooks.at++;
	const was = hooks.slots[i];
	if (was && sameDeps(was.deps, deps))
		return;
	if (was && typeof was.undo === 'function')
		was.undo();
	hooks.slots[i] = { deps: deps, undo: null, effect: true };
	hooks.slots[i].undo = fn();
}
globalThis.keepHooks = {
	useState: function (/** @type {unknown} */ init) {
		const i = hooks.at++;
		if (!(i in hooks.slots))
			hooks.slots[i] = { v: typeof init === 'function' ? init() : init };
		const slot = hooks.slots[i];
		return [slot.v, function (/** @type {unknown} */ next) { slot.v = typeof next === 'function' ? next(slot.v) : next; }];
	},
	useRef: function (/** @type {unknown} */ init) {
		const i = hooks.at++;
		if (!(i in hooks.slots))
			hooks.slots[i] = { current: init };
		return hooks.slots[i];
	},
	useEffect: effect,
	useLayoutEffect: effect,
	useMemo: function (/** @type {() => unknown} */ fn) { hooks.at++; return fn(); },
	useCallback: function (/** @type {unknown} */ fn) { hooks.at++; return fn; },
	useId: function () { return ':h' + (hooks.at++); },
};

const runtime = await import('../../data/ni-web/app/runtime.js');
const kept = await import('../../data/ni-web/app/ui/kept.js');
const actions = await import('../../data/ni-web/app/ui/actions.js');

let checked = 0;
let failed = 0;
/** @param {unknown} got @param {unknown} want @param {string} what */
function same(got, want, what) {
	checked++;
	if (JSON.stringify(got) !== JSON.stringify(want)) {
		failed++;
		process.stderr.write('keep: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

/** @typedef {{ type: unknown, of?: unknown, props: Record<string, unknown>, children?: unknown }} Drawn */

/** @type {Map<string, any[]>} */
const live = new Map();
/** @type {Set<string>} */
let seen = new Set();

/** @param {any[]} slots */
function undo(slots) {
	for (const s of slots)
		if (s && s.effect && typeof s.undo === 'function')
			s.undo();
}

/**
 * A drawn tree with every component in it drawn as well.
 *
 * @param {unknown} node
 * @param {string} path
 * @returns {unknown}
 */
function expand(node, path) {
	if (Array.isArray(node))
		return node.map(function (c, i) {
			const k = c && typeof c === 'object' && 'props' in c && c.props && c.props.key !== undefined && c.props.key !== null
				? 'k' + c.props.key : String(i);
			return expand(c, path + '.' + k);
		});
	if (!node || typeof node !== 'object' || !('type' in node))
		return node;
	const v = /** @type {{ type: any, props: Record<string, unknown>, children: unknown[] }} */ (node);
	if (typeof v.type !== 'function')
		return { type: v.type, props: v.props, children: expand(v.children, path + '/' + v.type) };
	if (v.type === runtime.Fragment)
		return expand(v.children, path + '/F');
	const at = path + '/' + (v.type.name || 'C');
	let slots = live.get(at);
	if (!slots) {
		slots = [];
		live.set(at, slots);
	}
	seen.add(at);
	const was = { slots: hooks.slots, at: hooks.at };
	hooks.slots = slots;
	hooks.at = 0;
	let out = null;
	try {
		const c = v.children.length === 1 ? v.children[0] : v.children;
		out = v.type(Object.assign({}, v.props, { children: c }));
	} catch (e) {
		failed++;
		process.stderr.write('keep: ' + at + ' threw ' + String(e) + '\n');
	} finally {
		Object.assign(hooks, was);
	}
	return { type: 'component', of: v.type, props: v.props, children: expand(out, at) };
}

/**
 * One drawing of a component under a name of its own; a place no longer drawn is unmounted.
 *
 * @param {string} name
 * @param {(props: any) => unknown} component
 * @param {Record<string, unknown>} props
 * @returns {unknown}
 */
function draw(name, component, props) {
	seen = new Set();
	const tree = expand(runtime.h(component, props), name);
	live.forEach(function (slots, at) {
		if (at.indexOf(name + '/') === 0 && !seen.has(at)) {
			undo(slots);
			live.delete(at);
		}
	});
	return tree;
}

/** @param {string} name every component drawn under it, unmounted */
function unmount(name) {
	live.forEach(function (slots, at) {
		if (at.indexOf(name + '/') === 0) {
			undo(slots);
			live.delete(at);
		}
	});
}

async function settle() {
	for (let i = 0; i < 8; ++i)
		await new Promise(function (done) { setTimeout(done, 0); });
}

/**
 * Drawn, and drawn again while the answers it asked for come in.
 *
 * @param {string} name
 * @param {(props: any) => unknown} component
 * @param {Record<string, unknown>} props
 * @returns {Promise<unknown>}
 */
async function drawn(name, component, props) {
	let tree = draw(name, component, props);
	for (let i = 0; i < 5; ++i) {
		await settle();
		tree = draw(name, component, props);
	}
	return tree;
}

/**
 * @param {unknown} node
 * @param {(n: Drawn) => boolean} match
 * @returns {Drawn[]}
 */
function nodes(node, match) {
	/** @type {Drawn[]} */
	const out = [];
	(function walk(/** @type {unknown} */ n) {
		if (Array.isArray(n)) {
			n.forEach(walk);
			return;
		}
		if (!n || typeof n !== 'object' || !('type' in n))
			return;
		const v = /** @type {Drawn} */ (n);
		if (v.props && match(v))
			out.push(v);
		walk(v.children);
	})(node);
	return out;
}

/** @param {unknown} tree @returns {Drawn[]} every row's actions, in order */
function rowsOf(tree) {
	return nodes(tree, function (n) { return n.type === 'component' && n.of === actions.RowActions; });
}

/** @param {unknown} row one row's actions as drawn @returns {boolean} */
function sheetOpen(row) {
	const sheet = nodes(row, function (n) { return n.type === 'component' && 'onClose' in n.props && 'label' in n.props; })[0];
	return !!sheet && sheet.props.open === true;
}

/** @param {unknown} row */
function pressMore(row) {
	const more = nodes(row, function (n) { return n.props.class === 'btn acts-more'; })[0];
	/** @type {() => void} */ (more.props.onClick)();
}

/** @type {Record<string, [number, unknown]>} */
const answers = {};
globalThis.fetch = /** @type {any} */ (async function (/** @type {string} */ url) {
	const path = String(url).split('?')[0];
	const a = answers[path] || [404, { type: '/errors/not-found', title: 'Not Found', status: 404, detail: 'no such route' }];
	return new Response(JSON.stringify(a[1]), { status: a[0],
		headers: { 'content-type': a[0] < 300 ? 'application/json' : 'application/problem+json' } });
});

// the helper

{
	same(kept.fold('k:a').open, false, 'a fold nobody opened is shut');
	kept.fold('k:a').onToggle(/** @type {any} */ ({ currentTarget: { open: true } }));
	same([kept.fold('k:a').open, kept.fold('k:b').open], [true, false], 'an opened fold is open when drawn again, and only that one');
	kept.fold('k:a').onToggle(/** @type {any} */ ({ currentTarget: { open: false } }));
	same(kept.fold('k:a').open, false, 'and shut once the viewer shuts it');
}

{
	/** @param {{ k: string }} p @returns {any} */
	function Kept(p) { return { type: 'kept', props: { value: kept.useKept(p.k, false) }, children: [] }; }
	/** @param {unknown} tree @returns {any} */
	function valueOf(tree) { return /** @type {any} */ (tree).children.props.value; }
	const first = valueOf(draw('u1', Kept, { k: 'k:s' }));
	same(first[0], false, 'a kept value starts as given');
	first[1](true);
	same(valueOf(draw('u2', Kept, { k: 'k:s' }))[0], true, 'a component mounted anew finds what the one before it set');
	kept.leave('/elsewhere');
	same(valueOf(draw('u3', Kept, { k: 'k:s' }))[0], false, 'and loses it once the address changes');
	valueOf(draw('u4', Kept, { k: '' }))[1](true);
	same(valueOf(draw('u4', Kept, { k: '' }))[0], true, 'without a key it is the component\'s own');
	same(valueOf(draw('u5', Kept, { k: '' }))[0], false, 'and gone with it');
}

{
	/** @param {string | undefined} keep */
	function rowProps(keep) {
		return { title: 'Row', keep: keep, actions: [{ id: 'go', label: 'Go', onAct: function () { } }] };
	}
	pressMore(draw('r1', actions.RowActions, rowProps('k:row')));
	same(sheetOpen(draw('r1', actions.RowActions, rowProps('k:row'))), true, 'the row sheet opens');
	same(sheetOpen(draw('r2', actions.RowActions, rowProps('k:row'))), true, 'and is open when the row is drawn anew');
	same(sheetOpen(draw('r3', actions.RowActions, rowProps('k:other'))), false, 'another row is not');
	pressMore(draw('r4', actions.RowActions, rowProps(undefined)));
	same(sheetOpen(draw('r5', actions.RowActions, rowProps(undefined))), false, 'a row without a key keeps nothing');
	const act = nodes(draw('r6', actions.RowActions, rowProps('k:row')), function (n) {
		return n.type === 'button' && n.props['data-act'] === 'go' && n.props.class !== 'btn';
	});
	same(act.length, 1, 'the sheet drawn anew carries its actions');
	if (act.length === 1)
		/** @type {() => void} */ (act[0].props.onClick)();
	same(sheetOpen(draw('r7', actions.RowActions, rowProps('k:row'))), false, 'an action taken from the sheet closes it for good');
}

// the screens

const session = await import('../../data/ni-web/app/session.js');
const store = await import('../../data/ni-web/app/store.js');
answers['/api/v1/session'] = [200, { authenticated: true, level: 'system', user: 'root', csrf: 'c', csrf_header: 'X-CSRF-Token', expires_in: 0 }];
await session.refresh();

// the keys of the remote no group claims
answers['/api/v1/osd/remote/keys'] = [200, { items: ['KEY_OK', 'KEY_UP', 'KEY_RED', 'KEY_PROG1', 'KEY_FAVORITES'] }];
answers['/api/v1/osd/remote'] = [200, { locked: false }];
{
	const remote = (await import('../../data/ni-web/app/screens/now/remote.js')).default;
	/** @param {unknown} tree */
	function rest(tree) { return nodes(tree, function (n) { return n.type === 'details' && /now-keyrest/.test(String(n.props.class)); }); }
	const before = rest(await drawn('s:remote', remote, {}));
	same([before.length, before.length === 1 && before[0].props.open], [1, false], 'the keys no group claims are folded away');
	if (before.length === 1 && typeof before[0].props.onToggle === 'function')
		/** @type {(e: unknown) => void} */ (before[0].props.onToggle)({ currentTarget: { open: true } });
	store.clear();
	const after = rest(await drawn('s:remote', remote, {}));
	same(after.map(function (n) { return n.props.open; }), [true], 'opened, they are open when the keys are read again');
	unmount('s:remote');
}

/**
 * The second row's sheet is opened, the box's answers are forgotten the way a gap in
 * the event stream forgets them, and the screen draws its list anew.
 *
 * @param {string} what
 * @param {(props: any) => unknown} screen
 * @param {Record<string, unknown>} props
 * @param {{ whole?: boolean, ask?: (tree: unknown, redraw: () => unknown) => void }} [how] whole:
 *        the whole screen mounted anew rather than its answers forgotten; ask: what the viewer does first
 */
async function rowSheetStays(what, screen, props, how) {
	kept.leave('/elsewhere');
	const name = 's:' + what;
	let first = await drawn(name, screen, props);
	if (how && how.ask) {
		how.ask(first, function () { return draw(name, screen, props); });
		first = await drawn(name, screen, props);
	}
	const before = rowsOf(first);
	same(before.length >= 2, true, what + ': two rows are drawn');
	if (before.length < 2)
		return;
	pressMore(before[1]);
	same(sheetOpen(rowsOf(draw(name, screen, props))[1]), true, what + ': the second row\'s sheet opens');
	if (how && how.whole)
		unmount(name);
	else
		store.clear();
	const after = rowsOf(await drawn(name, screen, props));
	same(after.map(sheetOpen), before.map(function (_, i) { return i === 1; }),
		what + ': drawn anew, the second row\'s sheet is open and no other');
	unmount(name);
	kept.leave('/elsewhere');
}

/** @param {string} id @param {number} n @returns {Api.Channel} */
function channelOf(id, n) {
	return { id: id, epg_id: id, number: n, name: 'Dummy ' + n, url: '', service_id: n, transport_stream_id: 1,
		original_network_id: 1, satellite_position: 192, freq_id: 1, kind: 'tv', scrambled: false, locked: false };
}

// two timers alike but for their day
answers['/api/v1/timers'] = [200, { items: [1800000000, 1800086400].map(function (start, i) {
	return { id: i + 1, kind: 'record', channel_id: 'b9b0040200016dcb', start: start, stop: start + 3600, title: 'Tatort', repeat: 0,
		repeat_count: 0, state: 0, announce: 0, epg_id: '0', epg_start: 0, standby_on: false, recording_dir: '' };
}) }];
await rowSheetStays('a timer', (await import('../../data/ni-web/app/screens/timers/list.js')).default, { param: '' });

// two recordings under one title
answers['/api/v1/recordings/archive'] = [200, { items: ['dd27cf8d43d5a28f', '9f340d243c304c86'].map(function (id, i) {
	return { id: id, title: 'Tatort', channel: 'Das Erste', channel_id: '0', start: 2 - i, duration: 60, size: 1, playing: false };
}), total: 2 }];
await rowSheetStays('a recording', (await import('../../data/ni-web/app/screens/recordings/archive.js')).default, {});

// two files in one directory
globalThis.keepUrl = '/files/list?path=%2Ftmp%2Foffen';
answers['/api/v1/storage/files'] = [200, { path: '/tmp/offen', items: ['eins.txt', 'zwei.txt'].map(function (id) {
	return { id: id, kind: 'regular', size: 5, modified: 1 };
}) }];
await rowSheetStays('a file', (await import('../../data/ni-web/app/screens/files/list.js')).default, {});

// two filled network drives and a free slot
answers['/api/v1/storage/netfs/fstab'] = [200, { table: 'fstab', path: '/var/etc/fstab', unreadable_lines: 0, items: [0, 1, 2].map(function (i) {
	const filled = i < 2;
	return { slot: i, active: filled, type: 'nfs', host: filled ? '192.168.1.' + (9 + i) : '', remote_dir: filled ? '/srv/' + i : '',
		local_dir: filled ? '/media/' + i : '', user: '', has_password: false, options: '' };
}) }];
answers['/api/v1/storage/netfs/automount'] = [200, { table: 'automount', path: '/var/etc/auto.net', items: [], unreadable_lines: 0 }];
await rowSheetStays('a network drive', (await import('../../data/ni-web/app/screens/files/netfs.js')).default, {});

// two programmes of one title on one channel, in its day and as search hits
const day = [1791064800, 1791065700].map(function (start, i) {
	return { id: '40200016dd0100' + i, channel_id: 'b9b0040200016dd0', title: 'Tagesschau', description: '', start: start, duration: 900 };
});
answers['/api/v1/epg'] = [200, { items: day }];
answers['/api/v1/channels/b9b0040200016dd0'] = [200, channelOf('b9b0040200016dd0', 1)];
answers['/api/v1/bouquets'] = [200, { items: [] }];
answers['/api/v1/channels'] = [200, { items: [channelOf('b9b0040200016dd0', 1)] }];
await rowSheetStays('a programme of a channel\'s day', (await import('../../data/ni-web/app/screens/epg/schedule.js')).default,
	{ param: 'b9b0040200016dd0' });
answers['/api/v1/epg/search'] = [200, { items: day, truncated: false }];
answers['/api/v1/channels/logos'] = [200, { items: [] }];
await rowSheetStays('a search hit', (await import('../../data/ni-web/app/screens/epg/search.js')).default, {}, {
	ask: function (tree, redraw) {
		const query = nodes(tree, function (n) { return n.type === 'input' && typeof n.props.onInput === 'function' && n.props.type !== 'date'; })[0];
		/** @type {(e: unknown) => void} */ (query.props.onInput)({ currentTarget: { value: 'Tagesschau' } });
		const form = nodes(redraw(), function (n) { return n.type === 'form'; })[0];
		/** @type {(e: unknown) => void} */ (form.props.onSubmit)({ preventDefault: function () { } });
	},
});

// two channels, the list walked again from the start the way a bouquet change starts it
answers['/api/v1/channels'] = [200, { items: [channelOf('b9b0040200016dcb', 1), channelOf('b9b0040200016dcc', 2)] }];
answers['/api/v1/epg/grid'] = [200, { items: [] }];
await rowSheetStays('a channel', (await import('../../data/ni-web/app/screens/channels/list.js')).default, {}, { whole: true });

// the sign in sheet while a sign in is on its way
answers['/api/v1/session'] = [200, { authenticated: false, level: 'read', user: '', csrf: '', csrf_header: 'X-CSRF-Token', expires_in: 0 }];
await session.refresh();
{
	const signin = await import('../../data/ni-web/app/ui/signin.js');
	const plain = globalThis.fetch;
	// The login is never answered, as on a box that has stopped answering.
	globalThis.fetch = /** @type {any} */ (function (/** @type {string} */ url, /** @type {RequestInit} */ init) {
		return String(url).split('?')[0] === '/api/v1/login' ? new Promise(function () { }) : plain(url, init);
	});
	/** @param {unknown} tree @param {(n: Drawn) => boolean} match @returns {Drawn | undefined} */
	function one(tree, match) { return nodes(tree, match)[0]; }
	/** @param {unknown} tree @returns {boolean} */
	function up(tree) { return !!one(tree, function (n) { return n.type === 'input' && n.props.id === 'signin-password'; }); }
	/** @param {unknown} tree @param {string} cls */
	function press(tree, cls) {
		const b = one(tree, function (n) { return typeof n.props.class === 'string' && n.props.class.split(' ').indexOf(cls) !== -1; });
		if (b && typeof b.props.onClick === 'function')
			/** @type {() => void} */ (b.props.onClick)();
	}
	/** @param {unknown} tree @param {string} id @param {string} value */
	function type(tree, id, value) {
		const f = one(tree, function (n) { return n.type === 'input' && n.props.id === id; });
		if (f && typeof f.props.onInput === 'function')
			/** @type {(e: unknown) => void} */ (f.props.onInput)({ target: { value: value } });
	}
	signin.askToSignIn();
	let tree = await drawn('s:signin', signin.SignIn, {});
	same(up(tree), true, 'the sheet is up once somebody asks to sign in');
	type(tree, 'signin-user', 'root');
	type(draw('s:signin', signin.SignIn, {}), 'signin-password', 'ni');
	press(draw('s:signin', signin.SignIn, {}), 'signin-go');
	await settle();
	press(draw('s:signin', signin.SignIn, {}), 'scrim');
	tree = await drawn('s:signin', signin.SignIn, {});
	same(up(tree), true, 'a press beside the sheet leaves a sign in under way standing');
	press(tree, 'signin-cancel');
	same(up(await drawn('s:signin', signin.SignIn, {})), false, 'cancel still takes the sheet away while the sign in hangs');
	unmount('s:signin');
	globalThis.fetch = plain;
}

// verdict

const FLOOR = 40;
if (checked < FLOOR) {
	process.stderr.write('keep-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('keep-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-keep.sh: ' + checked + ' assertions over what stays open across a reload\n');
