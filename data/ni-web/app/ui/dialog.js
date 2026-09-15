// A question that has to be answered before anything else happens.
//
// The same overlay as the menu and the sign in sheet, so it is trapped and
// escapable in the same way. What it adds is a title, a pair of answers and a
// default answer that is the harmless one.
import { html, useId } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { Sheet } from './sheet.js';
import { Button } from './button.js';

/**
 * @param {{
 *   open: boolean,
 *   title?: string,
 *   children?: unknown,
 *   cancelLabel?: string,
 *   confirmLabel?: string,
 *   onCancel: () => void,
 *   onConfirm: () => void
 * }} props
 * @returns {Web.Drawn}
 */
export function Dialog(props) {
	const id = useId();

	return html`<${Sheet} open=${props.open} onClose=${props.onCancel} centred=${true} label=${props.title}>
		<h2 id=${id}>${props.title}</h2>
		${props.children}
		<p>
			<${Button} onClick=${props.onCancel}>${props.cancelLabel || t(text, 'shell.cancel')}<//>
			${' '}
			<${Button} primary=${true} onClick=${props.onConfirm}>${props.confirmLabel || t(text, 'shell.confirm')}<//>
		</p>
	<//>`;
}
