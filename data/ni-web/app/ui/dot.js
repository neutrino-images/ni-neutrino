// A condition of the box, as a mark and a word.
//
// Never as one of the two. The mark is what is seen from across the room and
// the word is what is read by somebody the three colours do not separate, so
// this draws both and there is no way to ask it for only the mark. A screen
// that wants a red dot uses this and gets a word with it whether it thought of
// one or not.
import { html } from '../runtime.js';

/* The four conditions this page has a colour for, and the whole of them. The
   fourth is what the box is playing: it is the accent and not the red, because
   the red is the box writing to a disc and a vocabulary whose red means two
   things is one nobody can read from across the room. A screen marking the
   running channel hands over this word rather than reaching for a colour. */
/** @type {Record<string, string>} */
const MARKS = { recording: 'rec', running: 'run', warning: 'warn', onair: 'air' };

/**
 * @param {string} kind
 * @param {string} [word]
 * @returns {{ className: string, word: string }}
 */
export function dotParts(kind, word) {
	const mark = MARKS[kind];
	return {
		className: mark ? 'dot ' + mark : 'dot',
		// The kind itself when nothing better was handed in, so the word is
		// there in every case.
		word: word || kind || ''
	};
}

/**
 * @param {{ kind: string, word?: string }} props
 * @returns {Web.Drawn}
 */
export function Dot(props) {
	const parts = dotParts(props.kind, props.word);
	return html`<span class="state-mark">
		<span class=${parts.className} aria-hidden="true"></span>
		<span>${parts.word}</span>
	</span>`;
}

/* The same mark and the same word, drawn as a pill.

   A row of states read down a column is a column of words a person has to read one
   at a time. Given an outline each of them has a shape as well. The outline is the
   frame's four and no more: neutral, what is playing, running, wanting a look. There
   is no red outline, because the red belongs to the mark inside the pill and a red
   border around a word would spend it a second time on the same row. */
/**
 * @param {{ kind: string, word?: string, tone?: string }} props tone is one of
 *   the frame's pill words and nothing where the pill is the neutral one
 * @returns {Web.Drawn}
 */
export function StateChip(props) {
	const parts = dotParts(props.kind, props.word);
	return html`<span class=${props.tone ? 'chip ' + props.tone : 'chip'}>
		<span class=${parts.className} aria-hidden="true"></span>
		<span>${parts.word}</span>
	</span>`;
}
