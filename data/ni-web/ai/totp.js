// Two-factor sign-in for new connections from the internet: set up, confirm, turn off.
import { html, useState, useEffect, loadQrcode } from '../app/runtime.js';
import * as store from '../app/store.js';
import { t } from '../app/i18n.js';
import { Button } from '../app/ui/button.js';
import { Dialog } from '../app/ui/dialog.js';
import { Field } from '../app/ui/field.js';
import { CopyButton } from './copy.js';
import { readTotpSetup, secretGroups, qrPath } from './model.js';
import { codeOf, worded } from './read.js';
import { refusalText } from './parts.js';
import { errorText } from './guidewords.js';
import text from './ai.text.js';

const kQuiet = 4;

/**
 * @param {string} value
 * @returns {Promise<boolean[][]>}
 */
export function qrMatrix(value) {
	return loadQrcode().then(function (m) {
		const q = m.default(0, 'M');
		q.addData(value);
		q.make();
		const n = q.getModuleCount();
		/** @type {boolean[][]} */
		const rows = [];
		for (let r = 0; r < n; r++) {
			/** @type {boolean[]} */
			const row = [];
			for (let c = 0; c < n; c++)
				row.push(q.isDark(r, c) === true);
			rows.push(row);
		}
		return rows;
	});
}

/**
 * Black on white whatever the theme, or a camera cannot read it.
 *
 * @param {{ value: string }} props
 * @returns {Web.Drawn}
 */
function QrCode(props) {
	const [rows, setRows] = useState(/** @type {boolean[][] | null} */ (null));
	useEffect(function () {
		let live = true;
		setRows(null);
		qrMatrix(props.value).then(function (got) {
			if (live)
				setRows(got);
		}, function () {
			if (live)
				setRows([]);
		});
		return function () { live = false; };
	}, [props.value]);
	if (rows === null)
		return html`<p class="hint" data-part="totp-qr-wait">${t(text, 'ai.totp.qr.wait')}</p>`;
	if (rows.length === 0)
		return html`<p class="ai-warn" data-part="totp-qr-failed">${t(text, 'ai.totp.qr.failed')}</p>`;
	const size = rows.length + 2 * kQuiet;
	return html`<svg class="ai-qr" data-part="totp-qr" viewBox=${'0 0 ' + size + ' ' + size}
		shape-rendering="crispEdges" role="img" aria-label=${t(text, 'ai.totp.qr')}>
		<rect width=${size} height=${size} fill="#fff" />
		<path d=${qrPath(rows, kQuiet)} fill="#000" />
	</svg>`;
}

/**
 * @typedef {object} TotpFlow
 * @property {import('./model.js').AiTotpSetup | null} setup
 * @property {string} code
 * @property {string} codeError
 * @property {boolean} asking the turn-off dialog is open
 * @property {string} password
 * @property {string} passwordError
 * @property {boolean} busy
 * @property {string} said
 * @property {() => void} begin
 * @property {(value: string) => void} typeCode
 * @property {() => void} confirm
 * @property {() => void} cancel
 * @property {() => void} askOff
 * @property {(value: string) => void} typePassword
 * @property {() => void} turnOff
 * @property {() => void} cancelOff
 */

/**
 * Kept by the screen, so a redraw keeps what was typed.
 *
 * @returns {TotpFlow}
 */
export function useTotp() {
	const [setup, setSetup] = useState(/** @type {import('./model.js').AiTotpSetup | null} */ (null));
	const [code, setCode] = useState('');
	const [codeError, setCodeError] = useState('');
	const [asking, setAsking] = useState(false);
	const [password, setPassword] = useState('');
	const [passwordError, setPasswordError] = useState('');
	const [busy, setBusy] = useState(false);
	const [said, setSaid] = useState('');

	/**
	 * @param {unknown} caught
	 * @returns {string}
	 */
	function failure(caught) {
		const words = errorText(codeOf(caught));
		return worded(words) ? words : refusalText(caught);
	}

	function begin() {
		if (busy)
			return;
		setSaid('');
		setBusy(true);
		store.write('POST', '/api/v1/ai/totp/setup', {}).then(function (answer) {
			setBusy(false);
			const got = readTotpSetup(answer);
			if (!got) {
				setSaid(t(text, 'ai.refused.box'));
				return;
			}
			setCode('');
			setCodeError('');
			setSetup(got);
		}, function (caught) {
			setBusy(false);
			setSaid(failure(caught));
		});
	}

	function confirm() {
		if (busy || !setup)
			return;
		const typed = code.replace(/\s+/g, '');
		if (!/^[0-9]{6}$/.test(typed)) {
			setCodeError(t(text, 'ai.totp.code.missing'));
			return;
		}
		setBusy(true);
		store.write('POST', '/api/v1/ai/totp/confirm', { body: { code: typed }, touches: ['/api/v1/ai/settings'] }).then(function () {
			setBusy(false);
			setSetup(null);
			setCode('');
			setCodeError('');
			setSaid(t(text, 'ai.totp.done'));
		}, function (caught) {
			setBusy(false);
			if (codeOf(caught) === 'ai-totp-no-pending') {
				setSetup(null);
				setSaid(failure(caught));
				return;
			}
			setCodeError(failure(caught));
		});
	}

	function turnOff() {
		if (busy)
			return;
		if (password === '') {
			setPasswordError(t(text, 'ai.totp.password.missing'));
			return;
		}
		setBusy(true);
		store.write('POST', '/api/v1/ai/totp/disable', { body: { password: password }, touches: ['/api/v1/ai/settings'] }).then(function () {
			setBusy(false);
			setAsking(false);
			setPassword('');
			setPasswordError('');
			setSaid(t(text, 'ai.totp.gone'));
		}, function (caught) {
			setBusy(false);
			setPasswordError(codeOf(caught) === 'not-permitted' ? t(text, 'ai.totp.password.wrong') : failure(caught));
		});
	}

	return {
		setup: setup, code: code, codeError: codeError, asking: asking, password: password,
		passwordError: passwordError, busy: busy, said: said, begin: begin, confirm: confirm, turnOff: turnOff,
		typeCode: function (/** @type {string} */ value) { setCode(value); setCodeError(''); },
		cancel: function () { setSetup(null); setCode(''); setCodeError(''); },
		askOff: function () { setSaid(''); setPassword(''); setPasswordError(''); setAsking(true); },
		typePassword: function (/** @type {string} */ value) { setPassword(value); setPasswordError(''); },
		cancelOff: function () { setAsking(false); setPassword(''); setPasswordError(''); },
	};
}

/**
 * @param {{ flow: TotpFlow, on: boolean }} props
 * @returns {Web.Drawn}
 */
export function TotpPart(props) {
	const f = props.flow;
	return html`<div class="ai-totp" data-part="totp" data-totp=${props.on ? 'on' : 'off'}>
		<p class="ai-say">${t(text, props.on ? 'ai.totp.state.on' : 'ai.totp.state.off')}</p>
		${props.on
			? html`<div class="ai-row">
				<span data-act="totp-again"><${Button} disabled=${f.busy} onClick=${f.begin}>${t(text, 'ai.totp.again')}<//></span>
				<span data-act="totp-off"><${Button} disabled=${f.busy} onClick=${f.askOff}>${t(text, 'ai.totp.off')}<//></span>
			</div>`
			: html`<div class="ai-row" data-act="totp-setup">
				<${Button} primary=${true} disabled=${f.busy} onClick=${f.begin}>${t(text, 'ai.totp.setup')}<//>
			</div>`}
		${f.said === '' ? null : html`<p class="ai-said" role="status" data-part="totp-said">${f.said}</p>`}
		<${Dialog} open=${f.setup !== null} title=${t(text, 'ai.totp.setup')} confirmLabel=${t(text, 'ai.totp.confirm')}
			onCancel=${f.cancel} onConfirm=${f.confirm}>
			${f.setup ? html`<div class="ai-totp-setup" data-part="totp-setup">
				<p>${t(text, 'ai.totp.lead')}</p>
				<${QrCode} value=${f.setup.uri} />
				<p class="ai-row">${t(text, 'ai.totp.secret')} <code class="mono" data-part="totp-secret">${secretGroups(f.setup.secret)}</code>
					<${CopyButton} value=${f.setup.secret} /></p>
				<${Field} id="ai-totp-code" label=${t(text, 'ai.totp.code')} value=${f.code} autocomplete="one-time-code" inputMode="numeric"
					error=${f.codeError}
					onInput=${function (/** @type {Event} */ e) { f.typeCode(/** @type {HTMLInputElement} */ (e.currentTarget).value); }}
					onKeyDown=${function (/** @type {KeyboardEvent} */ e) { if (e.key === 'Enter') f.confirm(); }} />
			</div>` : null}
		<//>
		<${Dialog} open=${f.asking} title=${t(text, 'ai.totp.off')} confirmLabel=${t(text, 'ai.totp.off')}
			onCancel=${f.cancelOff} onConfirm=${f.turnOff}>
			<p data-part="totp-off-note">${t(text, 'ai.totp.off.note')}</p>
			<${Field} id="ai-totp-password" type="password" label=${t(text, 'ai.totp.password')} value=${f.password}
				autocomplete="current-password" error=${f.passwordError}
				onInput=${function (/** @type {Event} */ e) { f.typePassword(/** @type {HTMLInputElement} */ (e.currentTarget).value); }}
				onKeyDown=${function (/** @type {KeyboardEvent} */ e) { if (e.key === 'Enter') f.turnOff(); }} />
		<//>
	</div>`;
}
