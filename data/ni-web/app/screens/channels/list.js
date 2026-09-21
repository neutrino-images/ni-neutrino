/* The channel list.

   WHAT A ROW IS FOR. Somebody opens this to change the channel, so the name is
   the control that changes it. Not a button beside the name: the thing being
   pointed at is the channel, and a row where the name is inert and a button two
   columns over does the work is a row that has to be read before it can be used.
   The mark that starts playing in the browser is a different control, and it is
   never tinted on the running channel, because it does not mean "this one is
   on".

   WHAT IS ON, AND WHAT IS ON NEXT. The guide is asked for a window rather than
   for an instant, so one answer carries both the programme running and the one
   behind it, and the row says how much of the first is left and when the second
   starts.

   THE LISTS ARE LONG: the ones this box ships with hold 1238 and 2664 services.
   The answer is walked a page at a time and the walk ends where the box says it
   ends and not where a page happens to be short
   (screens/channels/list.paging.js). The price is stated rather than hidden: the
   guide is asked about every row that has been loaded and not only about the
   ones on screen, twenty channels to a request.

   THERE IS NO ROUTE THAT SEARCHES NAMES. The text filter therefore searches what
   has been walked so far, and says so beside the count, because a filter that
   quietly searched a hundred of two thousand channels would answer "no such
   channel" for a channel that is merely on a page nobody asked for. */

import { html, useState, useEffect, useRef, useCallback, route } from '../../runtime.js';
import { api } from '../../api.js';
import * as store from '../../store.js';
import * as events from '../../events.js';
import * as session from '../../session.js';
import { isAborted } from '../../problem.js';
import { t, language } from '../../i18n.js';
import { channelId, clock, sameChannel } from '../../fmt.js';
import { hrefFor } from '../../nav.js';
import { State } from '../../ui/state.js';
import { Button } from '../../ui/button.js';
import { Field } from '../../ui/field.js';
import { Select } from '../../ui/select.js';
import { Table } from '../../ui/table.js';
import { Logo } from '../../ui/logo.js';
import { Dot } from '../../ui/dot.js';
import { RowActions } from '../../ui/actions.js';
import { elapsedShare } from '../../ui/onair.js';
import { toast } from '../../ui/toast.js';
import text from './list.text.js';
import { fetchPlaylist } from './stream.js';
import { emptyWalk, beginPage, failPage, extend, wants, byName, isWeb } from './list.paging.js';

export const css = '/app/screens/channels/list.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'list.lead'); }


/* One hundred, which is what the route answers to a caller that names no limit
   (src/httpd/ep/ep_channels.cpp). Named here all the same, because the guide is
   asked about what a page brought and a page size this screen does not know is
   a cost it cannot reason about. */
const kPage = 100;

/* How many channels one ask of the guide covers. Twenty is the grid's own
   default, so this screen sends no limit at all and cannot name a ceiling the
   box would later refuse. */
const kGridChunk = 20;

/* How far ahead the guide is asked. Long enough that the programme after the
   one running is nearly always inside it, short enough that twenty channels of
   it is a small answer. A row that finds nothing behind the current programme
   says so rather than leaving the line empty. */
const kAheadSeconds = 4 * 3600;

// What is on now stops being what is on now. Nothing on the stream says the
// hour has turned, so the one thing here that is not driven by an event is
// driven by the clock.
const kNowMs = 60000;

/**
 * The selection, out of the one piece the address carries.
 *
 * Both halves in one segment because the router gives a screen one, and joined
 * with a hyphen and never a dot: a dot in the last segment is what tells the
 * server a mistyped file from an identifier, so an address carrying one is
 * answered 404 and not with this page (src/httpd/apppaths.cpp).
 *
 * @param {string} param
 * @returns {{ mode: 'tv' | 'radio', bouquet: number }}
 */
export function readSelection(param) {
	const parts = String(param || '').split('-');
	const mode = parts[0] === 'radio' ? 'radio' : 'tv';
	const asked = parts.length > 1 ? Number(parts[1]) : 0;
	// Counted from one by the box, so nought is this screen's word for "every
	// bouquet" and cannot be confused with a bouquet.
	const bouquet = Number.isFinite(asked) && asked > 0 ? Math.floor(asked) : 0;
	return { mode: mode, bouquet: bouquet };
}

/**
 * @param {'tv' | 'radio'} mode
 * @param {number} bouquet
 * @returns {string}
 */
export function writeSelection(mode, bouquet) {
	return bouquet > 0 ? mode + '-' + bouquet : mode;
}

/**
 * The name this screen holds a channel's guide under, which is the channel's own
 * identifier.
 *
 * Its epg_id would be the wrong one even though the guide answers on it: two
 * channels sharing one schedule share that field, so a map held under it folds
 * them into one entry and every row of such a pair would be drawn under whichever
 * of them was asked about first.
 *
 * @param {Api.Channel} channel
 * @returns {string}
 */
function guideRowKey(channel) {
	return channelId(channel.id);
}

/**
 * What a channel is showing at that moment and what it shows next, out of one
 * schedule.
 *
 * The window asked for is hours wide, so the answer holds several events and the
 * pair has to be picked out of them. A box answering more than one event over the
 * same instant is answered here rather than drawn twice.
 *
 * @param {Api.Event[]} list in start order, which is how the guide answers
 * @param {number} at
 * @returns {{ now: Api.Event | null, next: Api.Event | null }}
 */
export function eventsAround(list, at) {
	/** @type {Api.Event | null} */
	let running = null;
	/** @type {Api.Event | null} */
	let after = null;
	for (const event of list) {
		const ends = event.start + event.duration;
		if (running === null && event.start <= at && at < ends) {
			running = event;
			continue;
		}
		// The first that has not begun, whether or not one is running: a
		// channel between two programmes still has one coming.
		if (after === null && event.start > at)
			after = event;
	}
	return { now: running, next: after };
}

/**
 * @typedef {object} Showing what one row was told about its channel
 * @property {Api.Event | null} now
 * @property {Api.Event | null} next
 */

/**
 * @param {{ param: string, ctx: Web.Context, entry: Web.NavEntry }} props
 * @returns {Web.Drawn}
 */
export default function ChannelList(props) {
	const here = readSelection(props.param);
	const mode = here.mode;
	const bouquet = here.bouquet;

	const [walk, setWalk] = useState(emptyWalk());
	const [needle, setNeedle] = useState('');
	const [bouquets, setBouquets] = useState(/** @type {Api.Bouquet[]} */ ([]));
	const [bouquetsFailed, setBouquetsFailed] = useState(false);
	const [current, setCurrent] = useState(/** @type {Api.Channel | null} */ (null));
	// Bumped to walk the list again from its first page, which is what an
	// event that changed the list itself asks for.
	const [generation, setGeneration] = useState(0);
	/* Only ways to ask for another draw. What the guide answered is kept in
	   the map below, where the rows read it, so there is no second copy of it
	   in the state. */
	const setMinute = useState(0)[1];
	const setGuideAt = useState(0)[1];

	/* The walk is held in a reference beside the state because the answer to
	   one page names the page after it: the handler has to read what it just
	   added, and reading that out of the state would read what was there
	   before this render. */
	const held = useRef(emptyWalk());
	const running = useRef(/** @type {AbortController | null} */ (null));

	/* What the guide said about a channel, and an empty pair for a channel it
	   was asked about and said nothing for. The difference matters: without it
	   every draw would ask again about every channel with no schedule. */
	const guide = useRef(/** @type {Map<string, Showing>} */ (new Map()));
	const guideBusy = useRef(false);

	/**
	 * @param {import('./list.paging.js').Walk<Api.Channel>} next
	 */
	function apply(next) {
		held.current = next;
		setWalk(next);
	}

	const ask = useCallback(function () {
		const was = held.current;
		if (!wants(was)) {
			return;
		}
		const control = new AbortController();
		running.current = control;
		apply(beginPage(was));
		api('GET', '/api/v1/channels', {
			query: {
				mode: mode,
				// Left out rather than sent as nought: the route counts
				// bouquets from one and refuses everything below it, so a
				// value standing for "every bouquet" has to be no value.
				bouquet: bouquet > 0 ? bouquet : undefined,
				cursor: was.cursor !== '' ? was.cursor : undefined,
				limit: kPage,
			},
			signal: control.signal,
		}).then(function (page) {
			if (running.current !== control) {
				return;
			}
			running.current = null;
			apply(extend(held.current, page.items || [], page));
		}, function (error) {
			if (running.current !== control || isAborted(error)) {
				return;
			}
			running.current = null;
			apply(failPage(held.current, error));
		});
	}, [mode, bouquet]);

	// The walk begins again whenever what it is a walk over changes.
	useEffect(function () {
		if (running.current) {
			running.current.abort();
			running.current = null;
		}
		held.current = emptyWalk();
		setWalk(held.current);
		guide.current = new Map();
		setGuideAt(function (n) { return n + 1; });
		ask();
		return function () {
			if (running.current) {
				running.current.abort();
				running.current = null;
			}
		};
	}, [mode, bouquet, generation]);

	// What is playing, and the bouquets the filter offers. Both are one answer
	// each and both are corrected by the stream, so both are the store's.
	useEffect(function () {
		const stopCurrent = store.watch('GET', '/api/v1/channels/current', {}, function (seen) {
			/* A box playing nothing answers 404 here, which is an answer and
			   not a fault to draw: the list simply marks no row. */
			setCurrent(seen.state === store.READY ? seen.data : null);
		});
		const stopBouquets = store.watch('GET', '/api/v1/bouquets', {}, function (seen) {
			if (seen.state === store.READY && seen.data) {
				setBouquets(seen.data.items || []);
				setBouquetsFailed(false);
			} else if (seen.state === store.FAILED) {
				setBouquetsFailed(true);
			}
		});
		return function () {
			stopCurrent();
			stopBouquets();
		};
	}, []);

	/* WHERE THE LIST OPENS. The old web interface opened on the channel the box
	   is playing, inside the bouquet that holds it, and that is what somebody
	   coming to this screen is nearly always looking for. This one opened on
	   every bouquet at once, which on a full satellite list is thousands of rows
	   and the running channel somewhere among them.

	   Only when the address names no bouquet, so a link, a bookmark or the Back
	   button all keep naming what they named. Once per visit and not once per
	   answer: choosing "every bouquet" by hand writes the same address as naming
	   none, and a screen that jumped again on that would not let go of the wheel.

	   Which bouquet holds a channel is the box's answer and not this screen's to
	   work out: asking every bouquet for its members would be one request per
	   bouquet (src/httpd/ep/ep_channels.cpp, the holds parameter).

	   The address is replaced rather than pushed, so Back leaves the screen
	   instead of returning to the list nobody asked for. */
	const jumped = useRef(false);
	useEffect(function () {
		if (jumped.current || bouquet > 0 || current === null)
			return;
		jumped.current = true;
		const playing = current;
		const itsMode = (playing.kind === 'radio' || playing.kind === 'webradio') ? 'radio' : 'tv';
		store.load('GET', '/api/v1/bouquets', { query: { holds: playing.id } }).then(function (answer) {
			const holding = (answer && answer.items) || [];
			// A channel in no bouquet at all leaves the screen where it is,
			// which is every bouquet, and that is where it can be found. The
			// first is taken rather than a choice being made: the route answers
			// them in the box's own order, so the first is the one the box
			// would have opened on.
			const first = holding[0];
			if (!first)
				return;
			route(hrefFor('channels', 'list', writeSelection(itsMode, first.id)), true);
		}, function () {
			// The list is usable without the jump, and a screen that says so
			// would be saying it about something nobody asked for.
		});
	}, [current === null ? '' : String(current.id), bouquet]);

	/* And the row itself, once the page holding it has arrived. The table marks
	   the playing row for the frame's stylesheet, so that mark is what this
	   reaches for rather than a second way of saying which row it is. A bouquet
	   is smaller than one page nearly always, so the row is there; where it is
	   not, nothing scrolls and the marking still says where it is once the walk
	   reaches it. */
	const shown = useRef(false);
	useEffect(function () {
		if (shown.current || current === null || walk.items.length === 0)
			return;
		const row = document.querySelector('[data-on-air]');
		if (!row)
			return;
		shown.current = true;
		row.scrollIntoView({ block: 'center' });
	}, [current === null ? '' : String(current.id), walk.items.length]);

	/* The two events that change the list this screen is a walk over, rather
	   than one answer inside it. The store cannot help here: a walk is a
	   sequence of answers under a sequence of addresses, so what has to happen
	   is that the walk starts again. */
	useEffect(function () {
		const stopMode = events.on('mode', function () {
			setGeneration(function (n) { return n + 1; });
		});
		const stopBouquets = events.on('bouquets-changed', function () {
			setGeneration(function (n) { return n + 1; });
		});
		const stopGuide = events.on('epg-updated', function () {
			guide.current = new Map();
			setGuideAt(function (n) { return n + 1; });
		});
		return function () {
			stopMode();
			stopBouquets();
			stopGuide();
		};
	}, []);

	useEffect(function () {
		const tick = window.setInterval(function () {
			guide.current = new Map();
			setMinute(function (n) { return n + 1; });
		}, kNowMs);
		return function () { window.clearInterval(tick); };
	}, []);

	const rows = byName(walk.items, needle, language());

	/* What the guide says about the rows on screen, asked for after the draw and
	   only for the ones nothing is known about yet. One ask covers twenty of
	   them, and the next twenty go out when this one has landed, so a long list
	   fills in from the top rather than in one burst.

	   It cannot loop, because every identifier asked about is written into the
	   map whether the guide had anything for it or not. */
	useEffect(function () {
		if (guideBusy.current) {
			return;
		}
		/** @type {string[]} */
		const missing = [];
		for (const channel of rows) {
			const key = guideRowKey(channel);
			if (key !== '' && !guide.current.has(key) && missing.indexOf(key) === -1) {
				missing.push(key);
			}
			if (missing.length === kGridChunk) {
				break;
			}
		}
		if (missing.length === 0) {
			return;
		}

		const at = Math.floor(Date.now() / 1000);
		guideBusy.current = true;
		askGuide(missing, at, guide.current).then(function () {
			guideBusy.current = false;
			setGuideAt(function (n) { return n + 1; });
		}, function () {
			/* A guide that will not answer leaves the column empty. Every
			   identifier of the ask is marked as answered for anyway, so a
			   box without a guide is asked once per chunk and not once per
			   draw. */
			for (const key of missing) {
				if (!guide.current.has(key)) {
					guide.current.set(key, { now: null, next: null });
				}
			}
			guideBusy.current = false;
			setGuideAt(function (n) { return n + 1; });
		});
	});

	/**
	 * @param {'tv' | 'radio'} nextMode
	 * @param {number} nextBouquet
	 * @returns {void}
	 */
	function choose(nextMode, nextBouquet) {
		route(hrefFor('channels', 'list', writeSelection(nextMode, nextBouquet)));
	}

	/* Nothing on this screen says in advance that it may not be used. The sign
	   in is asked for when the control is touched, and the write goes on after
	   it; closing the sheet leaves the box as it was and says nothing, because
	   somebody who closed it knows what they did. */

	/**
	 * @param {Api.Channel} channel
	 */
	function zapTo(channel) {
		session.requireWrite().then(function () {
			store.write('POST', '/api/v1/zap', {
				/* What a zap makes stale, said by the caller because nothing
				   else could know it: the box is now on another channel, and
				   the guide answer for the running channel is about the one it
				   left. */
				touches: ['/api/v1/channels/current', '/api/v1/epg/current'],
				body: { channel_id: channel.id },
			}).then(function () {
				toast(t(text, 'list.zap.done', { name: channel.name }));
			}, function (error) {
				toast(error && error.problem ? error.problem.title : t(text, 'list.failed'), 'bad');
			});
		}, function () {});
	}

	function switchBox() {
		session.requireWrite().then(function () {
			store.write('POST', '/api/v1/mode', {
				touches: ['/api/v1/channels', '/api/v1/bouquets', '/api/v1/epg'],
				body: { mode: mode },
			}).then(function () {
				toast(t(text, 'list.boxmode.done', { mode: t(text, 'list.mode.' + mode) }));
			}, function (error) {
				toast(error && error.problem ? error.problem.title : t(text, 'list.failed'), 'bad');
			});
		}, function () {});
	}

	/**
	 * @param {Api.Channel} channel
	 * @returns {void}
	 */
	function copyAddress(channel) {
		api('GET', '/api/v1/stream/{id}', { params: { id: channel.id } }).then(function (where) {
			/* The clipboard is the browser's and is refused in more places
			   than it is granted: an insecure origin, a window that is not in
			   front, a setting. So the address is said out loud when it cannot
			   be put anywhere, which leaves it where somebody can select it. */
			const board = navigator.clipboard;
			if (!board || typeof board.writeText !== 'function') {
				toast(where.url, '');
				return;
			}
			board.writeText(where.url).then(function () {
				toast(t(text, 'list.copied', { name: channel.name }));
			}, function () {
				toast(where.url, '');
			});
		}, function (error) {
			toast(error && error.problem ? error.problem.title : t(text, 'list.failed'), 'bad');
		});
	}

	const loaded = walk.items.length;

	const options = [{ value: '', label: t(text, 'list.bouquet.all') }];
	for (const one of bouquets) {
		options.push({
			value: String(one.id),
			label: one.hidden ? t(text, 'list.bouquet.hidden', { name: one.name }) : one.name,
		});
	}

	/**
	 * @param {Api.Channel} channel
	 * @returns {boolean}
	 */
	function isPlaying(channel) {
		return !!current && sameChannel(current.id, channel.id);
	}

	const columns = [
		{
			id: 'number', label: t(text, 'list.col.number'), mono: true,
			/** @param {Api.Channel} channel */
			// Nought and not a number is a channel the box holds no place for.
			// Nothing at all rather than an empty string, so the cell is empty
			// and the folding leaves its column word off the card.
			cell: function (channel) { return channel.number > 0 ? String(channel.number) : null; }
		},
		{
			id: 'name', label: t(text, 'list.col.name'), mono: false,
			/** @param {Api.Channel} channel */
			cell: function (channel) {
				/* The name is the control. Its accessible name says what
				   pressing it does, because "ZDF" on its own does not, and the
				   word on screen stays the channel's name and nothing else. */
				return html`<span class="ch-who">
					<${Logo} channel=${channel} />
					<button
						type="button"
						class="ch-name"
						aria-label=${t(text, 'list.zap.to', { name: channel.name })}
						onClick=${function () { zapTo(channel); }}>${channel.name}</button>
				</span>`;
			}
		},
		{
			id: 'now', label: t(text, 'list.col.now'), mono: false,
			/** @param {Api.Channel} channel */
			cell: function (channel) {
				return html`<${Showing} showing=${guide.current.get(guideRowKey(channel)) || null} />`;
			}
		},
		{
			id: 'state', label: t(text, 'list.col.state'), mono: false,
			/** @param {Api.Channel} channel */
			cell: function (channel) {
				/* Nothing at all where there is nothing to say, so that a card
				   on a telephone does not carry the word Zustand over an empty
				   line: the folding hides a cell that is empty and cannot hide
				   one holding an empty box. */
				if (!isPlaying(channel) && !channel.scrambled && !channel.locked && !isWeb(channel))
					return null;
				return html`<span class="ch-marks">
					${channel.scrambled ? html`<span class="ch-mark ch-crypt" title=${t(text, 'list.flag.scrambled')}>
						<span aria-hidden="true">${t(text, 'list.flag.scrambled.short')}</span>
						<span class="sr">${t(text, 'list.flag.scrambled')}</span>
					</span>` : null}
					${channel.locked ? html`<span class="ch-mark ch-locked" title=${t(text, 'list.flag.locked')}>
						<span aria-hidden="true">${t(text, 'list.flag.locked.short')}</span>
						<span class="sr">${t(text, 'list.flag.locked')}</span>
					</span>` : null}
					${isWeb(channel) ? html`<span class="ch-mark ch-web" title=${t(text, 'list.flag.web')}>
						<span aria-hidden="true">${t(text, 'list.flag.web.short')}</span>
						<span class="sr">${t(text, 'list.flag.web')}</span>
					</span>` : null}
					${isPlaying(channel)
						? html`<span class="chip on-air"><${Dot} kind="onair" word=${t(text, 'list.running')} /></span>`
						: null}
				</span>`;
			}
		},
		{
			id: 'do', label: '', mono: false,
			/** @param {Api.Channel} channel */
			cell: function (channel) {
				/* Four marks in a line is four targets of eleven pixels on a
				   telephone. The shared control draws them side by side where
				   there is a pointer and as one sheet where there is a finger,
				   and neither is decided here. */
				return html`<span class="acts"><${RowActions}
					title=${channel.name}
					actions=${[
						{
							id: 'play', mark: '▶',
							label: t(text, 'list.act.play', { name: channel.name }),
							onAct: function () { route(hrefFor('channels', 'playback', channel.id)); }
						},
						{
							id: 'guide', mark: '▤',
							label: t(text, 'list.act.guide', { name: channel.name }),
							onAct: function () { route(hrefFor('epg', 'schedule', channel.id)); }
						},
						{
							id: 'm3u', mark: '↓',
							label: t(text, 'list.act.m3u', { name: channel.name }),
							onAct: function () { fetchPlaylist(channel); }
						},
						{
							id: 'copy', mark: '⧉',
							label: t(text, 'list.act.copy', { name: channel.name }),
							onAct: function () { copyAddress(channel); }
						}
					]} /></span>`;
			}
		}
	];

	/* WHAT THE FILTER REACHES IS SAID A ROW LOWER, beside the count of what is
	   loaded, and not under the field. Three controls decide what this list shows
	   and they stand in one row; a sentence under one of them lifted that one
	   control twenty one pixels above the other two. The bouquet screen moved its
	   own filter's sentence for the same reason and ties it back the same way.

	   Beside the count and not under the list, because the two are one thought:
	   how much is here, and how much of it the filter can see. */
	const hint = walk.more
		? t(text, 'list.filter.hint')
		: t(text, 'list.filter.complete');

	return html`<div class="ch-screen">

		<div class="ch-filters">
			<div class="ch-mode" role="group" aria-labelledby="ch-mode-label">
				<span class="label" id="ch-mode-label">${t(text, 'list.mode')}</span>
				<div class="ch-mode-row">
					${['tv', 'radio'].map(function (which) {
						return html`<button
							key=${which}
							type="button"
							class=${mode === which ? 'btn on' : 'btn'}
							aria-pressed=${mode === which ? 'true' : 'false'}
							onClick=${function () { choose(which === 'radio' ? 'radio' : 'tv', bouquet); }}>${t(text, 'list.mode.' + which)}</button>`;
					})}
				</div>
			</div>
			<${Select}
				label=${t(text, 'list.bouquet')}
				value=${bouquet > 0 ? String(bouquet) : ''}
				hint=${bouquetsFailed ? t(text, 'list.bouquets.failed') : null}
				options=${options}
				onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) { choose(mode, Number(e.currentTarget.value) || 0); }} />
			<${Field}
				id="ch-filter"
				label=${t(text, 'list.filter')}
				value=${needle}
				describedBy="ch-filter-note"
				onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { setNeedle(e.currentTarget.value); }} />
		</div>

		${/* The count alone is what is read out when it changes. The sentence
		     beside it names the same number, so a region holding both would
		     read the whole of it out again for every page that arrives, which
		     on a list of two thousand is a dozen times over. */ null}
		<div class="ch-bar">
			<p class="ch-count">
				<span role="status">${walk.more
					? t(text, 'list.count.more', { shown: rows.length, loaded: loaded })
					: t(text, 'list.count', { shown: rows.length, loaded: loaded })}</span>
				${/* Held to the word before it, so the mark never begins a line
				      of its own where the two sentences fold. */ '\u00a0· '}<span
					id="ch-filter-note">${hint}</span></p>
			<button type="button" class="btn ch-boxmode"
				onClick=${switchBox}>${t(text, 'list.boxmode', { mode: t(text, 'list.mode.' + mode) })}</button>
		</div>

		${walk.error
			? html`<${State} problem=${{ title: walk.error.problem.title, detail: walk.error.problem.detail }}
				onRetry=${function () { apply(failPageCleared(held.current)); ask(); }} />`
			: null}

		${rows.length === 0 && !walk.busy && !walk.more
			? html`<${State} empty=${emptyWord(needle, loaded, walk.more)} />`
			: null}

		${/* The next page is asked for while the last row of this one comes up.
		     The control below stays where it is: whoever is on a keyboard never
		     scrolls a row into view, and a list that only grows under a thumb is
		     a list that ends at a hundred for everybody else. */ null}
		${rows.length > 0
			? html`<${Table}
				columns=${columns}
				rows=${rows}
				onAir=${isPlaying}
				hasMore=${walk.more}
				onNearEnd=${ask}
				rowKey=${function (/** @type {Api.Channel} */ row) { return row.id; }} />`
			: null}

		${walk.more
			? html`<p class="ch-more">
				<${Button} disabled=${walk.busy} onClick=${ask}>${t(text, 'list.more')}<//>
				${walk.busy ? html`<span class="sr" role="status">${t(text, 'list.loading.more')}</span>` : null}
			</p>`
			: null}
	</div>`;
}

/**
 * What one channel is showing, and what it shows after that.
 *
 * The bar is drawn whether or not the guide answered, so a row with a programme
 * and a row without one are the same height and the column does not ripple as the
 * answers arrive.
 *
 * The line about the programme after this one is not, because there is nothing to
 * draw when the guide lists none: a sentence saying so takes the height of one
 * that carries information, and on a telephone the folding leaves an empty cell
 * off the card rather than giving it a heading of its own.
 *
 * @param {{ showing: Showing | null }} props
 * @returns {Web.Drawn}
 */
function Showing(props) {
	const showing = props.showing;
	const now = showing ? showing.now : null;
	const next = showing ? showing.next : null;

	if (!now && !next) {
		return html`<span class="ch-now-none">${t(text, showing ? 'list.now.none' : 'list.now.asking')}</span>`;
	}

	const at = Math.floor(Date.now() / 1000);
	const share = elapsedShare(now, at);
	const left = now ? Math.round((now.start + now.duration - at) / 60) : 0;

	return html`<span class="ch-now">
		${now
			? html`<span class="ch-now-line">
				<span class="ch-now-title">${now.title}</span>
				<span class="ch-now-left">${t(text, 'list.now.left', { minutes: left < 0 ? 0 : left })}</span>
			</span>
			<span class="ch-meter" role="img"
				aria-label=${t(text, 'list.now.share', { percent: share })}><i style=${{ width: share + '%' }}></i></span>`
			: html`<span class="ch-now-line"><span class="ch-now-title">${t(text, 'list.now.gap')}</span></span>`}
		${next
			? html`<span class="ch-next">${t(text, 'list.next', { at: clock(next.start), title: next.title })}</span>`
			: null}
	</span>`;
}

/**
 * What an empty result means, which is three different things.
 *
 * Nothing matched out of everything there is, nothing matched out of what has
 * been walked so far, and the selection itself is empty. The middle one is the
 * only one that ends in an invitation to keep looking, and telling the three
 * apart is why the filter is allowed to be a local one.
 *
 * @param {string} needle
 * @param {number} loaded
 * @param {boolean} more
 * @returns {string}
 */
function emptyWord(needle, loaded, more) {
	if (needle === '') {
		return t(text, 'list.empty');
	}
	if (more) {
		return t(text, 'list.filter.none', { needle: needle });
	}
	return t(text, 'list.filter.none.all', { needle: needle, loaded: loaded });
}

/**
 * The same walk with its refusal put down, so that asking again asks for the page
 * that did not arrive.
 *
 * @template T
 * @param {import('./list.paging.js').Walk<T>} walk
 * @returns {import('./list.paging.js').Walk<T>}
 */
function failPageCleared(walk) {
	return {
		items: walk.items,
		cursor: walk.cursor,
		more: walk.more,
		busy: false,
		error: null,
	};
}

/**
 * What the guide holds for a handful of channels over the next few hours, written
 * into the map the rows read.
 *
 * The grid answers a page of channels and a cursor, and a page holding fewer rows
 * than were asked for is not on that account the last: the events of a page reach
 * their own ceiling first (src/httpd/ep/ep_epg.cpp). So this follows the cursor and
 * stops where the cursor stops.
 *
 * @param {string[]} keys
 * @param {number} at
 * @param {Map<string, Showing>} into
 * @returns {Promise<void>}
 */
async function askGuide(keys, at, into) {
	let cursor = '';
	let guard = keys.length;
	while (guard >= 0) {
		guard--;
		const answer = await api('GET', '/api/v1/epg/grid', {
			query: {
				channels: keys.join(','),
				from: at,
				to: at + kAheadSeconds,
				cursor: cursor !== '' ? cursor : undefined,
			},
		});
		for (const row of (answer.items || [])) {
			into.set(channelId(row.channel_id), eventsAround(row.events || [], at));
		}
		cursor = typeof answer.next_cursor === 'string' ? answer.next_cursor : '';
		if (cursor === '') {
			break;
		}
	}
	// Every channel that was asked about is answered for, so that one without
	// a schedule is not asked about again on the next draw.
	for (const key of keys) {
		if (!into.has(key)) {
			into.set(key, { now: null, next: null });
		}
	}
}
