// The mark in front of a destination, in the bar along the bottom and in the
// sheet that holds the rest.
//
// DRAWN AND NOT TYPED. The marks had been characters out of the geometric
// blocks, chosen because every font carries them, and every font carries them
// differently: one draws the clock face a third smaller than the square beside
// it, one sits it on another baseline, and a system with an emoji font may draw
// the gear in colour. A row of four that does not line up is read as a fault.
// These are one set on one grid of 24, drawn in the colour of the words beside
// them, at the size of those words.
//
// A destination this set has no mark for keeps the character it carries, so a
// stream that adds one is drawn the day it is added and not on the day somebody
// draws it a picture.
import { html } from '../runtime.js';

/* Lines on a grid of 24, stroked; a part that is filled says so itself.
   Functions and not drawn nodes: the bar and the sheet draw the same mark at
   once, and one node standing in two places is one Preact keeps only one of. */
/** @type {Record<string, () => Web.Drawn>} */
const MARKS = {
	now: () => html`<rect x="3" y="4.5" width="18" height="12" rx="2" /><path d="M8.5 20h7M12 16.5V20" />`,
	channels: () => html`<path d="M9 6h11M9 12h11M9 18h11" /><circle cx="4.5" cy="6" r="1" /><circle cx="4.5" cy="12" r="1" /><circle cx="4.5" cy="18" r="1" />`,
	epg: () => html`<rect x="3" y="4" width="18" height="16" rx="2" /><path d="M3 9.5h18M3 14.75h18M9 9.5V20" />`,
	timers: () => html`<circle cx="12" cy="12" r="8.5" /><path d="M12 7.5V12l3 2" />`,
	recordings: () => html`<circle cx="12" cy="12" r="8.5" /><circle cx="12" cy="12" r="3.5" fill="currentColor" stroke="none" />`,
	files: () => html`<path d="M3 7.5a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2V17a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" />`,
	settings: () => html`<path d="M4 6.5h9M17 6.5h3M4 12h3M11 12h9M4 17.5h11M19 17.5h1" /><circle cx="15" cy="6.5" r="2" /><circle cx="9" cy="12" r="2" /><circle cx="17" cy="17.5" r="2" />`,
	system: () => html`<rect x="5.5" y="5.5" width="13" height="13" rx="2" /><rect x="9.5" y="9.5" width="5" height="5" rx="0.5" /><path d="M9.5 2.5v3M14.5 2.5v3M9.5 18.5v3M14.5 18.5v3M2.5 9.5h3M2.5 14.5h3M18.5 9.5h3M18.5 14.5h3" />`,
	ai: () => html`<path d="M12 3.5l1.9 5.1 5.1 1.9-5.1 1.9L12 17.5l-1.9-5.1L5 10.5l5.1-1.9z" /><path d="M18.5 16v4M16.5 18h4" />`,
	dev: () => html`<path d="M8.5 8 4.5 12l4 4M15.5 8l4 4-4 4M13.5 5.5l-3 13" />`,
	more: () => html`<circle cx="5.5" cy="12" r="1.5" fill="currentColor" stroke="none" /><circle cx="12" cy="12" r="1.5" fill="currentColor" stroke="none" /><circle cx="18.5" cy="12" r="1.5" fill="currentColor" stroke="none" />`
};

/**
 * @param {{ id: string, fallback?: string }} props
 * @returns {Web.Drawn}
 */
export function NavIcon(props) {
	const mark = Object.prototype.hasOwnProperty.call(MARKS, props.id) ? MARKS[props.id] : null;
	if (!mark)
		return html`<span class="ic" aria-hidden="true">${props.fallback || '\u2022'}</span>`;
	return html`<span class="ic" aria-hidden="true"><svg class="ic-mark" viewBox="0 0 24 24" focusable="false">${mark()}</svg></span>`;
}
