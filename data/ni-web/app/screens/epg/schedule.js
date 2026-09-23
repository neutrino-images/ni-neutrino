/* What one channel is showing over one day, and the event row every screen of
   this destination draws.

   THE GUIDE IS ASKED WITH BOTH ENDS OF A WINDOW AND NEITHER MAY BE LEFT OUT
   (src/httpd/ep/ep_epg.cpp). A read with no bound is a whole schedule of a channel,
   which is the largest answer the box can be asked for, so the day is a real
   state of this screen: from is the local midnight of the chosen day and to is
   the local midnight of the next one. The next one and not from plus a day,
   because two days a year are twenty three and twenty five hours long here.

   A LIST CARRIES SIX FIELDS AND NO MORE. The age an event is broadcast for, what
   it is about and the long text under the short one are answered by one route
   for one event, asked with both halves of its name: the guide files an
   identifier under every showing. That is why nothing of that is fetched until
   somebody asks for one event, and why they are shown a sheet and not a panel
   inside a row.

   AND THE SHORT TEXT OF A LIST IS NOT ALWAYS A SHORT TEXT. Where an event
   carries none, the event manager fills the field with the beginning of the long
   one cut at a hundred and twenty bytes. So the sheet draws the two texts the
   single event route answers and stops drawing the one the list gave it, or the
   same sentence would stand twice, once cut off. */
import { html, route, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { hrefFor } from '../../nav.js';
import { problemHref } from '../../problem.js';
import { channelId, clock, day, duration } from '../../fmt.js';
import { State } from '../../ui/state.js';
import { Select } from '../../ui/select.js';
import { Button } from '../../ui/button.js';
import { Field } from '../../ui/field.js';
import { Table } from '../../ui/table.js';
import { RowActions } from '../../ui/actions.js';
import { Sheet } from '../../ui/sheet.js';
import { Dot } from '../../ui/dot.js';
import text from './schedule.text.js';

export const css = '/app/screens/epg/schedule.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'epg.schedule.head'); }


/* The widest page the channel listing answers (src/httpd/ep/ep_channels.cpp), so
   a bouquet short of that comes back whole in one request and the picker below
   never has to walk a cursor. A bouquet longer than this is a bouquet somebody
   built as a list to be walked, and walking a list is the channel list's
   screen and not this one's. */
const kPickerLimit = 500;

/* ------------------------------------------------------------------- days */

/**
 * The day a moment falls on, as the value a date control holds. Local, because
 * the day a person means is the day their clock is on.
 *
 * @param {number} at seconds since the epoch
 * @returns {string} year, month and day, or the empty string for no moment
 */
export function dayKeyOf(at) {
	if (!Number.isFinite(at) || at <= 0) {
		return '';
	}
	const when = new Date(at * 1000);
	const month = when.getMonth() + 1;
	const date = when.getDate();
	return String(when.getFullYear()) + '-' +
		String(month).padStart(2, '0') + '-' +
		String(date).padStart(2, '0');
}

/**
 * Local midnight of that day. Nought for anything that is not a date, which is
 * what an emptied date control holds and what a hand edited address can carry.
 *
 * @param {string} key
 * @returns {number} seconds since the epoch
 */
export function dayStart(key) {
	const parts = String(key).split('-');
	if (parts.length !== 3) {
		return 0;
	}
	const year = Number(parts[0]);
	const month = Number(parts[1]);
	const date = Number(parts[2]);
	if (!Number.isFinite(year) || !Number.isFinite(month) || !Number.isFinite(date)) {
		return 0;
	}
	const at = new Date(year, month - 1, date, 0, 0, 0, 0);
	// A date the calendar does not have rolls over in the constructor, and a
	// day nobody asked for is worse than no day: the thirty first of February
	// would quietly become the third of March.
	if (at.getFullYear() !== year || at.getMonth() !== month - 1 || at.getDate() !== date) {
		return 0;
	}
	return Math.floor(at.getTime() / 1000);
}

/**
 * The day so many days along, over a calendar and not over a number of seconds,
 * so the two days a year that are not twenty four hours long land where a person
 * expects them to.
 *
 * @param {string} key
 * @param {number} by
 * @returns {string}
 */
export function shiftDay(key, by) {
	const at = dayStart(key);
	if (at === 0) {
		return '';
	}
	const moved = new Date(at * 1000);
	moved.setDate(moved.getDate() + by);
	return dayKeyOf(Math.floor(moved.getTime() / 1000));
}

/* ----------------------------------------------------------------- events */

/**
 * The word for what an event is about, out of the upper half of the one byte the
 * box answers. Empty for the classes the box itself draws as unknown, which is
 * nought and everything from eleven up (src/gui/epgview.cpp).
 *
 * @param {number} genre
 * @returns {string} a key of this screen's catalogue, or the empty string
 */
export function genreKey(genre) {
	if (!Number.isFinite(genre)) {
		return '';
	}
	const broad = (Math.floor(genre) >> 4) & 0x0f;
	return (broad >= 1 && broad <= 10) ? 'epg.genre.' + String(broad) : '';
}

/**
 * The way from an event to a timer made out of it.
 *
 * The timer list owns making and changing one, reached with a third part in its
 * address (app/screens/timers/nav.js). What the event is travels beside that
 * part rather than in it: one part of a path names one thing, and an event is
 * four.
 *
 * The names here are the ones the form reads (askedFor in
 * app/screens/timers/form.js) and not the ones an event happens to carry. An
 * event states how long it runs; the form asks when it ends, and a name it does
 * not read is a field left empty on a form the reader thinks was filled in.
 *
 * The channel is handed on as the event names it, and that is only safe because
 * every guide read now names a channel the way the channel routes do
 * (src/coreapi/epg.cpp). It did not always: the guide keeps the lower half of an
 * identifier, a search used to answer that half, and a timer made out of a hit
 * then named a channel this box does not have.
 *
 * @param {Api.Event} event
 * @returns {string}
 */
export function timerHref(event) {
	const values = [
		'channel=' + encodeURIComponent(event.channel_id),
		'epg=' + encodeURIComponent(event.id),
		'start=' + encodeURIComponent(String(event.start)),
		'stop=' + encodeURIComponent(String(event.start + event.duration)),
		'title=' + encodeURIComponent(event.title)
	];
	return hrefFor('timers', 'list', 'new') + '?' + values.join('&');
}

/**
 * A refusal as the shared state draws one: what the box said about itself, and
 * never a sentence made up here out of a status code.
 *
 * @param {Web.Failure | null} error
 * @returns {{ title: string, detail: string, href: string } | null}
 */
export function problemOf(error) {
	if (!error) {
		return null;
	}
	return {
		title: error.problem.title,
		detail: error.problem.detail,
		href: problemHref(error.problem)
	};
}

/**
 * Whether one event is the one on air. The half open interval, so the event
 * beginning at the moment another ends is the one on now and never both.
 *
 * @param {Api.Event} event
 * @param {number} at seconds since the epoch
 * @returns {boolean}
 */
export function isOnAir(event, at) {
	return event.start <= at && at < event.start + event.duration;
}

/**
 * When one event is, as the first column of a listing says it.
 *
 * @param {Api.Event} event
 * @returns {string}
 */
export function whenOf(event) {
	return t(text, 'epg.event.span', {
		start: clock(event.start),
		end: clock(event.start + event.duration)
	});
}

/**
 * What may be done to one event.
 *
 * Handed to the frame's own control rather than drawn here, because three marks
 * in a line is three targets of eleven pixels in one hand: that control draws
 * them side by side where there is a pointer and puts them in a sheet where
 * there is a finger.
 *
 * @param {{ event: Api.Event, channelHref?: string,
 *           onOpen: (event: Api.Event) => void }} props
 * @returns {Web.Drawn}
 */
export function EventActions(props) {
	const event = props.event;
	/** @type {{ id: string, label: string, mark: string, onAct: () => void }[]} */
	const actions = [
		{
			id: 'about',
			label: t(text, 'epg.event.details.open'),
			mark: 'i',
			onAct: function () { props.onOpen(event); }
		},
		{
			id: 'timer',
			label: t(text, 'epg.event.timer'),
			mark: '⏺',
			onAct: function () { route(timerHref(event)); }
		}
	];
	if (props.channelHref) {
		actions.push({
			id: 'day',
			label: t(text, 'epg.event.schedule'),
			mark: '▤',
			onAct: function () { route(String(props.channelHref)); }
		});
	}
	return html`<div class="acts"><${RowActions} title=${event.title} actions=${actions} /></div>`;
}

/**
 * The three things only the single event route carries, in the one thing this
 * page lays over itself.
 *
 * A sheet and not a panel under the row: the row is a row of a table at a desk
 * and a card in one hand, and a thing that unfolds inside it is two layouts to
 * keep right. The read happens while the sheet is open, so a list nobody opened
 * anything in costs the box nothing.
 *
 * @param {{ event: Api.Event | null, onClose: () => void }} props
 * @returns {Web.Drawn}
 */
export function EventSheet(props) {
	const event = props.event;
	const [detail, setDetail] = useState(/** @type {Web.Snapshot<Api.EventDetail> | null} */ (null));
	// Both halves of the name, because the guide files one identifier under
	// every showing of an event and the moment is which of them is meant.
	const id = event === null ? '' : event.id;
	const start = event === null ? 0 : event.start;

	useEffect(function () {
		if (id === '') {
			setDetail(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/epg/event', {
			query: { id: id, start: start }
		}, setDetail);
	}, [id, start]);

	return html`<${Sheet}
		open=${event !== null}
		centred=${true}
		label=${event === null ? '' : event.title}
		onClose=${props.onClose}>
		${event === null ? null : html`<div>
			<h2>${event.title}</h2>
			<p class="epg-window">${whenOf(event)} · ${duration(event.duration)}</p>
			<${EventFacts} shot=${detail} genre=${detail && detail.data ? genreKey(detail.data.genre) : ''} />
			<p class="epg-actions"><${Button} onClick=${props.onClose}>${t(text, 'epg.event.details.close')}<//></p>
		</div>`}
	<//>`;
}

/**
 * @param {{ shot: Web.Snapshot<Api.EventDetail> | null, genre: string }} props
 * @returns {Web.Drawn}
 */
function EventFacts(props) {
	const shot = props.shot;
	if (!shot || shot.state === 'empty' || (shot.state === 'loading' && shot.data === null)) {
		return html`<${State} phase="first" />`;
	}
	if (shot.state === 'error' && shot.data === null) {
		return html`<${State} problem=${problemOf(shot.error)} />`;
	}

	const known = shot.data;
	if (!known) {
		return html`<${State} phase="first" />`;
	}

	return html`<div>
		<dl class="epg-facts">
			<dt>${t(text, 'epg.event.rating')}</dt>
			<dd>${known.rating > 0
				? t(text, 'epg.event.rating.value', { years: known.rating })
				: t(text, 'epg.event.rating.none')}</dd>
			<dt>${t(text, 'epg.event.genre')}</dt>
			<dd>${props.genre ? t(text, props.genre) : t(text, 'epg.event.genre.none')}</dd>
		</dl>
		${known.description ? html`<p class="epg-text">${known.description}</p>` : null}
		${known.long_description ? html`<p class="epg-long">${known.long_description}</p>` : null}
	</div>`;
}

/* ----------------------------------------------------------------- picker */

/**
 * The bouquet a picker opens on: the first one that holds anything at all.
 *
 * @param {readonly Api.Bouquet[]} rows
 * @returns {number} the bouquet, or nought where there is none to open on
 */
export function firstBouquet(rows) {
	for (const row of rows) {
		if (row.tv_count + row.radio_count > 0) {
			return row.id;
		}
	}
	return 0;
}

/**
 * WHERE A SCREEN OPENS WHEN THE BOX HAS SAID WHICH BOUQUETS HOLD THE CHANNEL IT
 * IS OPENING ON.
 *
 * The first of them in the box's own order, which is the rule the box's own
 * control API has always answered the actual bouquet with
 * (src/httpd/compat/controlapi.cpp). Hidden ones are passed over: a hidden
 * bouquet is one somebody took out of the lists on the screen, and opening on it
 * would put it back.
 *
 * Nought for a channel no bouquet holds and for a box playing nothing.
 *
 * @param {readonly Api.Bouquet[]} rows the answer of the narrowed bouquet
 *        listing, which is already in the box's own order
 * @returns {number}
 */
export function holdingBouquet(rows) {
	for (const row of rows) {
		if (!row.hidden) {
			return row.id;
		}
	}
	return 0;
}

/**
 * What one bouquet holds, as the picker lists it: television first, then radio,
 * in the order the box holds each.
 *
 * Both halves, because a bouquet is one thing to whoever made it. The listing
 * narrows a bouquet to one of its two halves and answers television when it is
 * not told which, so the half that was not asked for would be missing with
 * nothing saying so.
 *
 * @param {Api.ChannelPage | null} tv
 * @param {Api.ChannelPage | null} radio
 * @returns {Api.Channel[]}
 */
export function bouquetChannels(tv, radio) {
	/** @type {Api.Channel[]} */
	const out = [];
	for (const one of (tv ? tv.items : [])) {
		out.push(one);
	}
	for (const one of (radio ? radio.items : [])) {
		out.push(one);
	}
	return out;
}

/* ----------------------------------------------------------------- screen */

/**
 * @param {{ param?: string }} props
 * @returns {Web.Drawn}
 */
export default function Schedule(props) {
	/* Whatever the address carries, in the one spelling everything here
	   compares. The guide takes the whole identifier and the lower forty eight
	   bits of it alike, which is what lets a search hit link straight here:
	   what a search answers beside an event is the key the guide files it
	   under and not the channel's own identifier. */
	const asked = channelId(props.param === undefined ? '' : props.param);

	const [today] = useState(dayKeyOf(Math.floor(Date.now() / 1000)));
	const [when, setWhen] = useState(today);
	const [bouquet, setBouquet] = useState(0);

	const [open, setOpen] = useState(/** @type {Api.Event | null} */ (null));

	const [current, setCurrent] = useState(/** @type {Web.Snapshot<Api.Channel> | null} */ (null));
	const [bouquets, setBouquets] = useState(/** @type {Web.Snapshot<Api.BouquetList> | null} */ (null));
	const [tv, setTv] = useState(/** @type {Web.Snapshot<Api.ChannelPage> | null} */ (null));
	const [radio, setRadio] = useState(/** @type {Web.Snapshot<Api.ChannelPage> | null} */ (null));
	const [events, setEvents] = useState(/** @type {Web.Snapshot<Api.EventList> | null} */ (null));

	// Asked only where the address named no channel. A box playing nothing
	// answers this with a refusal, and that is a state of this screen and not
	// a fault: it says pick one.
	useEffect(function () {
		if (asked !== '') {
			return undefined;
		}
		return store.watch('GET', '/api/v1/channels/current', null, setCurrent);
	}, [asked]);

	/* The name of the channel the address names, for the one case where the
	   picker cannot supply it: a channel outside the open bouquet is offered
	   under its own identifier, and an identifier on screen is sixteen
	   characters of hexadecimal where a name belongs. Asked for only then, so
	   a channel the picker already holds costs nothing, and never where the
	   address names none: the running channel arrives with its own name. */
	const [named, setNamed] = useState(/** @type {Web.Snapshot<Api.Channel> | null} */ (null));
	useEffect(function () {
		if (asked === '') {
			return undefined;
		}
		return store.watch('GET', '/api/v1/channels/{id}', { params: { id: asked } }, setNamed);
	}, [asked]);

	useEffect(function () {
		return store.watch('GET', '/api/v1/bouquets', null, setBouquets);
	}, []);

	const currentChannel = current && current.data ? current.data : null;
	/* The channel's own identifier and never its epg_id. The guide answers on
	   either, because the box maps one onto the other before it looks anything
	   up, but only one of them is a channel this box has: on a box where a
	   standard and a high definition service share one schedule, the epg_id of
	   the one is the identifier of the other, and a timer built out of a row
	   drawn under it would record the wrong channel. */
	const channel = asked !== '' ? asked : (currentChannel ? channelId(currentChannel.id) : '');
	/* And what that channel is called, out of whichever answer named it. The
	   two arrivals are not the same read: an address is looked up, what is
	   playing already carries its name. Reading only the first is what left the
	   control saying sixteen characters of hexadecimal on a screen opened with
	   no address at all, which is the commonest way of opening it. */
	const chosenName = asked !== ''
		? (named && named.data ? named.data.name : '')
		: (currentChannel ? currentChannel.name : '');

	/* WHICH BOUQUET THIS SCREEN OPENS ON.

	   The one that holds the channel it is showing, because that is where the
	   person looking is.

	   Decided once and then left alone. Choosing a channel out of the picker
	   changes what is shown; it must not also move the bouquet, or the control
	   would walk away from whoever was using it.

	   Null while that is still being worked out, so the two controls open on the
	   right bouquet rather than on the first one and then jump. */
	const [home, setHome] = useState(/** @type {number | null} */ (null));
	// Whether the box has said what it is playing, either way: a refusal is an
	// answer and it says nothing is.
	const answered = current !== null && current.state !== 'empty' && current.state !== 'loading';

	useEffect(function () {
		if (home !== null) {
			return undefined;
		}
		if (channel === '') {
			/* Nothing to ask about yet. Where the address named no channel and
			   the box has said it is playing none, that is the end of it and
			   the screen opens where it always did. */
			if (asked === '' && answered) {
				setHome(0);
			}
			return undefined;
		}
		return store.watch('GET', '/api/v1/bouquets', { query: { holds: channel } },
			function (/** @type {Web.Snapshot<Api.BouquetList>} */ shot) {
				if (shot.state === 'ready' || shot.state === 'error') {
					setHome(holdingBouquet(shot.data ? shot.data.items : []));
				}
			});
	}, [home, channel, asked, answered]);

	const bouquetRows = bouquets && bouquets.data ? bouquets.data.items : [];
	const openBouquet = bouquet !== 0
		? bouquet
		: (home === null ? 0 : (home !== 0 ? home : firstBouquet(bouquetRows)));
	let counts = { tv: 0, radio: 0 };
	for (const row of bouquetRows) {
		if (row.id === openBouquet) {
			counts = { tv: row.tv_count, radio: row.radio_count };
		}
	}

	useEffect(function () {
		if (openBouquet === 0 || counts.tv === 0) {
			setTv(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/channels', {
			query: { bouquet: openBouquet, mode: 'tv', limit: kPickerLimit }
		}, setTv);
	}, [openBouquet, counts.tv]);

	useEffect(function () {
		if (openBouquet === 0 || counts.radio === 0) {
			setRadio(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/channels', {
			query: { bouquet: openBouquet, mode: 'radio', limit: kPickerLimit }
		}, setRadio);
	}, [openBouquet, counts.radio]);

	const from = dayStart(when);
	const to = dayStart(shiftDay(when, 1));

	useEffect(function () {
		if (channel === '' || from === 0 || to <= from) {
			setEvents(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/epg', {
			query: { channel: channel, from: from, to: to }
		}, setEvents);
	}, [channel, from, to]);

	const picker = bouquetChannels(
		tv && tv.data ? tv.data : null,
		radio && radio.data ? radio.data : null);

	/** @param {{ currentTarget: HTMLSelectElement }} event */
	function pickBouquet(event) {
		setBouquet(Number(event.currentTarget.value));
	}

	/** @param {{ currentTarget: HTMLSelectElement }} event */
	function pickChannel(event) {
		const id = event.currentTarget.value;
		if (id !== '') {
			route(hrefFor('epg', 'schedule', id));
		}
	}

	/** @param {{ currentTarget: HTMLInputElement }} event */
	function pickDay(event) {
		const value = event.currentTarget.value;
		if (dayStart(value) !== 0) {
			setWhen(value);
		}
	}

	return html`<section class="epg">
		<div class="epg-bar">
			<${Select}
				label=${t(text, 'epg.schedule.bouquet')}
				value=${String(openBouquet)}
				onChange=${pickBouquet}
				options=${bouquetRows.map(function (row) {
					return { value: String(row.id), label: row.name };
				})} />
			<${Select}
				label=${t(text, 'epg.schedule.channel')}
				value=${channel}
				onChange=${pickChannel}
				options=${pickerOptions(picker, channel, chosenName)} />
			<${Field}
				label=${t(text, 'epg.schedule.day')}
				type="date"
				value=${when}
				onInput=${pickDay} />
		</div>
		<p class="epg-days">
			<${Button} class="epg-prev" onClick=${function () { setWhen(shiftDay(when, -1)); }}>${t(text, 'epg.schedule.previous')}<//>
			${' '}
			<${Button} class="epg-today" onClick=${function () { setWhen(today); }}>${t(text, 'epg.schedule.today')}<//>
			${' '}
			<${Button} class="epg-next" onClick=${function () { setWhen(shiftDay(when, 1)); }}>${t(text, 'epg.schedule.next')}<//>
		</p>
		${channel === ''
			? html`<p class="note epg-nochannel">${t(text, bouquetRows.length ? 'epg.schedule.nochannel' : 'epg.schedule.nobouquets')}</p>`
			: html`<${Listing} shot=${events} when=${when} onOpen=${setOpen} />`}
		<${EventSheet} event=${open} onClose=${function () { setOpen(null); }} />
	</section>`;
}

/**
 * The channels a picker offers. The one the address names is offered too where
 * the open bouquet does not hold it, so the control never shows a channel the
 * screen is not drawing.
 *
 * Each is offered under its own identifier: two channels sharing a schedule
 * share an epg_id, so a picker offering that would hold one value twice and hand
 * the wrong one of them to everything downstream.
 *
 * @param {readonly Api.Channel[]} rows
 * @param {string} chosen
 * @param {string} [name] what the chosen one is called, where the box has been
 *        asked separately because the open bouquet does not hold it
 * @returns {{ value: string, label: string }[]}
 */
export function pickerOptions(rows, chosen, name) {
	/** @type {{ value: string, label: string }[]} */
	const out = [];
	let seen = false;
	for (const row of rows) {
		const value = channelId(row.id);
		if (value === chosen) {
			seen = true;
		}
		out.push({ value: value, label: row.name });
	}
	if (!seen && chosen !== '') {
		// The identifier only while the name is still on its way, and never
		// where the box has answered with one.
		out.unshift({ value: chosen, label: (name === undefined || name === '') ? chosen : name });
	}
	return out;
}

/**
 * The day as a table, which below the phone width is not a table: the frame
 * turns every row of one into a card and puts the column's own word in front of
 * each cell.
 *
 * @param {{ shot: Web.Snapshot<Api.EventList> | null, when: string,
 *           onOpen: (event: Api.Event) => void }} props
 * @returns {Web.Drawn}
 */
function Listing(props) {
	const shot = props.shot;
	if (!shot || shot.state === 'empty' || (shot.state === 'loading' && shot.data === null)) {
		return html`<${State} phase="first" />`;
	}
	if (shot.state === 'error' && shot.data === null) {
		return html`<${State} problem=${problemOf(shot.error)} />`;
	}

	const rows = shot.data ? shot.data.items : [];
	const chosen = dayStart(props.when);
	const now = Math.floor(Date.now() / 1000);

	/** @type {import('../../ui/table.js').Column<Api.Event>[]} */
	const columns = [
		{
			id: 'when',
			label: t(text, 'epg.col.when'),
			mono: true,
			cell: function (one) { return whenOf(one); }
		},
		{
			id: 'what',
			label: t(text, 'epg.col.what'),
			wide: true,
			cell: function (one) {
				return html`<span class="epg-what">
					<b>${one.title}</b>
					${isOnAir(one, now) ? html`<${Dot} kind="onair" word=${t(text, 'epg.event.running')} />` : null}
				</span>`;
			}
		},
		{
			id: 'acts',
			label: '',
			cell: function (one) {
				return html`<${EventActions} event=${one} onOpen=${props.onOpen} />`;
			}
		}
	];

	return html`<${State} phase=${shot.phase}>
		<p class="epg-window">${t(text, 'epg.schedule.window', { day: day(chosen) })}</p>
		${rows.length === 0
			? html`<p class="note epg-empty">${t(text, 'epg.schedule.empty')}</p>`
			: html`<${Table}
				columns=${columns}
				rows=${rows}
				rowKey=${function (/** @type {Api.Event} */ one) { return one.id + '-' + String(one.start); }} />`}
	<//>`;
}
