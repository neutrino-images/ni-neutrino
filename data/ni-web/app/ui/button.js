// A control, and the one place a control that cannot work says why.
//
// A caller granted Read is refused a write before it reaches the box, and a
// button that is certain to be refused is not offered and not silently dead:
// it is disabled and carries the reason, tied to it so that it is read out
// with the button and not left as a sentence floating beside it.
import { html, useId } from '../runtime.js';

/**
 * @param {{
 *   children?: unknown,
 *   type?: string,
 *   class?: string,
 *   primary?: boolean,
 *   disabled?: boolean,
 *   reason?: string,
 *   onClick?: (event: Event) => void
 * }} props reason is why this control cannot work, and a control that carries
 *   one is disabled and says so where it is read out with the control
 * @returns {Web.Drawn}
 */
export function Button(props) {
	const auto = useId();
	const reasonId = props.reason ? auto + '-why' : null;
	const kind = props.primary ? 'btn primary' : 'btn';

	return html`<span>
		<button
			type=${props.type || 'button'}
			class=${props.class ? kind + ' ' + props.class : kind}
			disabled=${props.disabled || !!props.reason}
			aria-describedby=${reasonId}
			onClick=${props.onClick}>${props.children}</button>
		${reasonId ? html`<span id=${reasonId} class="hint">${props.reason}</span>` : null}
	</span>`;
}
