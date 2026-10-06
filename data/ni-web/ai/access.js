import { html, useState, useEffect } from '../app/runtime.js';
import * as store from '../app/store.js';
import { t } from '../app/i18n.js';
import { State } from '../app/ui/state.js';
import { Field } from '../app/ui/field.js';
import { Switch } from '../app/ui/switch.js';
import { Button } from '../app/ui/button.js';
import { Dialog } from '../app/ui/dialog.js';
import { useSystemSession, useWatch, SignInRow, refusalShown, refusalText, RemoteState } from './parts.js';
import { readSettings, readSaved, draftOf, settingsBody, mcpUrlOf, publicUrlProblem, publicUrlOffPort, reachOf } from './model.js';
import { codeOf, worded } from './read.js';
import { errorText } from './guidewords.js';
import { useTotp, TotpPart } from './totp.js';
import text from './ai.text.js';

export const css = '/ai/ai.css';
/** @returns {string} */
export function lead() { return t(text, 'ai.access.lead'); }

/** @returns {Web.Drawn} */
export default function Access() {
	const may = useSystemSession();
	const shot = useWatch(may, function (fn) {
		return store.watch('GET', '/api/v1/ai/settings', null, fn);
	});
	const now = shot ? readSettings(shot.data) : null;
	const [draft, setDraft] = useState(/** @type {import('./model.js').AiDraft | null} */ (null));
	const [asking, setAsking] = useState(false);
	const [busy, setBusy] = useState(false);
	const [said, setSaid] = useState('');
	const [restarting, setRestarting] = useState(false);
	const totp = useTotp();

	// Seeded once, so a reload does not take a field away from somebody typing.
	useEffect(function () {
		if (draft === null && now)
			setDraft(draftOf(now));
	}, [now, draft]);

	const body = (now && draft) ? settingsBody(draft, now) : {};
	const urlProblem = draft ? publicUrlProblem(draft.publicUrl) : '';
	const nothing = Object.keys(body).length === 0;

	/** @param {Partial<import('./model.js').AiDraft>} change */
	function edit(change) {
		if (draft)
			setDraft(Object.assign({}, draft, change));
	}

	function save() {
		setAsking(false);
		setSaid('');
		setBusy(true);
		// The answer is the saved settings: read again, they would meet the restart.
		store.write('PUT', '/api/v1/ai/settings', { body: body, touches: ['/api/v1/ai/guides'] }).then(function (answer) {
			setBusy(false);
			const saved = readSaved(answer);
			if (saved) {
				store.put('GET', '/api/v1/ai/settings', null,
					/** @type {Api.Result<'GET /api/v1/ai/settings'>} */ (/** @type {{ ai: unknown }} */ (answer).ai));
				setDraft(draftOf(saved.settings));
			}
			setRestarting(!!saved && saved.restarting);
			setSaid(t(text, saved && saved.restarting ? 'ai.restarting' : 'ai.saved'));
		}, function (caught) {
			setBusy(false);
			const words = errorText(codeOf(caught));
			setSaid(worded(words) ? words : refusalText(caught));
		});
	}

	return html`<div class="ai">
		<${SignInRow} shown=${!may} why=${t(text, 'ai.access.signin')} />
		<${State}
			phase=${shot === null ? '' : shot.phase}
			problem=${refusalShown(shot !== null ? shot.error : null)}>
			${(now && draft) ? html`<section class="ai-block">
				<p class="ai-reach" data-part="reach" data-reach=${reachOf(draftOf(now), now.defaultPassword)}>${t(text, 'ai.reach.' + reachOf(draftOf(now), now.defaultPassword))}</p>
				<${Switch} id="ai-enabled" label=${t(text, 'ai.enabled')} hint=${t(text, 'ai.enabled.hint')}
					checked=${draft.enabled}
					onChange=${function (/** @type {Event} */ e) { edit({ enabled: /** @type {HTMLInputElement} */ (e.currentTarget).checked }); }} />
				<h3>${t(text, 'ai.part.lan')}</h3>
				<${Switch} id="ai-allow-lan" label=${t(text, 'ai.lan')} hint=${t(text, 'ai.lan.hint')}
					checked=${draft.allowLan}
					onChange=${function (/** @type {Event} */ e) { edit({ allowLan: /** @type {HTMLInputElement} */ (e.currentTarget).checked }); }} />
				<h3>${t(text, 'ai.part.remote')}</h3>
				<${Field} id="ai-public-url" label=${t(text, 'ai.public')} hint=${t(text, 'ai.public.hint')}
					value=${draft.publicUrl} autocomplete="off"
					error=${urlProblem === '' ? '' : t(text, urlProblem)}
					onInput=${function (/** @type {Event} */ e) { edit({ publicUrl: /** @type {HTMLInputElement} */ (e.currentTarget).value }); }} />
				${urlProblem === '' && publicUrlOffPort(draft.publicUrl) ? html`<p class="ai-warn" data-part="public-port">${t(text, 'ai.public.port')}</p>` : null}
				<p class="ai-row" data-part="mcp-url">${mcpUrlOf(draft.publicUrl) === ''
					? t(text, 'ai.mcpurl.none')
					: html`${t(text, 'ai.mcpurl')} <code class="mono">${mcpUrlOf(draft.publicUrl)}</code>`}</p>
				<${Field} id="ai-proxies" label=${t(text, 'ai.proxies')} hint=${t(text, 'ai.proxies.hint')}
					value=${draft.proxies} autocomplete="off"
					onInput=${function (/** @type {Event} */ e) { edit({ proxies: /** @type {HTMLInputElement} */ (e.currentTarget).value }); }} />
				<${RemoteState} here="/ai/access" settings=${now} onTotp=${totp.begin} />
				<${TotpPart} flow=${totp} on=${now.totp} />
				${nothing ? null : html`<p class="hint" data-part="remote-unsaved">${t(text, 'ai.remote.unsaved')}</p>`}
				<div class="ai-row" data-act="save">
					<${Button} primary=${true} disabled=${busy || nothing || urlProblem !== ''}
						onClick=${function () { setAsking(true); }}>${t(text, 'ai.save')}<//>
				</div>
				${said === '' ? null : html`<p class="ai-said" role="status" data-part="said">${said}</p>`}
				${restarting ? html`<p class="ai-row" data-act="again"><${Button} onClick=${function () { window.location.reload(); }}>${t(text, 'ai.again')}<//></p>` : null}
			</section>` : null}
		<//>
		<${Dialog}
			open=${asking}
			title=${t(text, 'ai.save')}
			confirmLabel=${t(text, 'ai.save')}
			onCancel=${function () { setAsking(false); }}
			onConfirm=${save}>
			<p data-part="save-ask">${t(text, 'ai.save.ask')}</p>
		<//>
	</div>`;
}
