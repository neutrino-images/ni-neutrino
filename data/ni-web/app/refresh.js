/* The one soft refresh. Nothing the page loaded is thrown away: the store asks again for
   everything being watched and keeps drawing what it holds until the answers land, and the
   event stream is closed and reopened rather than left to its own retry. */
import * as store from './store.js';
import * as events from './events.js';

/** @returns {void} */
export function refreshAll() {
	events.reopen();
	store.clear();
}
