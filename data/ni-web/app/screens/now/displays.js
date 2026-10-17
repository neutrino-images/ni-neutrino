import { html } from '../../runtime.js';
import { t } from '../../i18n.js';
import text from './now.text.js';
import { Card, useResource } from './parts.js';
import { Displays } from './screenshot.js';

export const css = '/app/screens/now/now.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'now.displays.head'); }

/**
 * @returns {Web.Drawn}
 */
export default function Display() {
	return html`<div class="now-display">
		<${Displays} />
	</div>`;
}
