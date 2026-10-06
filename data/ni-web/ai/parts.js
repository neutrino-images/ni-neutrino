import { html, useState, useEffect } from '../app/runtime.js';
import * as session from '../app/session.js';
import { t } from '../app/i18n.js';
import { Button } from '../app/ui/button.js';
import { refusalKey } from './read.js';
import { remoteReasons, lanReasons } from './model.js';
import text from './ai.text.js';

/**
 * Every AI route is System, the reading included, so nothing is sent short of it.
 *
 * @returns {boolean}
 */
export function useSystemSession() {
	const [, setGranted] = useState(session.state());
	useEffect(function () {
		return session.subscribe(setGranted);
	}, []);
	const may = session.canSystem();
	useEffect(function () {
		if (!may)
			session.requireSystem().catch(function () { });
	}, [may]);
	return may;
}

/**
 * The caller writes the store call itself, so the address stays a literal the path check reads.
 *
 * @param {boolean} ask
 * @param {(fn: (s: Web.Snapshot<unknown>) => void) => () => void} start
 * @returns {Web.Snapshot<unknown> | null}
 */
export function useWatch(ask, start) {
	const [shot, setShot] = useState(/** @type {Web.Snapshot<unknown> | null} */ (null));
	useEffect(function () {
		if (!ask) {
			setShot(null);
			return undefined;
		}
		return start(setShot);
	}, [ask]);
	return shot;
}

/**
 * @param {{ shown: boolean, why?: string }} props why is the sentence that says what signing in is for
 * @returns {Web.Drawn}
 */
export function SignInRow(props) {
	if (!props.shown)
		return null;
	return html`<p class="ai-row" data-part="signin">
		${props.why ? html`<span data-part="signin-why">${props.why}</span>` : null}
		<${Button} primary=${true} onClick=${function () { session.requireSystem().catch(function () { }); }}>
			${t(text, 'ai.signin')}
		<//>
	</p>`;
}

/**
 * A failure in this area's own words, never the box's.
 *
 * @param {unknown} caught
 * @returns {string}
 */
export function refusalText(caught) {
	const key = refusalKey(caught);
	return t(text, key);
}

/**
 * A failed load as the state block draws it.
 *
 * @param {Web.Failure | null} error
 * @returns {Web.Shown | null}
 */
export function refusalShown(error) {
	return error ? { detail: refusalText(error) } : null;
}

/** Where each reason is put right, and the words of the link there. */
const kFix = {
	off: { href: '/ai/access', key: 'ai.why.go.on' },
	lan: { href: '/ai/access', key: 'ai.why.go.on' },
	password: { href: '/system/webserver', key: 'ai.why.go.password' },
	address: { href: '/ai/access', key: 'ai.why.go.access' },
	proxy: { href: '/ai/access', key: 'ai.why.go.access' },
	totp: { href: '/ai/access', key: 'ai.why.go.totp' },
};

/**
 * @param {{ why: 'off' | 'lan' | 'password' | 'address' | 'proxy' | 'totp', here: string, onTotp?: () => void }} props
 * @returns {Web.Drawn}
 */
function Reason(props) {
	const fix = kFix[props.why];
	return html`<p class="ai-warn" data-reason=${props.why}>
		${t(text, 'ai.why.' + props.why)}
		${props.why === 'password' ? html` <span data-part="strong">${t(text, 'ai.remote.strong')}</span>` : null}
		${props.why === 'totp' && props.onTotp
			? html` <span data-act="totp-reason"><${Button} onClick=${props.onTotp}>${t(text, 'ai.totp.setup')}<//></span>`
			: fix.href === props.here ? null : html` <a href=${fix.href}>${t(text, fix.key)}</a>`}
	</p>`;
}

/**
 * Whether clients can reach the box from the internet, and every reason they cannot.
 *
 * @param {{ settings: import('./model.js').AiSettings, here: string, onTotp?: () => void }} props here is the path of the screen, which is not linked to
 * @returns {Web.Drawn}
 */
export function RemoteState(props) {
	const why = remoteReasons(props.settings);
	return html`<div class="ai-state" data-part="remote-state">
		<p class="ai-say" data-say=${why.length ? 'shut' : 'open'}>${t(text, why.length ? 'ai.remote.shut' : 'ai.remote.open')}</p>
		${why.map(function (one) { return html`<${Reason} key=${one} why=${one} here=${props.here} onTotp=${props.onTotp} />`; })}
		${why.indexOf('password') === -1 ? html`<p class="hint" data-part="strong">${t(text, 'ai.remote.strong')}</p>` : null}
	</div>`;
}

/**
 * Every reason a client in the home network cannot reach the box; nothing when it can.
 *
 * @param {{ settings: import('./model.js').AiSettings, here: string }} props
 * @returns {Web.Drawn}
 */
export function LanState(props) {
	const why = lanReasons(props.settings);
	return why.length ? html`<div class="ai-state" data-part="lan-state">
		${why.map(function (one) { return html`<${Reason} key=${one} why=${one} here=${props.here} />`; })}
	</div>` : null;
}
