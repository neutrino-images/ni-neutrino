/* Making a timer and changing one.

   Not an entry of the navigation: it is the list screen reached with a third part in the
   address, so a timer is a place that can be typed, linked and put in a check.

       /timers/list/new     an empty one
       /timers/list/42      the one the daemon numbers 42

   And the way that counts in daily use, which is out of a guide entry. What the entry knows
   is handed over in the address, so whoever links here needs nothing of this file but its
   name:

       /timers/list/new?kind=record&channel=<hex>&epg=<hex>
                        &start=<seconds>&stop=<seconds>&title=<text>

   epg and start reach the box as fields of their own, because the box keeps them: a timer
   made that way knows which showing it came from.

   What this form will not do is send a recording without a duration; why stands in
   list.model.js beside the rule. */

import { html, useState, useEffect, route, getCurrentUrl } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { word, bytes, dayAndClock } from '../../fmt.js';
import { hrefFor } from '../../nav.js';
import { Field } from '../../ui/field.js';
import { Select } from '../../ui/select.js';
import { Switch } from '../../ui/switch.js';
import { Button } from '../../ui/button.js';
import { State } from '../../ui/state.js';
import { toast } from '../../ui/toast.js';
import text from './list.text.js';
import { useAllChannels } from '../../ui/channels.js';
import {
	KINDS, kindOf, isKnownKind, PLAIN_REPEATS, momentInput, emptyDraft, draftOf,
	draftProblems, createBody, changeBody, useAnswer,
} from './list.model.js';

const WEEKDAY_WORDS = ['weekday.mo', 'weekday.tu', 'weekday.we', 'weekday.th', 'weekday.fr', 'weekday.sa', 'weekday.su'];

/**
 * What the address asked for, read off the whole of it because the router hands
 * a screen the parts of the path and not the query behind them.
 * @param {string} url
 * @returns {{ kind: string, channel: string, epg: string, title: string, start: number, stop: number }}
 */
export function askedFor(url) {
	const whole = String(url === undefined || url === null ? '' : url);
	const at = whole.indexOf('?');
	const asked = new URLSearchParams(at === -1 ? '' : whole.slice(at + 1));
	const start = Number(asked.get('start'));
	const stop = Number(asked.get('stop'));
	return {
		kind: asked.get('kind') || '',
		channel: asked.get('channel') || '',
		epg: asked.get('epg') || '',
		title: asked.get('title') || '',
		start: Number.isFinite(start) && start > 0 ? Math.trunc(start) : 0,
		stop: Number.isFinite(stop) && stop > 0 ? Math.trunc(stop) : 0,
	};
}

/**
 * The draft a fresh form starts from, with whatever the address carried written over it. A
 * guide entry that is already running arrives with a start behind us and is left where it is
 * rather than quietly moved forward: the rule about it is said on screen.
 * @param {{ kind: string, channel: string, epg: string, title: string, start: number, stop: number }} asked
 * @param {number} now
 * @returns {import('./list.model.js').Draft}
 */
export function draftFromAddress(asked, now) {
	const draft = emptyDraft(asked.kind === '' ? 'record' : asked.kind, now);
	if (asked.channel !== '')
		draft.channel_id = asked.channel;
	if (asked.title !== '')
		draft.title = asked.title;
	if (asked.epg !== '') {
		draft.epg_id = asked.epg;
		draft.epg_start = asked.start;
	}
	if (asked.start > 0)
		draft.start = momentInput(asked.start);
	if (asked.start > 0 && asked.stop > asked.start)
		draft.minutes = String(Math.max(1, Math.round((asked.stop - asked.start) / 60)));
	return draft;
}

/**
 * The word for a kind, and the number the read side kept for one this build has
 * no word for (src/httpd/ep/ep_timers.cpp, kindName).
 * @param {string} kind
 * @returns {string}
 */
export function kindLabel(kind) {
	if (isKnownKind(kind))
		return t(text, 'timers.kind.' + kind);
	return t(text, 'timers.kind.other', { kind: kind });
}

/**
 * One timer out of the listing. The box offers no read of a single one, so a
 * change is built on what the listing holds and never on what a caller sent.
 * @param {{ items?: Api.Timer[] } | null} answer
 * @param {string} id
 * @returns {Api.Timer | null}
 */
export function findTimer(answer, id) {
	const wanted = Number(id);
	if (!Number.isFinite(wanted) || wanted <= 0)
		return null;
	for (const one of (answer && answer.items) || []) {
		if (one.id === wanted)
			return one;
	}
	return null;
}

/**
 * @param {Array<{ id: string, name: string, number: number }>} all
 * @param {string} chosen
 * @returns {Array<{ value: string, label: string }>}
 */
export function channelOptions(all, chosen) {
	const out = [{ value: '', label: t(text, 'form.channel.pick') }];
	let seen = false;
	for (const one of all) {
		if (one.id === chosen)
			seen = true;
		out.push({ value: one.id, label: one.number + '  ' + one.name });
	}
	/* A channel that arrived in the address and the list does not hold is still
	   the channel this timer is about, so it becomes an option of its own rather
	   than a value the control quietly drops. The walk may also still be running,
	   and then this is the option it is picked from until it ends. */
	if (chosen !== '' && !seen)
		out.push({ value: chosen, label: chosen });
	return out;
}

/* The trees the kernel keeps for itself, which are mounted filesystems and are
   not places to write a recording to. Named as the four they are rather than by
   the filesystem types under them: the type list is open ended and differs
   between an image and the next, and these four paths are what Linux means by
   them everywhere. Everything else the box reports is offered, because which of
   the real filesystems a recording belongs on is not this page's to decide. */
const KERNEL_TREES = ['/proc', '/sys', '/dev', '/run'];

/**
 * @param {string} path
 * @returns {boolean}
 */
export function isKernelTree(path) {
	for (const tree of KERNEL_TREES) {
		if (path === tree || path.indexOf(tree + '/') === 0)
			return true;
	}
	return false;
}

/**
 * @param {{ items?: Array<{ id: string, total: number, free: number }> } | null} answer
 * @param {string} chosen
 * @returns {Array<{ value: string, label: string }>}
 */
export function targetOptions(answer, chosen) {
	const out = [{ value: '', label: t(text, 'form.dir.default') }];
	let seen = false;
	for (const one of (answer && answer.items) || []) {
		// A filesystem whose counters answer nothing has no room to offer, and on
		// a running box a good half of the list is of that sort.
		if (!(one.total > 0) || isKernelTree(one.id))
			continue;
		if (one.id === chosen)
			seen = true;
		out.push({ value: one.id, label: t(text, 'form.dir.free', { path: one.id, free: bytes(one.free) }) });
	}
	if (chosen !== '' && !seen)
		out.push({ value: chosen, label: chosen });
	return out;
}

/**
 * @param {Array<{ id: string, name: string }>} all
 * @param {string} id
 * @returns {string}
 */
function channelName(all, id) {
	// Nought is how the box writes a kind that acts on no channel at all, and
	// it is not an identifier anything could be looked up by.
	if (id === '' || id === '0')
		return t(text, 'timers.nochannel');
	for (const one of all) {
		if (one.id === id)
			return one.name;
	}
	return id;
}

/**
 * @returns {Array<{ value: string, label: string }>}
 */
function repeatOptions() {
	const out = [];
	for (const name of PLAIN_REPEATS)
		out.push({ value: name, label: word('timer.repeat.' + name) });
	out.push({ value: 'weekdays', label: t(text, 'form.repeat.weekdays') });
	return out;
}

/**
 * The reason a control cannot be used, which is the first thing that is wrong and not the
 * list of them: every one is already named beside the control it belongs to, and a button
 * that repeated the lot would read them out twice to whoever listens to this page.
 * @param {string[]} wrong
 * @returns {string | null}
 */
function firstProblem(wrong) {
	const first = wrong[0];
	return first === undefined ? null : t(text, first);
}

/**
 * @param {string[]} wrong
 * @returns {string | null}
 */
function startError(wrong) {
	if (wrong.indexOf('form.bad.start') >= 0)
		return t(text, 'form.bad.start');
	if (wrong.indexOf('form.bad.past') >= 0)
		return t(text, 'form.bad.past');
	return null;
}

function nowSeconds() {
	return Math.floor(Date.now() / 1000);
}

/**
 * @param {{ id: string }} props
 * @returns {Web.Drawn}
 */
export function TimerForm(props) {
	const changing = props.id !== '' && props.id !== 'new';

	/* What the address brought with it, read once at the first draw and kept.
	   The line above the fields reports it, and reporting it out of the draft
	   instead would make that line follow the fields as they are edited, which
	   is a line that stops saying what was taken over the moment anybody
	   changes anything. */
	const [seeded] = useState(function () {
		return changing ? null : draftFromAddress(askedFor(getCurrentUrl()), nowSeconds());
	});
	const [draft, setDraft] = useState(/** @type {import('./list.model.js').Draft | null} */ (null));
	const [problem, setProblem] = useState(/** @type {{ title: string, detail: string } | null} */ (null));
	const [sending, setSending] = useState(false);

	const held = useAnswer('GET', '/api/v1/timers');
	const timer = findTimer(held.data, props.id);
	/* Narrowed here rather than where it is drawn: a draft carries the mode as
	   a plain string because that is what the control hands back, and the two
	   spellings this screen has are the two the listing knows. */
	const mode = /** @type {'tv' | 'radio'} */ ((draft !== null && draft.mode === 'radio') ? 'radio' : 'tv');
	/* Every channel of the mode and not the first page of them. The route
	   answers five hundred at most, and a box with a full satellite list has
	   several thousand: a channel past that page could not be picked at all,
	   so no timer could be written for it here (app/ui/channels.js). */
	const channels = useAllChannels(mode);
	const targets = useAnswer('GET', '/api/v1/storage/mounts');

	useEffect(function () {
		if (draft !== null)
			return;
		if (!changing) {
			setDraft(seeded);
			return;
		}
		if (timer !== null)
			setDraft(draftOf(timer));
	}, [changing, timer === null ? '' : String(timer.id)]);

	if (changing && held.state === 'error')
		return html`<${State} problem=${held.error ? held.error.problem : null} />`;
	/* Only once the listing has come back and does not hold it. The draft is
	   seeded after the draw and not during it, so without the second half of
	   this a timer that is there would show as one that is not for exactly one
	   frame, which is the frame a check catches. */
	if (draft === null && changing && held.state === 'ready' && timer === null)
		return html`<p class="note timers-gone">${t(text, 'form.gone')}</p>`;
	if (draft === null)
		return html`<${State} phase="first" />`;

	const here = draft;
	const shape = kindOf(here.kind);
	const wrong = draftProblems(here, nowSeconds());
	/* The one reason this form withholds its own button is a field that is not
	   filled in yet. What the request is granted is not read here at all: the
	   button is pressed first and the sign in is asked for after, so that a
	   form somebody has just filled in is not a form they are told they may not
	   have filled in. */
	const reason = firstProblem(wrong);

	/**
	 * @param {Partial<import('./list.model.js').Draft>} patch
	 */
	function change(patch) {
		setDraft(function (was) {
			return was === null ? was : Object.assign({}, was, patch);
		});
	}

	/**
	 * @param {number} index
	 * @param {boolean} on
	 */
	function pickDay(index, on) {
		setDraft(function (was) {
			if (was === null)
				return was;
			const days = was.days.slice();
			days[index] = on;
			return Object.assign({}, was, { days: days });
		});
	}

	/**
		 * The kind changed, and with it which fields the timer has. Built afresh rather
		 * than written over, because the kind that was there carried values the new one
		 * has no block for, and carrying them along would send what nothing reads back.
		 * @param {string} kind
		 */
	function pickKind(kind) {
		setDraft(function (was) {
			if (was === null)
				return was;
			const next = emptyDraft(kind, nowSeconds());
			next.mode = was.mode;
			next.channel_id = was.channel_id;
			next.title = was.title;
			next.start = was.start;
			next.epg_id = was.epg_id;
			next.epg_start = was.epg_start;
			return next;
		});
	}

	/**
	 * @param {Event} event
	 */
	async function send(event) {
		event.preventDefault();
		if (wrong.length > 0 || sending)
			return;
		setSending(true);
		setProblem(null);
		try {
			// The sheet, if this request does not carry a write yet, and what
			// was typed is still here on the other side of it.
			await session.requireWrite();
		} catch (refused) {
			// The person closed the sheet. They know why nothing was sent.
			setSending(false);
			return;
		}
		/* Read when the button is pressed and not when the screen was drawn. An
		   immediate recording begins now, and now is the press. */
		const now = nowSeconds();
		try {
			if (changing) {
				await store.write('PATCH', '/api/v1/timers/{id}', {
					/* The address carries the number as text and the route takes the
					   number. It cannot be anything else here: the form is only drawn
					   for an address the listing matched a timer to (findTimer). */
					params: { id: Number(props.id) },
					body: changeBody(here, now),
					touches: ['/api/v1/timers'],
				});
				toast(t(text, 'form.changed'));
			} else {
				await store.write('POST', '/api/v1/timers', {
					body: createBody(here, now),
					touches: ['/api/v1/timers'],
				});
				toast(t(text, 'form.created'));
			}
			route(hrefFor('timers', 'list'));
		} catch (caught) {
			const failed = /** @type {{ problem?: { title: string, detail: string } } | null} */ (caught);
			setProblem(failed && failed.problem ? failed.problem : null);
			setSending(false);
		}
	}

	return html`<div class="timers">
		<h2 id="timer-form-heading">${changing
			? t(text, 'form.change', { id: props.id })
			: t(text, 'form.new')}</h2>

		${problem ? html`<${State} problem=${problem} />` : null}

		${seeded !== null && seeded.epg_id !== '' && seeded.epg_start > 0
			? html`<p class="timers-from-epg">${t(text, 'form.epg.summary', {
				title: seeded.title,
				when: dayAndClock(seeded.epg_start),
				minutes: seeded.minutes,
			})}</p>`
			: null}

		<form class="timers-form" onSubmit=${send}>
			${changing
				? html`<${Field}
					id="timer-kind"
					label=${t(text, 'form.kind')}
					value=${kindLabel(here.kind)}
					readOnly=${true}
					hint=${t(text, 'form.kind.fixed')}
					onInput=${function () { }} />`
				: html`<${Select}
					id="timer-kind"
					label=${t(text, 'form.kind')}
					value=${here.kind}
					options=${KINDS.map(function (one) { return { value: one.id, label: kindLabel(one.id) }; })}
					onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) { pickKind(e.currentTarget.value); }} />`}

			${shape && shape.channel && changing ? html`
				<${Field}
					id="timer-channel"
					label=${t(text, 'form.channel')}
					value=${channelName(channels.items, here.channel_id)}
					readOnly=${true}
					onInput=${function () { }} />` : null}

			${shape && shape.channel && !changing ? html`
				<${Select}
					id="timer-mode"
					label=${t(text, 'form.mode')}
					value=${here.mode}
					options=${[
						{ value: 'tv', label: t(text, 'form.mode.tv') },
						{ value: 'radio', label: t(text, 'form.mode.radio') },
					]}
					onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) { change({ mode: e.currentTarget.value }); }} />
				<${Select}
					id="timer-channel"
					label=${t(text, 'form.channel')}
					value=${here.channel_id}
					error=${wrong.indexOf('form.bad.channel') >= 0 ? t(text, 'form.bad.channel') : null}
					hint=${channels.done ? null : t(text, 'form.channel.loading')}
					options=${channelOptions(channels.items, here.channel_id)}
					onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) { change({ channel_id: e.currentTarget.value }); }} />` : null}

			${shape && shape.start ? html`
				<${Field}
					id="timer-start"
					label=${t(text, 'form.start')}
					type="datetime-local"
					value=${here.start}
					error=${startError(wrong)}
					hint=${here.epg_id !== '' && here.epg_start > 0
						? t(text, 'form.epg', { when: dayAndClock(here.epg_start) })
						: null}
					onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { change({ start: e.currentTarget.value }); }} />`
				: html`<p class="note timers-now">${t(text, 'form.start.now')}</p>`}

			${shape && shape.duration ? html`
				<${Field}
					id="timer-minutes"
					label=${t(text, 'form.duration')}
					type="number"
					min="1"
					value=${here.minutes}
					hint=${t(text, 'form.duration.why')}
					error=${wrong.indexOf('form.bad.duration') >= 0 ? t(text, 'form.bad.duration') : null}
					onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { change({ minutes: e.currentTarget.value }); }} />` : null}

			${shape && shape.title !== '' ? html`
				<${Field}
					id="timer-title"
					label=${t(text, shape.title)}
					value=${here.title}
					onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { change({ title: e.currentTarget.value }); }} />` : null}

			${shape && shape.repeat ? html`
				<${Select}
					id="timer-repeat"
					label=${t(text, 'form.repeat')}
					value=${here.repeat}
					options=${repeatOptions()}
					onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) { change({ repeat: e.currentTarget.value }); }} />
				${here.repeat === 'weekdays' ? html`
					<fieldset class="timers-days">
						<legend>${t(text, 'form.days')}</legend>
						${WEEKDAY_WORDS.map(function (key, index) {
							return html`<${Switch}
								key=${key}
								label=${word(key)}
								checked=${here.days[index] === true}
								onChange=${function (/** @type {Web.On<HTMLInputElement>} */ e) { pickDay(index, e.currentTarget.checked); }} />`;
						})}
						${wrong.indexOf('form.bad.noday') >= 0
							? html`<p class="err">${t(text, 'form.bad.noday')}</p>`
							: null}
					</fieldset>` : null}
				${here.repeat !== 'once' ? html`
					<${Field}
						id="timer-count"
						label=${t(text, 'form.count')}
						type="number"
						min="0"
						value=${here.count}
						hint=${t(text, 'form.count.hint')}
						onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { change({ count: e.currentTarget.value }); }} />` : null}` : null}

			${shape && shape.announce ? html`
				<${Field}
					id="timer-announce"
					label=${t(text, 'form.announce')}
					type="number"
					min="0"
					value=${here.announce}
					hint=${t(text, 'form.announce.hint')}
					onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { change({ announce: e.currentTarget.value }); }} />` : null}

			${shape && shape.standby ? html`
				<${Switch}
					id="timer-standby"
					label=${t(text, 'form.standby')}
					checked=${here.standby_on}
					onChange=${function (/** @type {Web.On<HTMLInputElement>} */ e) { change({ standby_on: e.currentTarget.checked }); }} />` : null}

			${shape && shape.dir ? html`
				<${Select}
					id="timer-dir"
					label=${t(text, 'form.dir')}
					value=${here.recording_dir}
					options=${targetOptions(targets.data, here.recording_dir)}
					onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) { change({ recording_dir: e.currentTarget.value }); }} />` : null}

			<p class="timers-actions">
				<${Button}
					type="submit"
					primary=${true}
					disabled=${sending}
					reason=${reason}>${changing ? t(text, 'form.save') : t(text, 'form.create')}<//>
				${' '}
				<${Button} onClick=${function () { route(hrefFor('timers', 'list')); }}>${t(text, 'form.cancel')}<//>
			</p>
		</form>
	</div>`;
}
