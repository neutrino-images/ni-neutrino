/* The bouquets the box holds, what is in them, and in which order. One screen.
 *
 * IT USED TO BE TWO. A list of bouquets, and an editor reached from it, so that
 * somebody who had found the bouquet still had to know which of two names to
 * press to see the channels in it.
 *
 * A BOUQUET IS ADDRESSED BY ITS NAME AND NEVER BY THE NUMBER IT IS LISTED WITH.
 * Every writing route takes the name, and the number a listing answers with is
 * the place it currently sits at, which the box moves the moment anything above
 * it is deleted, moved or added. So the selection is a name, renaming carries
 * it, and a name that has left the list is said out loud rather than quietly
 * left pointing at whatever took its place.
 *
 * THE WHOLE HALF IS READ BEFORE ANYTHING MAY BE SAVED. The listing is paged at
 * five hundred, so a half of two thousand arrives in four answers, and saving
 * what one answer carried would replace the half with its own first page and
 * delete the rest.
 *
 * AND THE BOX CHANGES UNDER THIS SCREEN. A bouquet edited at the television
 * says so on the stream, and what is on screen here is a draft nothing else
 * knows about. With nothing changed the draft is simply read again; with
 * something changed the person is told that it no longer matches the box.
 *
 * ONE SAVE FOR THE SCREEN AND NOT ONE PER BOUQUET. A channel dragged onto
 * another bouquet changes two of them, and two saves would be a moment in which
 * the channel is in both or in neither. What the order costs is a run of single
 * moves, because that is the only move the box has (drag.js, stepsFor).
 *
 * WHAT IS NOT DRAFTED. Making, renaming and deleting a bouquet, and its two
 * flags, still reach the box as they are pressed: a rename held back would leave
 * the save addressing a name the box does not have. They are refused while there
 * is a draft, and the sentence says so.
 */
import { html, useState, useEffect, useRef } from '../../runtime.js';
import { t } from '../../i18n.js';
import text from './bouquets.text.js';
import shell from '../../shell.text.js';
import { api } from '../../api.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import * as events from '../../events.js';
import { Button } from '../../ui/button.js';
import { Field } from '../../ui/field.js';
import { Switch } from '../../ui/switch.js';
import { List } from '../../ui/list.js';
import { Sheet } from '../../ui/sheet.js';
import { Dialog } from '../../ui/dialog.js';
import { State } from '../../ui/state.js';
import { Logo } from '../../ui/logo.js';
import { toast } from '../../ui/toast.js';
import {
	MAX_IN_HALF, MAX_NAME_BYTES, rowsOf, byName, moved, idsOf, sameOrder, holds,
	placeOfId, matching, nameFault, readValue, readChecked
} from './bouquets.model.js';
import { dragging, spotFromPoint, placed, stepsFor } from './drag.js';

export const css = '/app/screens/channels/bouquets.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'bouquets.lead'); }


const TOUCHES = ['/api/v1/bouquets', '/api/v1/channels'];

// The most one answer carries (src/httpd/ep/ep_channels.cpp, kMaxPage). Asked for
// in full rather than in the hundred that is the default, because a half of
// four thousand would otherwise be forty round trips.
const PAGE = 500;

/* As many pages as the ceiling above can possibly need, and one more so that
   reaching this is a fault and not the ordinary last page. A loop that asks
   for the next page until the box stops offering one is a loop that a cursor
   the box keeps answering would never leave. */
const MOST_PAGES = 16;

/* What a row of the pool costs in height. The window arithmetic there needs
   one number and there is none in the stylesheet a script can read, so it is
   stated here and the stylesheet is held to it. It holds a row of a finger's
   height with the gap around it, and it is the same at every width: a window
   arithmetic that was right at one width only is a list that scrolls past its
   own rows the moment somebody turns a telephone. */
const ROW = 48;

/**
 * One half of one bouquet as this screen holds it.
 *
 * @typedef {object} Half
 * @property {Api.Channel[]} base what the box last said
 * @property {Api.Channel[]} draft what is on screen
 * @property {boolean} whole whether every page of it arrived
 * @property {boolean} loading
 * @property {Api.Problem | null} failed
 */

/**
 * One whole list, page by page, and whether it really is whole.
 *
 * @param {{ bouquet?: number, mode: 'tv' | 'radio' }} what
 * @param {AbortSignal} signal
 * @returns {Promise<{ items: Api.Channel[], whole: boolean }>}
 */
async function readAll(what, signal) {
	/** @type {Api.Channel[]} */
	const items = [];
	let cursor = '';
	for (let page = 0; page < MOST_PAGES; page++) {
		const answer = await api('GET', '/api/v1/channels', {
			query: {
				bouquet: what.bouquet,
				mode: what.mode,
				limit: PAGE,
				// Left out rather than sent empty: an empty value is an absent
				// one to every parameter this server reads, so an empty cursor
				// would be answered with the first page again.
				cursor: cursor === '' ? undefined : cursor,
			},
			signal: signal,
		});
		for (const one of (answer && answer.items) ? answer.items : [])
			items.push(one);
		const next = (answer && typeof answer.next_cursor === 'string') ? answer.next_cursor : '';
		if (next === '')
			return { items: items, whole: true };
		cursor = next;
	}
	return { items: items, whole: false };
}

/**
 * The names of a listing, in the order the box holds them.
 *
 * @param {Api.Bouquet[]} rows
 * @returns {string[]}
 */
export function namesOf(rows) {
	return rows.map(function (row) { return row.name; });
}

/**
 * Whether two orders are the same names in the same sequence.
 *
 * @param {readonly string[]} a
 * @param {readonly string[]} b
 * @returns {boolean}
 */
export function sameNames(a, b) {
	if (a.length !== b.length)
		return false;
	for (let at = 0; at < a.length; at++) {
		if (a[at] !== b[at])
			return false;
	}
	return true;
}

/**
 * A list of channels rearranged by identifier, as a new list.
 *
 * The arrangement itself is done over identifiers in drag.js, where it is
 * checked without a box; this only puts the channels back beside them.
 *
 * @param {Api.Channel[]} list
 * @param {string} id
 * @param {string} target
 * @param {boolean} after
 * @param {Api.Channel | null} coming the channel where it is not in the list yet
 * @returns {Api.Channel[]}
 */
export function rearranged(list, id, target, after, coming) {
	const was = idsOf(list);
	const now = placed(was, id, target, after);
	if (now === was)
		return list;
	/** @type {Record<string, Api.Channel>} */
	const known = {};
	for (const one of list)
		known[one.id] = one;
	if (coming)
		known[coming.id] = coming;
	/** @type {Api.Channel[]} */
	const out = [];
	for (const key of now) {
		const one = known[key];
		if (one)
			out.push(one);
	}
	return out;
}

/**
 * @param {{ param?: string }} props
 * @returns {Web.Drawn}
 */
export default function Bouquets(props) {
	const [answer, setAnswer] = useState(store.read('GET', '/api/v1/bouquets'));
	const [chosen, setChosen] = useState(props.param || '');
	const [mode, setMode] = useState(/** @type {'tv' | 'radio'} */ ('tv'));

	/* The order somebody arranged, and nought while it is the box's own. Not
	   the listing itself: the listing is the store's and arrives again on
	   every event, and an arrangement written into it would be lost with it. */
	const [order, setOrder] = useState(/** @type {string[] | null} */ (null));
	const [halves, setHalves] = useState(/** @type {Record<string, Half>} */ ({}));

	const [pool, setPool] = useState(/** @type {Api.Channel[]} */ ([]));
	const [poolWhole, setPoolWhole] = useState(false);
	const [filter, setFilter] = useState('');
	const [poolOpen, setPoolOpen] = useState(false);

	const [mark, setMark] = useState('');
	const [grabbed, setGrabbed] = useState(false);
	const [said, setSaid] = useState('');
	const [carry, setCarry] = useState(/** @type {import('./drag.js').Carry | null} */ (null));

	const [problem, setProblem] = useState(/** @type {Api.Problem | null} */ (null));
	const [busy, setBusy] = useState(false);
	const [saving, setSaving] = useState(false);
	const [stale, setStale] = useState(false);
	const [again, setAgain] = useState(0);

	const [asking, setAsking] = useState('');
	const [typed, setTyped] = useState('');
	/* Whether the name has been argued about yet. A field somebody has not
	   typed in is not a field with a mistake in it, and an empty box under a
	   red sentence the moment it opens is a page telling somebody off for
	   opening it. */
	const [argued, setArgued] = useState(false);
	/* When the listing this screen is holding stopped being old enough to
	   judge the selection by. Renaming moves the selection to a name the
	   listing in hand does not carry yet, and for the moment between the two
	   the selection looks like one somebody deleted at the television. */
	const settled = useRef(0);

	/* Read while an event is being handled, which is a moment in which the
	   values a draw closed over are the ones of the draw that registered the
	   handler. What the handler needs is what is true now. */
	const now = useRef({ dirty: false, saving: false });

	const rows = rowsOf(answer.data);
	const boxOrder = namesOf(rows);
	const shownOrder = order || boxOrder;
	const one = byName(rows, chosen);
	const mine = halves[chosen] || null;
	const members = mine ? mine.draft : [];

	let dirty = order !== null && !sameNames(order, boxOrder);
	for (const name of Object.keys(halves)) {
		const half = halves[name];
		if (half && !sameOrder(half.base, half.draft))
			dirty = true;
	}

	useEffect(function () {
		return store.watch('GET', '/api/v1/bouquets', null, setAnswer);
	}, []);

	useEffect(function () {
		now.current.dirty = dirty;
		now.current.saving = saving;
	});

	// An address carries the selection, so a link to one bouquet opens on it.
	useEffect(function () {
		if (props.param)
			setChosen(props.param);
	}, [props.param]);

	// Nothing chosen and the box has bouquets: open on the first, so that the
	// screen is about something the moment it is reached without a name.
	useEffect(function () {
		if (chosen === '' && rows.length > 0) {
			const first = rows[0];
			if (first)
				setChosen(first.name);
		}
	}, [chosen, rows.length]);

	/* The one thing an event cannot fix by itself. The store asks the listing
	   again whenever the box says the bouquets changed, and what comes back
	   may simply not carry the name this screen is holding. */
	useEffect(function () {
		if (chosen === '' || busy || saving || answer.state !== store.READY)
			return;
		if (answer.at < settled.current)
			return;
		if (byName(rows, chosen) === null) {
			setChosen('');
			setProblem(null);
			toast(t(text, 'bouquets.gone'), 'bad');
		}
	}, [answer.at, answer.state, chosen, busy, saving]);

	// Everything the box holds of that kind, for adding. Its own walk, because
	// this one is allowed to arrive a page at a time: nothing is saved out of
	// it.
	useEffect(function () {
		const control = new AbortController();
		setPool([]);
		setPoolWhole(false);
		readAll({ mode: mode }, control.signal).then(function (got) {
			setPool(got.items);
			setPoolWhole(got.whole);
		}, function () {
			// The halves are the screen; a pool that did not arrive costs the
			// ability to add and nothing else.
		});
		return function () { control.abort(); };
	}, [mode, again]);

	// Everything drafted is about one half, so a change of half starts again.
	useEffect(function () {
		setHalves({});
		setOrder(null);
		setMark('');
		setGrabbed(false);
	}, [mode, again]);

	/**
	 * The half of one bouquet, read if it is not in hand.
	 *
	 * Answers the half so a caller can go on with it, which is what a channel
	 * dropped on a bouquet nobody has opened needs: it cannot be added to a list
	 * this screen has not read, because saving that list would delete everything
	 * it did not read.
	 *
	 * @param {string} name
	 * @returns {Promise<Half>}
	 */
	function halfOf(name) {
		const have = halves[name];
		if (have && have.whole && !have.loading)
			return Promise.resolve(have);
		const bouquet = byName(rows, name);
		if (bouquet === null)
			return Promise.reject(null);

		setHalves(function (was) {
			const next = Object.assign({}, was);
			next[name] = {
				base: have ? have.base : [],
				draft: have ? have.draft : [],
				whole: false, loading: true, failed: null
			};
			return next;
		});

		return readAll({ bouquet: bouquet.id, mode: mode }, new AbortController().signal).then(function (got) {
			/** @type {Half} */
			const half = { base: got.items, draft: got.items, whole: got.whole, loading: false, failed: null };
			setHalves(function (was) {
				const next = Object.assign({}, was);
				/* What is on screen wins over what has just arrived only where
				   somebody has already changed it; otherwise a read that
				   crossed a drop would put the drop back. */
				const held = was[name];
				next[name] = (held && held.draft.length > 0 && !sameOrder(held.base, held.draft))
					? { base: got.items, draft: held.draft, whole: got.whole, loading: false, failed: null }
					: half;
				return next;
			});
			return half;
		}, function (failed) {
			const error = /** @type {{ problem?: Api.Problem, aborted?: boolean } | null} */ (failed);
			setHalves(function (was) {
				const next = Object.assign({}, was);
				// Held to nothing rather than to what was read before: a half
				// that could not be read is not one this screen may save over.
				next[name] = {
					base: [], draft: [], whole: false, loading: false,
					failed: (error && error.problem) ? error.problem : null
				};
				return next;
			});
			throw failed;
		});
	}

	// The half that is open, whenever the selection moves to one nobody has
	// read yet.
	useEffect(function () {
		if (chosen === '' || byName(rows, chosen) === null)
			return;
		if (halves[chosen])
			return;
		halfOf(chosen).catch(function () {
			// The half carries its own refusal and the pane draws it.
		});
	}, [chosen, rows.length, mode, again]);

	/* The box changed its bouquets while this screen was open. Registered once
	   and reading the two values it needs off a reference, because a handler
	   that closed over them would be answering with the state of the draw that
	   registered it. */
	useEffect(function () {
		return events.on('bouquets-changed', function () {
			if (now.current.saving)
				return;
			if (now.current.dirty) {
				setStale(true);
				return;
			}
			setAgain(function (n) { return n + 1; });
		});
	}, []);

	/**
	 * @param {string} sentence
	 * @returns {void}
	 */
	function say(sentence) {
		setSaid(sentence);
	}

	/**
	 * @param {string} name
	 * @param {(half: Half) => Api.Channel[]} change
	 * @returns {void}
	 */
	function redraft(name, change) {
		setHalves(function (was) {
			const half = was[name];
			if (!half)
				return was;
			const draft = change(half);
			if (draft === half.draft)
				return was;
			const next = Object.assign({}, was);
			next[name] = { base: half.base, draft: draft, whole: half.whole, loading: half.loading, failed: half.failed };
			return next;
		});
	}

	/**
	 * @param {Api.Channel[]} list
	 * @param {string} id
	 * @returns {Api.Channel | null}
	 */
	function byId(list, id) {
		for (const channel of list) {
			if (channel.id === id)
				return channel;
		}
		return null;
	}

	/**
	 * @param {number} delta
	 * @returns {void}
	 */
	function shift(delta) {
		if (!mine || mark === '')
			return;
		const from = placeOfId(mine.draft, mark);
		const next = moved(mine.draft, from, from + delta);
		if (next === mine.draft)
			return;
		const after = byId(next, mark);
		redraft(chosen, function () { return next; });
		say(t(text, 'editor.moved', {
			name: after ? after.name : mark,
			pos: from + delta + 1,
			count: next.length,
		}));
	}

	/**
	 * @param {Api.Channel} channel
	 * @returns {void}
	 */
	function add(channel) {
		if (!mine || holds(mine.draft, channel.id))
			return;
		redraft(chosen, function (half) { return half.draft.concat([channel]); });
		setMark(channel.id);
		setPoolOpen(false);
		say(t(text, 'editor.added', { name: channel.name }));
	}

	/**
	 * @param {string} id
	 * @returns {void}
	 */
	function remove(id) {
		if (!mine)
			return;
		const gone = byId(mine.draft, id);
		redraft(chosen, function (half) {
			/** @type {Api.Channel[]} */
			const next = [];
			for (const channel of half.draft) {
				if (channel.id !== id)
					next.push(channel);
			}
			return next;
		});
		setMark('');
		setGrabbed(false);
		say(t(text, 'editor.removed', { name: gone ? gone.name : id }));
	}

	/**
	 * One channel put beside another, wherever it came from.
	 *
	 * @param {string} id
	 * @param {string} target
	 * @param {boolean} after
	 * @returns {void}
	 */
	function place(id, target, after) {
		if (!mine || id === target)
			return;
		const coming = byId(mine.draft, id) || byId(pool, id);
		redraft(chosen, function (half) {
			return rearranged(half.draft, id, target, after, coming);
		});
		setMark(id);
		setGrabbed(false);
		say(t(text, 'editor.dropped', {
			name: coming ? coming.name : id,
			pos: placeOfId(rearranged(mine.draft, id, target, after, coming), id) + 1
		}));
	}

	/**
	 * A channel handed to a bouquet that is not the one on screen.
	 *
	 * The receiving half has to be read whole first: a channel appended to a
	 * half nobody read is a half that would be saved as that one channel.
	 *
	 * @param {string} id
	 * @param {string} to
	 * @param {boolean} take whether it also leaves the bouquet on screen
	 * @returns {void}
	 */
	function handOver(id, to, take) {
		const coming = (mine ? byId(mine.draft, id) : null) || byId(pool, id);
		if (!coming)
			return;
		say(t(text, 'editor.handing', { name: coming.name, bouquet: to }));
		halfOf(to).then(function (half) {
			if (holds(half.draft, id)) {
				say(t(text, 'editor.already.in', { name: coming.name, bouquet: to }));
				return;
			}
			redraft(to, function (was) { return was.draft.concat([coming]); });
			if (take)
				remove(id);
			say(t(text, 'editor.handed', { name: coming.name, bouquet: to }));
		}, function () {
			say(t(text, 'editor.handfailed', { bouquet: to }));
			toast(t(text, 'editor.handfailed', { bouquet: to }), 'bad');
		});
	}

	/**
	 * @param {string} name
	 * @param {number} delta
	 * @returns {void}
	 */
	function shiftBouquet(name, delta) {
		const from = shownOrder.indexOf(name);
		const next = moved(shownOrder, from, from + delta);
		if (next === shownOrder)
			return;
		setOrder(next);
		say(t(text, 'bouquets.moved', { name: name, pos: from + delta + 1, count: next.length }));
	}

	/**
	 * The keys, on the list and not on each row, so that a row that has just
	 * moved does not have to be the one that heard the key.
	 *
	 * @param {KeyboardEvent} event
	 * @returns {void}
	 */
	function onKeys(event) {
		if (mark === '' || !mine)
			return;
		const key = event.key;
		if (key === 'Enter' || key === ' ') {
			event.preventDefault();
			const taken = !grabbed;
			setGrabbed(taken);
			const channel = byId(mine.draft, mark);
			say(taken
				? t(text, 'editor.grabbed', { name: channel ? channel.name : mark })
				: t(text, 'editor.dropped', { name: channel ? channel.name : mark, pos: placeOfId(mine.draft, mark) + 1 }));
			return;
		}
		if (key === 'Escape' && grabbed) {
			event.preventDefault();
			setGrabbed(false);
			return;
		}
		const up = key === 'ArrowUp';
		const down = key === 'ArrowDown';
		if (!up && !down)
			return;
		// A plain arrow belongs to the page while nothing is held: it scrolls,
		// and taking that away from somebody who only tabbed through would be
		// taking a key nobody asked this list for.
		if (!grabbed && !event.altKey)
			return;
		event.preventDefault();
		shift(up ? -1 : 1);
	}

	/* THE GESTURE. The machine is held across draws in a reference and what it
	   answers is mirrored into the state, because the label that follows the
	   pointer and the line that says where the row would land are drawn and
	   not written into the document by hand. */
	const drag = useRef(dragging(spotFromPoint));

	/**
	 * @param {import('./drag.js').Grip} grip
	 * @param {PointerEvent} event
	 * @returns {void}
	 */
	function grab(grip, event) {
		const handle = /** @type {Element & { setPointerCapture?: (id: number) => void }} */ (event.currentTarget);
		// Everything after this reaches the handle whatever it is over, which
		// is what makes one gesture out of a press and everything after it.
		if (handle && typeof handle.setPointerCapture === 'function')
			handle.setPointerCapture(event.pointerId);
		event.preventDefault();
		setCarry(drag.current.start(grip, event));
	}

	/**
	 * @param {PointerEvent} event
	 * @returns {void}
	 */
	function slide(event) {
		if (!drag.current.held())
			return;
		event.preventDefault();
		setCarry(drag.current.to(event, chosen));
	}

	/**
	 * @returns {void}
	 */
	function letGo() {
		const done = drag.current.drop();
		setCarry(null);
		if (!done)
			return;
		if (done.aim.zone === 'bouquets') {
			const next = placed(shownOrder, done.grip.id, done.aim.id, done.aim.after);
			if (next !== shownOrder) {
				setOrder(next.slice());
				say(t(text, 'bouquets.moved', {
					name: done.grip.id, pos: next.indexOf(done.grip.id) + 1, count: next.length
				}));
			}
			return;
		}
		if (done.aim.zone === 'handover') {
			handOver(done.grip.id, done.aim.id, done.grip.kind === 'member');
			return;
		}
		place(done.grip.id, done.aim.id, done.aim.after);
	}

	/**
	 * @returns {void}
	 */
	function giveUp() {
		drag.current.cancel();
		setCarry(null);
	}

	/**
	 * Which of the two lines a row is drawn with while something is over it.
	 *
	 * @param {'bouquets' | 'members'} zone
	 * @param {string} id
	 * @returns {string}
	 */
	function dropMark(zone, id) {
		const aim = carry ? carry.aim : null;
		if (!aim || aim.id !== id)
			return '';
		if (aim.zone === 'handover' && zone === 'bouquets')
			return ' into';
		if (aim.zone !== zone)
			return '';
		// A list with no row in it is aimed at whole, so there is no row to
		// draw the line above or below.
		if (id === '')
			return ' into';
		return aim.after ? ' below' : ' above';
	}

	/* ------------------------------------------------- what reaches the box */

	/**
	 * One write that is not part of the arrangement, and everything that goes
	 * with one.
	 *
	 * @param {() => Promise<unknown>} call
	 * @param {string} sentence
	 * @param {boolean} raise
	 * @returns {void}
	 */
	function change(call, sentence, raise) {
		session.requireWrite().then(function () {
			setBusy(true);
			setProblem(null);
			settled.current = Date.now();
			call().then(function () {
				setBusy(false);
				say(sentence);
				if (raise)
					toast(sentence, '');
				/* Asked again rather than believed, because the answer already
				   says the box carries the change: each of these routes
				   answers 204 only once the channel stack has taken it,
				   written the list out and read it back (src/coreapi/
				   channels.cpp). What is asked for is the whole listing and
				   not the one row: a move renumbers everything behind it. */
				store.reload('GET', '/api/v1/bouquets').catch(function () {});
			}, function (failed) {
				const error = /** @type {{ problem?: Api.Problem } | null} */ (failed);
				setBusy(false);
				setProblem((error && error.problem) ? error.problem : null);
			});
		}, function () {});
	}

	/**
	 * @template T
	 * @param {T} more
	 * @returns {T & { touches: readonly string[] }}
	 */
	function opts(more) {
		return Object.assign({ touches: TOUCHES }, more);
	}

	function makeBouquet() {
		setArgued(true);
		if (nameFault(typed, rows, '') !== '')
			return;
		const name = typed.trim();
		setAsking('');
		change(function () {
			return store.write('POST', '/api/v1/bouquets', opts({ body: { name: name } }));
		}, t(text, 'bouquets.done.new', { name: name }), true);
	}

	function renameBouquet() {
		setArgued(true);
		if (nameFault(typed, rows, chosen) !== '' || one === null)
			return;
		const name = typed.trim();
		const was = one.name;
		setAsking('');
		// The key moves with the name, so the selection has to move with it or
		// the next button on this screen would address a bouquet that is gone.
		setChosen(name);
		change(function () {
			return store.write('PUT', '/api/v1/bouquets/{bouquet}/name',
				opts({ params: { bouquet: was }, body: { name: name } }));
		}, t(text, 'bouquets.done.rename', { name: name }), false);
	}

	function removeBouquet() {
		if (one === null)
			return;
		const name = one.name;
		setAsking('');
		setChosen('');
		change(function () {
			return store.write('DELETE', '/api/v1/bouquets/{bouquet}', opts({ params: { bouquet: name } }));
		}, t(text, 'bouquets.done.delete', { name: name }), true);
	}

	/**
	 * @param {'hidden' | 'locked'} which
	 * @param {boolean} on
	 * @returns {void}
	 */
	function setFlag(which, on) {
		if (one === null)
			return;
		const name = one.name;
		change(function () {
			return which === 'hidden'
				? store.write('PUT', '/api/v1/bouquets/{bouquet}/hidden',
					opts({ params: { bouquet: name }, body: { on: on } }))
				: store.write('PUT', '/api/v1/bouquets/{bouquet}/locked',
					opts({ params: { bouquet: name }, body: { on: on } }));
		}, t(text, 'bouquets.done.flag'), false);
	}

	/**
	 * Everything the arrangement changed, in one go.
	 *
	 * The halves first and the order after them: a membership write addresses a
	 * bouquet by name and does not care where it sits, and the single moves that
	 * carry the order renumber everything they pass. Every step waits for the one
	 * before it, because the box answers each only once it has written the list
	 * out, and two in flight would be two writers of one file.
	 *
	 * @returns {void}
	 */
	function save() {
		/** @type {Array<{ name: string, ids: string[] }>} */
		const writes = [];
		for (const name of Object.keys(halves)) {
			const half = halves[name];
			if (half && !sameOrder(half.base, half.draft))
				writes.push({ name: name, ids: idsOf(half.draft) });
		}
		const steps = order === null ? [] : stepsFor(boxOrder, order);
		if (writes.length === 0 && steps.length === 0)
			return;

		session.requireWrite().then(function () {
			setSaving(true);
			setProblem(null);
			settled.current = Date.now();

			/** @type {Promise<unknown>} */
			let chain = Promise.resolve();
			for (const write of writes) {
				chain = chain.then(function () {
					return store.write('PUT', '/api/v1/bouquets/{bouquet}/channels', {
						params: { bouquet: write.name },
						query: { mode: mode },
						/* The body of this one route is the list itself and
						   not a member of an object, so it is written out
						   here: everything else this page sends is a flat
						   object and the client spells that one for it. */
						body: JSON.stringify(write.ids),
						touches: TOUCHES,
					});
				});
			}
			for (const step of steps) {
				chain = chain.then(function () {
					return store.write('PUT', '/api/v1/bouquets/{bouquet}/position',
						{ params: { bouquet: step.name }, body: { direction: step.direction }, touches: TOUCHES });
				});
			}

			chain.then(function () {
				setSaving(false);
				setStale(false);
				setOrder(null);
				toast(t(text, 'editor.saved'), '');
				/* Read back, and the reading is worth something: the answer is
				   204 only once the channel stack has taken the list, written
				   it out and found it there again. */
				setAgain(function (n) { return n + 1; });
				store.reload('GET', '/api/v1/bouquets').catch(function () {});
			}, function (failed) {
				const error = /** @type {{ problem?: Api.Problem } | null} */ (failed);
				setSaving(false);
				/* Left as it is rather than put back: some of the run may have
				   reached the box, and what is on screen is now the only place
				   the rest of it exists. */
				setProblem((error && error.problem) ? error.problem : null);
			});
		}, function () {});
	}

	function revert() {
		setHalves(function (was) {
			/** @type {Record<string, Half>} */
			const next = {};
			for (const name of Object.keys(was)) {
				const half = was[name];
				if (half)
					next[name] = { base: half.base, draft: half.base, whole: half.whole, loading: half.loading, failed: half.failed };
			}
			return next;
		});
		setOrder(null);
		setMark('');
		setGrabbed(false);
		setStale(false);
	}

	/* ------------------------------------------------------- what is drawn */

	const tooMany = members.length > MAX_IN_HALF;
	/** @type {string[]} */
	const unread = [];
	for (const name of Object.keys(halves)) {
		const half = halves[name];
		if (half && !sameOrder(half.base, half.draft) && !half.whole)
			unread.push(name);
	}

	/**
	 * @returns {string | null}
	 */
	function whyNotSave() {
		if (tooMany)
			return t(text, 'editor.toomany', { max: MAX_IN_HALF, count: members.length });
		/* A half that did not come back whole is one this screen has only part
		   of, which for the reader is the same refusal as one over the
		   ceiling: what is on screen is not what may be sent. */
		if (unread.length > 0)
			return t(text, 'editor.partial', { bouquet: unread.join(', ') });
		return null;
	}

	/**
	 * @returns {string | null}
	 */
	function whyNotChange() {
		return dirty ? t(text, 'editor.switchlock') : null;
	}

	const fault = (asking === '' || !argued) ? '' : nameFault(typed, rows, asking === 'rename' ? chosen : '');
	const shownPool = matching(pool, filter);

	/**
	 * @param {Api.Bouquet | null} bouquet
	 * @param {string} name
	 * @returns {Web.Drawn}
	 */
	function bouquetRow(bouquet, name) {
		const half = halves[name];
		const count = half ? half.draft.length : (bouquet ? (mode === 'tv' ? bouquet.tv_count : bouquet.radio_count) : 0);
		const at = shownOrder.indexOf(name);
		const picked = name === chosen;
		return html`<li
			key=${name}
			class=${'brow' + (picked ? ' picked' : '') + dropMark('bouquets', name)}
			data-drop-zone="bouquets"
			data-drop-id=${name}>
			<button
				type="button"
				class="grip"
				aria-label=${t(text, 'bouquets.grip', { name: name })}
				onPointerDown=${function (/** @type {PointerEvent} */ e) { grab({ kind: 'bouquet', id: name, label: name }, e); }}
				onPointerMove=${slide}
				onPointerUp=${letGo}
				onPointerCancel=${giveUp}>⠿</button>
			<button
				type="button"
				class="bq-name"
				data-name=${name}
				aria-pressed=${picked ? 'true' : 'false'}
				title=${t(text, 'bouquets.pick', { name: name })}
				onClick=${function () { setChosen(name); setProblem(null); }}>${name}</button>
			${bouquet && bouquet.hidden ? html`<span class="bq-flag">${t(text, 'bouquets.mark.hidden')}</span>` : null}
			${bouquet && bouquet.locked ? html`<span class="bq-flag warn">${t(text, 'bouquets.mark.locked')}</span>` : null}
			<span class="bq-count mono">${count}</span>
			${picked ? html`<button
				type="button" class="bq-mini"
				disabled=${at <= 0}
				aria-label=${t(text, 'bouquets.up')}
				title=${t(text, 'bouquets.up')}
				onClick=${function () { shiftBouquet(name, -1); }}>▲</button>` : null}
			${picked ? html`<button
				type="button" class="bq-mini"
				disabled=${at < 0 || at >= shownOrder.length - 1}
				aria-label=${t(text, 'bouquets.down')}
				title=${t(text, 'bouquets.down')}
				onClick=${function () { shiftBouquet(name, 1); }}>▼</button>` : null}
		</li>`;
	}

	/**
	 * @param {Api.Channel} channel
	 * @param {number} at
	 * @returns {Web.Drawn}
	 */
	function memberRow(channel, at) {
		const marked = channel.id === mark;
		return html`<li
			key=${channel.id}
			class=${'brow' + (marked ? (grabbed ? ' marked grabbed' : ' marked') : '') + dropMark('members', channel.id)}
			data-drop-zone="members"
			data-drop-id=${channel.id}>
			<button
				type="button"
				class="grip"
				aria-label=${t(text, 'editor.grip', { name: channel.name })}
				onPointerDown=${function (/** @type {PointerEvent} */ e) { grab({ kind: 'member', id: channel.id, label: channel.name }, e); }}
				onPointerMove=${slide}
				onPointerUp=${letGo}
				onPointerCancel=${giveUp}>⠿</button>
			<span class="bq-pos mono">${at + 1}</span>
			<${Logo} channel=${channel} size="sm" />
			<button
				type="button"
				class="bq-name"
				data-place=${String(at + 1)}
				aria-pressed=${marked ? 'true' : 'false'}
				title=${t(text, 'editor.place', { pos: at + 1, count: members.length })}
				onClick=${function () { setMark(channel.id); setGrabbed(false); }}>${channel.name}</button>
			${marked ? html`<button type="button" class="bq-mini"
				aria-pressed=${grabbed ? 'true' : 'false'}
				onClick=${function () { setGrabbed(!grabbed); }}>${t(text, grabbed ? 'editor.drop' : 'editor.grab')}</button>` : null}
			${marked ? html`<button type="button" class="bq-mini"
				disabled=${at === 0}
				aria-label=${t(text, 'editor.up')} title=${t(text, 'editor.up')}
				onClick=${function () { shift(-1); }}>▲</button>` : null}
			${marked ? html`<button type="button" class="bq-mini"
				disabled=${at === members.length - 1}
				aria-label=${t(text, 'editor.down')} title=${t(text, 'editor.down')}
				onClick=${function () { shift(1); }}>▼</button>` : null}
			${marked ? html`<button type="button" class="bq-mini"
				aria-label=${t(text, 'editor.remove.one', { name: channel.name })}
				onClick=${function () { remove(channel.id); }}>✕</button>` : null}
		</li>`;
	}

	/* THE FILTER STANDS IN THE POOL'S HEADING ROW AND NOT IN FRONT OF ITS LIST.
	   It filters what is loaded and nothing else, so it is not a line over all
	   three columns. But a label, a box and a sentence in front of one of three
	   lists put that list ninety six pixels below its two neighbours, and three
	   lists that begin at three heights read as three unrelated things. So it is
	   one row beside the heading, and the sentence goes under the list it is
	   about. */
	const poolFilter = html`<${Field}
		id="bq-filter"
		label=${t(text, 'editor.filter')}
		describedBy="bq-pool-note"
		value=${filter}
		onInput=${function (/** @type {Event} */ event) { setFilter(readValue(event)); }} />`;

	const poolPane = html`<div class="bq-pool">
		${shownPool.length === 0
			? html`<p class="note">${t(text, pool.length === 0 ? 'editor.nopool' : 'editor.nomatch')}</p>`
			: html`<div class="bq-scroll">
				<${List}
					rows=${shownPool}
					rowHeight=${ROW}
					hasMore=${false}
					rowKey=${function (/** @type {Api.Channel} */ channel) { return channel.id; }}
					row=${function (/** @type {Api.Channel} */ channel) {
						const inside = mine !== null && holds(mine.draft, channel.id);
						return html`<span class=${inside ? 'brow inside' : 'brow'}>
							<button
								type="button"
								class="grip"
								aria-label=${t(text, 'editor.grip', { name: channel.name })}
								onPointerDown=${function (/** @type {PointerEvent} */ e) { grab({ kind: 'pool', id: channel.id, label: channel.name }, e); }}
								onPointerMove=${slide}
								onPointerUp=${letGo}
								onPointerCancel=${giveUp}>⠿</button>
							<${Logo} channel=${channel} size="sm" />
							<span class="bq-name">${channel.name}</span>
							${inside
								? html`<span class="bq-flag">${t(text, 'editor.already')}</span>`
								: html`<button type="button" class="bq-mini"
									disabled=${mine === null}
									title=${t(text, 'editor.add.one', { name: channel.name })}
									aria-label=${t(text, 'editor.add.one', { name: channel.name })}
									onClick=${function () { add(channel); }}>+</button>`}
						</span>`;
					}} />
			</div>`}
		<p class="bq-foot">${t(text, 'editor.loaded', { count: pool.length })}${poolWhole ? '' : ' ...'}${html` · <span id="bq-pool-note">${t(text, 'editor.filter.hint')}</span>`}</p>
	</div>`;

	return html`<section class="bq">
		<p class="sr" role="status" aria-live="polite">${said}</p>

		${stale ? html`<div class="note bad bq-stale" role="alert">
			<h2>${t(text, 'editor.stale')}</h2>
			<p><${Button} onClick=${function () { setStale(false); revert(); setAgain(function (n) { return n + 1; }); }}>${t(text, 'editor.reload')}<//></p>
		</div>` : null}

		${problem ? html`<div class="note bad bq-problem" role="alert">
			<h2>${problem.title}</h2>
			${problem.detail ? html`<p>${problem.detail}</p>` : null}
		</div>` : null}

		<div class="bq-bar">
			<span class=${dirty ? 'chip warn bq-dirty' : 'chip bq-clean'}>${t(text, dirty ? 'editor.dirty' : 'editor.clean')}</span>
			<div class="bq-halves" role="group" aria-label=${t(text, 'editor.mode')}>
				${['tv', 'radio'].map(function (which) {
					return html`<button
						key=${which}
						type="button"
						class=${mode === which ? 'btn on' : 'btn'}
						aria-pressed=${mode === which ? 'true' : 'false'}
						onClick=${function () {
							if (dirty) { say(t(text, 'editor.switchlock')); return; }
							setMode(which === 'radio' ? 'radio' : 'tv');
						}}>${t(text, 'editor.mode.' + which)}</button>`;
				})}
			</div>
			<span class="bq-gap"></span>
			<${Button} class="bq-new" reason=${whyNotChange()}
				onClick=${function () { setTyped(''); setArgued(false); setAsking('new'); }}>${t(text, 'bouquets.new')}<//>
			<${Button} class="bq-revert" disabled=${saving || !dirty}
				onClick=${revert}>${t(text, 'editor.revert')}<//>
			<${Button} class="bq-save" primary=${true} disabled=${saving || !dirty}
				reason=${whyNotSave()}
				onClick=${save}>${t(text, 'editor.save')}<//>
		</div>

		<p class="bq-keys">${t(text, 'editor.keys')}</p>

		<div class="bq3">
			<div class="pane pane-bouquets">
				<h2>${t(text, 'bouquets.title')} <span class="bq-count mono">${rows.length}</span></h2>
				<${State}
					phase=${answer.phase}
					problem=${answer.error ? answer.error.problem : null}
					empty=${rows.length === 0 ? t(text, 'bouquets.empty') : false}
					onRetry=${function () { store.reload('GET', '/api/v1/bouquets').catch(function () {}); }}>
					<ul class="vrows">
						${shownOrder.map(function (name) { return bouquetRow(byName(rows, name), name); })}
					</ul>
				<//>
			</div>

			<div class="pane pane-members">
				<h2 class="named">${chosen === '' ? t(text, 'bouquets.nochoice') : chosen}
					${mine ? html` <span class="bq-count mono">${members.length}</span>` : null}</h2>
				${/* THE EMPTY HALF IS DRAWN HERE AND NOT HANDED TO State AS its
				     empty. A drop lands on whatever is under the pointer and
				     says which list it belongs to, and the rows were the only
				     things that said it, so a half with no row was a half
				     nothing could be dragged into at all. The words that say
				     it is empty are that target now. They carry it and the
				     list around them does not: put on the list, every gap
				     between the rows of a full one would become a place that
				     appends to the end, which is not what aiming between two
				     rows means. */
				one === null
					? html`<p class="note">${t(text, rows.length === 0 ? 'bouquets.empty' : 'bouquets.nochoice')}</p>`
					: html`<${State}
						phase=${mine && mine.loading ? 'first' : ''}
						problem=${mine && mine.failed ? mine.failed : null}
						onRetry=${function () { halfOf(chosen).catch(function () {}); }}>
						${mine && !mine.loading && members.length === 0
							? html`<p
								class=${'bq-nothing' + dropMark('members', '')}
								data-drop-zone="members"
								data-drop-id="">
								<span>${t(text, 'editor.nomembers')}</span>
								<span>${t(text, 'editor.drophere')}</span>
							</p>`
							: html`<ul class="vrows" onKeyDown=${onKeys}>
								${members.map(memberRow)}
							</ul>`}
					<//>`}
				<p class="bq-foot">
					<span class="bq-size">${members.length}</span> / ${MAX_IN_HALF}
					${members.length === 0 && mine && mine.whole ? html` · ${t(text, 'editor.willempty')}` : null}
				</p>
				<button type="button" class="btn bq-addpool"
					onClick=${function () { setPoolOpen(true); }}>${t(text, 'editor.pool.open')}</button>
				${one === null ? null : html`<div class="bq-flags">
					<${Switch}
						id="bq-hidden"
						label=${t(text, 'bouquets.hidden')}
						hint=${t(text, 'bouquets.hidden.hint')}
						checked=${one.hidden}
						disabled=${busy || dirty}
						onChange=${function (/** @type {Event} */ event) { setFlag('hidden', readChecked(event)); }} />
					<${Switch}
						id="bq-locked"
						label=${t(text, 'bouquets.locked')}
						hint=${t(text, 'bouquets.locked.hint')}
						checked=${one.locked}
						disabled=${busy || dirty}
						onChange=${function (/** @type {Event} */ event) { setFlag('locked', readChecked(event)); }} />
					<p class="bq-owns">
						<${Button} class="bq-rename" reason=${whyNotChange()}
							onClick=${function () { setTyped(one.name); setArgued(false); setAsking('rename'); }}>${t(text, 'bouquets.rename')}<//>
						<${Button} class="bq-delete" reason=${whyNotChange()}
							onClick=${function () { setAsking('delete'); }}>${t(text, 'bouquets.delete')}<//>
					</p>
				</div>`}
			</div>

			<div class="pane pane-pool">
				<div class="bq-poolhead">
					<h2>${t(text, 'editor.pool')}</h2>
					${poolOpen ? null : poolFilter}
				</div>
				${poolOpen ? null : poolPane}
			</div>
		</div>

		${carry && carry.moved ? html`<div class="flyer" style=${{ left: (carry.x + 14) + 'px', top: (carry.y - 14) + 'px' }}>
			<span aria-hidden="true">⠿</span> ${carry.grip.label}
		</div>` : null}

		<${Sheet} open=${poolOpen} onClose=${function () { setPoolOpen(false); }} label=${t(text, 'editor.pool')}>
			<h2>${t(text, 'editor.pool')}</h2>
			${poolOpen ? poolFilter : null}
			${poolOpen ? poolPane : null}
		<//>

		<${Dialog}
			open=${asking === 'new' || asking === 'rename'}
			title=${t(text, asking === 'rename' ? 'bouquets.rename.title' : 'bouquets.new.title')}
			confirmLabel=${t(text, asking === 'rename' ? 'bouquets.rename' : 'bouquets.new')}
			onCancel=${function () { setAsking(''); }}
			onConfirm=${function () { if (asking === 'rename') { renameBouquet(); } else { makeBouquet(); } }}>
			<${Field}
				id="bq-name"
				label=${t(text, asking === 'rename' ? 'bouquets.rename.label' : 'bouquets.new.label')}
				value=${typed}
				hint=${asking === 'rename' ? t(text, 'bouquets.rename.note') : null}
				error=${fault === '' ? null : t(text, fault, { max: MAX_NAME_BYTES })}
				onInput=${function (/** @type {Event} */ event) { setArgued(true); setTyped(readValue(event)); }} />
		<//>

		<${Dialog}
			open=${asking === 'delete'}
			title=${t(text, 'bouquets.delete.title')}
			confirmLabel=${t(text, 'bouquets.delete.yes')}
			cancelLabel=${t(shell, 'shell.cancel')}
			onCancel=${function () { setAsking(''); }}
			onConfirm=${removeBouquet}>
			<p class="bq-delete-body">${t(text, 'bouquets.delete.body')}</p>
		<//>
	</section>`;
}
