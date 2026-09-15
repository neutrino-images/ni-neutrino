// One choice out of a list the box named.
//
// The same notes as the field beside it and tied the same way, because the
// generated form draws a row as one or the other and a person reading the two
// should not be able to tell which file drew it.
import { html, useId } from '../runtime.js';
import { describedBy, Notes } from './field.js';

/**
 * @param {{
 *   id?: string,
 *   label?: string,
 *   value?: string | number,
 *   options?: Array<{ value: string | number, label: string }>,
 *   hint?: string,
 *   error?: string,
 *   needsRestart?: boolean,
 *   onChange?: (event: Event) => void
 * }} props
 * @returns {Web.Drawn}
 */
export function Select(props) {
	const auto = useId();
	const id = props.id || auto;
	const options = props.options || [];

	return html`<label class=${props.error ? 'field bad' : 'field'}>
		<span class="label">${props.label}</span>
		<select
			id=${id}
			value=${props.value}
			aria-invalid=${props.error ? 'true' : null}
			aria-describedby=${describedBy(id, props)}
			onChange=${props.onChange}>
			${options.map(function (option) {
				return html`<option key=${option.value} value=${option.value}>${option.label}</option>`;
			})}
		</select>
		<${Notes} id=${id} hint=${props.hint} error=${props.error} needsRestart=${props.needsRestart} />
	</label>`;
}
