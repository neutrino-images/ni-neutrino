import { html, useState, useRef } from '../app/runtime.js';
import { t } from '../app/i18n.js';
import text from './ai.text.js';

/**
 * @typedef {object} CopyEnv
 * @property {boolean} secure
 * @property {{ writeText: (s: string) => Promise<void> } | null} clipboard
 * @property {Document} doc
 */

/**
 * The clipboard exists only in a secure context, and the box is reached over plain http at home.
 *
 * @param {string} value
 * @param {CopyEnv} env
 * @returns {Promise<'copied' | 'failed'>}
 */
export function copyText(value, env) {
	if (env.secure && env.clipboard) {
		return env.clipboard.writeText(value).then(
			function () { return /** @type {'copied'} */ ('copied'); },
			function () { return viaCommand(value, env.doc); });
	}
	return Promise.resolve(viaCommand(value, env.doc));
}

/**
 * @param {string} value
 * @param {Document} doc
 * @returns {'copied' | 'failed'}
 */
function viaCommand(value, doc) {
	const area = doc.createElement('textarea');
	area.value = value;
	area.setAttribute('readonly', '');
	area.className = 'ai-offscreen';
	doc.body.appendChild(area);
	area.select();
	let done = false;
	try {
		done = doc.execCommand('copy');
	} catch (e) {
		done = false;
	}
	doc.body.removeChild(area);
	return done ? 'copied' : 'failed';
}

/**
 * @param {{ value: string, label?: string, select?: () => void, disabled?: boolean }} props
 * @returns {Web.Drawn}
 */
export function CopyButton(props) {
	const [said, setSaid] = useState('');
	function press() {
		copyText(props.value, {
			secure: window.isSecureContext === true,
			clipboard: navigator.clipboard || null,
			doc: document,
		}).then(function (how) {
			if (how === 'failed' && props.select)
				props.select();
			setSaid(t(text, how === 'copied' ? 'ai.copy.done' : 'ai.copy.selected'));
		});
	}
	return html`<span class="ai-copy">
		<button type="button" class="btn" data-copy="" disabled=${!!props.disabled} onClick=${press}>${props.label || t(text, 'ai.copy')}</button>
		${said === '' ? null : html`<span class="hint" role="status" data-copied="">${said}</span>`}
	</span>`;
}

/**
 * @param {{ id: string, label: string, value: string, codeId?: string }} props
 * @returns {Web.Drawn}
 */
export function Snippet(props) {
	const ref = useRef(/** @type {HTMLElement | null} */ (null));
	function select() {
		const sel = window.getSelection();
		if (sel && ref.current)
			sel.selectAllChildren(ref.current);
	}
	return html`<figure class="ai-snippet" data-snippet=${props.id}>
		<figcaption>${props.label}</figcaption>
		<pre ref=${ref}><code id=${props.codeId || null}>${props.value}</code></pre>
		<${CopyButton} value=${props.value} select=${select} />
	</figure>`;
}
