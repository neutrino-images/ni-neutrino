// What this destination holds, stated whole.
//
// Making and changing a timer is not an entry of its own: it is the list's own
// screen reached with a third part in the address, the way a channel is
// reached under the channel list.
/** @type {Web.NavArea} */
export default {
	id: 'timers',
	text: 'nav.timers',
	/* A clock face out of the geometric shapes and not the stopwatch this had.
	   The stopwatch is in the emoji set and a system without an emoji font
	   draws an empty box for it, measured in this container; the seven beside
	   it are all out of blocks every font carries. */
	ic: '\u25f7',
	items: [
		{ id: 'list', text: 'nav.timers.list', load: () => import('./list.js') },
		{ id: 'recordings', text: 'nav.timers.recordings', load: () => import('./recordings.js') }
	]
};
