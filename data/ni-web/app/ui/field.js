// One labelled input, with its hint, its error and its restart note tied to it.
//
// Tied and not merely placed beside it: a label that is only next to a field is a
// label for whoever can see the layout. The generated settings form is built out of
// these and writes no input of its own, so every one of the several hundred rows the
// box declares is labelled the same way and wrong in at most one place.
import { html, useId } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';

// The three notes that can hang under a control, named once here because the
// select and the switch beside this file tie theirs the same way.
/**
 * @param {string} id
 * @param {{ hint?: string, error?: string, needsRestart?: boolean, describedBy?: string }} props
 *        describedBy names a note that is already on the screen somewhere else
 * @returns {string | null} the identifiers of the notes under it, and null
 *          where there are none
 */
export function describedBy(id, props) {
	/** @type {string[]} */
	const parts = [];
	if (props.hint)
		parts.push(id + '-hint');
	if (props.error)
		parts.push(id + '-err');
	if (props.needsRestart)
		parts.push(id + '-restart');
	/* A sentence about this control that the layout put somewhere other than
	   under it. Whoever sees the screen finds it by looking; whoever does not
	   would never be told it existed, and the further away it is drawn the
	   more that matters. */
	if (props.describedBy)
		parts.push(props.describedBy);
	return parts.length ? parts.join(' ') : null;
}

/**
 * @param {{ id: string, hint?: string, error?: string, needsRestart?: boolean }} props
 * @returns {Web.Drawn}
 */
export function Notes(props) {
	return html`<span>
		${props.hint ? html`<span id=${props.id + '-hint'} class="hint">${props.hint}</span>` : null}
		${props.needsRestart ? html`<span id=${props.id + '-restart'} class="hint">${t(text, 'shell.restart')}</span>` : null}
		${props.error ? html`<span id=${props.id + '-err'} class="err">${props.error}</span>` : null}
	</span>`;
}

/**
 * @param {{
 *   id?: string,
 *   label?: string,
 *   type?: string,
 *   value?: string | number,
 *   rows?: number,
 *   min?: number,
 *   max?: number,
 *   readOnly?: boolean,
 *   autocomplete?: string,
 *   hint?: string,
 *   error?: string,
 *   needsRestart?: boolean,
 *   describedBy?: string,
 *   onInput?: (event: Event) => void,
 *   onKeyDown?: (event: KeyboardEvent) => void
 * }} props onKeyDown is here because the document states form-action 'none':
 *   there is no form on this page to press return in, so a field that answers
 *   return answers it itself
 * @returns {Web.Drawn}
 */
export function Field(props) {
	const auto = useId();
	const id = props.id || auto;
	const many = (props.rows || 0) > 1;

	return html`<label class=${props.error ? 'field bad' : 'field'}>
		<span class="label">${props.label}</span>
		${many
			? html`<textarea
				id=${id}
				rows=${props.rows}
				value=${props.value}
				readOnly=${props.readOnly}
				aria-invalid=${props.error ? 'true' : null}
				aria-describedby=${describedBy(id, props)}
				onInput=${props.onInput}
				onKeyDown=${props.onKeyDown} />`
			: html`<input
				id=${id}
				type=${props.type || 'text'}
				value=${props.value}
				min=${props.min}
				max=${props.max}
				readOnly=${props.readOnly}
				autocomplete=${props.autocomplete}
				aria-invalid=${props.error ? 'true' : null}
				aria-describedby=${describedBy(id, props)}
				onInput=${props.onInput}
				onKeyDown=${props.onKeyDown} />`}
		<${Notes} id=${id} hint=${props.hint} error=${props.error} needsRestart=${props.needsRestart} />
	</label>`;
}
