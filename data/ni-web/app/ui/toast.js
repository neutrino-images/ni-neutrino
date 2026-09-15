// What the page says about something that already happened.
//
// Kept out of the screens: a message about a write belongs to the page and not to
// whichever list happened to be open, and a screen that is left while the box is still
// answering would take its own message with it.
//
// Read out politely and never interrupting: none of these is a question.
import { html, useState, useEffect } from '../runtime.js';

/**
 * @typedef {object} Note
 * @property {number} id
 * @property {string} message
 * @property {string} kind
 */

/** @type {Note[]} */
const notes = [];
/** @type {Set<(shown: Note[]) => void>} */
const listeners = new Set();
let nextId = 1;

// Long enough to read a sentence, short enough not to sit over a control.
const LINGER = 6000;

/**
 * @param {string} message
 * @param {string} [kind] 'bad' for a refusal, and nothing for everything else
 * @returns {number} the identifier, for a caller that wants to take it back
 */
export function toast(message, kind) {
	const note = { id: nextId++, message: message, kind: kind || '' };
	notes.push(note);
	listeners.forEach(function (fn) { fn(notes.slice()); });

	window.setTimeout(function () { dismiss(note.id); }, LINGER);
	return note.id;
}

/**
 * @param {number} id
 * @returns {void}
 */
export function dismiss(id) {
	const at = notes.findIndex(function (note) { return note.id === id; });
	if (at < 0)
		return;
	notes.splice(at, 1);
	listeners.forEach(function (fn) { fn(notes.slice()); });
}

/* All of them, in the layer they are drawn in.

   WHAT THAT LAYER DOES TO A PRESS, which lives in app/css/shell.css under .toasts and
   .toast. It is fixed to a corner of every screen for as long as a message stands, and
   that corner is somebody's next press, so:

   the layer takes no pointer at all, and each message takes its own back. A press
   anywhere the layer covers and a message does not reaches whatever is underneath;

   and each message ends at its own text rather than being stretched to the width of
   the longest one beside it. Stretched, the empty part of a short message is the
   message and not the layer, so it goes on swallowing presses however the layer is
   drawn. On the narrow folding that empty part is nearly the width of the window.

   Measured on a handset width: the layer left seven free pixels over a list row twenty
   two pixels tall while the messages were stretched, and fifty six once they were
   not. */
/** @returns {Web.Drawn} */
export function Toasts() {
	const [shown, setShown] = useState(notes.slice());

	useEffect(function () {
		listeners.add(setShown);
		return function () { listeners.delete(setShown); };
	}, []);

	if (!shown.length)
		return null;

	return html`<div class="toasts" role="status" aria-live="polite">
		${shown.map(function (note) {
			return html`<div class=${note.kind === 'bad' ? 'toast bad' : 'toast'} key=${note.id}>
				<span>${note.message}</span>
			</div>`;
		})}
	</div>`;
}
