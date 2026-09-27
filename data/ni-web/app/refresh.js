/* The one soft refresh. Nothing the page loaded is thrown away: the store forgets what
   it holds and asks again for everything being watched, and the event stream is closed
   and reopened rather than left to its own retry. */
import * as store from './store.js';
import * as events from './events.js';

/** @returns {void} */
export function refreshAll() {
	events.reopen();
	store.clear();
}
