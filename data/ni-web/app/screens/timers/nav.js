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
		{ id: 'list', text: 'nav.timers.list', load: () => import('./list.js') }
	]
};

/* HOW MANY TIMERS ARE SET, which is the number the bar draws beside this
   destination's name.

   Set means still to come or running now: the daemon's scheduled, announced
   and running (lib/timerdclient/timerdtypes.h). One that has finished or was
   stopped stays in the list until the daemon drops it, and counting it would
   draw a mark over the bar for a box that has nothing left to do.

   Here, beside the destination, and with nothing imported, so that the frame
   can ask it without loading the list and a check can run it with no
   browser. */
const HAS_FINISHED = 3;

/**
 * @param {readonly { state?: number }[] | null | undefined} items
 * @returns {number}
 */
export function setTimers(items) {
	let n = 0;
	for (const one of items || []) {
		if (one && !(typeof one.state === 'number' && one.state >= HAS_FINISHED))
			n++;
	}
	return n;
}
