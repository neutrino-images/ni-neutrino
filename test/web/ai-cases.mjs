// What the AI area reads out of the box's answers and sends back, run without a browser.
import * as loader from 'node:module';
import { existsSync } from 'node:fs';

// setLanguage writes the document's language, and node has no document.
globalThis.document = { documentElement: { lang: 'de' } };

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('ai-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

// The real template parser, so a case reads the document a screen would draw.
const kHtm = new URL('./node_modules/htm/dist/htm.module.js', import.meta.url);
if (!existsSync(kHtm)) {
	process.stderr.write('ai-cases.mjs: no ' + kHtm.pathname + '; sh test/web/fetch-types.sh puts it there\n');
	process.exit(1);
}

const kStubs = {
	'/vendor/preact.module.js':
		'export function h(type, props) { return { type: type, props: props || {}, children: Array.prototype.slice.call(arguments, 2) }; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId']
			.map(function (name) { return 'export function ' + name + '(a, b) { return globalThis.aiHooks.' + name + '(a, b); }\n'; }).join(''),
	'/vendor/preact-router.module.js':
		'export default function Router() { return null; }\n' +
		'export function Link() { return null; }\n' +
		'export function route() {}\n' +
		'export function getCurrentUrl() { return ""; }\n',
};

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (spec === '/vendor/htm.module.js') {
			return { url: kHtm.href, shortCircuit: true };
		}
		if (Object.prototype.hasOwnProperty.call(kStubs, spec)) {
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		}
		if (spec.indexOf('/vendor/') === 0) {
			throw new Error('ai-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

// Hooks keep their values by call position, so a screen drawn twice keeps what it
// was handed in between, the way it would in a browser.
const hooks = { slots: /** @type {any[]} */ ([]), at: 0, reading: false };
/**
 * @param {unknown[] | undefined} was
 * @param {unknown[] | undefined} now
 */
function sameDeps(was, now) {
	return !!was && !!now && was.length === now.length && now.every(function (d, i) { return Object.is(d, was[i]); });
}
/** @param {() => unknown} fn @param {unknown[] | undefined} deps */
function effect(fn, deps) {
	if (hooks.reading)
		return;
	const i = hooks.at++;
	const was = hooks.slots[i];
	if (was && sameDeps(was.deps, deps))
		return;
	if (was && typeof was.undo === 'function')
		was.undo();
	hooks.slots[i] = { deps: deps, undo: null };
	hooks.slots[i].undo = fn();
}
globalThis.aiHooks = {
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

const read = await import('../../data/ni-web/ai/read.js');
const scopes = await import('../../data/ni-web/ai/scopes.js');
const model = await import('../../data/ni-web/ai/model.js');
const i18n = await import('../../data/ni-web/app/i18n.js');

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
		process.stderr.write('ai: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

// ------------------------------------------------------------ reading answers

same(read.str(3), '', 'a number is not a string');
same(read.num('5'), 0, 'a string is not a number');
same(read.strings(['a', 2, 'b']), ['a', 'b'], 'a list keeps its strings only');
same(read.strings(null), [], 'nothing is an empty list');
same(read.codeOf({ problem: { type: '/errors/ai-caller-would-be-tunnel' } }), 'ai-caller-would-be-tunnel', 'the code of a refusal');
same(read.codeOf({ problem: { type: 'https://example.org/errors/x' } }), '', 'only the box spells a code');
const refusalKey = read.refusalKey;
same([403, 401, 409, 400, 500, 0].map(function (status) { return refusalKey({ problem: { title: 'Forbidden', status: status,
	detail: 'this endpoint is not open to this caller' } }); }),
	['ai.refused.forbidden', 'ai.refused.signin', 'ai.refused.conflict', 'ai.refused.other', 'ai.refused.box', 'ai.failed'],
	'a failure is one of this area\'s own sentences, chosen by its status');
same(refusalKey({}), 'ai.failed', 'nothing said is the box not answering');
same(read.worded('Die Adresse fehlt.'), true, 'words are words');
same(read.worded('ai.tunnel.frp.title'), false, 'an unknown id answers its key');
same(read.worded(''), false, 'nothing is no words');

// ------------------------------------------------------------------- scopes

same(scopes.canonical(['offline_access', 'system', 'read', 'admin', 'read']),
	['read', 'system', 'offline_access'], 'known scopes in level order, once each');
const all = ['read', 'write', 'system', 'offline_access'];
same(scopes.toggle([], 'system', true, all), ['read', 'write', 'system'], 'a level brings the levels under it');
same(scopes.toggle(['read', 'write', 'system'], 'read', false, all), [], 'dropping a level drops the ones over it');
same(scopes.toggle(['read', 'write', 'system'], 'write', false, all), ['read'], 'dropping write keeps read');
same(scopes.toggle(['read'], 'offline_access', true, all), ['read', 'offline_access'], 'offline stands apart');
same(scopes.toggle([], 'write', true, ['write']), ['write'], 'never more than was offered');
same(scopes.toggle(['read'], 'admin', true, all.concat(['admin'])), ['read'], 'an unknown scope is never granted');
same(scopes.scopeString(['system', 'read', 'offline_access'], ['read', 'system', 'offline_access']),
	'read system offline_access', 'spelled space separated in level order');
same(scopes.scopeString(['read', 'write'], ['read']), 'read', 'cut to what was offered');
same(scopes.hasLevel(['offline_access']), false, 'staying connected alone grants nothing');
same(scopes.hasLevel(['read']), true, 'reading is a level');

// ----------------------------------------------------------------- settings

const doc = { enabled: true, public_url: 'https://tv.example.org', trusted_proxies: '192.168.1.5/32', allow_lan: false,
	mcp_url: 'https://tv.example.org/mcp', tunnel_paths: ['/mcp', 7, '/oauth/'], port: 8081, default_password: true };
const box = model.readSettings(doc);
same(box, { enabled: true, publicUrl: 'https://tv.example.org', trustedProxies: '192.168.1.5/32', allowLan: false,
	mcpUrl: 'https://tv.example.org/mcp', tunnelPaths: ['/mcp', '/oauth/'], port: 8081, defaultPassword: true }, 'settings read');
same(model.readSettings('nonsense'), null, 'a body that is not an object is no settings');
same(model.readSettings({}), { enabled: false, publicUrl: '', trustedProxies: '', allowLan: true,
	mcpUrl: '', tunnelPaths: [], port: 0, defaultPassword: false }, 'missing members read as the delivered state');
same((model.readSettings({ default_password: 'yes' }) || {}).defaultPassword, false, 'only true is the shipped password');
same((model.readSettings({ trusted_proxies: ['10.0.0.1'] }) || {}).trustedProxies, '', 'a list where one string belongs is nothing');
same(model.readSaved({ ai: doc, restarting: true }), { settings: box, restarting: true }, 'a save answers the settings and the restart');
same(model.readSaved({ ai: doc }), { settings: box, restarting: false }, 'no word about a restart is none');
same(model.readSaved(doc), null, 'the settings alone are not a save answer');
same(model.parseProxies(' 10.0.0.1, 192.168.0.0/16\n\t::1 ,,'), ['10.0.0.1', '192.168.0.0/16', '::1'], 'commas and whitespace separate');
const draft = model.draftOf(box);
same(draft, { enabled: true, publicUrl: 'https://tv.example.org', proxies: '192.168.1.5/32', allowLan: false }, 'the form starts from the box');
same(model.settingsBody(draft, box), {}, 'nothing changed sends nothing');
same(model.settingsBody({ enabled: false, publicUrl: 'https://tv.example.org', proxies: '192.168.1.5/32', allowLan: false }, box),
	{ enabled: false }, 'only what differs');
same(model.settingsBody({ enabled: true, publicUrl: 'https://tv.example.org', proxies: '192.168.1.5/32, 10.0.0.2', allowLan: false }, box),
	{ trusted_proxies: '192.168.1.5/32,10.0.0.2' }, 'the list goes as one string');
same(model.settingsBody({ enabled: true, publicUrl: ' https://tv.example.org/ ', proxies: ' 192.168.1.5/32 ', allowLan: false }, box),
	{}, 'a trailing slash and spaces are not a change');
same(model.mcpUrlOf('https://tv.example.org'), 'https://tv.example.org/mcp', 'the connector address');
same(model.mcpUrlOf('https://tv.example.org/'), 'https://tv.example.org/mcp', 'no double slash');
same(model.mcpUrlOf(''), '', 'no public address, no connector address');
same(model.publicUrlProblem(''), '', 'empty is allowed, it means home network only');
same(model.publicUrlProblem('https://tv.example.org'), '', 'https host');
same(model.publicUrlProblem('https://tv.example.org:8443/'), '', 'https host with port and slash');
same(model.publicUrlProblem('tv.example.org'), 'ai.public.bad.scheme', 'a bare host');
same(model.publicUrlProblem('http://tv.example.org'), 'ai.public.bad.scheme', 'plain http');
same(model.publicUrlProblem('https://tv.example.org/mcp'), 'ai.public.bad.path', 'a path');
same(model.publicUrlProblem('https://tv.example.org/?x=1'), 'ai.public.bad.path', 'a query');
same(model.publicUrlProblem('https://me@tv.example.org'), 'ai.public.bad.path', 'a user');
same(['https://tv.example.org', 'https://tv.example.org/', 'https://tv.example.org:443', 'https://tv.example.org:443/', '', 'tv.example.org:8443']
	.map(model.publicUrlOffPort), [false, false, false, false, false, false], 'no port, port 443 or no usable address is no port warning');
same(['https://tv.example.org:8443', ' https://tv.example.org:80/ ', 'https://[2001:db8::1]:4443'].map(model.publicUrlOffPort), [true, true, true],
	'any other port is warned about');
same(model.reachOf({ enabled: false, publicUrl: 'https://a', proxies: '', allowLan: true }), 'off', 'off is off');
same(model.reachOf({ enabled: true, publicUrl: '', proxies: '', allowLan: true }), 'lan', 'home network only');
same(model.reachOf({ enabled: true, publicUrl: 'https://a', proxies: '192.168.1.5', allowLan: false }), 'tunnel', 'tunnel only');
same(model.reachOf({ enabled: true, publicUrl: 'https://a', proxies: '192.168.1.5', allowLan: true }), 'both', 'both');
same(model.reachOf({ enabled: true, publicUrl: 'https://a', proxies: ' , ', allowLan: false }), 'noproxy', 'a tunnel without a proxy is not seen');
same(model.reachOf({ enabled: true, publicUrl: 'https://a', proxies: '', allowLan: true }), 'lan-noproxy', 'home network, tunnel not seen');
same(model.reachOf({ enabled: true, publicUrl: '', proxies: '192.168.1.5', allowLan: true }), 'lan', 'a proxy without an address is no tunnel');
same(model.reachOf({ enabled: true, publicUrl: '', proxies: '', allowLan: false }), 'none', 'on and unreachable');
same(model.reachOf({ enabled: true, publicUrl: 'https://a', proxies: '192.168.1.5', allowLan: true }, true), 'lan', 'the shipped password shuts the tunnel');
same(model.reachOf({ enabled: true, publicUrl: 'https://a', proxies: '192.168.1.5', allowLan: false }, true), 'none', 'and with it the only way in');

// ------------------------------------------------------------------ clients

const listed = model.readClients({ clients: [
	{ id: 'c1', name: 'Claude', kind: 'registered', scopes: ['write', 'read'], redirect_host: 'claude.ai', loopback_only: false,
		created: 1800000000, last_used: 0, token: 'leak' },
	{ id: '', name: 'nobody' },
	{ id: 'c2', name: '', kind: 'other', scopes: [], redirect_host: 7, loopback_only: 'yes', created: 'x' },
	{ id: 'c3', name: 'Code', kind: 'metadata', scopes: ['read'], redirect_host: '', loopback_only: true, created: 1, last_used: 2 },
] });
same(listed, [
	{ id: 'c1', name: 'Claude', kind: 'registered', scopes: ['read', 'write'], redirectHost: 'claude.ai', loopbackOnly: false,
		created: 1800000000, lastUsed: 0, groups: [] },
	{ id: 'c2', name: 'c2', kind: 'registered', scopes: [], redirectHost: '', loopbackOnly: false, created: 0, lastUsed: 0, groups: [] },
	{ id: 'c3', name: 'Code', kind: 'metadata', scopes: ['read'], redirectHost: '', loopbackOnly: true, created: 1, lastUsed: 2, groups: [] },
], 'rows read, a row without id dropped, a token never kept');
same(model.readClients({ items: [{ id: 'c1' }] }), [], 'rows only under clients');
same(model.readClients(null), [], 'no answer is no rows');
same(model.readCreated({ id: 's1', name: 'HA', kind: 'static', scopes: ['read'], token: 'nis_abc', created: 1 }),
	{ client: { id: 's1', name: 'HA', kind: 'static', scopes: ['read'], redirectHost: '', loopbackOnly: false, created: 1, lastUsed: 0, groups: [] },
		token: 'nis_abc' }, 'the token is read once, beside the row');
same(model.readCreated({ id: 's1', name: 'HA' }), null, 'no token, nothing created');
same(model.tokenNameProblem('  '), 'ai.token.name.empty', 'a name is needed');
same(model.tokenNameProblem('x'.repeat(65)), 'ai.token.name.long', 'and is short');
same(model.tokenNameProblem('ä'.repeat(33)), 'ai.token.name.long', 'counted in bytes');
same(model.tokenNameProblem('Home\tAssistant'), 'ai.token.name.bad', 'printable text only');
same(model.tokenNameProblem(' Home Assistant '), '', 'spaces round it do not count');

// ------------------------------------------------------------------- guides

const guides = model.readGuides({
	mcp_url: 'https://tv.example.org/mcp', lan_mcp_url: 'http://192.168.1.20:8081/mcp', paths: ['/mcp'],
	tunnels: [
		{ id: 'caddy', file: 'Caddyfile', snippet: 'tv.example.org {}', warnings: ['device', 3], ready: true },
		{ id: '' },
		{ id: 'frp', file: 7, snippet: 'x', ready: 'yes' },
	],
	clients: [
		{ id: 'claude', needs: 'public', url: 'https://tv.example.org/mcp', command: '', ready: true },
		{ id: 'home-assistant', needs: 'carrier-pigeon', url: '', ready: false },
		{ id: 9 },
	],
});
same(guides, {
	mcpUrl: 'https://tv.example.org/mcp', lanMcpUrl: 'http://192.168.1.20:8081/mcp', paths: ['/mcp'],
	tunnels: [
		{ id: 'caddy', file: 'Caddyfile', snippet: 'tv.example.org {}', warnings: ['device'], ready: true },
		{ id: 'frp', file: '', snippet: 'x', warnings: [], ready: false },
	],
	clients: [
		{ id: 'claude', needs: 'public', url: 'https://tv.example.org/mcp', command: '', ready: true },
		{ id: 'home-assistant', needs: '', url: '', command: '', ready: false },
	],
}, 'guides read, an unknown id kept, nameless entries dropped');
same(model.readGuides(undefined).tunnels, [], 'no answer, no guides');
same(model.publicOf('https://tv.example.org:8443/mcp'), { host: 'tv.example.org', url: 'https://tv.example.org:8443' },
	'the public host and origin out of the connector address');
same(model.publicOf(''), { host: 'YOUR-DOMAIN', url: 'https://YOUR-DOMAIN' }, 'the snippet tokens while no address is set');
same(model.connectUrl({ id: 'claude', needs: 'public', url: '', command: '', ready: false }), 'https://YOUR-DOMAIN/mcp',
	'a public client without an address');
same(model.connectUrl({ id: 'home-assistant', needs: 'token', url: '', command: '', ready: false }), 'http://BOX-ADDRESS/mcp',
	'a home network client without an address');
same(model.connectUrl({ id: 'claude-code', needs: 'token', url: 'http://192.168.1.20:8081/mcp', command: 'x', ready: true }),
	'http://192.168.1.20:8081/mcp', 'the address the box named');

// --------------------------------------------------------------- visibility

const nav = await import('../../data/ni-web/app/nav.js');
const ai = nav.areaById('ai');
same(ai !== null && ai.needs, 'mcp', 'the destination says what it needs');
same(nav.ids.indexOf('ai') === nav.ids.indexOf('dev') - 1, true, 'the area comes before dev');
/** @param {unknown} build */
function shown(build) {
	return nav.visibleAreas(nav.areas, /** @type {any} */ (build)).map(function (a) { return a.id; }).indexOf('ai') !== -1;
}
same(shown({ apiDoc: true, mcp: true }), true, 'offered where the box says yes');
same(shown({ apiDoc: true, mcp: false }), false, 'not offered where the box says no');
same(shown(null), false, 'not offered before the box has said');

// -------------------------------------------------------------------- copy

const copy = await import('../../data/ni-web/ai/copy.js');
/** @param {boolean | 'throw'} works */
function fakeDoc(works) {
	const doc = {
		ran: /** @type {string[]} */ ([]),
		body: { appendChild: function () {}, removeChild: function () {} },
		createElement: function () { return { value: '', setAttribute: function () {}, select: function () { doc.ran.push('select'); }, className: '' }; },
		execCommand: function (/** @type {string} */ name) {
			doc.ran.push(name);
			if (works === 'throw')
				throw new Error('not allowed');
			return works;
		},
	};
	return doc;
}
{
	const written = [];
	const doc = fakeDoc(true);
	const how = await copy.copyText('abc', { secure: true, clipboard: { writeText: function (s) { written.push(s); return Promise.resolve(); } }, doc: /** @type {any} */ (doc) });
	same([how, written, doc.ran], ['copied', ['abc'], []], 'a secure page uses the clipboard');
}
{
	const doc = fakeDoc(true);
	const how = await copy.copyText('abc', { secure: false, clipboard: null, doc: /** @type {any} */ (doc) });
	same([how, doc.ran], ['copied', ['select', 'copy']], 'plain http in the home network still copies');
}
{
	const doc = fakeDoc(false);
	same(await copy.copyText('abc', { secure: false, clipboard: null, doc: /** @type {any} */ (doc) }), 'failed', 'a refused command is said');
}
{
	const doc = fakeDoc('throw');
	same(await copy.copyText('abc', { secure: false, clipboard: null, doc: /** @type {any} */ (doc) }), 'failed', 'a throwing command is said');
}
{
	const doc = fakeDoc(true);
	const how = await copy.copyText('abc', { secure: true, clipboard: { writeText: function () { return Promise.reject(new Error('denied')); } }, doc: /** @type {any} */ (doc) });
	same([how, doc.ran], ['copied', ['select', 'copy']], 'a refused clipboard falls back to the command');
}

// connections list

const clients = await import('../../data/ni-web/ai/clients.js');
same(clients.clientColumnIds(), ['name', 'kind', 'where', 'scopes', 'groups', 'created', 'used', 'act'],
	'the connections list draws its columns in this order');
same(clients.cellTitle('Home-Assistant-Wohnzimmer'), 'Home-Assistant-Wohnzimmer',
	'a truncated cell carries its whole text');
same(clients.cellTitle(''), null, 'an empty cell carries no title');

// groups

same(model.readClients({ clients: [{ id: 'a', name: 'A', kind: 'static', scopes: ['read'], groups: ['programme', 'status'],
	redirect_host: '', loopback_only: false, created: 1, last_used: 0 }] })[0].groups, ['programme', 'status'],
	'a client keeps its groups');
same(model.readClients({ clients: [{ id: 'b', name: 'B', kind: 'static', scopes: ['read'],
	redirect_host: '', loopback_only: false, created: 1, last_used: 0 }] })[0].groups, [],
	'a client from an older box has no groups listed');
same(model.readGroups({ groups: [{ key: 'programme', tools: ['whats_on'], least: 'read', default: true, approx_tokens: 2100 },
	{ key: 'x', tools: 'nope', least: 'root', default: 'yes', approx_tokens: -3 }] }),
	[{ key: 'programme', tools: ['whats_on'], least: 'read', isDefault: true, approxTokens: 2100 },
	 { key: 'x', tools: [], least: 'system', isDefault: false, approxTokens: 0 }],
	'groups are read as plain values and a malformed one as the strictest');
same(model.readGroups('nonsense'), [], 'no groups out of something that is not an answer');
{
	const told = model.readGroups({ groups: ['programme', 'status', 'weather'].map(function (key) {
		return { key: key, tools: [], least: 'read', default: false, approx_tokens: 0 }; }) });
	same(model.groupString(['status', 'programme', 'status'], told), 'programme status', 'groups are sent once each in the box\'s order');
	same(model.groupString(['weather', 'programme'], told), 'programme weather', 'a group the box names later is kept, in its place');
	same(model.groupString(['gone', 'status'], told), 'status', 'a group the box no longer names is dropped');
}
same(clients.groupAllowed('settings', ['read', 'write'], model.readGroups({ groups: [
	{ key: 'settings', tools: [], least: 'system', default: false, approx_tokens: 0 }] })), false,
	'a group whose tools need system is off for a token without it');
same(clients.groupAllowed('programme', ['read'], model.readGroups({ groups: [
	{ key: 'programme', tools: [], least: 'read', default: true, approx_tokens: 0 }] })), true,
	'a read group is open to a read token');
same(clients.createBody('Home Assistant', ['read'], [], ['programme']), { name: 'Home Assistant', scopes: 'read' },
	'a create body carries no groups while the groups route has not answered');
same(clients.createBody('Home Assistant', ['read'], model.readGroups({ groups: [
	{ key: 'programme', tools: [], least: 'read', default: true, approx_tokens: 0 }] }), []),
	{ name: 'Home Assistant', scopes: 'read', groups: '' },
	'a create body says none once the groups route answered and nothing is ticked');

// the group tool count, singular right in both languages
i18n.setLanguage('de');
same(clients.costWords(1, 500), '1 Werkzeug, ca. 500 Tokens', 'one tool is singular in German');
same(clients.costWords(2, 500), '2 Werkzeuge, ca. 500 Tokens', 'two tools are plural in German');
same(clients.costWords(0, 0), '0 Werkzeuge, ca. 0 Tokens', 'no tool is plural in German');
i18n.setLanguage('en');
same(clients.costWords(1, 500), '1 tool, about 500 tokens', 'one tool is singular in English');
same(clients.costWords(2, 500), '2 tools, about 500 tokens', 'two tools are plural in English');
same(clients.costWords(0, 0), '0 tools, about 0 tokens', 'no tool is plural in English');
i18n.setLanguage('de');

// allowlists

same(model.readAllowlists({ plugins: [{ name: 'Tierpark', allowed: true }, { name: 7 }],
	sections: [{ id: 'audio', allowed: false, denied: '' }, { id: 'network', allowed: true, denied: 'network' }] }),
	{ plugins: [{ name: 'Tierpark', allowed: true }],
	  sections: [{ id: 'audio', allowed: false, denied: '' }, { id: 'network', allowed: false, denied: 'network' }] },
	'a denied section is never shown allowed and a nameless plugin is left out');
same(model.allowBody(['Wetter', 'Tierpark'], ['video', 'audio']), { plugins: 'Wetter,Tierpark', sections: 'video,audio' },
	'the lists go back as names separated by commas');

// what the screens put on screen

const runtime = await import('../../data/ni-web/app/runtime.js');

/**
 * A drawn tree with every component in it drawn as well, each on hooks of its own
 * so it cannot disturb the screen's.
 *
 * @param {unknown} node
 * @returns {unknown}
 */
function expand(node) {
	if (Array.isArray(node))
		return node.map(expand);
	if (!node || typeof node !== 'object' || !('type' in node))
		return node;
	const v = /** @type {{ type: unknown, props: Record<string, unknown>, children: unknown[] }} */ (node);
	if (typeof v.type !== 'function')
		return { type: v.type, props: v.props, children: expand(v.children) };
	if (v.type === runtime.Fragment)
		return expand(v.children);
	const kept = { slots: hooks.slots, at: hooks.at, reading: hooks.reading };
	hooks.slots = [];
	hooks.at = 0;
	hooks.reading = true;
	try {
		const c = v.children.length === 1 ? v.children[0] : v.children;
		return { type: 'component', props: v.props, children: expand(/** @type {Function} */ (v.type)(Object.assign({}, v.props, { children: c }))) };
	} catch (e) {
		failed++;
		process.stderr.write('ai: ' + (/** @type {Function} */ (v.type).name || 'a component') + ' threw ' + String(e) + '\n');
		return { type: 'component', props: v.props, children: [] };
	} finally {
		Object.assign(hooks, kept);
	}
}

/**
 * Everything a drawn tree would put into the document, attributes and the values
 * handed to components included.
 *
 * @param {unknown} node
 * @returns {string}
 */
function words(node) {
	if (node === null || node === undefined || typeof node === 'boolean' || typeof node === 'function')
		return '';
	if (typeof node === 'string' || typeof node === 'number')
		return String(node);
	if (Array.isArray(node))
		return node.map(words).join(' ');
	const o = /** @type {Record<string, unknown>} */ (node);
	return Object.keys(o).filter(function (k) { return k !== 'type'; }).map(function (k) { return words(o[k]); }).join(' ');
}

/**
 * The values one attribute has on the elements of a drawn tree, in document order.
 *
 * @param {unknown} node
 * @param {string} name
 * @returns {unknown[]}
 */
function attrs(node, name) {
	/** @type {unknown[]} */
	const out = [];
	(function walk(/** @type {unknown} */ n) {
		if (Array.isArray(n)) {
			n.forEach(walk);
			return;
		}
		if (!n || typeof n !== 'object' || !('type' in n))
			return;
		const v = /** @type {{ type: unknown, props: Record<string, unknown>, children: unknown }} */ (n);
		if (v.type !== 'component' && v.props && Object.prototype.hasOwnProperty.call(v.props, name))
			out.push(v.props[name]);
		walk(v.children);
	})(node);
	return out;
}

/**
 * The elements of a drawn tree whose attributes match.
 *
 * @param {unknown} node
 * @param {(props: Record<string, unknown>) => boolean} match
 * @returns {Array<{ props: Record<string, unknown> }>}
 */
function nodes(node, match) {
	/** @type {Array<{ props: Record<string, unknown> }>} */
	const out = [];
	(function walk(/** @type {unknown} */ n) {
		if (Array.isArray(n)) {
			n.forEach(walk);
			return;
		}
		if (!n || typeof n !== 'object' || !('type' in n))
			return;
		const v = /** @type {{ type: unknown, props: Record<string, unknown>, children: unknown }} */ (n);
		if (v.type !== 'component' && v.props && match(v.props))
			out.push(v);
		walk(v.children);
	})(node);
	return out;
}

/**
 * The text of every h3 in a drawn tree, in document order: what a screen
 * reads as its own part headings, in the order it draws them.
 *
 * @param {unknown} node
 * @returns {string[]}
 */
function headings(node) {
	/** @type {string[]} */
	const out = [];
	(function walk(/** @type {unknown} */ n) {
		if (Array.isArray(n)) {
			n.forEach(walk);
			return;
		}
		if (!n || typeof n !== 'object' || !('type' in n))
			return;
		const v = /** @type {{ type: unknown, children: unknown }} */ (n);
		if (v.type === 'h3')
			out.push(words(v.children));
		walk(v.children);
	})(node);
	return out;
}

/** @type {string[]} */
const asked = [];
/** @type {Record<string, [number, unknown]>} */
const answers = {};
globalThis.fetch = /** @type {any} */ (async function (/** @type {string} */ url, /** @type {RequestInit | undefined} */ init) {
	const path = String(url).split('?')[0];
	asked.push(((init && init.method) || 'GET') + ' ' + path);
	const a = answers[path] || [404, { type: '/errors/not-found', title: 'Not Found', status: 404, detail: 'no such route' }];
	return new Response(JSON.stringify(a[1]), { status: a[0],
		headers: { 'content-type': a[0] < 300 ? 'application/json' : 'application/problem+json' } });
});
async function settle() {
	for (let i = 0; i < 5; ++i)
		await new Promise(function (done) { setTimeout(done, 0); });
}
function unmount() {
	for (const slot of hooks.slots)
		if (slot && typeof slot.undo === 'function')
			slot.undo();
	hooks.slots = [];
}
/** @param {() => unknown} screen */
function draw(screen) {
	hooks.at = 0;
	return expand(screen());
}

const session = await import('../../data/ni-web/app/session.js');
const store = await import('../../data/ni-web/app/store.js');
const access = await import('../../data/ni-web/ai/access.js');
const aiText = (await import('../../data/ni-web/ai/ai.text.js')).default;
/** @param {string} key */
function say(key) { return i18n.t(aiText, key); }
const kRaw = 'this endpoint is not open to this caller';
const settingsDoc = { enabled: true, public_url: '', trusted_proxies: '', allow_lan: true, mcp_url: '',
	tunnel_paths: ['/mcp'], port: 8081, default_password: true };
i18n.setLanguage('de');

answers['/api/v1/session'] = [200, { authenticated: false, level: 'read', user: '', csrf: '', csrf_header: 'X-CSRF-Token', expires_in: 0 }];
await session.refresh();
{
	unmount();
	const before = asked.length;
	draw(clients.default);
	await settle();
	const tree = draw(clients.default);
	same(asked.slice(before).filter(function (one) { return one.indexOf('/api/v1/ai/') !== -1; }), [],
		'without a System session the connections screen asks the box nothing');
	same(words(tree).indexOf(say('ai.clients.signin')) !== -1 && read.worded(say('ai.clients.signin')), true,
		'without a System session the connections screen says why to sign in');
	unmount();
}

answers['/api/v1/session'] = [200, { authenticated: true, level: 'system', user: 'root', csrf: 'c', csrf_header: 'X-CSRF-Token', expires_in: 0 }];
await session.refresh();
answers['/api/v1/ai/clients'] = [403, { type: '/errors/not-permitted', title: 'Forbidden', status: 403, detail: kRaw }];
answers['/api/v1/ai/groups'] = [200, { groups: [] }];
answers['/api/v1/ai/settings'] = [200, settingsDoc];
{
	unmount();
	const before = asked.length;
	draw(clients.default);
	await settle();
	const text = words(draw(clients.default));
	same(asked.slice(before).indexOf('GET /api/v1/ai/clients') !== -1, true, 'with a System session the list is asked for');
	same([text.indexOf(kRaw), text.indexOf('Forbidden')], [-1, -1], 'a refusal the box wrote never reaches the connections screen');
	same(text.indexOf(say('ai.refused.forbidden')) !== -1 && read.worded(say('ai.refused.forbidden')), true,
		'the refused list is said in this area\'s own words');
	unmount();
}

answers['/api/v1/ai/clients'] = [200, { clients: [
	{ id: 's1', name: 'HA', kind: 'static', scopes: ['read'], redirect_host: '', loopback_only: false, created: 1, last_used: 0 },
	{ id: 'r1', name: 'Claude', kind: 'registered', scopes: ['read'], redirect_host: 'claude.ai', loopback_only: false, created: 1, last_used: 0 },
] }];
await store.reload('GET', '/api/v1/ai/clients').catch(function () { });

/**
 * The connections and the access screen drawn over one set of settings.
 *
 * @param {Record<string, unknown>} change
 * @returns {Promise<{ clients: unknown, access: unknown }>}
 */
async function drawnWith(change) {
	store.put('GET', '/api/v1/ai/settings', null, /** @type {any} */ (Object.assign({}, settingsDoc, change)));
	unmount();
	draw(clients.default);
	await settle();
	const c = draw(clients.default);
	unmount();
	draw(access.default);
	await settle();
	draw(access.default);
	const a = draw(access.default);
	unmount();
	return { clients: c, access: a };
}

/** @param {unknown} tree @returns {string[]} */
function reasons(tree) {
	return /** @type {string[]} */ (attrs(tree, 'data-reason'));
}

{
	const d = await drawnWith({ default_password: true, public_url: '', mcp_url: '' });
	same([attrs(d.clients, 'data-remote'), attrs(d.clients, 'aria-disabled'), reasons(d.clients)],
		[['shut'], ['true'], ['password', 'address']],
		'the remote part is greyed out with both reasons while both hold');
	same(words(d.clients).split(say('ai.remote.strong')).length - 1, 1, 'the remote part says once why the password must be strong');
	same(reasons(d.access), ['password', 'address'], 'the access screen gives the same reasons');
	same(words(d.access).indexOf(say('ai.remote.strong')) !== -1, true, 'the access screen says why the password must be strong');
}
{
	const d = await drawnWith({ default_password: true, public_url: 'https://tv.example.org', mcp_url: 'https://tv.example.org/mcp',
		trusted_proxies: '192.168.1.5/32' });
	same([attrs(d.clients, 'data-remote'), attrs(d.clients, 'aria-disabled'), reasons(d.clients)], [['shut'], ['true'], ['password']],
		'the shipped password alone greys the remote part out');
	same(words(d.clients).indexOf(say('ai.why.password')) !== -1 && read.worded(say('ai.why.password')), true,
		'and says to change it under System, Webserver');
	const connect = nodes(d.clients, function (p) { return p['data-part'] === 'remote-connect'; })[0];
	same([words(nodes(connect, function (p) { return p.class === 'mono'; })).indexOf('https://tv.example.org/mcp') !== -1,
		nodes(connect, function (p) { return 'data-copy' in p; }).map(function (n) { return n.props.disabled; })], [true, [true]],
		'a shut remote part still names its address, with Copy disabled');
	const remote = nodes(d.clients, function (p) { return p['data-part'] === 'remote'; })[0];
	same(nodes(remote, function (p) { return 'data-revoke' in p || 'data-groups-edit' in p; })
		.map(function (n) { return !!n.props.disabled; }), [false, false],
		'revoking and narrowing an internet client still work while the part is shut');
}
{
	const d = await drawnWith({ default_password: false, public_url: '', mcp_url: '' });
	same([attrs(d.clients, 'data-remote'), attrs(d.clients, 'aria-disabled'), reasons(d.clients)], [['shut'], ['true'], ['address']],
		'no public address alone greys the remote part out');
	same(words(d.clients).indexOf(say('ai.why.address')) !== -1 && read.worded(say('ai.why.address')), true,
		'and points to the access screen');
	same(attrs(d.clients, 'href').indexOf('/ai/access') !== -1, true, 'with a link to it');
}
{
	const d = await drawnWith({ default_password: false, public_url: 'https://tv.example.org', mcp_url: 'https://tv.example.org/mcp',
		trusted_proxies: '192.168.1.5/32' });
	same([attrs(d.clients, 'data-remote'), attrs(d.clients, 'aria-disabled'), reasons(d.clients)], [['open'], ['false'], []],
		'with an own password and a public address the remote part is open');
	same(words(d.clients).indexOf('https://tv.example.org/mcp') !== -1, true, 'and names the connector address');
	same(reasons(d.access), [], 'and the access screen gives no reason either');
	same(headings(d.access), [say('ai.part.lan'), say('ai.part.remote')],
		'the access screen offers Heimnetz before Remote / Internet, the order the connections screen lists them in');
}
{
	store.put('GET', '/api/v1/ai/settings', null, /** @type {any} */ (Object.assign({}, settingsDoc, { default_password: false })));
	unmount();
	draw(access.default);
	await settle();
	draw(access.default);
	const before = draw(access.default);
	same(nodes(before, function (p) { return p['data-part'] === 'remote-unsaved'; }).length, 0, 'a saved form says nothing about saving');
	/** @param {string} id @param {string} value */
	function type(id, value) {
		const field = nodes(draw(access.default), function (p) { return p.id === id; })[0];
		/** @type {any} */ (field.props).onInput({ currentTarget: { value: value } });
	}
	type('ai-public-url', 'https://tv.example.org');
	type('ai-proxies', '192.168.1.5/32');
	const typed = draw(access.default);
	same([attrs(typed, 'data-say'), reasons(typed), attrs(typed, 'data-reach')], [['shut'], ['address'], attrs(before, 'data-reach')],
		'what is typed and not saved leaves the remote state and the reach as the box holds them');
	/** @param {unknown} tree @returns {unknown[]} */
	function portWarning(tree) { return nodes(tree, function (p) { return p['data-part'] === 'public-port'; }); }
	same(portWarning(typed).length, 0, 'an address without a port is not warned about');
	type('ai-public-url', 'https://tv.example.org:443');
	same(portWarning(draw(access.default)).length, 0, 'nor one on port 443');
	type('ai-public-url', 'https://tv.example.org:8443');
	const warned = portWarning(draw(access.default));
	same(warned.length === 1 && words(warned).indexOf(say('ai.public.port')) !== -1 && read.worded(say('ai.public.port'))
		&& say('ai.public.port').indexOf('443') !== -1, true, 'an address on another port is warned about, naming 443');
	i18n.setLanguage('en');
	same(read.worded(say('ai.public.port')) && say('ai.public.port').indexOf('443') !== -1, true, 'the port warning in English too');
	i18n.setLanguage('de');
	type('ai-public-url', 'https://tv.example.org');
	const unsaved = nodes(typed, function (p) { return p['data-part'] === 'remote-unsaved'; });
	same(unsaved.length === 1 && words(unsaved).indexOf(say('ai.remote.unsaved')) !== -1 && read.worded(say('ai.remote.unsaved')), true,
		'and says the change holds only once saved');
	unmount();
}

i18n.setLanguage('en');
same(say('ai.part.remote'), 'Remote / Internet', 'the internet part has one name in both languages');
i18n.setLanguage('de');

const remoteReasons = model.remoteReasons;
same(remoteReasons(model.readSettings(Object.assign({}, settingsDoc, { enabled: false }))), ['off', 'password', 'address'],
	'switched off is a reason of its own, beside the others');
same(remoteReasons(model.readSettings(Object.assign({}, settingsDoc, { default_password: false, public_url: 'https://tv.example.org' }))),
	['proxy'], 'a public address without a trusted proxy is not recognised as a tunnel');

answers['/api/v1/ai/settings'] = [403, { type: '/errors/not-permitted', title: 'Forbidden', status: 403, detail: kRaw }];
await store.reload('GET', '/api/v1/ai/settings').catch(function () { });
{
	unmount();
	draw(access.default);
	await settle();
	const text = words(draw(access.default));
	same([text.indexOf(kRaw), text.indexOf(say('ai.refused.forbidden')) !== -1], [-1, true],
		'the access screen says a refusal in its own words, never the box\'s');
	unmount();
}

// guides screen

const guidesScreen = await import('../../data/ni-web/ai/guides.js');
const kLan = 'http://192.168.1.20:8081/mcp';
const desktopConfig = model.desktopConfig;
{
	const one = JSON.parse(desktopConfig(kLan));
	same(one, { mcpServers: { neutrino: { command: 'npx', args: ['-y', 'mcp-remote', kLan, '--allow-http', '--header', 'Authorization:${AUTH_HEADER}'],
		env: { AUTH_HEADER: 'Bearer YOUR-TOKEN' } } } },
		'Claude Desktop bridges to the home network address with the token in the environment, where a space survives');
	same(JSON.parse(desktopConfig('https://tv.example.org/mcp')).mcpServers.neutrino.args.indexOf('--allow-http'), -1,
		'plain http is allowed only where the address needs it');
}
answers['/api/v1/ai/settings'] = [200, Object.assign({}, settingsDoc, { default_password: false })];
await store.reload('GET', '/api/v1/ai/settings').catch(function () { });
answers['/api/v1/ai/guides'] = [200, { mcp_url: 'https://tv.example.org/mcp', lan_mcp_url: kLan, paths: ['/mcp', '/oauth/'],
	tunnels: ['cloudflare', 'tailscale', 'caddy', 'nginx', 'dyndns', 'frp'].map(function (id) {
		return { id: id, file: 'f', snippet: 'snippet of ' + id, warnings: id === 'dyndns' ? ['device', 'dyndns-fixed-address', 'dyndns-no-direct'] : ['device'], ready: true };
	}),
	clients: [
		{ id: 'claude', needs: 'public', url: 'https://tv.example.org/mcp', command: '', ready: true },
		{ id: 'claude-code', needs: 'token', url: kLan, command: 'claude mcp add --transport http neutrino ' + kLan, ready: true },
		{ id: 'chatgpt', needs: 'public', url: 'https://tv.example.org/mcp', command: '', ready: true },
		{ id: 'home-assistant', needs: 'token', url: kLan, command: '', ready: true },
	] }];
/** @returns {Promise<unknown>} */
async function drawnGuides() {
	unmount();
	draw(guidesScreen.default);
	await settle();
	draw(guidesScreen.default);
	const tree = draw(guidesScreen.default);
	unmount();
	return tree;
}
/** @param {unknown} tree @param {string} part @returns {unknown} */
function partOf(tree, part) {
	return nodes(tree, function (p) { return p['data-part'] === part; })[0] || null;
}
{
	const tree = await drawnGuides();
	same(attrs(tree, 'data-part').filter(function (p) { return /^guides-/.test(String(p)); }), ['guides-lan', 'guides-remote', 'guides-after'],
		'the guides are the home network, then on the go, then what is possible afterwards');
	same(attrs(partOf(tree, 'guides-lan'), 'data-client'), ['claude-desktop', 'claude-code', 'home-assistant'],
		'the home network part has the token clients, Claude Desktop first');
	same(attrs(partOf(tree, 'guides-remote'), 'data-client'), ['claude', 'chatgpt'], 'on the go has the clients that sign in');
	const port = nodes(partOf(tree, 'guides-remote'), function (p) { return p['data-part'] === 'prereq-port'; });
	same(port.length === 1 && words(port).indexOf(say('ai.guides.prereq.port')) !== -1 && read.worded(say('ai.guides.prereq.port'))
		&& /443/.test(say('ai.guides.prereq.port')) && /Claude/.test(say('ai.guides.prereq.port')) && /ChatGPT/.test(say('ai.guides.prereq.port')), true,
		'the prerequisites say the public address must be on port 443, because Claude and ChatGPT only connect there');
	same(attrs(tree, 'data-way'), ['tunnel', 'forward', 'other'], 'on the go has two named ways, and an unknown guide apart');
	same(['tunnel', 'forward', 'other'].map(function (w) { return attrs(nodes(tree, function (p) { return p['data-way'] === w; })[0], 'data-tunnel'); }),
		[['cloudflare', 'tailscale'], ['dyndns', 'caddy', 'nginx'], ['frp']],
		'the tunnels without the router, the fixed name and both proxies with the port forward');
	same(attrs(tree, 'data-tunnel').length, 6, 'no guide appears twice');
	same(attrs(nodes(tree, function (p) { return p['data-way'] === 'forward'; })[0], 'data-step'), ['name', 'forward', 'proxy'],
		'the own address is three steps: a name, the port forward, the proxy');
	same(attrs(tree, 'data-snippet').indexOf('tunnel-dyndns'), -1, 'the fixed name step carries no second Caddyfile');
	const forward = nodes(tree, function (p) { return p['data-step'] === 'forward'; })[0];
	same(attrs(forward, 'data-warning'), ['dyndns-fixed-address', 'dyndns-no-direct'], 'the port forward step carries its two warnings');
	same(words(partOf(tree, 'guides-remote')).indexOf('remote-state') !== -1 && attrs(partOf(tree, 'guides-remote'), 'href').indexOf('/ai/access') !== -1,
		true, 'on the go starts with its prerequisites and leads to the access screen');
	const desktop = nodes(tree, function (p) { return p['data-client'] === 'claude-desktop'; })[0];
	same(words(desktop).indexOf(desktopConfig(kLan)) !== -1 && words(desktop).indexOf('claude_desktop_config.json') !== -1, true,
		'Claude Desktop is handed its file filled with the box address');
	same(attrs(desktop, 'href'), ['/ai/clients'], 'and is sent for a token');
	const after = partOf(tree, 'guides-after');
	same(attrs(after, 'data-example').length, 8, 'eight examples of what to ask');
	same(attrs(after, 'data-groups').map(function (g) { return String(g); }),
		[['programme'], ['programme', 'timers'], ['control'], ['recordings'], ['status'],
			['programme', 'timers'], ['programme', 'control'], ['plugins']].map(function (g) { return g.join(' '); }),
		'each example names the tool groups it needs');
	same(attrs(after, 'data-level').map(function (l) { return String(l); }),
		['read', 'write', 'write', 'write', 'read', 'write', 'write', 'system'],
		'each example names the permission level it needs');
	same(words(after).indexOf(say('ai.scope.read')) !== -1 && words(after).indexOf(say('ai.scope.write')) !== -1
		&& words(after).indexOf(say('ai.scope.system')) !== -1, true,
		'the examples cover all three permission levels, in the words the Connections tab uses for them');
	same(words(after).indexOf(say('ai.group.timers')) !== -1 && words(after).indexOf('Home Assistant') !== -1, true,
		'the groups in the words the groups screen uses, and speaking to the box as well');
	same(attrs(tree, 'open').filter(function (o) { return o !== false; }), [], 'every guide is drawn shut until it is opened');
	const caddy = nodes(tree, function (p) { return p['data-tunnel'] === 'caddy'; })[0];
	if (typeof caddy.props.onToggle === 'function')
		/** @type {(e: unknown) => void} */ (caddy.props.onToggle)({ currentTarget: { open: true } });
	const again = await drawnGuides();
	same(nodes(again, function (p) { return p.open === true; }).map(function (n) { return n.props['data-tunnel'] || n.props['data-client']; }),
		['caddy'], 'a guide the viewer opened is open when the guides are drawn anew, and no other');
	if (typeof caddy.props.onToggle === 'function')
		/** @type {(e: unknown) => void} */ (caddy.props.onToggle)({ currentTarget: { open: false } });
	same(/ai\.(guides|tunnel|client|warning|group)\./.test(words(tree)), false, 'every key has words');
	i18n.setLanguage('en');
	const en = await drawnGuides();
	same([/ai\.(guides|tunnel|client|warning|group)\./.test(words(en)), words(en) !== words(tree)], [false, true], 'the guides in English too');
	i18n.setLanguage('de');
}

// ------------------------------------------------------------------ verdict

const FLOOR = 163;
if (checked < FLOOR) {
	process.stderr.write('ai-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('ai-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-ai.sh: ' + checked + ' assertions over what the AI area reads, sends and offers\n');
