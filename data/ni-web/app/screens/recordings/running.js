/* Recordings in progress. The event stream announces only the first start and the
   last stop, so this screen polls instead of following events. */
import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import text from './recordings.text.js';
import { clock, duration, bytes, isTimeshift, recordingMark } from '../../fmt.js';
import { State } from '../../ui/state.js';
import { Button } from '../../ui/button.js';
import { StateChip } from '../../ui/dot.js';
import { Table } from '../../ui/table.js';
import { Dialog } from '../../ui/dialog.js';
import { Select } from '../../ui/select.js';
import { ChannelName } from '../../ui/channels.js';
import { toast } from '../../ui/toast.js';
import { refusalKey } from './archive.model.js';

export const css = '/app/screens/recordings/recordings.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'rec.lead'); }


const RELOAD_MS = 5000;

// No event announces a single stop, so the stopping mark expires.
const ENDING_MS = 15000;

const kHourChoices = [1, 2, 3, 4];

// Builds without the recording settings section answer 501.
const kFallbackHours = 2;

const kSecondsPerHour = 3600;

/**
 * @param {number} count
 * @returns {{ mark: string, said: string }}
 */
export function summaryOf(count) {
	if (count <= 0)
		return { mark: 'rec-none', said: t(text, 'rec.empty') };
	if (count === 1)
		return { mark: 'rec-one', said: t(text, 'rec.count.one') };
	return { mark: 'rec-many', said: t(text, 'rec.count.many', { count: count }) };
}

/**
 * @param {string} startedBy
 * @returns {string}
 */
export function startedByWord(startedBy) {
	if (startedBy === 'timer')
		return t(text, 'rec.by.timer');
	if (startedBy === 'immediate')
		return t(text, 'rec.by.immediate');
	return t(text, 'rec.by.other', { word: startedBy });
}

/**
 * @param {Api.SettingValueList | null} settings
 * @returns {number}
 */
export function boxHours(settings) {
	const rows = (settings && settings.items) || [];
	for (const row of rows) {
		if (row.id !== 'record_hours')
			continue;
		const hours = Number(row.value);
		return Number.isFinite(hours) && hours > 0 ? hours : 0;
	}
	return 0;
}

/**
 * @param {number} preferred
 * @returns {{ value: string, label: string }[]}
 */
export function hourOptions(preferred) {
	const all = kHourChoices.slice();
	if (preferred > 0 && all.indexOf(preferred) === -1)
		all.push(preferred);
	all.sort(function (a, b) { return a - b; });
	return all.map(function (one) {
		return {
			value: String(one),
			label: one === 1 ? t(text, 'rec.hours.one') : t(text, 'rec.hours.many', { count: one }),
		};
	});
}

/** @returns {Web.Drawn} */
export default function Running() {
	const [shot, setShot] = useState(function () {
		return store.read('GET', '/api/v1/recordings');
	});
	const [current, setCurrent] = useState(function () {
		return store.read('GET', '/api/v1/channels/current');
	});
	const [settings, setSettings] = useState(function () {
		return store.read('GET', '/api/v1/settings/{section}', { params: { section: 'recording' } });
	});
	// Redraw only; durations read the clock where drawn.
	const bump = useState(0)[1];
	const [ending, setEnding] = useState(/** @type {{ id: number, until: number }[]} */([]));
	const [asked, setAsked] = useState(/** @type {Api.Recording | null} */(null));
	const [hours, setHours] = useState(0);

	/**
	 * @param {() => void} act
	 * @returns {void}
	 */
	function whenAllowed(act) {
		session.requireWrite().then(act, function () { });
	}

	useEffect(function () {
		const stopList = store.watch('GET', '/api/v1/recordings', null, setShot);
		const stopCurrent = store.watch('GET', '/api/v1/channels/current', null, setCurrent);
		const stopSettings = store.watch('GET', '/api/v1/settings/{section}',
			{ params: { section: 'recording' } }, setSettings);
		const tick = window.setInterval(function () {
			bump(function (n) { return n + 1; });
			store.reload('GET', '/api/v1/recordings');
		}, RELOAD_MS);

		return function () {
			stopList();
			stopCurrent();
			stopSettings();
			window.clearInterval(tick);
		};
	}, []);

	const rows = (shot.data && shot.data.items) || [];
	const shift = rows.filter(function (one) { return one.timeshift; })[0] || null;
	const channel = store.lastAnswer(current);
	const preferred = boxHours(settings.data);
	const chosen = hours > 0 ? hours : (preferred > 0 ? preferred : kFallbackHours);
	const summary = summaryOf(rows.length);

	/**
	 * @param {Api.Recording} one
	 * @returns {boolean}
	 */
	function stopping(one) {
		const at = Date.now();
		for (const mark of ending) {
			if (mark.id === one.id && mark.until > at)
				return true;
		}
		return false;
	}

	const anyStopping = rows.filter(stopping).length > 0;

	/**
	 * @param {string} said
	 * @returns {void}
	 */
	function afterWrite(said) {
		toast(said);
		store.reload('GET', '/api/v1/recordings');
	}

	/**
	 * @param {unknown} caught
	 * @returns {void}
	 */
	function refused(caught) {
		const key = refusalKey(caught);
		toast(t(text, key), 'bad');
	}

	/**
	 * @param {Api.Recording} one
	 * @returns {void}
	 */
	function stopOne(one) {
		setEnding(function (was) {
			const at = Date.now();
			const live = was.filter(function (mark) { return mark.until > at; });
			return live.concat([{ id: one.id, until: at + ENDING_MS }]);
		});
		// The timeshift route also keeps the box from starting the next one.
		const asking = one.timeshift
			? store.write('DELETE', '/api/v1/recordings/timeshift', {
				touches: ['/api/v1/recordings'],
			})
			: store.write('DELETE', '/api/v1/recordings/{id}', {
				params: { id: one.id },
				touches: ['/api/v1/recordings'],
			});

		asking.then(function () {
			afterWrite(t(text, 'rec.stop.asked', { what: whatOf(one) }));
		}, refused);
	}

	function startShift() {
		store.write('POST', '/api/v1/recordings/timeshift', {
			touches: ['/api/v1/recordings'],
		}).then(function () {
			afterWrite(t(text, 'rec.shift.start.asked'));
		}, refused);
	}

	function startHere() {
		if (!channel)
			return;
		const from = Math.floor(Date.now() / 1000);
		store.write('POST', '/api/v1/timers', {
			body: {
				kind: 'immediate-record',
				channel_id: channel.id,
				start: from,
				stop: from + chosen * kSecondsPerHour,
			},
			touches: ['/api/v1/recordings', '/api/v1/timers'],
		}).then(function () {
			afterWrite(t(text, 'rec.start.asked', { channel: channel.name }));
		}, refused);
	}

	/**
	 * @param {Api.Recording} one
	 * @returns {string}
	 */
	function whatOf(one) {
		if (one.timeshift)
			return t(text, 'rec.kind.timeshift');
		return one.title || t(text, 'rec.title.none');
	}

	const columns = [
		{
			id: 'state', label: t(text, 'rec.col.state'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) {
				return html`<${StateChip}
					kind=${recordingMark(one)}
					tone=${isTimeshift(one) ? 'good' : ''}
					word=${one.timeshift ? t(text, 'rec.kind.timeshift') : t(text, 'rec.kind.recording')} />`;
			},
		},
		{
			id: 'channel', label: t(text, 'rec.col.channel'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) { return html`<${ChannelName} id=${one.channel_id} class="rec-channel" />`; },
		},
		{
			id: 'title', label: t(text, 'rec.col.title'), sortable: false, mono: false, wide: true,
			cell: function (/** @type {Api.Recording} */ one) {
				return html`<span>
					${one.title || t(text, 'rec.title.none')}
					<span class="hint">${startedByWord(one.started_by)}</span>
				</span>`;
			},
		},
		{
			id: 'since', label: t(text, 'rec.col.since'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) { return clock(one.start); },
		},
		{
			id: 'running', label: t(text, 'rec.col.running'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) { return duration(Math.floor(Date.now() / 1000) - one.start); },
		},
		{
			id: 'size', label: t(text, 'rec.col.size'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) {
				return one.size === undefined ? t(text, 'rec.size.unknown') : bytes(one.size);
			},
		},
		{
			id: 'path', label: t(text, 'rec.col.path'), sortable: false, mono: true,
			cell: function (/** @type {Api.Recording} */ one) { return html`<span title=${one.path}>${one.path}</span>`; },
		},
		{
			id: 'act', label: t(text, 'rec.col.act'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) {
				const busy = stopping(one);
				return html`<${Button}
					class=${one.timeshift ? 'rec-stop rec-stop-shift' : 'rec-stop rec-stop-rec'}
					disabled=${busy}
					onClick=${function () { whenAllowed(function () { setAsked(one); }); }}>
					${busy
						? t(text, 'rec.stopping')
						: (one.timeshift ? t(text, 'rec.stop.shift') : t(text, 'rec.stop'))}
				<//>`;
			},
		},
	];

	return html`<section class="rec">
		${shot.data && shot.state !== store.FAILED
				? html`<p class=${'rec-summary ' + summary.mark}>${summary.said}</p>`
				: null}

		<${State}
			problem=${shot.error ? shot.error.problem : null}
			phase=${shot.phase}
			onRetry=${function () { store.reload('GET', '/api/v1/recordings'); }}>
			${rows.length
				? html`<${Table}
					columns=${columns}
					rows=${rows}
					rowKey=${function (/** @type {Api.Recording} */ one) { return one.id; }} />`
				: null}
		<//>

		${anyStopping ? html`<p class="hint rec-accepted">${t(text, 'rec.accepted')}</p>` : null}

		<div class="rec-acts">
			<${Select}
				id="rec-hours"
				label=${t(text, 'rec.hours')}
				value=${String(chosen)}
				hint=${preferred > 0 ? t(text, 'rec.hours.box') : t(text, 'rec.hours.unknown')}
				options=${hourOptions(preferred)}
				onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ event) { setHours(Number(event.currentTarget.value)); }} />
			<p class="rec-on">${channel
				? t(text, 'rec.start.on', { channel: channel.name })
				: t(text, 'rec.start.nochannel')}</p>
			<${Button}
				class="rec-start"
				primary=${true}
				disabled=${!channel}
				onClick=${function () { whenAllowed(startHere); }}>${t(text, 'rec.start')}<//>
			${shift
				? null
				: html`<${Button} class="rec-shift" onClick=${function () { whenAllowed(startShift); }}>
					${t(text, 'rec.shift.start')}<//>`}
		</div>

		<${Dialog}
			open=${!!asked}
			title=${t(text, 'rec.stop.ask.title')}
			confirmLabel=${t(text, 'rec.stop')}
			onCancel=${function () { setAsked(null); }}
			onConfirm=${function () {
				const one = asked;
				setAsked(null);
				if (one)
					stopOne(one);
			}}>
			<p>${asked ? t(text, 'rec.stop.ask.body', { what: whatOf(asked) }) : ''}</p>
			${asked ? html`<p><${ChannelName} id=${asked.channel_id} class="rec-channel" /></p>` : null}
		<//>
	</section>`;
}
