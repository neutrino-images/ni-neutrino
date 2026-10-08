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
import { useAllChannels } from '../../ui/channels.js';
import { numberFault, textFault, namedValue, labelFor } from './model.js';
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
	const said = textFault(row, value);
	if (said !== null) {
		return t(text, said === 'short' ? 'settings.badshort' : said === 'long' ? 'settings.badlong' : 'settings.badchars', {
			min: String(row.minLength),
			max: String(row.maxLength),
			chars: row.allowedChars
		});
	}
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
 * The text that follows a number: this page's own word for the unit the box names, and
 * nothing for a name it has no word for. The raw name is never shown, because it is a
 * key of the box's catalog and says nothing to the person reading the page.
 *
 * @param {import('./model.js').Row} row
 * @returns {string}
 */
export function unitText(row) {
	if (!row.unit)
		return '';
	const key = 'settings.' + row.unit;
	const word = t(text, key);
	return word === key ? '' : word;
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
	return wordOf(row, row.fallback);
}

/**
 * A stored value in the words the control beside it uses.
 *
 * @param {import('./model.js').Row} row
 * @param {string} value
 * @returns {string}
 */
export function wordOf(row, value) {
	if (row.type === 'bool')
		return t(text, value === '0' ? 'settings.off' : 'settings.on');
	return labelFor(row, value);
}

/**
 * The choices of a channel picker: the channels as the box numbers them, a first entry for
 * none, and the stored channel when the list does not hold it, so a select never draws
 * empty over a value the box keeps.
 *
 * @param {ReadonlyArray<{ id: string, number: number, name: string }>} channels
 * @param {string} chosen
 * @returns {Array<{ value: string, label: string }>}
 */
export function channelChoices(channels, chosen) {
	const out = [{ value: '0', label: t(text, 'settings.channel.none') }];
	let seen = chosen === '' || chosen === '0';
	for (const one of channels) {
		if (one.id === chosen)
			seen = true;
		out.push({ value: one.id, label: one.number + '  ' + one.name });
	}
	if (!seen)
		out.push({ value: chosen, label: chosen });
	return out;
}

/**
 * A start channel: chosen from the channels of its kind, written as the identifier.
 *
 * @param {{ row: import('./model.js').Row, value: string, onChange: (id: string, value: string) => void }} props
 * @returns {Web.Drawn}
 */
export function ChannelPick(props) {
	const channels = useAllChannels(props.row.picker === 'radio' ? 'radio' : 'tv');
	return html`<${ChannelSelect} row=${props.row} value=${props.value} channels=${channels} onChange=${props.onChange} />`;
}

/**
 * The picker over a channel list already in hand, which is all the hook above adds.
 *
 * @param {{ row: import('./model.js').Row, value: string, channels: { items: Api.Channel[], failed: boolean }, onChange: (id: string, value: string) => void }} props
 * @returns {Web.Drawn}
 */
export function ChannelSelect(props) {
	const row = props.row;
	const chosen = props.value === '' ? '0' : props.value;
	return html`<${Select}
		label=${row.label}
		value=${chosen}
		needsRestart=${row.needsRestart}
		hint=${props.channels.failed ? t(text, 'settings.channel.partial') : undefined}
		options=${channelChoices(props.channels.items, chosen)}
		onChange=${function (/** @type {Event} */ event) {
			const chooser = /** @type {HTMLSelectElement} */ (event.currentTarget);
			props.onChange(row.id, chooser.value);
		}} />`;
}

/**
 * A setting this page shows and does not set: one the box sets itself, or one that follows
 * the row above it.
 *
 * @param {{ row: import('./model.js').Row, value: string }} props
 * @returns {Web.Drawn}
 */
export function ReadOnlyRow(props) {
	return html`<div class="field set-locked">
		<span class="label">${props.row.label}</span>
		<p class="mono">${props.value}</p>
		<span class="hint">${t(text, props.row.readOnly === 'box' ? 'settings.pair.box' : 'settings.pair.follows')}</span>
	</div>`;
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
 * A setting the box could not state the choices of, or one its parental lock holds.
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
		<p class="mono">${wordOf(props.row, props.value)}</p>
		<span class="hint">${t(text, props.row.held ? 'settings.held' : 'settings.locked')}</span>
	</div>`;
}

/**
 * The control alone, without anything this screen says around it.
 *
 * @param {{ row: import('./model.js').Row, value: string, also?: string, onChange: (id: string, value: string) => void, onClear: (row: import('./model.js').Row) => void }} props
 *   also is the line a pair written whole is drawn with, the other member's value joined to this one's
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

	if (row.readOnly)
		return html`<${ReadOnlyRow} row=${row} value=${row.partner === '' ? value : (props.also === undefined ? value : props.also)} />`;

	if (row.picker)
		return html`<${ChannelPick} row=${row} value=${value} onChange=${props.onChange} />`;

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

	if (row.type === 'enum' || (row.type === 'key' && row.choices.length > 0) || (row.type === 'int' && row.listed)) {
		/** @type {Array<{ value: string, label: string, disabled?: boolean }>} */
		const options = row.choices.map(function (choice) {
			return { value: String(choice.value), label: choice.label };
		});
		// A value another box wrote and this one does not offer: shown, so the
		// select does not draw empty, and not choosable.
		if (value !== '' && !options.some(function (o) { return o.value === value; })) {
			// A key the box accepts and has no name for is a real value and stays choosable,
			// where an enum value the box does not offer is not.
			if (row.type === 'key')
				options.push({ value: value, label: t(text, 'settings.key.unnamed', { value: value }) });
			else
				options.push({ value: value, label: t(text, 'settings.value.unlisted', { value: value }), disabled: true });
		}
		return html`<${Select}
			label=${row.label}
			value=${value}
			needsRestart=${row.needsRestart}
			options=${options}
			onChange=${function (/** @type {Event} */ event) {
				const chooser = /** @type {HTMLSelectElement} */ (event.currentTarget);
				changed(chooser.value);
			}} />`;
	}

	if (row.type === 'string' && row.choices.length > 0) {
		/** @type {Array<{ value: string, label: string, disabled?: boolean }>} */
		const options = row.choices.map(function (choice) {
			return { value: String(choice.text), label: choice.label };
		});
		/* An empty text is how the box says it picks for itself, and it is what such a row
		   falls back to, so it is a choice of its own even though no list names it. Without
		   it a stored empty text would draw as the first entry, and the way back to the
		   default could not be picked. */
		if (!options.some(function (o) { return o.value === ''; }) && (row.fallback === '' || value === ''))
			options.unshift({ value: '', label: t(text, 'settings.value.automatic') });
		/* What is stored passes a write again, so a text the box does not list right now stays
		   shown and is still the value the row holds; it is not offered as a new pick. */
		if (value !== '' && !options.some(function (o) { return o.value === value; }))
			options.push({ value: value, label: t(text, 'settings.value.unlisted', { value: value }), disabled: true });
		return html`<${Select}
			label=${row.label}
			value=${value}
			needsRestart=${row.needsRestart}
			options=${options}
			onChange=${function (/** @type {Event} */ event) {
				const chooser = /** @type {HTMLSelectElement} */ (event.currentTarget);
				changed(chooser.value);
			}} />`;
	}

	const fault = row.type === 'int' || row.type === 'string' ? faultText(row, value) : '';
	// The row's own word for the value it names, beside the number: the input stays
	// a number whatever is shown.
	const named = namedValue(row, value);
	const colorHint = row.channels === null ? undefined : t(text, row.channels === 4 ? 'settings.color.rgba' : 'settings.color.rgb');
	return html`<${Field}
		label=${row.label}
		type=${row.type === 'int' || row.type === 'key' ? 'number' : 'text'}
		value=${value}
		hint=${named === null ? colorHint : named.label}
		unit=${row.type === 'int' ? unitText(row) : ''}
		min=${row.min === null ? null : String(row.min)}
		max=${row.max === null ? null : String(row.max)}
		maxLength=${row.maxLength > 0 ? row.maxLength : undefined}
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
 * @param {{ row: import('./model.js').Row, value: string, also?: string, drifts: boolean, place: string, onChange: (id: string, value: string) => void, onClear: (row: import('./model.js').Row) => void, onRevert: (row: import('./model.js').Row) => void }} props
 *   also is the other member's value for a pair drawn as one line
   drifts is whether what is on screen differs from the value the box falls
 *   back to, which is what the mark and the way back are about
 * @returns {Web.Drawn}
 */
export function Row(props) {
	const row = props.row;
	const drifts = props.drifts;
	const control = html`<${Control}
		row=${row}
		value=${props.value}
		also=${props.also}
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
			// the line out, which would read as agreement. A locked row takes
			// no write, so it is offered no way back either.
			? html`<p class="set-back">
				<span class="chip warn">${t(text, 'settings.drift.mark')}</span>
				<span class="hint">${row.secret
					? t(text, 'settings.drift.unknown')
					: t(text, 'settings.drift.default', { value: deliveredWord(row) })}</span>
				${row.secret || row.locked || row.readOnly ? null : html`<button
					type="button"
					class="btn"
					aria-label=${t(text, 'settings.drift.revert.one', { label: row.label })}
					onClick=${function () { props.onRevert(row); }}>${t(text, 'settings.drift.revert')}</button>`}
			</p>`
			: null}
	</div>`;
}
