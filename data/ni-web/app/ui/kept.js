// What the viewer opened, under a key, since an automatic reload can draw its node
// anew and shut. A fold lasts the visit, a sheet until the address changes.
import { useState } from '../runtime.js';

/** @type {Set<string>} */
const folds = new Set();
/** @type {Map<string, { at: string, value: unknown }>} */
const sheets = new Map();

/** @returns {string} */
function here() {
	return typeof location === 'undefined' ? '' : location.pathname + location.search;
}

/**
 * What a <details> is drawn with to stay as the viewer left it.
 *
 * @param {string} key
 * @returns {{ open: boolean, onToggle: (event: Event) => void }}
 */
export function fold(key) {
	return {
		open: folds.has(key),
		onToggle: function (event) {
			if (/** @type {HTMLDetailsElement} */ (event.currentTarget).open)
				folds.add(key);
			else
				folds.delete(key);
		},
	};
}

/**
 * State that outlives the component holding it, until the address changes. An
 * empty key keeps nothing.
 *
 * @template T
 * @param {string} key
 * @param {T} initial
 * @returns {[T, (next: T) => void]}
 */
export function useKept(key, initial) {
	const [own, setOwn] = useState(initial);
	const redraw = useState(0)[1];
	if (key === '')
		return [own, setOwn];
	const held = sheets.get(key);
	return [held === undefined ? initial : /** @type {T} */ (held.value), function (next) {
		if (next === initial)
			sheets.delete(key);
		else
			sheets.set(key, { at: here(), value: next });
		redraw(function (n) { return n + 1; });
	}];
}

/**
 * @param {string} url the address now shown, as the router names it
 * @returns {void}
 */
export function leave(url) {
	sheets.forEach(function (held, key) {
		if (held.at !== url)
			sheets.delete(key);
	});
}
