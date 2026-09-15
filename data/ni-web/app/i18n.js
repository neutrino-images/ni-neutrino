// A function, not a runtime.
//
// Every part of the page keeps its own words in a file beside it and hands one of
// them here with a key, so nothing has to load a catalogue of the whole interface to
// draw a button. There is no fallback chain, no plural rules and no date formatting.
//
// Placeholders are the one thing the old catalogue could not do, which is why
// sentences there were cut into pieces and reassembled in the wrong order for half
// the languages. A value here is a whole sentence with {name} in it.
/** @type {readonly string[]} */
const KNOWN = ['de', 'en'];

// German is the product's language and English lies beside it, so an unknown
// value anywhere in the chain lands on German rather than on nothing.
/** @type {string} */
let current = 'de';

/** @type {Set<(lang: string) => void>} */
const listeners = new Set();

/** @returns {string} */
export function language() {
	return current;
}

// Returns whether it changed, so a caller can spare itself a redraw.
//
// The document is told either way and not only when something changed, and what
// it is told for is the browser: that attribute is what hyphenates a word and
// picks a voice to read the page out in. Nothing on this page reads it back.
// Something did: the data layer kept a language of its own that nobody set and
// read the document as its fallback, so this write was the only bridge between
// the two switches.
/**
 * @param {string} lang
 * @returns {boolean} whether it changed
 */
export function setLanguage(lang) {
	const next = KNOWN.indexOf(lang) >= 0 ? lang : 'de';
	document.documentElement.lang = next;
	if (next === current)
		return false;

	current = next;
	listeners.forEach(function (fn) { fn(next); });
	return true;
}

/**
 * @param {(lang: string) => void} fn
 * @returns {() => void} stops the listening
 */
export function onLanguage(fn) {
	listeners.add(fn);
	return function () { listeners.delete(fn); };
}

// The box first, the browser after it, German last.
//
// What the box holds is the name of a locale file and not a code, because that is
// what the setting is. Anything but the two this page has words for falls through
// to the browser, so a box running in Italian does not force a German page on
// somebody whose browser asks for English.
/**
 * @param {string | null | undefined} boxValue what the box calls itself in,
 *        which is the name of a locale file and not a code
 * @param {readonly string[] | null | undefined} [browserLanguages]
 * @returns {string}
 */
export function pickLanguage(boxValue, browserLanguages) {
	const fromBox = codeFor(boxValue);
	if (fromBox)
		return fromBox;

	for (const one of browserLanguages || []) {
		const code = codeFor(one);
		if (code)
			return code;
	}

	return 'de';
}

/**
 * @param {string | null | undefined} value
 * @returns {string} the code, or empty for one this page has no words for
 */
function codeFor(value) {
	if (!value)
		return '';

	const v = String(value).toLowerCase();
	if (v === 'deutsch' || v === 'de' || v.indexOf('de-') === 0)
		return 'de';
	if (v === 'english' || v === 'en' || v.indexOf('en-') === 0)
		return 'en';
	return '';
}

// The key itself when nothing answers, because an empty label is a hole nobody
// reports and a key on screen is a bug report from the person who saw it.
/**
 * @param {Web.Catalog | null | undefined} catalog
 * @param {string} key
 * @param {Web.Values} [values]
 * @returns {string} the sentence, or the key itself where nothing answers
 */
export function t(catalog, key, values) {
	const chosen = (catalog && catalog[current]) || {};
	const fallback = (catalog && catalog['de']) || {};
	const text = chosen[key] !== undefined ? chosen[key] : fallback[key];
	if (text === undefined)
		return key;
	if (!values)
		return text;

	return String(text).replace(/\{([A-Za-z0-9_]+)\}/g, function (whole, name) {
		return values[name] === undefined ? whole : String(values[name]);
	});
}
