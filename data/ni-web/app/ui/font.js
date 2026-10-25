// Which face the writing is drawn in, which is this browser's business like the theme.
//
// Ubuntu is what the page ships with (fonts/ubuntu.css), and it stays the default.
// "system" is the face the device draws everything else in, for whoever reads that
// one more easily or simply prefers it. A face a page does not use is never fetched,
// so choosing the system one costs the box nothing either.
//
// Kept in this browser's localStorage and nowhere else, for the same reasons as the
// theme and the text size. Both halves are wrapped because a browser told to keep no
// site data throws. The default writes the attribute away, so the stylesheet's own
// --font applies (app/css/tokens.css).
//
// The same key is read by app/themeboot.js before the first paint; change both
// together.

const KEY = 'ni-web.font';

export const FONTS = ['ubuntu', 'system'];

/**
 * @returns {string} one of FONTS
 */
export function chosenFont() {
	let saved = '';
	try {
		saved = window.localStorage.getItem(KEY) || '';
	} catch (e) {
		// No site data: the default applies.
	}
	return FONTS.indexOf(saved) >= 0 ? saved : 'ubuntu';
}

/**
 * @param {string} id one of FONTS
 * @returns {void}
 */
export function chooseFont(id) {
	const root = document.documentElement;
	if (id === 'system')
		root.setAttribute('data-font', id);
	else
		root.removeAttribute('data-font');

	try {
		if (id === 'system')
			window.localStorage.setItem(KEY, id);
		else
			window.localStorage.removeItem(KEY);
	} catch (e) {
		// Applies until the page is loaded again.
	}
}
