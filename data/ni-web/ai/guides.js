import { html } from '../app/runtime.js';
import * as store from '../app/store.js';
import { t } from '../app/i18n.js';
import { State } from '../app/ui/state.js';
import { fold } from '../app/ui/kept.js';
import { useSystemSession, useWatch, SignInRow, refusalShown, RemoteState, LanState } from './parts.js';
import { CopyButton, Snippet } from './copy.js';
import { readGuides, readSettings, publicOf, connectUrl, desktopClient } from './model.js';
import { worded } from './read.js';
import { tunnelSteps, nameSteps, clientSteps, tunnelTitle, clientTitle, clientFile, fileText, warningText, noteText, tokenText } from './guidewords.js';
import text from './ai.text.js';

export const css = '/ai/ai.css';
/** @returns {string} */
export function lead() { return t(text, 'ai.guides.lead'); }

/**
 * An id the words do not know is drawn under what the box said, never as a key.
 *
 * @param {string} said
 * @param {string} fallback
 * @returns {string}
 */
function or(said, fallback) {
	return worded(said) ? said : fallback;
}

/** @param {{ steps: string[] }} props @returns {Web.Drawn} */
function Steps(props) {
	return props.steps.length ? html`<ol class="ai-steps">${props.steps.map(function (s, i) { return html`<li key=${i}>${s}</li>`; })}</ol>` : null;
}

/** @param {{ ids: string[] }} props @returns {Web.Drawn} */
function Warnings(props) {
	return props.ids.length ? html`${props.ids.map(function (w) { return html`<p class="ai-warn" key=${w} data-warning=${w}>${or(warningText(w), w)}</p>`; })}` : null;
}

/**
 * @param {{ tunnel: import('./model.js').AiTunnel, values: { host: string, url: string } }} props
 * @returns {Web.Drawn}
 */
function Tunnel(props) {
	const g = props.tunnel;
	return html`<details class="ai-guide" data-tunnel=${g.id} ...${fold('ai.guides:tunnel:' + g.id)}>
		<summary>${or(tunnelTitle(g.id), g.id)}</summary>
		${g.ready ? null : html`<p class="ai-warn" data-part="tokens">${or(tokenText(), '')}</p>`}
		<${Steps} steps=${tunnelSteps(g.id, props.values)} />
		<${Warnings} ids=${g.warnings} />
		${g.snippet === '' ? null : html`<${Snippet} id=${'tunnel-' + g.id} label=${or(fileText(g.id), g.file || g.id)} value=${g.snippet} />`}
	</details>`;
}

/**
 * DynDNS as the fixed name of the own-address way; its warnings belong to the port forward.
 *
 * @param {{ values: { host: string, url: string } }} props
 * @returns {Web.Drawn}
 */
function FixedName(props) {
	return html`<details class="ai-guide" data-tunnel="dyndns" ...${fold('ai.guides:tunnel:dyndns')}>
		<summary>${tunnelTitle('dyndns')}</summary>
		<${Steps} steps=${nameSteps(props.values)} />
	</details>`;
}

/**
 * @param {{ client: import('./model.js').AiConnect }} props
 * @returns {Web.Drawn}
 */
function Connect(props) {
	const c = props.client;
	const url = connectUrl(c);
	return html`<details class="ai-guide" data-client=${c.id} ...${fold('ai.guides:client:' + c.id)}>
		<summary>${or(clientTitle(c.id), c.id)}</summary>
		${c.needs === '' ? null : html`<p class="hint" data-part="note">${or(noteText(c.needs, c.ready), '')}</p>`}
		<p class="ai-row" data-part="url">${t(text, 'ai.guides.address')} <code class="mono" data-url=${c.id}>${url}</code> <${CopyButton} value=${url} /></p>
		<${Steps} steps=${clientSteps(c.id, { url: url })} />
		${c.command === '' ? null : html`<${Snippet} id=${'client-' + c.id} label=${or(clientFile(c.id), t(text, 'ai.guides.command'))} value=${c.command} />`}
		${c.needs === 'token' ? html`<p class="ai-row" data-part="token"><a href="/ai/clients">${t(text, 'ai.guides.totoken')}</a></p>` : null}
	</details>`;
}

/** The ids each way draws, in its order; any other id the box sends is drawn apart. */
const kTunnelWay = ['cloudflare', 'tailscale'];
const kProxies = ['caddy', 'nginx'];
const kKnown = kTunnelWay.concat(kProxies, ['dyndns']);
const kLanFirst = ['claude-code', 'home-assistant'];
const kRemoteFirst = ['claude', 'chatgpt'];

/**
 * What to ask once connected, with the tool groups and the permission level
 * each request needs: the highest level among the tools it calls, in the
 * words the Connections tab itself uses for a scope (`ai.scope.*`).
 */
const kExamples = [
	{ key: 'now', groups: ['programme'], level: 'read' },
	{ key: 'record', groups: ['programme', 'timers'], level: 'write' },
	{ key: 'switch', groups: ['control'], level: 'write' },
	{ key: 'play', groups: ['recordings'], level: 'write' },
	{ key: 'space', groups: ['status'], level: 'read' },
	{ key: 'search', groups: ['programme', 'timers'], level: 'write' },
	{ key: 'wake', groups: ['programme', 'control'], level: 'write' },
	{ key: 'plugin', groups: ['plugins'], level: 'system' },
];

/**
 * @template {{ id: string }} T
 * @param {T[]} list
 * @param {string[]} ids
 * @returns {T[]}
 */
function inOrder(list, ids) {
	const named = ids.map(function (id) { return list.filter(function (one) { return one.id === id; })[0]; })
		.filter(function (one) { return !!one; });
	return named.concat(list.filter(function (one) { return ids.indexOf(one.id) === -1; }));
}

/** @param {import('./model.js').AiTunnel[]} list @param {string[]} ids @returns {import('./model.js').AiTunnel[]} */
function only(list, ids) {
	return inOrder(list, ids).filter(function (one) { return ids.indexOf(one.id) !== -1; });
}

/** @returns {Web.Drawn} */
function After() {
	return html`<section class="ai-part" data-part="guides-after" aria-labelledby="ai-guides-after">
		<div class="ai-block">
			<h2 id="ai-guides-after">${t(text, 'ai.guides.after')}</h2>
			<p class="hint">${t(text, 'ai.guides.after.lead')}</p>
			<ul class="ai-examples">${kExamples.map(function (ex) {
				const names = ex.groups.map(function (k) { return t(text, 'ai.group.' + k); }).join(', ');
				const level = t(text, 'ai.scope.' + ex.level);
				return html`<li key=${ex.key} data-example=${ex.key}>${t(text, 'ai.guides.ex.' + ex.key)}
					${' '}<small data-groups=${ex.groups.join(' ')} data-level=${ex.level}>${t(text, 'ai.guides.ex.needs', { groups: names, level: level })}</small></li>`;
			})}</ul>
			<p data-part="voice">${t(text, 'ai.guides.voice')}</p>
		</div>
	</section>`;
}

/** @returns {Web.Drawn} */
export default function Guides() {
	const may = useSystemSession();
	const shot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/guides', null, fn);
	});
	const settingsShot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/settings', null, fn);
	});
	const g = (shot && shot.data !== null) ? readGuides(shot.data) : null;
	const settings = (settingsShot && settingsShot.data !== null) ? readSettings(settingsShot.data) : null;
	const values = publicOf(g ? g.mcpUrl : '');
	const tunnels = g ? g.tunnels : [];
	const dyndns = tunnels.filter(function (one) { return one.id === 'dyndns'; })[0];
	const others = tunnels.filter(function (one) { return kKnown.indexOf(one.id) === -1; });
	const lan = g ? [desktopClient(g)].concat(inOrder(g.clients.filter(function (c) { return c.needs === 'token' && c.id !== 'claude-desktop'; }), kLanFirst)) : [];
	const remote = g ? inOrder(g.clients.filter(function (c) { return c.needs !== 'token'; }), kRemoteFirst) : [];

	return html`<div class="ai">
		<${SignInRow} shown=${!may} why=${t(text, 'ai.guides.signin')} />
		<${State}
			phase=${shot === null ? '' : shot.phase}
			problem=${refusalShown(shot !== null ? shot.error : null)}>
			${g ? html`<section class="ai-part" data-part="guides-lan" aria-labelledby="ai-guides-lan">
				<div class="ai-block">
					<h2 id="ai-guides-lan">${t(text, 'ai.guides.lan')}</h2>
					<p class="hint">${t(text, 'ai.guides.lan.lead')}</p>
					${settings ? html`<${LanState} settings=${settings} here="/ai/guides" />` : null}
					${lan.map(function (one) { return html`<${Connect} key=${one.id} client=${one} />`; })}
				</div>
			</section>
			<section class="ai-part" data-part="guides-remote" aria-labelledby="ai-guides-remote">
				<div class="ai-block">
					<h2 id="ai-guides-remote">${t(text, 'ai.guides.remote')}</h2>
					<p class="hint">${t(text, 'ai.guides.remote.lead')}</p>
					<h3>${t(text, 'ai.guides.prereq')}</h3>
					<p>${t(text, 'ai.guides.prereq.lead')}</p>
					<p data-part="prereq-port">${t(text, 'ai.guides.prereq.port')}</p>
					${settings ? html`<${RemoteState} settings=${settings} here="/ai/guides" />` : null}
					<p class="ai-row" data-part="prereq"><a href="/ai/access">${t(text, 'ai.guides.prereq.access')}</a></p>
					<p class="ai-row" data-part="paths">${t(text, 'ai.guides.paths')} <code class="mono">${g.paths.join(', ')}</code></p>
				</div>
				<div class="ai-block" data-way="tunnel">
					<h3>${t(text, 'ai.guides.way.tunnel')}</h3>
					<p class="hint">${t(text, 'ai.guides.way.tunnel.lead')}</p>
					${only(tunnels, kTunnelWay).map(function (one) { return html`<${Tunnel} key=${one.id} tunnel=${one} values=${values} />`; })}
				</div>
				<div class="ai-block" data-way="forward">
					<h3>${t(text, 'ai.guides.way.forward')}</h3>
					<p class="hint">${t(text, 'ai.guides.way.forward.lead')}</p>
					<ol class="ai-ways">
						<li data-step="name">
							<strong>${t(text, 'ai.guides.step.name')}</strong>
							<p>${t(text, 'ai.guides.step.name.lead', values)}</p>
							${dyndns ? html`<${FixedName} values=${values} />` : null}
						</li>
						<li data-step="forward">
							<strong>${t(text, 'ai.guides.step.forward')}</strong>
							<p>${t(text, 'ai.guides.step.forward.lead')}</p>
							<${Warnings} ids=${dyndns ? dyndns.warnings.filter(function (w) { return w !== 'device'; }) : []} />
						</li>
						<li data-step="proxy">
							<strong>${t(text, 'ai.guides.step.proxy')}</strong>
							<p>${t(text, 'ai.guides.step.proxy.lead')}</p>
							${only(tunnels, kProxies).map(function (one) { return html`<${Tunnel} key=${one.id} tunnel=${one} values=${values} />`; })}
						</li>
					</ol>
				</div>
				${others.length ? html`<div class="ai-block" data-way="other">
					<h3>${t(text, 'ai.guides.other')}</h3>
					${others.map(function (one) { return html`<${Tunnel} key=${one.id} tunnel=${one} values=${values} />`; })}
				</div>` : null}
				<div class="ai-block" data-part="remote-clients">
					<h3>${t(text, 'ai.guides.connect')}</h3>
					<p class="hint">${t(text, 'ai.guides.connect.lead')}</p>
					${remote.map(function (one) { return html`<${Connect} key=${one.id} client=${one} />`; })}
				</div>
			</section>
			<${After} />` : null}
		<//>
	</div>`;
}
