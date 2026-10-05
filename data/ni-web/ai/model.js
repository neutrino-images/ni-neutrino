// What the AI routes answer, as values the screens draw, and what a screen sends back.

import { obj, str, num, strings } from './read.js';
import { canonical } from './scopes.js';

/**
 * @typedef {object} AiSettings
 * @property {boolean} enabled
 * @property {string} publicUrl
 * @property {string} trustedProxies comma separated, as the box writes it
 * @property {boolean} allowLan
 * @property {string} mcpUrl
 * @property {string[]} tunnelPaths
 * @property {number} port
 * @property {boolean} defaultPassword the login is the shipped one, so the tunnel is shut
 */

/**
 * @typedef {object} AiSaved
 * @property {AiSettings} settings
 * @property {boolean} restarting
 */

/**
 * @typedef {object} AiDraft
 * @property {boolean} enabled
 * @property {string} publicUrl
 * @property {string} proxies
 * @property {boolean} allowLan
 */

/**
 * @typedef {object} AiSettingsBody
 * @property {boolean} [enabled]
 * @property {string} [public_url]
 * @property {string} [trusted_proxies]
 * @property {boolean} [allow_lan]
 */

/**
 * @typedef {object} AiClient
 * @property {string} id
 * @property {string} name
 * @property {'registered' | 'metadata' | 'static'} kind
 * @property {string[]} scopes
 * @property {string} redirectHost
 * @property {boolean} loopbackOnly
 * @property {number} created
 * @property {number} lastUsed 0 for never
 * @property {string[]} groups
 */

/**
 * @typedef {object} AiGroup
 * @property {string} key
 * @property {string[]} tools
 * @property {'read' | 'write' | 'system'} least
 * @property {boolean} isDefault
 * @property {number} approxTokens
 */

/**
 * @typedef {object} AiCreated
 * @property {AiClient} client
 * @property {string} token shown once
 */

/**
 * @typedef {object} AiTunnel
 * @property {string} id
 * @property {string} file
 * @property {string} snippet
 * @property {string[]} warnings
 * @property {boolean} ready
 */

/**
 * @typedef {object} AiConnect
 * @property {string} id
 * @property {'public' | 'token' | ''} needs
 * @property {string} url
 * @property {string} command
 * @property {boolean} ready
 */

/**
 * @typedef {object} AiGuides
 * @property {string} mcpUrl
 * @property {string} lanMcpUrl
 * @property {string[]} paths
 * @property {AiTunnel[]} tunnels
 * @property {AiConnect[]} clients
 */

const kNameMax = 64;

/**
 * @param {unknown} answer
 * @returns {AiSettings | null}
 */
export function readSettings(answer) {
	const a = obj(answer);
	if (!a)
		return null;
	return {
		enabled: a.enabled === true,
		publicUrl: str(a.public_url),
		trustedProxies: str(a.trusted_proxies),
		// Missing reads as the delivered state, which lets the home network in.
		allowLan: a.allow_lan !== false,
		mcpUrl: str(a.mcp_url),
		tunnelPaths: strings(a.tunnel_paths),
		port: num(a.port),
		defaultPassword: a.default_password === true,
	};
}

/**
 * @param {unknown} answer
 * @returns {AiSaved | null}
 */
export function readSaved(answer) {
	const a = obj(answer);
	const settings = a ? readSettings(obj(a.ai)) : null;
	if (!a || !settings)
		return null;
	return { settings: settings, restarting: a.restarting === true };
}

/**
 * @param {AiSettings} s
 * @returns {AiDraft}
 */
export function draftOf(s) {
	return { enabled: s.enabled, publicUrl: s.publicUrl, proxies: s.trustedProxies, allowLan: s.allowLan };
}

/**
 * @param {string} text
 * @returns {string[]}
 */
export function parseProxies(text) {
	return text.split(/[\s,]+/).filter(function (one) { return one !== ''; });
}

/**
 * @param {string} value
 * @returns {string}
 */
function trimUrl(value) {
	return value.trim().replace(/\/+$/, '');
}

/**
 * Only what differs: restating everything would write back a value the box changed meanwhile.
 *
 * @param {AiDraft} draft
 * @param {AiSettings} now
 * @returns {AiSettingsBody}
 */
export function settingsBody(draft, now) {
	/** @type {AiSettingsBody} */
	const body = {};
	if (draft.enabled !== now.enabled)
		body.enabled = draft.enabled;
	if (trimUrl(draft.publicUrl) !== trimUrl(now.publicUrl))
		body.public_url = trimUrl(draft.publicUrl);
	const proxies = parseProxies(draft.proxies).join(',');
	if (proxies !== parseProxies(now.trustedProxies).join(','))
		body.trusted_proxies = proxies;
	if (draft.allowLan !== now.allowLan)
		body.allow_lan = draft.allowLan;
	return body;
}

/**
 * @param {string} publicUrl
 * @returns {string}
 */
export function mcpUrlOf(publicUrl) {
	const base = trimUrl(publicUrl);
	return base === '' ? '' : base + '/mcp';
}

/**
 * Said before the box is asked; the box still decides.
 *
 * @param {string} value
 * @returns {string} a catalogue key, or empty
 */
export function publicUrlProblem(value) {
	const v = value.trim();
	if (v === '')
		return '';
	if (!/^https:\/\//.test(v))
		return 'ai.public.bad.scheme';
	if (!/^https:\/\/[^/?#@\s]+\/?$/.test(v))
		return 'ai.public.bad.path';
	return '';
}

/**
 * Claude's and ChatGPT's servers connect to 443 only.
 *
 * @param {string} value
 * @returns {boolean}
 */
export function publicUrlOffPort(value) {
	const m = /^https:\/\/[^/?#@\s]*?:([0-9]+)\/?$/.exec(value.trim());
	return !!m && m[1] !== '443';
}

/**
 * @param {AiDraft} d
 * @param {boolean} [shut] the box keeps the tunnel shut whatever the address says
 * @returns {'off' | 'lan' | 'tunnel' | 'both' | 'none' | 'noproxy' | 'lan-noproxy'}
 */
export function reachOf(d, shut) {
	if (!d.enabled)
		return 'off';
	const outside = !shut && trimUrl(d.publicUrl) !== '';
	// The box takes a request for the tunnel only when it comes from a listed proxy.
	if (outside && parseProxies(d.proxies).length === 0)
		return d.allowLan ? 'lan-noproxy' : 'noproxy';
	if (outside)
		return d.allowLan ? 'both' : 'tunnel';
	return d.allowLan ? 'lan' : 'none';
}

/**
 * Why a client cannot reach the box from the internet, in the order they are told; none when it can.
 *
 * @param {AiSettings} s
 * @returns {Array<'off' | 'password' | 'address' | 'proxy'>}
 */
export function remoteReasons(s) {
	/** @type {Array<'off' | 'password' | 'address' | 'proxy'>} */
	const out = [];
	if (!s.enabled)
		out.push('off');
	if (s.defaultPassword)
		out.push('password');
	if (trimUrl(s.publicUrl) === '')
		out.push('address');
	else if (parseProxies(s.trustedProxies).length === 0)
		out.push('proxy');
	return out;
}

/**
 * Why a client in the home network cannot reach the box; none when it can.
 *
 * @param {AiSettings} s
 * @returns {Array<'off' | 'lan'>}
 */
export function lanReasons(s) {
	if (!s.enabled)
		return ['off'];
	return s.allowLan ? [] : ['lan'];
}

/**
 * @param {Record<string, unknown>} row
 * @returns {AiClient}
 */
function clientOf(row) {
	const id = str(row.id);
	return {
		id: id,
		name: str(row.name) || id,
		kind: (row.kind === 'static' || row.kind === 'metadata') ? row.kind : 'registered',
		scopes: canonical(strings(row.scopes)),
		redirectHost: str(row.redirect_host),
		loopbackOnly: row.loopback_only === true,
		created: num(row.created),
		lastUsed: num(row.last_used),
		groups: strings(row.groups),
	};
}

/**
 * @param {unknown} answer
 * @returns {AiGroup[]}
 */
export function readGroups(answer) {
	const top = obj(answer);
	const list = top && Array.isArray(top.groups) ? top.groups : [];
	return list.map(function (raw) {
		const one = obj(raw) || {};
		const least = str(one.least);
		const tokens = num(one.approx_tokens);
		return {
			key: str(one.key),
			tools: strings(one.tools),
			least: least === 'read' || least === 'write' ? least : 'system',
			isDefault: one.default === true,
			approxTokens: tokens > 0 ? Math.floor(tokens) : 0,
		};
	});
}

/**
 * @param {readonly string[]} keys
 * @param {AiGroup[]} groups the groups route's answer, whose order the box keeps
 * @returns {string}
 */
export function groupString(keys, groups) {
	return groups.filter(function (g) { return keys.indexOf(g.key) !== -1; })
		.map(function (g) { return g.key; }).join(' ');
}

/**
 * Keeps no token, whatever the answer carries: the rows are drawn.
 *
 * @param {unknown} answer
 * @returns {AiClient[]}
 */
export function readClients(answer) {
	const a = obj(answer);
	const rows = a && Array.isArray(a.clients) ? a.clients : [];
	/** @type {AiClient[]} */
	const out = [];
	for (const one of rows) {
		const row = obj(one);
		if (row && str(row.id) !== '')
			out.push(clientOf(row));
	}
	return out;
}

/**
 * @param {unknown} answer
 * @returns {AiCreated | null}
 */
export function readCreated(answer) {
	const row = obj(answer);
	if (!row || str(row.id) === '' || str(row.token) === '')
		return null;
	return { client: clientOf(row), token: str(row.token) };
}

/**
 * @param {string} name
 * @returns {string} a catalogue key, or empty
 */
export function tokenNameProblem(name) {
	const v = name.trim();
	if (v === '')
		return 'ai.token.name.empty';
	if (/[\u0000-\u001f\u007f]/.test(v))
		return 'ai.token.name.bad';
	if (new TextEncoder().encode(v).length > kNameMax)
		return 'ai.token.name.long';
	return '';
}

/**
 * @param {unknown} list
 * @returns {AiTunnel[]}
 */
function tunnelsOf(list) {
	/** @type {AiTunnel[]} */
	const out = [];
	for (const one of Array.isArray(list) ? list : []) {
		const g = obj(one);
		if (!g || str(g.id) === '')
			continue;
		out.push({ id: str(g.id), file: str(g.file), snippet: str(g.snippet), warnings: strings(g.warnings), ready: g.ready === true });
	}
	return out;
}

/**
 * @param {unknown} list
 * @returns {AiConnect[]}
 */
function connectsOf(list) {
	/** @type {AiConnect[]} */
	const out = [];
	for (const one of Array.isArray(list) ? list : []) {
		const c = obj(one);
		if (!c || str(c.id) === '')
			continue;
		const needs = (c.needs === 'public' || c.needs === 'token') ? c.needs : '';
		out.push({ id: str(c.id), needs: needs, url: str(c.url), command: str(c.command), ready: c.ready === true });
	}
	return out;
}

/**
 * @param {unknown} answer
 * @returns {AiGuides}
 */
export function readGuides(answer) {
	const a = obj(answer) || {};
	return {
		mcpUrl: str(a.mcp_url),
		lanMcpUrl: str(a.lan_mcp_url),
		paths: strings(a.paths),
		tunnels: tunnelsOf(a.tunnels),
		clients: connectsOf(a.clients),
	};
}

/**
 * The values the tunnel steps are filled with; the snippets' own tokens while no address is set.
 *
 * @param {string} mcpUrl
 * @returns {{ host: string, url: string }}
 */
export function publicOf(mcpUrl) {
	const url = mcpUrl.replace(/\/mcp$/, '');
	if (url === '')
		return { host: 'YOUR-DOMAIN', url: 'https://YOUR-DOMAIN' };
	return { host: url.replace(/^https:\/\//, '').replace(/:[0-9]+$/, ''), url: url };
}

/**
 * @param {AiConnect} c
 * @returns {string}
 */
export function connectUrl(c) {
	if (c.url !== '')
		return c.url;
	return c.needs === 'token' ? 'http://BOX-ADDRESS/mcp' : 'https://YOUR-DOMAIN/mcp';
}

/**
 * Claude Desktop's config entry: a local bridge to the box. The token goes through the
 * environment because Claude Desktop on Windows splits arguments at spaces.
 *
 * @param {string} url
 * @returns {string}
 */
export function desktopConfig(url) {
	const args = ['-y', 'mcp-remote', url];
	if (/^http:/.test(url))
		args.push('--allow-http');
	args.push('--header', 'Authorization:${AUTH_HEADER}');
	return JSON.stringify({ mcpServers: { neutrino: { command: 'npx', args: args, env: { AUTH_HEADER: 'Bearer YOUR-TOKEN' } } } }, null, 2) + '\n';
}

/**
 * Claude Desktop in the home network, drawn from the address and state of Claude Code's guide.
 *
 * @param {AiGuides} g
 * @returns {AiConnect}
 */
export function desktopClient(g) {
	const code = g.clients.filter(function (c) { return c.id === 'claude-code'; })[0];
	const url = code ? connectUrl(code) : (g.lanMcpUrl || 'http://BOX-ADDRESS/mcp');
	return { id: 'claude-desktop', needs: 'token', url: url, command: desktopConfig(url), ready: code ? code.ready : false };
}

/**
 * @typedef {object} AiAllowPlugin
 * @property {string} name
 * @property {boolean} allowed
 */

/**
 * @typedef {object} AiAllowSection
 * @property {string} id
 * @property {boolean} allowed
 * @property {string} denied "" when the section may be allowed
 */

/**
 * @typedef {object} AiAllowlists
 * @property {AiAllowPlugin[]} plugins
 * @property {AiAllowSection[]} sections
 */

/**
 * @param {unknown} answer
 * @returns {AiAllowlists}
 */
export function readAllowlists(answer) {
	const a = obj(answer);
	/** @type {AiAllowPlugin[]} */
	const plugins = [];
	for (const one of (a && Array.isArray(a.plugins)) ? a.plugins : []) {
		const row = obj(one);
		if (!row)
			continue;
		const name = str(row.name);
		if (name !== '')
			plugins.push({ name: name, allowed: row.allowed === true });
	}
	/** @type {AiAllowSection[]} */
	const sections = [];
	for (const one of (a && Array.isArray(a.sections)) ? a.sections : []) {
		const row = obj(one);
		if (!row)
			continue;
		const id = str(row.id);
		if (id === '')
			continue;
		const denied = str(row.denied);
		sections.push({ id: id, allowed: denied === '' && row.allowed === true, denied: denied });
	}
	return { plugins: plugins, sections: sections };
}

/**
 * @param {readonly string[]} plugins
 * @param {readonly string[]} sections
 * @returns {{ plugins: string, sections: string }}
 */
export function allowBody(plugins, sections) {
	return { plugins: plugins.join(','), sections: sections.join(',') };
}
