import { html, Fragment, useState, useEffect, useRef } from '../app/runtime.js';
import * as store from '../app/store.js';
import { t } from '../app/i18n.js';
import { dayAndClock } from '../app/fmt.js';
import { State } from '../app/ui/state.js';
import { Field } from '../app/ui/field.js';
import { Button } from '../app/ui/button.js';
import { Dialog } from '../app/ui/dialog.js';
import { Table } from '../app/ui/table.js';
import { toast } from '../app/ui/toast.js';
import { useSystemSession, useWatch, SignInRow, refusalShown, refusalText, RemoteState, LanState } from './parts.js';
import { Snippet, CopyButton } from './copy.js';
import { readClients, readCreated, readGroups, readSettings, groupString, tokenNameProblem, remoteReasons } from './model.js';
import { LEVELS, toggle, scopeString, hasLevel } from './scopes.js';
import { codeOf } from './read.js';
import text from './ai.text.js';

export const css = '/ai/ai.css';
/** @returns {string} */
export function lead() { return t(text, 'ai.clients.lead'); }

/**
 * @param {readonly string[]} list
 * @returns {string}
 */
function scopeWords(list) {
	return list.map(function (one) { return t(text, 'ai.scope.' + one); }).join(', ');
}

/** @returns {string[]} the column ids, in the order the list draws them */
export function clientColumnIds() {
	return ['name', 'kind', 'where', 'scopes', 'groups', 'created', 'used', 'act'];
}

/**
 * @param {string} value
 * @returns {string | null}
 */
export function cellTitle(value) {
	return value === '' ? null : value;
}

/**
 * @param {string} key
 * @param {readonly string[]} scopes
 * @param {import('./model.js').AiGroup[]} groups
 * @returns {boolean}
 */
export function groupAllowed(key, scopes, groups) {
	const g = groups.find(function (one) { return one.key === key; });
	if (!g)
		return false;
	if (g.least === 'system')
		return scopes.indexOf('system') !== -1;
	if (g.least === 'write')
		return scopes.indexOf('write') !== -1 || scopes.indexOf('system') !== -1;
	return true;
}

/**
 * The body for POST /api/v1/ai/clients. `groups` is left out while the
 * groups list has not loaded, so the box falls back to its own defaults
 * instead of reading an empty choice as "none".
 *
 * @param {string} name
 * @param {readonly string[]} scopes
 * @param {import('./model.js').AiGroup[]} groups
 * @param {readonly string[]} tokenGroups
 * @returns {{ name: string, scopes: string, groups?: string }}
 */
export function createBody(name, scopes, groups, tokenGroups) {
	return groups.length
		? { name: name, scopes: scopeString(scopes, LEVELS), groups: groupString(tokenGroups, groups) }
		: { name: name, scopes: scopeString(scopes, LEVELS) };
}

/**
 * @param {import('./model.js').AiClient} r
 * @returns {string}
 */
function whereOf(r) {
	if (r.kind === 'static')
		return t(text, 'ai.where.token');
	return r.loopbackOnly ? r.redirectHost + ' ' + t(text, 'ai.where.loopback') : r.redirectHost;
}

/**
 * Only the host is cut; the loopback note always shows.
 *
 * @param {import('./model.js').AiClient} r
 * @returns {Web.Drawn}
 */
function whereCell(r) {
	return html`<span data-where=${r.id} title=${cellTitle(whereOf(r))}>${r.kind === 'static'
		? t(text, 'ai.where.token')
		: html`<span class="ai-cut">${r.redirectHost}</span>${r.loopbackOnly ? ' ' + t(text, 'ai.where.loopback') : null}`}</span>`;
}

/**
 * The words for a group's tool count and token estimate, singular right for
 * exactly one tool in both languages.
 *
 * @param {number} toolCount
 * @param {number} approxTokens
 * @returns {string}
 */
export function costWords(toolCount, approxTokens) {
	return t(text, toolCount === 1 ? 'ai.groups.cost.one' : 'ai.groups.cost', { tools: toolCount, tokens: approxTokens });
}

/**
 * One checkbox per group, each with its words, what it costs, and why it
 * cannot be chosen when it cannot.
 *
 * @param {{
 *   groups: import('./model.js').AiGroup[],
 *   chosen: readonly string[],
 *   scopes: readonly string[],
 *   onToggle: (key: string, on: boolean) => void
 * }} props
 * @returns {Web.Drawn}
 */
function GroupChoice(props) {
	return html`<${Fragment}>
		${props.groups.map(function (g) {
			const allowed = groupAllowed(g.key, props.scopes, props.groups);
			const checked = props.chosen.indexOf(g.key) !== -1;
			return html`<label class="ai-group" data-group=${g.key} key=${g.key}>
				<input type="checkbox" checked=${checked} disabled=${!allowed && !checked}
					onChange=${function (/** @type {Event} */ e) { props.onToggle(g.key, /** @type {HTMLInputElement} */ (e.currentTarget).checked); }} />
				<span>${t(text, 'ai.group.' + g.key)}</span>
				<small>${t(text, 'ai.group.' + g.key + '.what')}</small>
				<span data-cost=${g.key}>${costWords(g.tools.length, g.approxTokens)}${allowed ? '' : ' ' + t(text, 'ai.groups.needs')}</span>
			</label>`;
		})}
	<//>`;
}

/** @returns {Web.Drawn} */
export default function Clients() {
	const may = useSystemSession();
	const shot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/clients', null, fn);
	});
	const groupsShot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/groups', null, fn);
	});
	const settingsShot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/settings', null, fn);
	});
	const rows = shot ? readClients(shot.data) : [];
	const lanRows = rows.filter(function (r) { return r.kind === 'static'; });
	const remoteRows = rows.filter(function (r) { return r.kind !== 'static'; });
	const settings = (settingsShot && settingsShot.data !== null) ? readSettings(settingsShot.data) : null;
	const shut = !settings || remoteReasons(settings).length > 0;
	const groups = groupsShot ? readGroups(groupsShot.data) : [];
	const [asking, setAsking] = useState(/** @type {import('./model.js').AiClient | null} */ (null));
	const [editing, setEditing] = useState(/** @type {import('./model.js').AiClient | null} */ (null));
	const [chosenGroups, setChosenGroups] = useState(/** @type {string[]} */ ([]));
	const [name, setName] = useState('');
	const [chosen, setChosen] = useState(/** @type {string[]} */ (['read']));
	const [tokenGroups, setTokenGroups] = useState(/** @type {string[]} */ ([]));
	const groupsInit = useRef(false);
	const [busy, setBusy] = useState(false);
	// The secret lives here and nowhere else, and only until it is put away.
	const [created, setCreated] = useState(/** @type {import('./model.js').AiCreated | null} */ (null));
	const [said, setSaid] = useState('');
	const shown = useRef(/** @type {HTMLElement | null} */ (null));

	useEffect(function () {
		if (!may)
			setCreated(null);
	}, [may]);

	// The defaults are only known once the box answers, and only taken once:
	// a redraw after that must not overwrite what the owner already chose.
	useEffect(function () {
		if (!groupsInit.current && groups.length) {
			setTokenGroups(groups.filter(function (g) { return g.isDefault; }).map(function (g) { return g.key; }));
			groupsInit.current = true;
		}
	}, [groups]);

	// Drawn above the form, so on a narrow screen it would start out of sight.
	useEffect(function () {
		if (created && shown.current)
			shown.current.focus();
	}, [created]);

	function revoke() {
		const one = asking;
		setAsking(null);
		if (!one)
			return;
		store.write('DELETE', '/api/v1/ai/clients/{id}', { params: { id: one.id }, touches: ['/api/v1/ai/clients'] }).then(
			function () { toast(t(text, 'ai.revoked', { name: one.name })); },
			function (caught) { toast(refusalText(caught), 'bad'); });
	}

	/**
	 * @param {import('./model.js').AiClient} r
	 */
	function openGroups(r) {
		setEditing(r);
		setChosenGroups(r.groups.slice());
	}

	function toggleEditGroup(/** @type {string} */ key, /** @type {boolean} */ on) {
		setChosenGroups(function (prev) { return on ? prev.concat([key]) : prev.filter(function (k) { return k !== key; }); });
	}

	function saveGroups() {
		const row = editing;
		const send = chosenGroups;
		setEditing(null);
		// Without the box's groups every key would be dropped and the client left with none.
		if (!row || !groups.length)
			return;
		store.write('PATCH', '/api/v1/ai/clients/{id}', {
			params: { id: row.id },
			body: { groups: groupString(send, groups) },
			touches: ['/api/v1/ai/clients'],
		}).then(
			function () { toast(t(text, 'ai.groups.saved', { name: row.name })); },
			function (caught) { toast(codeOf(caught) === 'unknown-group' ? t(text, 'ai.groups.unknown') : refusalText(caught), 'bad'); });
	}

	function toggleTokenGroup(/** @type {string} */ key, /** @type {boolean} */ on) {
		setTokenGroups(function (prev) { return on ? prev.concat([key]) : prev.filter(function (k) { return k !== key; }); });
	}

	/**
	 * @param {string} scope
	 * @param {boolean} on
	 */
	function toggleScope(scope, on) {
		const next = toggle(chosen, scope, on, LEVELS);
		setChosen(next);
		// A group that needs system is dropped the moment system is, rather
		// than left ticked and refused at the next save.
		if (next.indexOf('system') === -1)
			setTokenGroups(function (prev) { return prev.filter(function (k) {
				return !groups.some(function (g) { return g.key === k && g.least === 'system'; });
			}); });
	}

	function create() {
		setSaid('');
		setBusy(true);
		store.write('POST', '/api/v1/ai/clients', {
			body: createBody(name.trim(), chosen, groups, tokenGroups),
			touches: ['/api/v1/ai/clients'],
		}).then(function (answer) {
			setBusy(false);
			const made = readCreated(answer);
			if (!made) {
				setSaid(t(text, 'ai.failed'));
				return;
			}
			setCreated(made);
			setName('');
			setChosen(['read']);
			setTokenGroups(groups.filter(function (g) { return g.isDefault; }).map(function (g) { return g.key; }));
		}, function (caught) {
			setBusy(false);
			setSaid(codeOf(caught) === 'no-room-for-a-result' ? t(text, 'ai.token.full') : refusalText(caught));
		});
	}

	/** @type {Record<string, import('../app/ui/table.js').Column<import('./model.js').AiClient>>} */
	const byId = {
		name: { id: 'name', label: t(text, 'ai.col.name'), wide: true, cell: function (r) { return html`<span class="ai-cut" title=${cellTitle(r.name)}>${r.name}</span>`; } },
		kind: { id: 'kind', label: t(text, 'ai.col.kind'), cell: function (r) { return t(text, 'ai.kind.' + r.kind); } },
		where: { id: 'where', label: t(text, 'ai.col.where'), cell: whereCell },
		scopes: { id: 'scopes', label: t(text, 'ai.col.scopes'), cell: function (r) { return html`<span data-scopes=${r.id}>${scopeWords(r.scopes)}</span>`; } },
		groups: { id: 'groups', label: t(text, 'ai.col.groups'), cell: function (r) {
			const words = r.groups.map(function (k) { return t(text, 'ai.group.' + k); }).join(', ') || t(text, 'ai.groups.none');
			return html`<span class="ai-cut" data-groups=${r.id} title=${cellTitle(words)}>${words}</span>`;
		} },
		created: { id: 'created', label: t(text, 'ai.col.created'), cell: function (r) { return dayAndClock(r.created); } },
		used: { id: 'used', label: t(text, 'ai.col.used'), cell: function (r) {
			return html`<span data-used=${r.id}>${r.lastUsed === 0 ? t(text, 'ai.never') : dayAndClock(r.lastUsed)}</span>`;
		} },
		act: { id: 'act', label: t(text, 'ai.col.act'), align: 'end', cell: function (r) {
			return html`<${Fragment}>
				<button type="button" class="btn" data-groups-edit=${r.id} onClick=${function () { openGroups(r); }}>${t(text, 'ai.groups.edit')}</button>
				${' '}
				<button type="button" class="btn" data-revoke=${r.id} onClick=${function () { setAsking(r); }}>${t(text, 'ai.revoke')}</button>
			<//>`;
		} },
	};
	const columns = clientColumnIds().map(function (id) { return byId[id]; });

	const nameProblem = name === '' ? '' : tokenNameProblem(name);

	/**
	 * @param {import('./model.js').AiClient[]} list
	 * @returns {Web.Drawn}
	 */
	function listOf(list) {
		return list.length
			? html`<${Table} columns=${columns} rows=${list} rowKey=${function (/** @type {import('./model.js').AiClient} */ r) { return r.id; }} />`
			: null;
	}
	const loaded = shot !== null && shot.data !== null;

	return html`<div class="ai">
		<${SignInRow} shown=${!may} why=${t(text, 'ai.clients.signin')} />
		${may ? html`<section class="ai-part" data-part="lan" aria-labelledby="ai-part-lan">
			<div class="ai-block">
				<h2 id="ai-part-lan">${t(text, 'ai.part.lan')}</h2>
				<p class="hint">${t(text, 'ai.part.lan.lead')}</p>
				${settings ? html`<${LanState} settings=${settings} here="/ai/clients" />` : null}
			</div>
			<div class="ai-block ai-clients ai-list">
				<${State}
					phase=${shot === null ? '' : shot.phase}
					problem=${refusalShown(shot !== null ? shot.error : null)}
					empty=${loaded && lanRows.length === 0 ? t(text, 'ai.lan.none') : false}>
					${listOf(lanRows)}
				<//>
			</div>

			${created ? html`<div class="ai-block ai-token" data-part="token">
				<h3 tabindex="-1" ref=${shown}>${t(text, 'ai.token.shown', { name: created.client.name })}</h3>
				<p class="ai-warn">${t(text, 'ai.token.once')}</p>
				<${Snippet} id="token" label=${t(text, 'ai.token.header')} value=${created.token} codeId="ai-token-value" />
				<p class="ai-row"><button type="button" class="btn primary" data-act="token-done" onClick=${function () { setCreated(null); }}>${t(text, 'ai.token.done')}</button></p>
			</div>` : null}

			<div class="ai-block">
				<h3>${t(text, 'ai.token.title')}</h3>
				<p class="hint">${t(text, 'ai.token.lead')}</p>
				<${Field} id="ai-token-name" label=${t(text, 'ai.token.name')} hint=${t(text, 'ai.token.name.hint')}
					value=${name} autocomplete="off" error=${nameProblem === '' ? '' : t(text, nameProblem)}
					onInput=${function (/** @type {Event} */ e) { setName(/** @type {HTMLInputElement} */ (e.currentTarget).value); }} />
				<fieldset class="ai-scopes"><legend>${t(text, 'ai.token.scopes')}</legend>
					${LEVELS.map(function (scope) {
						return html`<label class="ai-scope" data-scope=${scope} key=${scope}>
							<input type="checkbox" checked=${chosen.indexOf(scope) !== -1}
								onChange=${function (/** @type {Event} */ e) { toggleScope(scope, /** @type {HTMLInputElement} */ (e.currentTarget).checked); }} />
							<span>${t(text, 'ai.scope.' + scope)}</span>
						</label>`;
					})}
				</fieldset>
				<fieldset class="ai-token-groups"><legend>${t(text, 'ai.col.groups')}</legend>
					<${GroupChoice} groups=${groups} chosen=${tokenGroups} scopes=${chosen} onToggle=${toggleTokenGroup} />
				</fieldset>
				<div class="ai-row" data-act="create">
					<${Button} primary=${true} disabled=${busy || tokenNameProblem(name) !== '' || !hasLevel(chosen)} onClick=${create}>${t(text, 'ai.token.create')}<//>
				</div>
				${said === '' ? null : html`<p class="ai-said" role="status" data-part="said">${said}</p>`}
			</div>
		</section>

		<section class="ai-part" data-part="remote" data-remote=${shut ? 'shut' : 'open'} aria-labelledby="ai-part-remote">
			<div class="ai-block">
				<h2 id="ai-part-remote">${t(text, 'ai.part.remote')}</h2>
				<p class="hint">${t(text, 'ai.part.remote.lead')}</p>
				<${State}
					phase=${settingsShot === null ? '' : settingsShot.phase}
					problem=${refusalShown(settingsShot !== null ? settingsShot.error : null)}>
					${settings ? html`<${RemoteState} settings=${settings} here="/ai/clients" />` : null}
				<//>
				<div class="ai-row ai-connect" role="group" data-part="remote-connect"
					aria-label=${t(text, 'ai.part.remote')} aria-disabled=${shut ? 'true' : 'false'}>
					<span>${t(text, 'ai.mcpurl')}</span>
					<code class="mono">${settings && settings.mcpUrl !== '' ? settings.mcpUrl : '–'}</code>
					<${CopyButton} value=${settings ? settings.mcpUrl : ''} disabled=${shut} />
				</div>
			</div>
			${loaded ? html`<div class="ai-block ai-clients ai-list" data-part="remote-list">
				<${State} empty=${remoteRows.length === 0 ? t(text, 'ai.remote.none') : false}>
					${listOf(remoteRows)}
				<//>
			</div>` : null}
		</section>` : null}

		<${Dialog}
			open=${asking !== null}
			title=${t(text, 'ai.revoke')}
			confirmLabel=${t(text, 'ai.revoke')}
			onCancel=${function () { setAsking(null); }}
			onConfirm=${revoke}>
			<p data-part="revoke-ask">${asking ? t(text, 'ai.revoke.ask', { name: asking.name }) : ''}</p>
		<//>

		<${Dialog}
			open=${editing !== null}
			title=${editing ? t(text, 'ai.groups.title', { name: editing.name }) : ''}
			confirmLabel=${t(text, 'ai.groups.save')}
			onCancel=${function () { setEditing(null); }}
			onConfirm=${saveGroups}>
			${editing ? html`<${GroupChoice} groups=${groups} chosen=${chosenGroups} scopes=${editing.scopes} onToggle=${toggleEditGroup} />` : null}
		<//>
	</div>`;
}
