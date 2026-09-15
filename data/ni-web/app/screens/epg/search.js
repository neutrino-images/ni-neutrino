/* Everything the guide holds whose text matches, inside a window.

   The floor of two characters is not rebuilt here. It is enforced one layer under the
   endpoint (src/coreapi/epg.cpp) and comes back as a refusal naming itself, so what a
   person typing one letter sees is what the box said. A second check in this file
   would be a second place the same rule lives.

   Neither is the ceiling. The answer says whether it was cut, because a list exactly
   as long as the limit is the one case where a whole answer and a cut one look the
   same.

   The hits name a channel the way the channel routes do. They did not always: the
   guide keeps the lower forty eight bits of an identifier and a hit used to carry
   that half, which is not a channel anything here answers on, and a timer made out of
   one named a channel this box does not have. The widening is the box's, because the
   channel list is what widens it and only the box has all of it.

   What is still looked up here is the name, out of the listing that carries it beside
   the identifier, asked for only once something was found and without the walk of the
   picture directories. */
import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { hrefFor } from '../../nav.js';
import { channelId, clock, day } from '../../fmt.js';
import { State } from '../../ui/state.js';
import { Field } from '../../ui/field.js';
import { Select } from '../../ui/select.js';
import { Button } from '../../ui/button.js';
import { Table } from '../../ui/table.js';
import { Dot } from '../../ui/dot.js';
import {
	EventActions, EventSheet, isOnAir, whenOf,
	dayKeyOf, dayStart, shiftDay, problemOf
} from './schedule.js';
import shared from './schedule.text.js';
import text from './search.text.js';

export const css = '/app/screens/epg/schedule.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'epg.search.head'); }


// What the guide is asked for when nobody said. A week, because that is what
// the event manager is configured to hold, and a window wider than what is
// held only costs the box a walk over nothing.
const kDefaultDays = 7;

// The ceiling the endpoint declares (src/coreapi/epg.h, MAX_SEARCH_RESULTS) and
// three steps under it. The largest is offered because a search over a week is
// a search somebody means to be wide.
const kLimits = [25, 50, 100, 200];

/**
 * @typedef {{ q: string, from: number, to: number, limit: number }} Asked
 */

/**
 * What one channel is called, under the identifier a hit names it by.
 *
 * @param {Api.LogoList | null} tv
 * @param {Api.LogoList | null} radio
 * @returns {Map<string, string>}
 */
export function namesById(tv, radio) {
	/** @type {Map<string, string>} */
	const out = new Map();
	for (const one of (tv ? tv.items : [])) {
		out.set(channelId(one.id), one.name);
	}
	for (const one of (radio ? radio.items : [])) {
		out.set(channelId(one.id), one.name);
	}
	return out;
}

/**
 * @param {Record<string, never>} _props
 * @returns {Web.Drawn}
 */
export default function Search(_props) {
	const [today] = useState(dayKeyOf(Math.floor(Date.now() / 1000)));
	const [query, setQuery] = useState('');
	const [fromDay, setFromDay] = useState(today);
	const [toDay, setToDay] = useState(shiftDay(today, kDefaultDays));
	const [limit, setLimit] = useState(kLimits[2] === undefined ? 100 : kLimits[2]);
	const [asked, setAsked] = useState(/** @type {Asked | null} */ (null));

	const [open, setOpen] = useState(/** @type {Api.Event | null} */ (null));

	const [hits, setHits] = useState(/** @type {Web.Snapshot<Api.EventSearch> | null} */ (null));
	const [tv, setTv] = useState(/** @type {Web.Snapshot<Api.LogoList> | null} */ (null));
	const [radio, setRadio] = useState(/** @type {Web.Snapshot<Api.LogoList> | null} */ (null));

	useEffect(function () {
		if (asked === null) {
			setHits(null);
			return undefined;
		}
		/* The window is sent whole and even where it is upside down. A window
		   that ends before it begins is refused by name, and showing that
		   refusal is worth more than a control that quietly will not let it be
		   asked: the person learns what the box thinks, once, and the rule
		   stays in the one place it is enforced. */
		return store.watch('GET', '/api/v1/epg/search', {
			query: { q: asked.q, from: asked.from, to: asked.to, limit: asked.limit }
		}, setHits);
	}, [asked === null ? '' : asked.q, asked === null ? 0 : asked.from,
		asked === null ? 0 : asked.to, asked === null ? 0 : asked.limit]);

	const rows = hits && hits.data ? hits.data.items : [];
	const wantsNames = rows.length > 0;

	useEffect(function () {
		if (!wantsNames) {
			return undefined;
		}
		return store.watch('GET', '/api/v1/channels/logos', { query: { mode: 'tv' } }, setTv);
	}, [wantsNames]);

	useEffect(function () {
		if (!wantsNames) {
			return undefined;
		}
		return store.watch('GET', '/api/v1/channels/logos', { query: { mode: 'radio' } }, setRadio);
	}, [wantsNames]);

	const names = namesById(
		tv && tv.data ? tv.data : null,
		radio && radio.data ? radio.data : null);

	/** @param {{ currentTarget: HTMLInputElement }} event */
	function typeQuery(event) {
		setQuery(event.currentTarget.value);
	}

	/** @param {{ currentTarget: HTMLInputElement }} event */
	function typeFrom(event) {
		setFromDay(event.currentTarget.value);
	}

	/** @param {{ currentTarget: HTMLInputElement }} event */
	function typeTo(event) {
		setToDay(event.currentTarget.value);
	}

	/** @param {{ currentTarget: HTMLSelectElement }} event */
	function pickLimit(event) {
		setLimit(Number(event.currentTarget.value));
	}

	/** @param {{ preventDefault: () => void }} event */
	function submit(event) {
		event.preventDefault();
		setAsked({
			q: query,
			from: dayStart(fromDay),
			// The end of the day named and not its beginning, because a person
			// naming a day as the end of a window means that day included.
			to: dayStart(shiftDay(toDay, 1)),
			limit: limit
		});
	}

	return html`<section class="epg">
		<form onSubmit=${submit}>
			<div class="epg-form">
				<div class="epg-query">
					<${Field}
						label=${t(text, 'epg.search.query')}
						value=${query}
						hint=${t(text, 'epg.search.hint')}
						onInput=${typeQuery} />
				</div>
				<${Field} label=${t(text, 'epg.search.begins')} type="date" value=${fromDay} onInput=${typeFrom} />
				<${Field} label=${t(text, 'epg.search.ends')} type="date" value=${toDay} onInput=${typeTo} />
				<${Select}
					label=${t(text, 'epg.search.limit')}
					value=${String(limit)}
					onChange=${pickLimit}
					options=${kLimits.map(function (one) {
						return { value: String(one), label: String(one) };
					})} />
			</div>
			<p class="epg-submit">
				<${Button} type="submit" primary=${true}>${t(text, 'epg.search.submit')}<//>
			</p>
		</form>
		<${Hits} shot=${hits} names=${names} onOpen=${setOpen} />
		<${EventSheet} event=${open} onClose=${function () { setOpen(null); }} />
	</section>`;
}

/**
 * @param {{ shot: Web.Snapshot<Api.EventSearch> | null, names: Map<string, string>,
 *           onOpen: (event: Api.Event) => void }} props
 * @returns {Web.Drawn}
 */
function Hits(props) {
	const shot = props.shot;
	/* Nothing asked for yet is not nothing found, and the two say different
	   things: this one says what to type, with something to try, because a
	   screen that answers an empty form with an empty table has told nobody
	   anything. */
	if (!shot || shot.state === 'empty') {
		return html`<div class="note">
			<h2>${t(text, 'epg.search.idle')}</h2>
			<p>${t(text, 'epg.search.idle.try')}</p>
		</div>`;
	}
	if (shot.state === 'loading' && shot.data === null) {
		return html`<${State} phase="first" />`;
	}
	if (shot.state === 'error' && shot.data === null) {
		return html`<${State} problem=${problemOf(shot.error)} />`;
	}

	const found = shot.data;
	if (!found) {
		return html`<${State} phase="first" />`;
	}
	if (found.items.length === 0) {
		return html`<div class="note">
			<h2>${t(text, 'epg.search.empty')}</h2>
			<p>${t(text, 'epg.search.empty.try')}</p>
		</div>`;
	}

	const now = Math.floor(Date.now() / 1000);

	/** @type {import('../../ui/table.js').Column<Api.Event>[]} */
	const columns = [
		{
			id: 'when',
			label: t(shared, 'epg.col.when'),
			mono: true,
			cell: function (one) { return day(one.start) + ' ' + clock(one.start); }
		},
		{
			id: 'channel',
			label: t(shared, 'epg.col.channel'),
			cell: function (one) {
				const named = props.names.get(channelId(one.channel_id));
				/* The identifier while the listing is on its way, marked as
				   one, so a screen drawing it for good is not mistaken for a
				   screen still waiting. */
				return named === undefined || named === ''
					? html`<span class="epg-unnamed mono">${one.channel_id}</span>`
					: named;
			}
		},
		{
			id: 'what',
			label: t(shared, 'epg.col.what'),
			wide: true,
			cell: function (one) {
				return html`<span class="epg-what">
					<b>${one.title}</b>
					<small>${whenOf(one)}</small>
					${isOnAir(one, now) ? html`<${Dot} kind="onair" word=${t(shared, 'epg.event.running')} />` : null}
				</span>`;
			}
		},
		{
			id: 'acts',
			label: '',
			cell: function (one) {
				return html`<${EventActions}
					event=${one}
					channelHref=${hrefFor('epg', 'schedule', one.channel_id)}
					onOpen=${props.onOpen} />`;
			}
		}
	];

	return html`<${State} phase=${shot.phase}>
		<h3>${t(text, 'epg.search.results')}</h3>
		<p class="epg-window">${t(text, 'epg.search.found', { count: found.items.length })}</p>
		${found.truncated
			? html`<p class="epg-cut" role="status">${t(text, 'epg.search.truncated')}</p>`
			: null}
		<${Table}
			columns=${columns}
			rows=${found.items}
			rowKey=${function (/** @type {Api.Event} */ one) { return one.id + '-' + String(one.start); }} />
	<//>`;
}
