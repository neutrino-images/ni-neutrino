// What this destination holds, stated whole.
//
// Every screen below is named with a literal, so what the page can reach is
// something a reader of this file knows without running it.
//
// TWO AND NOT THREE. The television picture was an entry of its own beside the
// keys, and a person driving the box from here could not see what they were
// driving. They are one screen; screens/now/screenshot.js is still the file that
// draws the picture, it is just no longer a destination.
/** @type {Web.NavArea} */
export default {
	id: 'now',
	text: 'nav.now',
	ic: '▦',
	items: [
		{ id: 'overview', text: 'nav.now.overview', load: () => import('./overview.js') },
		{ id: 'remote', text: 'nav.now.remote', was: ['screenshot'], load: () => import('./remote.js') }
	]
};
