// Naming a channel, and holding every channel there is.
//
// Both were being done out of one page of the listing, which answers at most
// five hundred and holds one mode. A box with a full satellite list has several
// thousand of each, so a timer on anything past that page had no name to draw
// and showed its bare identifier, and no timer could be written for one at all
// because the picker never offered it. Neither had a rule anybody could see
// from outside: what answered was whatever the listing happened to put first.
//
// So the two questions are asked apart, because they are not the same question.
// Naming one channel is a lookup of one channel and costs one small answer that
// the store then holds for every other row asking after the same one. Filling a
// picker needs all of them, and that is a walk of the pages.
import { html, useState, useEffect } from '../runtime.js';
import * as store from '../store.js';
/* What ends a walk of this route is written down once, beside the screen that
   first walked it. Reaching across to it rather than restating it here: the
   rule is the route's and not either caller's, and two copies of it drift. */
import { cursorOf } from '../screens/channels/list.paging.js';

// What one page of the walk asks for, which is what the route allows at most
// (src/httpd/ep/ep_channels.cpp, kMaxPage). Fewer pages is fewer round trips,
// and the walk holds no more at once than the answer it is joining.
const PAGE = 500;

/**
 * One channel's name, and its bare identifier until the box has answered.
 *
 * The identifier is what a row shows for a channel the box no longer has,
 * which is a timer written before the channel list was scanned again. It says
 * less than a name and it is the whole truth about that timer.
 *
 * @param {{ id: string, class?: string }} props
 * @returns {Web.Drawn}
 */
export function ChannelName(props) {
	const [shot, setShot] = useState(function () {
		return store.read('GET', '/api/v1/channels/{id}', { params: { id: props.id } });
	});

	useEffect(function () {
		return store.watch('GET', '/api/v1/channels/{id}', { params: { id: props.id } }, setShot);
	}, [props.id]);

	const one = shot.data;
	return html`<span class=${props.class || ''}>${(one && one.name) ? one.name : props.id}</span>`;
}

/**
 * Every channel of one mode, in the order the box numbers them.
 *
 * Walked page by page and answered as it fills, so a picker is usable from the
 * first page rather than after the last. done says the walk reached the end;
 * until it does, what is missing is at the end of the list and not scattered
 * through it, because the route answers in one order and pages it in that order.
 *
 * @param {'tv' | 'radio'} mode
 * @returns {{ items: Api.Channel[], done: boolean, failed: boolean }}
 */
export function useAllChannels(mode) {
	const [held, setHeld] = useState(/** @type {{ items: Api.Channel[], done: boolean, failed: boolean }} */
		({ items: [], done: false, failed: false }));

	useEffect(function () {
		/* The walk this effect owns. A mode changed while one is running leaves
		   it finishing against a list nobody is drawing, and its pages would
		   otherwise be joined onto the new mode's. */
		let mine = true;
		setHeld({ items: [], done: false, failed: false });

		/** @param {string | undefined} cursor @param {Api.Channel[]} sofar */
		function step(cursor, sofar) {
			/** @type {Record<string, unknown>} */
			const query = { mode: mode, limit: PAGE };
			if (cursor !== undefined)
				query.cursor = cursor;
			store.load('GET', '/api/v1/channels', { query: query }).then(function (page) {
				if (!mine)
					return;
				const items = sofar.concat(page.items || []);
				const next = cursorOf(page);
				if (next === '') {
					setHeld({ items: items, done: true, failed: false });
					return;
				}
				setHeld({ items: items, done: false, failed: false });
				step(next, items);
			}, function () {
				if (!mine)
					return;
				// What came back before the fault is still every channel up to
				// it, and a picker holding some of them beats one holding none.
				setHeld(function (was) {
					return { items: was.items, done: false, failed: true };
				});
			});
		}

		step(undefined, []);
		return function () { mine = false; };
	}, [mode]);

	return held;
}
