/* The stream the box pushes, read along.

   NOT A SECOND STREAM. The frame already holds one connection to the box and
   every screen on these pages is redrawn out of it; this listens to that one
   rather than opening another, because two streams from one page is two of the
   handful of listeners the server allows, spent on one browser.

   HELD IN THE SCREEN AND NOWHERE ELSE. What arrives is kept while this screen
   is on the page and is gone when it leaves. Nothing is written to the box and
   nothing is written to the browser's own store: a log that outlived the
   looking would be a page quietly keeping a record of a box somebody was
   merely passing through.

   NEWEST FIRST, because what a person came here to see is what just happened,
   and a list that grows downwards puts that below the fold within a minute. */

import { html, useState, useEffect, useRef } from '../../runtime.js';
import * as events from '../../events.js';
import { clock } from '../../fmt.js';
import { t } from '../../i18n.js';
import { Button } from '../../ui/button.js';
import { Dot } from '../../ui/dot.js';
import text from './dev.text.js';

export const css = '/app/screens/dev/dev.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'dev.events.lead'); }


/* How many are kept. A box that zaps and records leaves a few of these a
   minute, and a page held open for an afternoon would otherwise grow without
   end in a browser on a television. Enough to see what a sequence looked like,
   and no more. */
const kKeep = 200;

/**
 * @typedef {object} Seen
 * @property {number} id
 * @property {number} at seconds, so that the one clock of this application
 *           formats it
 * @property {string} type
 * @property {string} body what the event carried, as one line
 */

/**
 * What an event carried, as one line, leaving out the members it did not use.
 *
 * Every event carries all four members whether or not it means them, so a line
 * printing all four would say channel and nought and nothing for most of them.
 *
 * @param {{ channel_id: string, value: number, text: string }} event
 * @returns {string}
 */
export function bodyOf(event) {
	/** @type {string[]} */
	const parts = [];
	if (event.channel_id !== '')
		parts.push('channel_id=' + event.channel_id);
	if (event.value !== 0)
		parts.push('value=' + String(event.value));
	if (event.text !== '')
		parts.push('text=' + event.text);
	return parts.join('  ');
}

/**
 * The list with one more on the front, cut to what is kept.
 *
 * @param {readonly Seen[]} held
 * @param {Seen} one
 * @returns {Seen[]}
 */
export function pushed(held, one) {
	const out = [one].concat(held);
	return out.length > kKeep ? out.slice(0, kKeep) : out;
}

/**
 * @returns {Web.Drawn}
 */
export default function DevEvents() {
	const [held, setHeld] = useState(/** @type {Seen[]} */ ([]));
	const [status, setStatus] = useState(events.streamStatus());
	/* Only a number that goes up, so that two events arriving in the same
	   second are two rows and not one. Held in a ref because changing it must
	   not itself draw anything. */
	const next = useRef(0);

	useEffect(function () {
		return events.subscribe(function (event, type) {
			next.current++;
			const one = {
				id: next.current,
				at: Math.floor(Date.now() / 1000),
				type: type,
				body: bodyOf(event)
			};
			setHeld(function (was) { return pushed(was, one); });
		});
	}, []);

	useEffect(function () {
		return events.watchStatus(setStatus);
	}, []);

	return html`<div class="dev">
		<section class="dev-block">
			<div class="dev-row">
				${/* Whether a stream is open, and not whether the box answered.
				     A box refusing every stream because it is already carrying
				     as many as it will answers perfectly well, so the first of
				     those was true while this page was being told nothing at
				     all and said so on the one screen that is about the
				     stream. */''}
				<${Dot}
					kind=${status.carrying ? 'running' : 'warning'}
					word=${t(text, status.carrying ? 'dev.events.connected' : 'dev.events.away')} />
				<${Button} onClick=${function () { setHeld([]); }} disabled=${held.length === 0}>
					${t(text, 'dev.events.clear')}
				<//>
			</div>
			${status.denied ? html`<p class="note bad" role="alert">${t(text, 'dev.events.denied')}</p>` : null}
			${held.length === 0
				? html`<p class="note">${t(text, 'dev.events.waiting')}</p>`
				: html`<div>
					<p class="dev-lead">${t(text, 'dev.events.held', { count: held.length, max: kKeep })}</p>
					<div class="dev-log" role="log">
						${held.map(function (one) {
							return html`<div class="dev-op" key=${one.id}>
								<span class="dev-method mono">${clock(one.at)}</span>
								<span class="mono">${one.type}</span>
								<span class="mono dev-said">${one.body}</span>
							</div>`;
						})}
					</div>
				</div>`}
		</section>
	</div>`;
}
