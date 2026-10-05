// The words the KI tab shows beside the snippets the box fills, in both languages.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const top = join(here, '..', '..');
const web = join(top, 'data', 'ni-web');

// setLanguage writes the document's language, and node has no document.
globalThis.document = { documentElement: { lang: 'de' } };

const i18n = await import(pathToFileURL(join(web, 'app', 'i18n.js')).href);
const words = await import(pathToFileURL(join(web, 'ai', 'guidewords.js')).href);

let compared = 0;
function holds(value, what) { assert.ok(value, what); compared += 1; }

// Read off the server's source so the two cannot drift.
const server = readFileSync(join(top, 'src', 'httpd', 'mcp', 'aiguides.cpp'), 'utf8');
const tunnelIds = ['cloudflare', 'tailscale', 'caddy', 'nginx', 'dyndns'];
const clientIds = ['claude', 'claude-code', 'chatgpt', 'home-assistant'];
// Drawn by the page from the home network address, so the server never names it.
const pageClientIds = ['claude-desktop'];
// A step of the own-address way, not a guide of its own: no shared tunnel steps.
const nameOnly = ['dyndns'];
const warningIds = ['device', 'tailscale-name', 'tailscale-port', 'nginx-acme', 'cloudflare-ratelimit',
	'dyndns-fixed-address', 'dyndns-no-direct'];
for (const id of tunnelIds.concat(warningIds))
	holds(server.includes('"' + id + '"'), 'the server still names ' + id);
holds(server.includes('{ "claude", "claude-code", "chatgpt", "home-assistant" }'), 'the server names the four clients in this order');
holds(server.includes('c.needs = token[i] ? "token" : "public";'), 'the server answers needs as token or public');

const values = { host: 'tv.example.org', url: 'https://tv.example.org' };
const codes = ['ai-public-url-refused', 'ai-trusted-proxies-refused', 'ai-caller-would-be-tunnel', 'ai-default-password', 'forwarded-by-untrusted-peer'];

// Per language, how many steps each tunnel and each client gets, so the two
// languages can be held to each other below: a step dropped in one language
// only still clears the ">= 4" floor but is caught by the comparison.
/** @typedef {Record<string, Record<string, number>>} StepCounts */
/** @type {{ tunnel: StepCounts, client: StepCounts }} */
const stepCounts = { tunnel: { de: {}, en: {} }, client: { de: {}, en: {} } };

/** @param {string} lang */
function walk(lang) {
	i18n.setLanguage(lang);
	for (const id of tunnelIds.filter(function (id) { return nameOnly.indexOf(id) === -1; })) {
		const steps = words.tunnelSteps(id, values);
		stepCounts.tunnel[lang][id] = steps.length;
		holds(steps.length >= 4, lang + ' ' + id + ' has its steps');
		holds(steps.every(function (s) { return s.length > 0 && !s.startsWith('ai.') && !/\{[a-z]+\}/.test(s); }), lang + ' ' + id + ' steps are words with every value filled');
		holds(!words.tunnelTitle(id).startsWith('ai.'), lang + ' ' + id + ' has a title');
		holds(!words.fileText(id).startsWith('ai.'), lang + ' ' + id + ' says where the snippet goes');
	}
	holds(words.tunnelSteps('cloudflare', values).some(function (s) { return s.includes('tv.example.org'); }), lang + ' cloudflare names the host');
	holds(words.tunnelSteps('nginx', values).some(function (s) { return s.includes('https://tv.example.org/mcp'); }), lang + ' the check step names the public mcp address');
	holds(words.tunnelSteps('nginx', values).some(function (s) { return s.includes('/oauth/register') && s.includes('/oauth/authorize'); }), lang + ' nginx says what the limit covers');
	const nameSteps = words.nameSteps(values);
	stepCounts.tunnel[lang].dyndns = nameSteps.length;
	holds(nameSteps.length >= 3, lang + ' the fixed name has its steps');
	holds(nameSteps.every(function (s) { return s.length > 0 && !s.startsWith('ai.') && !/\{[a-z]+\}/.test(s); }), lang + ' the fixed name steps are words with every value filled');
	holds(nameSteps.some(function (s) { return s.includes('https://tv.example.org') && !s.includes('/mcp'); }), lang + ' dyndns names the public address to enter');
	holds(!nameSteps.some(function (s) { return s.includes('/mcp'); }), lang + ' the fixed name is not a proxy guide with its checks');
	holds(!words.tunnelSteps('caddy', values).concat(words.tunnelSteps('nginx', values)).some(function (s) { return /\b(80|443)\b/.test(s) && /Router|router/.test(s); }),
		lang + ' the proxies leave the router to the step before them');
	for (const id of pageClientIds) {
		const steps = words.clientSteps(id, { url: 'http://192.168.1.20/mcp' });
		stepCounts.client[lang][id] = steps.length;
		holds(steps.length >= 4, lang + ' ' + id + ' has its steps');
		holds(!words.clientTitle(id).startsWith('ai.'), lang + ' ' + id + ' has a title');
	}
	const desktop = words.clientSteps('claude-desktop', { url: 'x' }).join('\n');
	holds(['Node.js', 'claude_desktop_config.json', '~/Library/Application Support/Claude/', '%APPDATA%\\Claude\\', 'YOUR-TOKEN', '--allow-http', 'Developer']
		.every(function (k) { return desktop.includes(k); }), lang + ' claude desktop names node, the file in both places, the token and the http switch');
	const remote = words.clientSteps('claude', { url: 'https://tv.example.org/mcp' }).join('\n');
	holds(remote.includes('Customize > Connectors') && remote.includes('Anthropic'), lang + ' claude remote names the menu and where it connects from');
	holds(words.clientTitle('claude').includes('Claude Desktop'), lang + ' the remote claude guide is for Claude Desktop as well');
	const chatgpt = words.clientSteps('chatgpt', { url: 'https://tv.example.org/mcp' }).join('\n');
	holds(chatgpt.includes('https://chatgpt.com/plugins') && chatgpt.includes('OAuth') && chatgpt.includes('https://tv.example.org/mcp')
		&& !/Entwicklermodus|developer mode/i.test(chatgpt), lang + ' chatgpt starts at its plugins page, signs in with OAuth and needs no developer mode');
	holds(lang !== 'de' || (chatgpt.includes('„Hinzufügen“') && chatgpt.includes('„Benutzerdefinierten MCP-Server erstellen“')),
		lang + ' chatgpt names its buttons as ChatGPT shows them');
	holds(words.warningText('cloudflare-ratelimit').includes('/oauth/register'), lang + ' the cloudflare hint names the paths');
	for (const id of clientIds) {
		const steps = words.clientSteps(id, { url: 'https://tv.example.org/mcp' });
		stepCounts.client[lang][id] = steps.length;
		holds(steps.length >= 2, lang + ' ' + id + ' has its steps');
		holds(steps.every(function (s) { return s.length > 0 && !s.startsWith('ai.') && !/\{[a-z]+\}/.test(s); }), lang + ' ' + id + ' steps are words with every value filled');
		holds(!words.clientTitle(id).startsWith('ai.'), lang + ' ' + id + ' has a title');
	}
	holds(words.clientSteps('claude', { url: 'https://tv.example.org/mcp' }).some(function (s) { return s.includes('https://tv.example.org/mcp'); }), lang + ' claude is given the address');
	holds(words.clientSteps('home-assistant', { url: 'http://192.168.1.20/mcp' }).some(function (s) { return s.includes('http://192.168.1.20/mcp'); }), lang + ' home assistant is given the address');
	holds(words.clientSteps('claude-code', { url: 'x' }).some(function (s) { return s.includes('YOUR-TOKEN'); }), lang + ' claude code is told to replace the token');
	for (const id of warningIds)
		holds(!words.warningText(id).startsWith('ai.'), lang + ' warning ' + id + ' has words');
	for (const needs of ['public', 'token'])
		for (const ready of [true, false])
			holds(!words.noteText(needs, ready).startsWith('ai.'), lang + ' note ' + needs + ' ' + ready);
	for (const needs of ['public', 'token'])
		holds(words.noteText(needs, true) !== words.noteText(needs, false), lang + ' a waiting client reads differently from a ready one');
	holds(words.noteText('public', true) !== words.noteText('token', true), lang + ' the two ways in read differently');
	holds(['YOUR-DOMAIN', 'BOX-ADDRESS', 'TUNNEL-ID', 'YOUR-TOKEN'].every(function (k) { return words.tokenText().includes(k); }), lang + ' the tokens are explained');
	for (const code of codes)
		holds(!words.errorText(code).startsWith('ai.'), lang + ' ' + code + ' has words');
	return words.tunnelSteps('nginx', values).join('\n');
}

const de = walk('de');
const en = walk('en');
holds(de !== en, 'the steps follow the language');

for (const id of tunnelIds)
	holds(stepCounts.tunnel.de[id] === stepCounts.tunnel.en[id], 'tunnel ' + id + ' has the same number of steps in both languages');
for (const id of clientIds.concat(pageClientIds))
	holds(stepCounts.client.de[id] === stepCounts.client.en[id], 'client ' + id + ' has the same number of steps in both languages');

holds(words.tunnelSteps('nobody', values).length === 0, 'an unknown tunnel has no steps');
holds(words.clientSteps('nobody', values).length === 0, 'an unknown client has no steps');
holds(words.tunnelTitle('nobody') === 'ai.tunnel.nobody.title', 'an unknown id answers its key');
i18n.setLanguage('de');

console.log('aiguides-cases.mjs: ' + compared + ' comparisons');
assert.ok(compared >= 162, 'fewer comparisons than the cases above make: ' + compared);
