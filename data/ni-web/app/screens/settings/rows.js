/* One declared setting, drawn as whatever kind it says it is.
 *
 * Three of the five kinds draw the frame's own controls. The two written out here are the
 * two none of those cover: a credential, which needs a field that says "unchanged" while
 * it is empty and the deliberate act beside it that is the only way to empty one, and a
 * choice the box could not state the choices of, which is a row that exists and cannot be
 * answered.
 *
 * Nothing here reaches the box.
 */
import { html, useId } from '../../runtime.js';
import { t } from '../../i18n.js';
import { Field, Notes, describedBy } from '../../ui/field.js';
import { Select } from '../../ui/select.js';
import { Switch } from '../../ui/switch.js';
import { Button } from '../../ui/button.js';
import { numberFault, fallbackLabel } from './model.js';
import text from './settings.text.js';

/**
 * What a number field says about what is in it, and the empty string when
 * there is nothing to say.
 *
 * @param {import('./model.js').Row} row
 * @param {string} value
 * @returns {string}
 */
export function faultText(row, value) {
	const fault = numberFault(row, value);
	if (fault === null)
		return '';
	if (fault === 'range') {
		return t(text, 'settings.badrange', {
			min: row.min === null ? '' : String(row.min),
			max: row.max === null ? '' : String(row.max)
		});
	}
	if (fault === 'notnumber')
		return t(text, 'settings.badnumber');
	return t(text, 'settings.badempty');
}

/**
 * What the box falls back to, in the words the control beside it uses.
 *
 * A yes or no is a word here and a nought or a one in the settings file, and the file's
 * spelling is the one thing on this screen nobody outside the program has ever seen.
 * Everything else the model already renders, because only it knows what a choice's number
 * is called.
 *
 * @param {import('./model.js').Row} row
 * @returns {string}
 */
export function deliveredWord(row) {
	if (row.type === 'bool')
		return t(text, row.fallback === '0' ? 'settings.off' : 'settings.on');
	return fallbackLabel(row);
}

/**
 * A credential: an empty field, the word that says what empty means, and the one act that
 * empties it.
 *
 * The field is empty because the box withholds the value and not because there is none,
 * and those are two different states this screen must not blur. So it says "unchanged"
 * where a value would stand, it sends nothing while nothing has been typed, and emptying
 * is a button and a route of its own.
 *
 * @param {{ row: import('./model.js').Row, value: string, onInput: (value: string) => void, onClear: () => void }} props
 * @returns {Web.Drawn}
 */
export function SecretRow(props) {
	const id = useId();
	const row = props.row;
	const note = t(text, 'settings.secret.note');

	return html`<div class="set-secret">
		<label class="field">
			<span class="label">${row.label}</span>
			<input
				id=${id}
				type="password"
				value=${props.value}
				placeholder=${t(text, 'settings.secret.placeholder')}
				autocomplete="new-password"
				aria-describedby=${describedBy(id, { hint: note, error: '', needsRestart: row.needsRestart })}
				onInput=${function (/** @type {Event} */ event) {
					const field = /** @type {HTMLInputElement} */ (event.currentTarget);
					props.onInput(field.value);
				}} />
			<${Notes} id=${id} hint=${note} error=${null} needsRestart=${row.needsRestart} />
		</label>
		<${Button} onClick=${props.onClear}>${t(text, 'settings.secret.clear')}<//>
	</div>`;
}

/**
 * A setting the box could not state the choices of.
 *
 * Shown and not hidden, and not offered as free text either: the row exists, the box is
 * simply unable to say what it takes at the moment, and every write to it is refused for
 * as long as that lasts. A chooser with nothing in it would read as a setting with no
 * answers, and a text field would invite a value that cannot land.
 *
 * @param {{ row: import('./model.js').Row, value: string }} props
 * @returns {Web.Drawn}
 */
export function LockedRow(props) {
	return html`<div class="field set-locked">
		<span class="label">${props.row.label}</span>
		<p class="mono">${props.value}</p>
		<span class="hint">${t(text, 'settings.locked')}</span>
	</div>`;
}

/**
 * The control alone, without anything this screen says around it.
 *
 * @param {{ row: import('./model.js').Row, value: string, onChange: (id: string, value: string) => void, onClear: (row: import('./model.js').Row) => void }} props
 * @returns {Web.Drawn}
 */
export function Control(props) {
	const row = props.row;
	const value = props.value;

	/** @param {string} next */
	function changed(next) {
		props.onChange(row.id, next);
	}

	if (row.secret) {
		return html`<${SecretRow}
			row=${row}
			value=${value}
			onInput=${changed}
			onClear=${function () { props.onClear(row); }} />`;
	}

	if (row.locked)
		return html`<${LockedRow} row=${row} value=${value} />`;

	if (row.type === 'bool') {
		// Anything but the box's own nought is on. A value this page has never
		// seen is not a reason to draw a switch as off and then write that
		// reading back as if it had been there all along.
		return html`<${Switch}
			label=${row.label}
			checked=${value !== '0' && value !== ''}
			needsRestart=${row.needsRestart}
			onChange=${function (/** @type {Event} */ event) {
				const box = /** @type {HTMLInputElement} */ (event.currentTarget);
				changed(box.checked ? '1' : '0');
			}} />`;
	}

	if (row.type === 'enum') {
		return html`<${Select}
			label=${row.label}
			value=${value}
			needsRestart=${row.needsRestart}
			options=${row.choices.map(function (choice) {
				return { value: String(choice.value), label: choice.label };
			})}
			onChange=${function (/** @type {Event} */ event) {
				const chooser = /** @type {HTMLSelectElement} */ (event.currentTarget);
				changed(chooser.value);
			}} />`;
	}

	const fault = row.type === 'int' ? faultText(row, value) : '';
	return html`<${Field}
		label=${row.label}
		type=${row.type === 'int' ? 'number' : 'text'}
		value=${value}
		min=${row.min === null ? null : String(row.min)}
		max=${row.max === null ? null : String(row.max)}
		error=${fault === '' ? null : fault}
		needsRestart=${row.needsRestart}
		onInput=${function (/** @type {Event} */ event) {
			const field = /** @type {HTMLInputElement} */ (event.currentTarget);
			changed(field.value);
		}} />`;
}

/**
 * One row: the control, and what the view around it adds to it.
 *
 * @param {{ row: import('./model.js').Row, value: string, drifts: boolean, place: string, onChange: (id: string, value: string) => void, onClear: (row: import('./model.js').Row) => void, onRevert: (row: import('./model.js').Row) => void }} props
 *   drifts is whether what is on screen differs from the value the box falls
 *   back to, which is what the mark and the way back are about
 * @returns {Web.Drawn}
 */
export function Row(props) {
	const row = props.row;
	const drifts = props.drifts;
	const control = html`<${Control}
		row=${row}
		value=${props.value}
		onChange=${props.onChange}
		onClear=${props.onClear} />`;

	/* The identifier is on the row and not only in the label, because it is the one name of
	   a setting that does not change with the language and does not change with the words
	   the box was given. A check that walks this screen addresses a row by it; a person
	   reading the settings file has it in front of them; and the search below matches on
	   it. */
	/* A DIFFERENCE IS A SHAPE AND NOT ONLY A WORD. A row that is not on the value the box
	   shipped carries a tinted edge, a mark, and the way back to that value, so it can be
	   picked out of forty rows by looking. The three go together: colour alone is not a
	   thing everybody can see. */
	return html`<div class=${drifts ? 'set-row set-drift' : 'set-row'} data-setting=${row.id} data-drift=${drifts ? 'yes' : null}>
		${control}
		${props.place === '' ? null : html`<p class="hint set-place">${props.place}</p>`}
		${drifts
			// Never a claim about a credential: there is no value to hold
			// against the delivered one, and this says so rather than leaving
			// the line out, which would read as agreement.
			? html`<p class="set-back">
				<span class="chip warn">${t(text, 'settings.drift.mark')}</span>
				<span class="hint">${row.secret
					? t(text, 'settings.drift.unknown')
					: t(text, 'settings.drift.default', { value: deliveredWord(row) })}</span>
				${row.secret ? null : html`<button
					type="button"
					class="btn"
					aria-label=${t(text, 'settings.drift.revert.one', { label: row.label })}
					onClick=${function () { props.onRevert(row); }}>${t(text, 'settings.drift.revert')}</button>`}
			</p>`
			: null}
	</div>`;
}
