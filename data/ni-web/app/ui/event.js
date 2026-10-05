/* One guide event and what may be done to it. A list's short text may be the long one
   cut, so details come from the single event route only. */

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
import { zap } from './wake.js';
import text from './event.text.js';

const kCss = '/app/ui/event.css';

const kRecordTouches = ['/api/v1/recordings', '/api/v1/timers'];

/**
 * Empty for the classes the box draws as unknown (src/gui/epgview.cpp).
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
 * Half open, so at a boundary only one event is on.
 *
 * @param {Api.Event} event
 * @param {number} at seconds since the epoch
 * @returns {boolean}
 */
export function isOnAir(event, at) {
	return event.start <= at && at < event.start + event.duration;
}

/**
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
 * The kind is always named: the form falls back to a recording.
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

/**
 * @param {() => Promise<unknown>} run
 * @param {string} said what is put on screen once the box has taken it
 * @returns {void}
 */
function ask(run, said) {
	session.requireWrite().then(function () {
		run().then(function (sent) {
			// false is somebody declining to switch the box on.
			if (sent !== false) {
				toast(said, '');
			}
		}, function (failed) {
			toast(failed && failed.problem ? failed.problem.title : t(text, 'epg.event.failed'), 'bad');
		});
	}, function () { });
}

/**
 * @param {string} id the channel, hexadecimal
 * @param {string} said what is put on screen once the box has taken it
 * @returns {void}
 */
export function zapTo(id, said) {
	ask(function () { return zap(id); }, said);
}

// The start is read now: the box refuses a one-off that begins in the past.
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
 * @param {{ event: Api.Event, at: number, channelHref?: string,
 *           onOpen?: (event: Api.Event) => void, onClose?: () => void }} props
 * @returns {Web.Drawn}
 */
export function EventButtons(props) {
	const event = props.event;
	const onAir = isOnAir(event, props.at);
	const leave = props.onClose;
	useEffect(function () { ensureCss(kCss); }, []);

	return html`<p class="ev-acts">
		${eventActions(props).map(function (one) {
			return html`<${Button} primary=${onAir && one.id === 'record'} key=${one.id} onClick=${one.onAct}>${one.label}<//>`;
		})}
		${leave ? html`<${Button} onClick=${leave}>${t(text, 'epg.event.details.close')}<//>` : null}
	</p>`;
}

/**
 * @param {{ event: Api.Event }} props
 * @returns {Web.Drawn}
 */
export function EventFacts(props) {
	const [shot, setShot] = useState(/** @type {Web.Snapshot<Api.EventDetail> | null} */ (null));
	// The guide files one id under every showing; the start picks which.
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

/**
 * @param {number} genre
 * @returns {string}
 */
export function genreWord(genre) {
	const key = genreKey(genre);
	return key ? t(text, key) : '';
}

/**
 * A fact without a value is left out.
 *
 * @param {{ facts: readonly { term: string, value: string }[] }} props
 * @returns {Web.Drawn}
 */
export function FactList(props) {
	useEffect(function () { ensureCss(kCss); }, []);
	const shown = props.facts.filter(function (one) { return one.value !== ''; });
	if (!shown.length) {
		return null;
	}
	return html`<dl class="ev-facts">
		${shown.map(function (one) { return html`<dt>${one.term}</dt><dd>${one.value}</dd>`; })}
	</dl>`;
}

/**
 * @param {{ open: boolean, title: string, when: string, onClose: () => void, children?: unknown }} props
 * @returns {Web.Drawn}
 */
export function DetailSheet(props) {
	useEffect(function () { ensureCss(kCss); }, []);
	return html`<${Sheet}
		open=${props.open}
		centred=${true}
		label=${props.title}
		onClose=${props.onClose}>
		${props.open ? html`<div>
			<h2>${props.title}</h2>
			<p class="ev-when">${props.when}</p>
			${props.children}
		</div>` : null}
	<//>`;
}
