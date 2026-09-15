// The four answers that are not the screen itself.
//
// Loading for the first time, loading again, writing, failed, and nothing
// there. A screen draws none of these itself, because five screens drawing
// their own spinner is five spinners that behave differently on a slow box.
//
// Loading again is not loading: what is already on screen stays where it is
// and says it is working, since a curtain over known content is how a list
// loses the row somebody was reading.
//
// The failure is drawn as it is handed in and is not interpreted here: what a
// refusal says about itself is decided where the refusal is read, and this
// only follows a link if one was put in front of it.
//
// The phase is named the way the store names it, and that is the whole reason
// it is named that way. This widget used to have words of its own, so every
// screen handing it a snapshot translated between the two. Six screens wrote
// that translation, each one slightly differently, and one of them did not
// write it at all: its reloading and writing marks simply never appeared, and
// nothing failed, because a word that matches nothing here draws the content
// and no mark. One vocabulary and no translation is what stops that.
import { html } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { Button } from './button.js';

/**
 * @param {{
 *   problem?: Web.Shown | null,
 *   phase?: Web.Phase,
 *   empty?: boolean | string,
 *   children?: unknown,
 *   onRetry?: () => void
 * }} props
 * @returns {Web.Drawn}
 */
export function State(props) {
	const problem = props.problem;
	if (problem) {
		return html`<div class="note bad" role="alert">
			<h2>${problem.title || t(text, 'shell.failed')}</h2>
			${problem.detail ? html`<p>${problem.detail}</p>` : null}
			${problem.href ? html`<p><a href=${problem.href}>${t(text, 'shell.more')}</a></p>` : null}
			${props.onRetry ? html`<p><${Button} onClick=${props.onRetry}>${t(text, 'shell.retry')}<//></p>` : null}
		</div>`;
	}

	if (props.phase === 'first')
		return html`<p class="note" role="status">${t(text, 'shell.loading')}</p>`;

	if (props.empty)
		return html`<p class="note">${props.empty === true ? t(text, 'shell.empty') : props.empty}</p>`;

	const busy = props.phase === 'again' || props.phase === 'writing';
	return html`<div class=${busy ? 'busy' : null} aria-busy=${busy ? 'true' : null}>
		${busy ? html`<span class="sr" role="status">${t(text, props.phase === 'writing' ? 'shell.writing' : 'shell.reloading')}</span>` : null}
		${props.children}
	</div>`;
}
