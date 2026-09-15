// A stylesheet arrives with the screen that needs it.
//
// A module cannot ask for a style the way it asks for another module, and one link
// per screen in the document would be the one file every screen has to edit. So a
// screen names its own stylesheet and this attaches it once.
//
// What is returned settles when the browser has the bytes, so a screen that awaits
// it draws once and drawn right. A sheet that fails to load settles too.
/** @type {Map<string, Promise<string>>} */
const attached = new Map();

/**
 * @param {string} href
 * @returns {Promise<string>} settles when the browser has the bytes, or has
 *          decided it will not get them
 */
export function ensureCss(href) {
	const known = attached.get(href);
	if (known)
		return known;

	const waited = new Promise(function (settle) {
		const link = document.createElement('link');
		link.rel = 'stylesheet';
		link.href = href;
		link.addEventListener('load', function () { settle(href); });
		link.addEventListener('error', function () { settle(href); });
		document.head.appendChild(link);
	});

	attached.set(href, waited);
	return waited;
}
