import { html, useState, useEffect } from '../app/runtime.js';
import * as store from '../app/store.js';
import { t } from '../app/i18n.js';
import { State } from '../app/ui/state.js';
import { Button } from '../app/ui/button.js';
import { useSystemSession, useWatch, SignInRow, refusalShown, refusalText } from './parts.js';
import { readAllowlists, allowBody } from './model.js';
import { codeOf } from './read.js';
import text from './ai.text.js';

export const css = '/ai/ai.css';
/** @returns {string} */
export function lead() { return t(text, 'ai.allow.lead'); }

/**
 * @param {readonly string[]} list
 * @param {string} item
 * @param {boolean} on
 * @returns {string[]}
 */
function toggled(list, item, on) {
	if (on)
		return list.indexOf(item) !== -1 ? list.slice() : list.concat([item]);
	return list.filter(function (one) { return one !== item; });
}

/**
 * @typedef {object} Ticks
 * @property {string[]} plugins
 * @property {string[]} sections
 */

/**
 * @param {import('./model.js').AiAllowlists} lists
 * @returns {Ticks}
 */
function ticksOf(lists) {
	return {
		plugins: lists.plugins.filter(function (p) { return p.allowed; }).map(function (p) { return p.name; }),
		sections: lists.sections.filter(function (s) { return s.allowed; }).map(function (s) { return s.id; }),
	};
}

/** @returns {Web.Drawn} */
export default function Allow() {
	const may = useSystemSession();
	const shot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/allowlists', null, fn);
	});
	const data = shot ? shot.data : null;
	const lists = data ? readAllowlists(data) : { plugins: /** @type {import('./model.js').AiAllowPlugin[]} */ ([]), sections: /** @type {import('./model.js').AiAllowSection[]} */ ([]) };
	const [ticks, setTicks] = useState(/** @type {Ticks | null} */ (null));
	const [busy, setBusy] = useState(false);
	const [said, setSaid] = useState('');
	const plugins = ticks ? ticks.plugins : [];
	const sections = ticks ? ticks.sections : [];

	// Seeded once, from the first answer. An answer that arrives afterwards
	// for a reason that has nothing to do with this screen's own save (an
	// event, a stream reconnect, a reload out of the page cache) must not
	// take back a tick nobody has saved yet; only a successful save of this
	// screen's own does that, in save() below.
	useEffect(function () {
		if (ticks === null && data)
			setTicks(ticksOf(readAllowlists(data)));
	}, [data, ticks]);

	/**
	 * @param {string[]} next
	 */
	function setPlugins(next) {
		setTicks(function (prev) { return { plugins: next, sections: prev ? prev.sections : [] }; });
	}

	/**
	 * @param {string[]} next
	 */
	function setSections(next) {
		setTicks(function (prev) { return { plugins: prev ? prev.plugins : [], sections: next }; });
	}

	function save() {
		setSaid('');
		setBusy(true);
		store.write('PUT', '/api/v1/ai/allowlists', { body: allowBody(plugins, sections), touches: ['/api/v1/ai/allowlists'] }).then(function (answer) {
			setBusy(false);
			// The answer is the saved lists, read directly rather than through
			// the watched entry: what is on screen matches what the box just
			// confirmed, whatever else happens to that entry meanwhile.
			setTicks(ticksOf(readAllowlists(answer)));
			setSaid(t(text, 'ai.allow.saved'));
		}, function (caught) {
			setBusy(false);
			setSaid(codeOf(caught) === 'settings-section-denied' ? t(text, 'ai.allow.denied') : refusalText(caught));
		});
	}

	return html`<div class="ai">
		<${SignInRow} shown=${!may} why=${t(text, 'ai.allow.signin')} />
		${may ? html`<section class="ai-block ai-allow-plugins">
			<h2>${t(text, 'ai.allow.plugins')}</h2>
			<p class="ai-warn">${t(text, 'ai.allow.plugins.warn')}</p>
			<${State} phase=${shot === null ? '' : shot.phase} problem=${refusalShown(shot !== null ? shot.error : null)}>
				${lists.plugins.length === 0 ? html`<p class="hint">${t(text, 'ai.allow.none')}</p>` : html`<div class="ai-list ai-allow">
					${lists.plugins.map(function (p) {
						return html`<label class="ai-allow-row" data-plugin=${p.name} key=${p.name}>
							<input type="checkbox" checked=${plugins.indexOf(p.name) !== -1}
								onChange=${function (/** @type {Event} */ e) { setPlugins(toggled(plugins, p.name, /** @type {HTMLInputElement} */ (e.currentTarget).checked)); }} />
							<span class="ai-wrap" title=${p.name}>${p.name}</span>
							<small>${t(text, 'ai.allow.plugin')}</small>
						</label>`;
					})}
				</div>`}
			<//>
		</section>
		<section class="ai-block ai-allow-sections">
			<h2>${t(text, 'ai.allow.sections')}</h2>
			<p class="note" data-part="never">${t(text, 'ai.allow.never')}</p>
			<div class="ai-list ai-allow">
				${lists.sections.map(function (s) {
					return html`<label class="ai-allow-row" data-section=${s.id} key=${s.id}>
						<input type="checkbox" disabled=${s.denied !== ''} checked=${s.denied === '' && sections.indexOf(s.id) !== -1}
							onChange=${function (/** @type {Event} */ e) { setSections(toggled(sections, s.id, /** @type {HTMLInputElement} */ (e.currentTarget).checked)); }} />
						<span>${s.id}</span>
						<small>${s.denied === '' ? t(text, 'ai.allow.section') : t(text, 'ai.allow.denied.' + s.denied)}</small>
					</label>`;
				})}
			</div>
		</section>
		<div class="ai-row" data-act="allow-save"><${Button} primary=${true} disabled=${busy || ticks === null} onClick=${save}>${t(text, 'ai.allow.save')}<//></div>
		${said === '' ? null : html`<p class="ai-said" role="status" data-part="said">${said}</p>`}` : null}
	</div>`;
}
