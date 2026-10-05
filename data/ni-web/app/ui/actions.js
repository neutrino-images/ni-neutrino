// What may be done to one row.
//
// Four marks in a line is four targets of eleven pixels on a phone, and three of
// them are wrong every time. So the same list is drawn twice and the stylesheet
// shows one of the two: the marks side by side where there is a pointer, and one
// control that opens a sheet where there is a finger. No width is read in
// JavaScript.
//
// Drawn twice and not moved, because a control that is moved between two places as
// the window changes loses the focus that was on it. The hidden one is out of the
// tab order with it.
import { html, Fragment } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { Sheet } from './sheet.js';
import { useKept } from './kept.js';

/**
 * @typedef {object} RowAction
 * @property {string} id written as data-act on both of its controls
 * @property {string} label what it does, spelled out, which is also what the
 *           mark beside a pointer is named by
 * @property {string | Web.Drawn} [mark] the one character, or small drawing,
 *           the wide folding draws, and the label alone where there is none
 * @property {string} [named] the accessible name and tooltip where a short
 *           label needs the row's name beside it; the label otherwise
 * @property {boolean} [disabled]
 * @property {() => void} onAct
 */

/**
 * @param {{
 *   title?: string,
 *   keep?: string,
 *   actions?: readonly RowAction[]
 * }} props title names the row the sheet is about, so that a sheet opened from
 *   the fourth row says which row it came from; keep names the row for good, so
 *   its sheet stays open when the list is read again
 * @returns {Web.Drawn}
 */
export function RowActions(props) {
	const actions = props.actions || [];
	const [open, setOpen] = useKept(props.keep || '', false);

	if (actions.length === 0)
		return null;

	/**
	 * @param {RowAction} one
	 * @returns {void}
	 */
	function run(one) {
		setOpen(false);
		one.onAct();
	}

	return html`<${Fragment}>
		<span class="acts-inline">
			${actions.map(function (one) {
				return html`<button
					key=${one.id}
					type="button"
					class="btn"
					data-act=${one.id}
					disabled=${one.disabled}
					title=${one.named || one.label}
					aria-label=${one.named || one.label}
					onClick=${function () { one.onAct(); }}>${one.mark || one.label}</button>`;
			})}
		</span>
		<button
			type="button"
			class="btn acts-more"
			aria-haspopup="dialog"
			aria-expanded=${open ? 'true' : 'false'}
			aria-label=${props.title ? t(text, 'shell.actions.for', { name: props.title }) : t(text, 'shell.actions')}
			onClick=${function () { setOpen(true); }}>⋯</button>
		<${Sheet} open=${open} onClose=${function () { setOpen(false); }} label=${t(text, 'shell.actions')}>
			<h2>${props.title || t(text, 'shell.actions')}</h2>
			<ul class="sheet-areas row-actions">
				${actions.map(function (one) {
					return html`<li key=${one.id}>
						<button
							type="button"
							data-act=${one.id}
							disabled=${one.disabled}
							title=${one.named || one.label}
							aria-label=${one.named || null}
							onClick=${function () { run(one); }}>
							${one.mark ? html`<span class="ic" aria-hidden="true">${one.mark}</span>` : null}
							<span>${one.label}</span>
						</button>
					</li>`;
				})}
			</ul>
		<//>
	<//>`;
}
