// What this destination holds, stated whole.
//
// THREE AND NOT FOUR. The editor was a fourth entry beside the bouquets, and a
// person reaching a bouquet had to know which of the two names to press to see
// the channels in it. They are one screen now: the entry is gone from here and
// so is the module.
/** @type {Web.NavArea} */
export default {
	id: 'channels',
	text: 'nav.channels',
	ic: '☰',
	items: [
		{ id: 'list', text: 'nav.channels.list', load: () => import('./list.js') },
		{ id: 'playback', text: 'nav.channels.playback', load: () => import('./playback.js') },
		{ id: 'bouquets', text: 'nav.channels.bouquets', load: () => import('./bouquets.js') }
	]
};
