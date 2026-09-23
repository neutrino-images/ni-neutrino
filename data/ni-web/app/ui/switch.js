// Yes or no.
//
// A checkbox and not a painted slider: it is reachable with the keyboard, it
// says what it is to a screen reader, and it costs nothing. The label is the
// element around it, so the word is part of the control and not a target a
// finger misses.
import { html, useId } from '../runtime.js';
import { describedBy, Notes } from './field.js';

/**
 * @param {{
 *   id?: string,
 *   label?: string,
 *   checked?: boolean,
 *   disabled?: boolean,
 *   hint?: string,
 *   error?: string,
 *   needsRestart?: boolean,
 *   onChange?: (event: Event) => void
 * }} props
 * @returns {Web.Drawn}
 */
export function Switch(props) {
	const auto = useId();
	const id = props.id || auto;

	/* Named apart from the other fields, because a yes or no is one child of
	   the field where the others are two: a form laying a field out as a name
	   and a control has to reach inside this one instead. */
	return html`<div class="field field-switch">
		<label class="switch">
			<input
				id=${id}
				type="checkbox"
				checked=${!!props.checked}
				disabled=${props.disabled}
				aria-describedby=${describedBy(id, props)}
				onChange=${props.onChange} />
			<span>${props.label}</span>
		</label>
		<${Notes} id=${id} hint=${props.hint} error=${props.error} needsRestart=${props.needsRestart} />
	</div>`;
}
