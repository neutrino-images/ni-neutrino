/* ONE EVENT OF THE GUIDE: WHAT IT SAYS ABOUT ITSELF, AND WHAT MAY BE DONE TO IT.

   Here and not in a screen because three screens reach the same event. The grid,
   the day of one channel and the search over the whole guide all pick one out,
   and what may be done to it belongs to the event and not to the page it was
   picked on.

   A LIST CARRIES SIX FIELDS AND NO MORE. The age an event is broadcast for, what
   it is about and the long text under the short one are answered by one route for
   one event, asked with both halves of its name: the guide files an identifier
   under every showing (src/httpd/ep/ep_epg.cpp). So none of that is fetched until
   somebody picks one event out, and then it is fetched wherever they picked it.

   AND THE SHORT TEXT OF A LIST IS NOT ALWAYS A SHORT TEXT. Where an event carries
   none, the event manager fills the field with the beginning of the long one cut
   at a hundred and twenty bytes. So the facts below draw the two texts the single
   event route answers and never the one a list handed them, or the same sentence
   would stand twice, once cut off.

   WHAT IS ON NOW IS DONE NOW, AND WHAT IS NOT YET ON IS WRITTEN DOWN. A programme
   that has begun is recorded and switched to at once; one that has not is a timer
   of the kind the button is named after. Every rule about what a timer may be is
   the box's own (src/coreapi/timers.cpp), so nothing here decides whether a thing
   may be done: it is sent, and what comes back is what is said on screen. */

import { html, route, useState, useEffect } from '../runtime.js';
import * as store from '../store.js';
import * as session from '../session.js';
import { t } from '../i18n.js';
import { clock, duration } from '../fmt.js';
import { hrefFor } from '../nav.js';
import { problemHref } from '../problem.js';
import { ensureCss } from '../css.js';
import { Button } from './button.js';
import { RowActions } from './actions.js';
import { Sheet } from './sheet.js';
import { State } from './state.js';
import { toast } from './toast.js';
import text from './event.text.js';

const kCss = '/app/ui/event.css';

/* What a zap makes stale, said here because nothing else could know it: the box
   is on another channel afterwards, and the guide answer for the running channel
   is about the one it left. */
const kZapTouches = ['/api/v1/channels/current', '/api/v1/epg/current'];

/* And what a recording started now makes stale. The timer list because that is
   where the row is made, the recordings because that is where it shows up as
   something running. */
const kRecordTouches = ['/api/v1/recordings', '/api/v1/timers'];

/**
 * The word for what an event is about, out of the upper half of the one byte the
 * box answers. Empty for the classes the box itself draws as unknown, which is
 * nought and everything from eleven up (src/gui/epgview.cpp).
 *
 * @param {number} genre
 * @returns {string} a key of the catalogue beside this file, or the empty string
 */
export function genreKey(genre) {
	if (!Number.isFinite(genre)) {
		return '';
	}
	const broad = (Math.floor(genre) >> 4) & 0x0f;
	return (broad >= 1 && broad <= 10) ? 'epg.genre.' + String(broad) : '';
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
 * When one event is, as a listing says it.
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
 * The way from an event to a timer made out of it.
 *
 * The timer list owns making and changing one, reached with a third part in its
 * address (app/screens/timers/nav.js). What the event is travels beside that
 * part rather than in it: one part of a path names one thing, and an event is
 * four.
 *
 * THE KIND IS NAMED AND NEVER LEFT OUT. The form falls back to a recording for a
 * caller that says nothing (app/screens/timers/form.js), so a switch timer asked
 * for without a kind would open a form that records.
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
 * @param {'record' | 'zapto'} kind what the timer is to do, as the box spells it
 *        (src/httpd/ep/ep_timers.cpp)
 * @returns {string}
 */
export function timerHref(event, kind) {
	const values = [
		'kind=' + encodeURIComponent(kind),
		'channel=' + encodeURIComponent(event.channel_id),
		'epg=' + encodeURIComponent(event.id),
		'start=' + encodeURIComponent(String(event.start)),
		'stop=' + encodeURIComponent(String(event.start + event.duration)),
		'title=' + encodeURIComponent(event.title)
	];
	return hrefFor('timers', 'list', 'new') + '?' + values.join('&');
}

/* A change this page asks the box for: the sign in first, then the call, then a
   line about what came of it. Nothing on these screens says in advance that it
   may not be used, because a visitor may sign in at the moment they reach for a
   control; a sheet somebody closed says nothing afterwards, because whoever
   closed it knows what they did. */
/**
 * @param {() => Promise<unknown>} run
 * @param {string} said what is put on screen once the box has taken it
 * @returns {void}
 */
function ask(run, said) {
	session.requireWrite().then(function () {
		run().then(function () {
			toast(said, '');
		}, function (failed) {
			toast(failed && failed.problem ? failed.problem.title : t(text, 'epg.event.failed'), 'bad');
		});
	}, function () { });
}

/**
 * The box, on that channel, now.
 *
 * @param {string} id the channel, hexadecimal
 * @param {string} said what is put on screen once the box has taken it
 * @returns {void}
 */
export function zapTo(id, said) {
	ask(function () {
		return store.write('POST', '/api/v1/zap', {
			touches: kZapTouches,
			body: { channel_id: id }
		});
	}, said);
}

/* RECORDING SOMETHING THAT HAS ALREADY BEGUN IS A TIMER WHOSE START HAS COME.
   That is the one shape of an immediate recording the daemon runs, which is why
   the kind carries a name of its own (src/coreapi/timers.cpp). It ends when the
   programme does, and the guide entry travels with it so that the box knows
   which showing it was made from.

   The start is read off the clock here and not taken from whatever moment the
   screen last drew itself to: the box refuses a one-off that begins before the
   current minute, and a screen a minute behind would be refused for asking to
   record in the past. */
/**
 * @param {Api.Event} event
 * @returns {void}
 */
function recordNow(event) {
	const from = Math.floor(Date.now() / 1000);
	ask(function () {
		return store.write('POST', '/api/v1/timers', {
			touches: kRecordTouches,
			body: {
				kind: 'immediate-record',
				channel_id: event.channel_id,
				start: from,
				stop: event.start + event.duration,
				title: event.title,
				epg_id: event.id,
				epg_start: event.start
			}
		});
	}, t(text, 'epg.event.record.done', { title: event.title }));
}

/**
 * WHAT MAY BE DONE TO ONE EVENT, AS ONE LIST TWO DRAWINGS SHARE.
 *
 * Which of them there are is decided here and once: a row of a listing draws
 * them as marks and a panel draws them as words, and a screen that built its own
 * list would be a screen where the same programme offers different things.
 *
 * at is the moment the caller is drawing to, and it decides the shape of the two
 * middle ones: what is on now is done now, what is not yet on is written down.
 * The moment the box is asked is read off the clock inside the act, so a screen
 * that has not drawn itself for a minute still asks about now.
 *
 * @param {{ event: Api.Event, at: number, channelHref?: string,
 *           onOpen?: (event: Api.Event) => void }} props
 * @returns {import('./actions.js').RowAction[]}
 */
export function eventActions(props) {
	const event = props.event;
	const onAir = isOnAir(event, props.at);
	const open = props.onOpen;
	/** @type {import('./actions.js').RowAction[]} */
	const actions = [];

	// Only where there is somewhere to open it. A panel that already draws the
	// whole of an event has nothing to open.
	if (open) {
		actions.push({
			id: 'about',
			label: t(text, 'epg.event.details.open'),
			mark: 'i',
			onAct: function () { open(event); }
		});
	}

	actions.push(onAir
		? {
			id: 'record',
			label: t(text, 'epg.event.record.now'),
			mark: '⏺',
			onAct: function () { recordNow(event); }
		}
		: {
			id: 'record',
			label: t(text, 'epg.event.record.timer'),
			mark: '⏺',
			onAct: function () { route(timerHref(event, 'record')); }
		});

	actions.push(onAir
		? {
			id: 'zap',
			label: t(text, 'epg.event.zap.now'),
			mark: '▶',
			onAct: function () {
				zapTo(event.channel_id, t(text, 'epg.event.zap.done', { title: event.title }));
			}
		}
		: {
			id: 'zap',
			label: t(text, 'epg.event.zap.timer'),
			mark: '▶',
			onAct: function () { route(timerHref(event, 'zapto')); }
		});

	if (props.channelHref) {
		actions.push({
			id: 'day',
			label: t(text, 'epg.event.schedule'),
			mark: '▤',
			onAct: function () { route(String(props.channelHref)); }
		});
	}
	return actions;
}

/**
 * The same list in a row of a listing.
 *
 * Handed to the frame's own control rather than drawn here, because four marks
 * in a line is four targets of eleven pixels in one hand: that control draws
 * them side by side where there is a pointer and puts them in a sheet where
 * there is a finger.
 *
 * @param {{ event: Api.Event, at: number, channelHref?: string,
 *           onOpen: (event: Api.Event) => void }} props
 * @returns {Web.Drawn}
 */
export function EventActions(props) {
	return html`<div class="acts"><${RowActions}
		title=${props.event.title}
		actions=${eventActions(props)} /></div>`;
}

/**
 * And the same list where there is room for words: a panel that has already said
 * which event it is about, so a mark whose meaning has to be guessed would be
 * the only thing on it that has to be.
 *
 * The way out of a thing that lies over the page is in the same row, because it
 * is one of the things to be done there and not a control of its own.
 *
 * @param {{ event: Api.Event, at: number, channelHref?: string,
 *           onOpen?: (event: Api.Event) => void, onClose?: () => void }} props
 * @returns {Web.Drawn}
 */
export function EventButtons(props) {
	const leave = props.onClose;
	useEffect(function () { ensureCss(kCss); }, []);

	return html`<p class="ev-acts">
		${eventActions(props).map(function (one) {
			return html`<${Button} key=${one.id} onClick=${one.onAct}>${one.label}<//>`;
		})}
		${leave ? html`<${Button} onClick=${leave}>${t(text, 'epg.event.details.close')}<//>` : null}
	</p>`;
}

/**
 * The three things only the single event route carries, fetched for the one
 * event somebody picked and drawn wherever that was.
 *
 * The read happens while this is on screen, so a listing nobody picked anything
 * out of costs the box nothing.
 *
 * @param {{ event: Api.Event }} props
 * @returns {Web.Drawn}
 */
export function EventFacts(props) {
	const [shot, setShot] = useState(/** @type {Web.Snapshot<Api.EventDetail> | null} */ (null));
	// Both halves of the name, because the guide files one identifier under
	// every showing of an event and the moment is which of them is meant.
	const id = props.event.id;
	const start = props.event.start;

	useEffect(function () { ensureCss(kCss); }, []);

	useEffect(function () {
		return store.watch('GET', '/api/v1/epg/event', {
			query: { id: id, start: start }
		}, setShot);
	}, [id, start]);

	if (!shot || shot.state === 'empty' || (shot.state === 'loading' && shot.data === null)) {
		return html`<${State} phase="first" />`;
	}
	if (shot.state === 'error' && shot.data === null) {
		// What the box said about its own refusal, and no sentence of this
		// page's making beside it.
		const failed = shot.error;
		return html`<${State} problem=${failed === null ? null : {
			title: failed.problem.title,
			detail: failed.problem.detail,
			href: problemHref(failed.problem)
		}} />`;
	}

	const known = shot.data;
	if (!known) {
		return html`<${State} phase="first" />`;
	}
	const genre = genreKey(known.genre);

	return html`<div>
		<dl class="ev-facts">
			<dt>${t(text, 'epg.event.rating')}</dt>
			<dd>${known.rating > 0
				? t(text, 'epg.event.rating.value', { years: known.rating })
				: t(text, 'epg.event.rating.none')}</dd>
			<dt>${t(text, 'epg.event.genre')}</dt>
			<dd>${genre ? t(text, genre) : t(text, 'epg.event.genre.none')}</dd>
		</dl>
		${known.description ? html`<p class="ev-text">${known.description}</p>` : null}
		${known.long_description ? html`<p class="ev-long">${known.long_description}</p>` : null}
	</div>`;
}

/**
 * One event whole, in the thing a listing lays over itself.
 *
 * A sheet and not a panel under the row: the row is a row of a table at a desk
 * and a card in one hand, and a thing that unfolds inside it is two layouts to
 * keep right.
 *
 * @param {{ event: Api.Event | null, at: number, channelHref?: string,
 *           onClose: () => void }} props
 * @returns {Web.Drawn}
 */
export function EventSheet(props) {
	const event = props.event;

	return html`<${Sheet}
		open=${event !== null}
		centred=${true}
		label=${event === null ? '' : event.title}
		onClose=${props.onClose}>
		${event === null ? null : html`<div>
			<h2>${event.title}</h2>
			<p class="ev-when">${whenOf(event)} · ${duration(event.duration)}</p>
			<${EventFacts} event=${event} />
			<${EventButtons}
				event=${event}
				at=${props.at}
				channelHref=${props.channelHref}
				onClose=${props.onClose} />
		</div>`}
	<//>`;
}
