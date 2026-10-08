// Light, dark, or whatever the system says, which is this browser's business.
//
// Kept in this browser's localStorage and nowhere else: not an account setting and
// not a key of the box, so a phone and a TV room may differ. Both halves are wrapped
// because a browser told to keep no site data throws. "system" is the default and
// writes the attribute away, so the stylesheet follows prefers-color-scheme.
//
// The same key is read by app/themeboot.js before the first paint; change both
// together.

const KEY = 'ni-web.theme';

export const THEMES = ['system', 'light', 'dark'];

/**
 * @returns {string} one of THEMES
 */
export function chosenTheme() {
	let saved = '';
	try {
		saved = window.localStorage.getItem(KEY) || '';
	} catch (e) {
		// No site data: the default applies.
	}
	return THEMES.indexOf(saved) >= 0 ? saved : 'system';
}

/**
 * @param {string} id one of THEMES
 * @returns {void}
 */
export function chooseTheme(id) {
	const root = document.documentElement;
	if (id === 'light' || id === 'dark')
		root.setAttribute('data-theme', id);
	else
		root.removeAttribute('data-theme');

	try {
		if (id === 'light' || id === 'dark')
			window.localStorage.setItem(KEY, id);
		else
			window.localStorage.removeItem(KEY);
	} catch (e) {
		// Applies until the page is loaded again.
	}
}
