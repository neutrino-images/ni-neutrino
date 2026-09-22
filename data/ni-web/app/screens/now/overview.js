// What the box is doing, on one screen.
//
// One card leads and the rest stand under it. The leading one is what is
// playing, because that is the answer to the question somebody opens this page
// with. The others are the box's own conditions, each asking the box its own
// question and each failing on its own: a box that turns one of them down
// colours that card and leaves the rest standing.
//
// The cards are not pulled to one height, and the two that carry a set of
// readings rather than one figure are given two columns instead of one.
//
// What changes by itself arrives as an event and is never polled. Zapping, the
// mode, standby, the volume and the silence are all on the stream, the store
// throws away what each of them makes stale, and whatever is on screen asks
// again. The signal is the one exception, because the box publishes no event for
// a reading.
//
// EVERY CONTROL HERE IS LIVE. What needs more than this request carries raises
// the sign in sheet as it is pressed and goes through afterwards, and the two
// that take the picture away ask first.
import { html, useState, useEffect, useRef } from '../../runtime.js';
import { api, buildUrl } from '../../api.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import * as events from '../../events.js';
import { t } from '../../i18n.js';
import text from './now.text.js';
import { clock, day, duration, bytes, percent, ratio, channelId, recordingMark } from '../../fmt.js';
import { Button } from '../../ui/button.js';
import { Switch } from '../../ui/switch.js';
import { Field } from '../../ui/field.js';
import { Dot } from '../../ui/dot.js';
import { Dialog } from '../../ui/dialog.js';
import { toast } from '../../ui/toast.js';
import { monogram } from '../../ui/onair.js';
import { hrefFor, areaById, entryById, labelOf } from '../../nav.js';
import { Card, Meter, useResource, useOnScreen } from './parts.js';

export const css = '/app/screens/now/now.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'now.head'); }


/* How often the tuner is asked while its card is on screen. There is no event
   for a reading, so this is the only way to draw one; it is slow enough that
   four worker threads are not spent on it and fast enough that a dish being
   turned is visible. One request at a time, never a new one before the last
   answered, so a box that takes three seconds is asked every three seconds and
   not piled on. */
const SIGNAL_EVERY_MS = 3000;

/* How far ahead the guide is read for what comes after. Long enough for a
   film, and it is a window and not a count because that is what the route
   takes. */
const AHEAD_SECONDS = 6 * 3600;

/* The window has to hold still, or every draw would be a new address and the
   store would ask the box again for each of them. It is therefore cut to this
   many seconds wherever there is no programme to hang it on, which makes it
   move four times an hour instead of once a frame. */
const WINDOW_STEP_SECONDS = 900;

// How often the page reads its own clock again, for the bar that says how far
// the programme has run. A second would be a redraw a second for a bar that
// moves a pixel a minute.
const TICK_MS = 30000;

/* What a recording started here runs for when the guide says nothing about the
   channel. The box has its own answer to that, record_hours, and it is asked
   for rather than guessed; this is what stands in when the box cannot be asked
   either, and it is the same number the box falls back to. */
const RECORD_HOURS_FALLBACK = 4;

/* The four ways a box can put a 4:3 picture on a 16:9 screen, as the driver
   numbers them (DISPLAY_AR_MODE in the hardware library, and the same four the
   video screen of the box offers). Written out here and not read off the
   schema, because the setting is declared as a number and not as a choice: the
   screen at the box builds this list as it opens and drops the last one where
   the box cannot pan and scan 14:9, which no scan of the tables can see. A box
   that cannot do one of them turns that write down. */
const ASPECT_MODES = [
	{ value: 0, key: 'now.quick.43.panscan' },
	{ value: 1, key: 'now.quick.43.letterbox' },
	{ value: 2, key: 'now.quick.43.full' },
	{ value: 3, key: 'now.quick.43.panscan149' },
];

/**
 * The clock of whoever is looking, in the unit the box counts in, redrawn now
 * and then so that a bar drawn from it does not stand still.
 *
 * @returns {number}
 */
function useNowSeconds() {
	const [at, setAt] = useState(Math.floor(Date.now() / 1000));

	useEffect(function () {
		const timer = window.setInterval(function () {
			setAt(Math.floor(Date.now() / 1000));
		}, TICK_MS);
		return function () { window.clearInterval(timer); };
	}, []);

	return at;
}

/**
 * What the box said about a call that changed something, as one line.
 *
 * @param {Promise<unknown>} running
 * @param {string} said
 * @returns {Promise<void>}
 */
function announce(running, said) {
	return running.then(function () {
		toast(said, '');
	}, function (failed) {
		/* A sign in sheet that was closed is not a refusal by the box, and a
		   line saying the box turned something down when nobody sent it would
		   be a lie about the box. */
		if (failed && failed.aborted === true) {
			return;
		}
		toast(failed && failed.problem ? failed.problem.title : t(text, 'now.failed'), 'bad');
	});
}

/**
 * One setting of a section, as the box currently reads it.
 *
 * @param {{ items?: readonly { id: string, value: string }[] } | null} section
 * @param {string} id
 * @returns {string}
 */
export function settingValue(section, id) {
	const rows = (section && section.items) || [];
	for (const row of rows) {
		if (row.id === id) {
			return row.value;
		}
	}
	return '';
}

/**
 * The choices the box declares for one setting, which for the video modes is a
 * list only the box can state.
 *
 * @param {{ items?: readonly { id: string, choices?: readonly { value: number, label: string }[] }[] } | null} schema
 * @param {string} id
 * @returns {{ value: number, label: string }[]}
 */
export function settingChoices(schema, id) {
	const rows = (schema && schema.items) || [];
	for (const row of rows) {
		if (row.id === id) {
			return (row.choices || []).slice();
		}
	}
	return [];
}

/**
 * The picture of a channel, and its initials where the box has none.
 *
 * @param {{ id: string, name: string }} channel
 * @returns {Web.Drawn}
 */
function Logo(channel) {
	/* A picture the box does not have for this channel is a 404, which a
	   browser draws as a broken image. Asked for once, and the initials take
	   over the moment the answer says there is none. This is the frame's own
	   arrangement for the same picture, kept the same here on purpose. */
	const [broken, setBroken] = useState(false);

	useEffect(function () {
		setBroken(false);
	}, [channel.id]);

	if (broken) {
		return html`<span class="now-logo now-monogram" aria-hidden="true">${monogram(channel.name)}</span>`;
	}
	return html`<img
		class="now-logo"
		src=${'/api/v1/channels/' + encodeURIComponent(channel.id) + '/logo'}
		alt=""
		onError=${function () { setBroken(true); }} />`;
}

/* The screen that plays a channel in the browser, as the navigation states it.
   Taken from there and not written out, so that the word on the tile is the
   word in the bars: the screen was renamed and its identifier was not, and a
   second spelling here would have been the one place still saying the old
   name. */
const streaming = entryById(areaById('channels'), 'playback');

/**
 * What is playing, what is on, what comes after it, and what can be done with
 * it without leaving this screen.
 *
 * @returns {Web.Drawn}
 */
function Playing() {
	const at = useNowSeconds();
	const channel = useResource('GET', '/api/v1/channels/current');
	const running = useResource('GET', '/api/v1/recordings');
	const playing = channel.data;
	const id = playing ? channelId(playing.id) : '';

	/* The guide is asked for only once there is a channel to ask about, and
	   the identifier is the one the guide files a schedule under rather than
	   the one the channel list names: two channels sharing a schedule share
	   that second one, which is the whole reason the answer carries both. */
	const guideId = playing ? channelId(playing.epg_id) || id : '';
	const event = useResource('GET', '/api/v1/epg/current',
		{ query: { channel: guideId } }, guideId !== '');
	const on = event.data;

	/* Hung on the end of what is running where there is something running, so
	   that the address holds still for as long as the programme does. Cut to a
	   quarter of an hour otherwise, for the reason written above the constant. */
	const opens = on && on.start > 0
		? on.start + on.duration
		: Math.floor(at / WINDOW_STEP_SECONDS) * WINDOW_STEP_SECONDS;
	const after = useResource('GET', '/api/v1/epg',
		{ query: { channel: guideId, from: opens, to: opens + AHEAD_SECONDS } },
		guideId !== '');

	/* The programme that is running is left out of what comes after it. The
	   window opens where it ends, so the guide answers it only where the two
	   overlap by a second, and a screen naming the same programme twice is
	   worth one comparison here. */
	const rows = [];
	const items = (after.data && after.data.items) || [];
	for (const row of items) {
		if (!on || row.id !== on.id) {
			rows.push(row);
		}
	}

	/* Which of the recordings the box is taking is this channel's, and whether
	   one of them is the timeshift. Both are read off the list rather than
	   held here, so a recording started at the box shows up on this screen. */
	let recordingId = 0;
	let shifting = false;
	for (const one of (running.data && running.data.items) || []) {
		if (one.timeshift) {
			shifting = true;
			continue;
		}
		if (recordingId === 0 && (id === '' || channelId(one.channel_id) === id)) {
			recordingId = one.id;
		}
	}

	const ran = on && on.duration > 0 ? at - on.start : -1;
	const left = on && on.duration > 0 ? on.start + on.duration - at : -1;
	const touchesRecordings = ['/api/v1/recordings', '/api/v1/timers'];

	function startRecording() {
		if (!playing) {
			return;
		}
		/* As long as the programme, where the guide named one, and as long as
		   the box itself says a recording with no programme behind it should
		   run otherwise. Asked for at the moment it is needed and not held on
		   screen, because it is one number out of a whole section and this is
		   the only place on the screen that wants it. */
		const known = on && on.duration > 0 && on.start + on.duration > at;
		const ends = known && on
			? Promise.resolve(on.start + on.duration)
			: store.load('GET', '/api/v1/settings/{section}', { params: { section: 'recording' } })
				.then(function (answer) {
					const hours = Number(settingValue(answer, 'record_hours'));
					const runs = Number.isFinite(hours) && hours > 0 ? hours : RECORD_HOURS_FALLBACK;
					return at + runs * 3600;
				}, function () {
					return at + RECORD_HOURS_FALLBACK * 3600;
				});

		announce(ends.then(function (stop) {
			return store.write('POST', '/api/v1/timers', {
				body: {
					kind: 'immediate-record',
					channel_id: playing.id,
					start: at,
					stop: stop,
					title: on ? on.title : playing.name,
					epg_id: on && guideId !== '' ? guideId : undefined,
					epg_start: on ? on.start : undefined,
				},
				touches: touchesRecordings,
			});
		}), t(text, 'now.act.recording.started'));
	}

	function stopRecording() {
		announce(store.write('DELETE', '/api/v1/recordings/{id}', {
			params: { id: recordingId },
			touches: touchesRecordings,
		}), t(text, 'now.act.recording.stopped'));
	}

	function startShift() {
		announce(store.write('POST', '/api/v1/recordings/timeshift', {
			touches: touchesRecordings,
		}), t(text, 'now.act.timeshift.started'));
	}

	function stopShift() {
		announce(store.write('DELETE', '/api/v1/recordings/timeshift', {
			touches: touchesRecordings,
		}), t(text, 'now.act.timeshift.stopped'));
	}

	return html`<${Card}
		title=${t(text, 'now.card.playing')}
		snapshot=${channel}
		wide=${true}
		lead=${true}>
		${playing
			? html`<div class="now-head">
				<${Logo} id=${playing.id} name=${playing.name} />
				<div class="now-head-what">
					<p class="now-channel-name">${playing.name}</p>
					<p class="now-channel-facts">
						${playing.number > 0 ? html`<span>${t(text, 'now.channel.number', { number: playing.number })}</span>` : null}
						${playing.scrambled ? html`<span>${t(text, 'now.channel.scrambled')}</span>` : null}
						${playing.url ? html`<span>${t(text, 'now.channel.web')}</span>` : null}
					</p>
				</div>
				${recordingId !== 0
					? html`<${Dot} kind="recording" word=${t(text, 'now.card.recording')} />`
					: html`<${Dot} kind="onair" word=${t(text, 'now.card.playing')} />`}
			</div>
			<div class="now-event">
				${on
					? html`<p class="now-event-title">${on.title}</p>
						<p class="now-event-when mono">
							${t(text, 'now.event.window', { from: clock(on.start), to: clock(on.start + on.duration) })}
						</p>
						${on.description ? html`<p class="now-event-text">${on.description}</p>` : null}
						${on.duration > 0
							? html`<${Meter}
								label=${t(text, 'now.event.progress')}
								value=${ran}
								max=${on.duration}
								text=${left > 0 ? t(text, 'now.event.left', { duration: duration(left) }) : ''} />`
							: null}`
					: html`<p class="now-empty">${t(text, 'now.event.none')}</p>`}
			</div>
			<div>
				<h3 class="now-sub">${t(text, 'now.card.next')}</h3>
				${rows.length
					? html`<ul class="now-next-list">
						${rows.slice(0, 3).map(function (row) {
							return html`<li key=${row.id}>
								<span class="mono now-next-time">${clock(row.start)}</span>
								<span class="now-next-title">${row.title}</span>
							</li>`;
						})}
					</ul>`
					: html`<p class="now-empty">${t(text, 'now.next.none')}</p>`}
			</div>
			<p class="now-buttons">
				${recordingId !== 0
					? html`<${Button} primary=${true} onClick=${stopRecording}>${t(text, 'now.act.recordstop')}<//>`
					: html`<${Button} primary=${true} onClick=${startRecording}>${t(text, 'now.act.record')}<//>`}
				${shifting
					? html`<${Button} onClick=${stopShift}>${t(text, 'now.act.timeshiftstop')}<//>`
					: html`<${Button} onClick=${startShift}>${t(text, 'now.act.timeshift')}<//>`}
				${/* The file for a player elsewhere, and the same channel played
				     here, side by side: they are the two ways of watching what
				     is on without the television, and somebody who wants one
				     of them was looking for the other a moment before. */''}
				<a class="btn" href=${buildUrl('/api/v1/stream/playlist/{id}', { id: playing.id }, null)}>
					${t(text, 'now.act.m3u')}
				</a>
				${streaming
					? html`<a class="btn" href=${hrefFor('channels', 'playback', playing.id)}>${labelOf(streaming)}</a>`
					: null}
				<a class="btn" href=${hrefFor('channels', 'list')}>${t(text, 'now.act.zap')}</a>
			</p>`
			: html`<p class="now-empty">${t(text, 'now.channel.none')}</p>`}
	<//>`;
}

/**
 * How loud the box is, and whether it is making any sound at all.
 *
 * @returns {Web.Drawn}
 */
function Sound() {
	const sound = useResource('GET', '/api/v1/osd/volume');
	const level = sound.data;
	const [draft, setDraft] = useState(/** @type {number | null} */ (null));
	const shown = draft === null ? (level ? level.percent : 0) : draft;

	/**
	 * @param {number} next
	 * @returns {void}
	 */
	function writeVolume(next) {
		/* Stated here and drawn, and nothing read back after it. The event the
		   box sends is what corrects this belief if the box did something
		   else, and a read placed straight after the write would race it. */
		if (level) {
			store.put('GET', '/api/v1/osd/volume', null, { percent: next, muted: level.muted });
		}
		setDraft(null);
		store.write('PUT', '/api/v1/osd/volume', {
			body: { percent: next },
			touches: ['/api/v1/osd/volume'],
		}).catch(function (failed) {
			store.reload('GET', '/api/v1/osd/volume');
			toast(failed && failed.problem ? failed.problem.title : t(text, 'now.failed'), 'bad');
		});
	}

	/**
	 * @param {boolean} off
	 * @returns {void}
	 */
	function writeMute(off) {
		if (level) {
			store.put('GET', '/api/v1/osd/volume', null, { percent: level.percent, muted: off });
		}
		store.write('PUT', '/api/v1/osd/mute', {
			body: { on: off },
			touches: ['/api/v1/osd/volume'],
		}).catch(function (failed) {
			store.reload('GET', '/api/v1/osd/volume');
			toast(failed && failed.problem ? failed.problem.title : t(text, 'now.failed'), 'bad');
		});
	}

	return html`<${Card} title=${t(text, 'now.card.sound')} snapshot=${sound}>
		${!level
			/* Nothing is drawn from a reading that has not arrived. A slider
			   standing at nought while the box is being asked is a control
			   that says the sound is off, and somebody would move it. */
			? html`<p class="now-empty" role="status">${t(text, 'now.loading')}</p>`
			: html`<div class="now-state">
				${/* THE WORD IS THE NAME OF THE SWITCH AND NEVER ITS STATE, the
				     same as on the card beside it. It used to be the state, and
				     the name of a checkbox is what a screen reader reads out
				     before saying whether it is ticked, so with the sound off
				     it announced itself as "Stumm, not ticked", which is the
				     opposite of the truth. "An, not ticked" is true.

				     What the eye needs beyond the switch is under it, and only
				     where the state is not the ordinary one: a note nobody has
				     to read where the sound is on. It is a hint, so it is tied
				     to the control by aria-describedby and is read after the
				     state rather than in place of it. How loud it is is the
				     slider's own business. */''}
				<${Switch}
					label=${t(text, 'now.volume.audible')}
					hint=${level.muted ? t(text, 'now.volume.muted') : ''}
					checked=${!level.muted}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { writeMute(!e.currentTarget.checked); }} />
			</div>
			<div class="now-volume">
				${/* The word, then the figure, then the slider across both: the
				     order the reading beside it is written in, and the one the
				     grid needs. See now.css. */''}
				<label class="now-meter-label" for="now-volume">${t(text, 'now.volume.label')}</label>
				<span id="now-volume-value" class="now-meter-value">${shown} %</span>
				<input
					id="now-volume"
					class="now-range"
					type="range"
					min="0"
					max="100"
					step="1"
					value=${shown}
					aria-describedby="now-volume-value"
					onInput=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { setDraft(Number(e.currentTarget.value)); }}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { writeVolume(Number(e.currentTarget.value)); }} />
			</div>`}
	<//>`;
}

/**
 * The box itself: whether it is on, what it is, how long it has been up.
 *
 * @returns {Web.Drawn}
 */
function BoxCard() {
	const info = useResource('GET', '/api/v1/system/info');
	const rest = useResource('GET', '/api/v1/system/standby');

	/* THE EVENT AND NOT A SECOND READING. The box accepts the command,
	   publishes the event and reaches the mode a fraction of a second after
	   that, measured at 240ms, so anything sent off by the event is answered
	   with the mode the box is leaving. The address is asked again as well
	   (app/events.js); this writes the value down at once and spares the
	   screen the round trip before it can draw. */
	useEffect(function () {
		return events.on('standby', function (one) {
			store.put('GET', '/api/v1/system/standby', null, { on: Number(one.value) === 1 });
		});
	}, []);

	const box = info.data;
	const [asking, setAsking] = useState(false);

	/**
	 * @param {boolean} on
	 * @returns {void}
	 */
	function sendStandby(on) {
		setAsking(false);
		announce(session.requireSystem().then(function () {
			return store.write('POST', '/api/v1/system/standby', {
				body: { on: on },
				touches: ['/api/v1/system/info', '/api/v1/tuner'],
			});
		}), t(text, on ? 'now.standby.sent' : 'now.wake.sent'));
	}

	const mode = rest.data;

	return html`<${Card} title=${t(text, 'now.card.box')} snapshot=${info}>
		${/* ONE SWITCH AND NOT TWO BUTTONS, because two of them meant one
		     press that changes nothing. Its word is its name and never its
		     state, the same as on the card beside it and for the same reason:
		     "Standby, not ticked" is a box that is running, which is the
		     opposite of what it was drawn to say. The state is the switch, and
		     the one that is not the ordinary one is written under it as a hint,
		     which is read after the state and not instead of it.

		     Going into standby asks first, since it takes the picture off the
		     television. Coming out of it does not. */''}
		${!mode
			/* Nothing is drawn from an answer that has not arrived. A switch
			   standing at off while the box is being asked says the box is in
			   standby, and somebody would press it. */
			? html`<p class="now-empty" role="status">${t(text, 'now.loading')}</p>`
			: html`<div class="now-state">
				<${Switch}
					label=${mode.on ? t(text, 'now.box.standby') : t(text, 'now.box.on')}
					checked=${!mode.on}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) {
						if (!e.currentTarget.checked) {
							setAsking(true);
							return;
						}
						sendStandby(false);
					}} />
			</div>`}
		${box
			? html`<p class="now-facts">
				<span>${t(text, 'now.box.what', { vendor: box.vendor, model: box.model })}</span>
				<span>${t(text, 'now.box.uptime', { duration: duration(box.uptime) })}</span>
			</p>`
			: null}
		<${Dialog}
			open=${asking}
			title=${t(text, 'now.standby')}
			onCancel=${function () { setAsking(false); }}
			onConfirm=${function () { sendStandby(true); }}>
			<p>${t(text, 'now.standby.ask')}</p>
		<//>
	<//>`;
}

/**
 * Note somebody at a browser puts on the television without walking over.
 *
 * @returns {Web.Drawn}
 */
function Message() {
	const [message, setMessage] = useState('');
	const [waits, setWaits] = useState(false);

	function sendMessage() {
		if (message === '') {
			return;
		}
		announce(store.write('POST', '/api/v1/osd/message', {
			body: { text: message, kind: waits ? 'box' : 'hint' },
		}).then(function () {
			setMessage('');
		}), t(text, 'now.message.sent'));
	}

	return html`<${Card} title=${t(text, 'now.card.message')} snapshot=none>
		<div class="now-message">
			<${Field}
				label=${t(text, 'now.message.label')}
				value=${message}
				onInput=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { setMessage(e.currentTarget.value); }} />
		</div>
		<${Switch}
			label=${t(text, 'now.message.wait')}
			checked=${waits}
			onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { setWaits(e.currentTarget.checked); }} />
		<${Button}
			primary=${true}
			disabled=${message === ''}
			onClick=${sendMessage}>${t(text, 'now.message.send')}<//>
	<//>`;
}

/**
 * What the box is writing to a disc right now, and what it is going to write
 * next.
 *
 * @returns {Web.Drawn}
 */
function Recordings() {
	const at = useNowSeconds();
	const all = useResource('GET', '/api/v1/recordings');
	const timers = useResource('GET', '/api/v1/timers');
	const rows = (all.data && all.data.items) || [];

	/* The soonest timer that has not fired, which is the only one of the list
	   this card is about. Anything behind us is a timer the daemon is done
	   with or is running, and the running one is in the list above. */
	let next = /** @type {{ start: number } | null} */ (null);
	for (const one of (timers.data && timers.data.items) || []) {
		if (one.start > at && (next === null || one.start < next.start)) {
			next = one;
		}
	}

	/**
	 * @param {number} id
	 * @returns {void}
	 */
	function stop(id) {
		announce(store.write('DELETE', '/api/v1/recordings/{id}', {
			params: { id: id },
			touches: ['/api/v1/recordings', '/api/v1/timers'],
		}), t(text, 'now.act.recording.stopped'));
	}

	return html`<${Card}
		title=${t(text, 'now.card.recording')}
		snapshot=${all}
		empty=${all.data && rows.length === 0 ? t(text, 'now.rec.none') : undefined}>
		<ul class="now-recordings">
			${rows.map(function (row) {
				return html`<li key=${row.id} class="now-recording">
					<${Dot}
						kind=${recordingMark(row)}
						word=${row.timeshift
							? t(text, 'now.rec.timeshift')
							: t(text, row.started_by === 'timer' ? 'now.rec.bytimer' : 'now.rec.immediate')} />
					<span class="now-recording-title">${row.title || row.path}</span>
					<span class="mono">${t(text, 'now.rec.since', { clock: clock(row.start) })}</span>
					<span class="mono">
						${row.size === undefined ? t(text, 'now.rec.unmeasured') : bytes(row.size)}
					</span>
					<${Button} onClick=${function () { stop(row.id); }}>${t(text, 'now.rec.stop')}<//>
				</li>`;
			})}
		</ul>
		<p class="now-hint">
			${next
				? t(text, 'now.rec.next', { day: day(next.start), clock: clock(next.start) })
				: t(text, 'now.rec.nonext')}
		</p>
	<//>`;
}

/**
 * How much room the box still has, which is the other half of the question a
 * running recording raises.
 *
 * @returns {Web.Drawn}
 */
function Storage() {
	const all = useResource('GET', '/api/v1/storage/mounts');
	const rows = (all.data && all.data.items) || [];

	return html`<${Card} double=${true}
		title=${t(text, 'now.card.storage')}
		snapshot=${all}
		empty=${all.data && rows.length === 0 ? t(text, 'now.store.none') : undefined}>
		<ul class="now-mounts">
			${rows.map(function (row) {
				return html`<li key=${row.id} class="now-mount">
					<span class="now-mount-name mono">${row.id}</span>
					${row.total > 0
						? html`<${Meter}
							label=${t(text, 'now.store.used', { mount: row.id })}
							value=${row.total - row.free}
							max=${row.total}
							text=${t(text, 'now.store.free', { free: bytes(row.free), total: bytes(row.total) })} />`
						: html`<span class="now-hint">${t(text, 'now.store.unknown')}</span>`}
				</li>`;
			})}
		</ul>
	<//>`;
}

/**
 * Everything the aerial side of the box will say: what the tuner carrying the
 * picture is measuring, and what every tuner it has is doing.
 *
 * The readings are taken while this card can be seen and no longer.
 *
 * @returns {Web.Drawn}
 */
function Reception() {
	const box = useRef(/** @type {HTMLElement | null} */ (null));
	const watching = useOnScreen(box);
	const all = useResource('GET', '/api/v1/tuner/frontends');
	const [reading, setReading] = useState(/** @type {Api.Result<'GET /api/v1/tuner/signal'> | null} */ (null));
	/* Empty while a reading arrives, 'now' while nothing is carrying one and
	   'never' on a box that has no tuner to carry it. */
	const [noTuner, setNoTuner] = useState('');
	const [problem, setProblem] = useState(/** @type {{ title: string, detail: string, href: string } | null} */ (null));

	useEffect(function () {
		if (!watching) {
			return undefined;
		}

		/* One controller for the whole run of this effect, so that leaving the
		   screen or scrolling this card away stops the request that is in the
		   air and not only the one after it. */
		const control = new AbortController();
		let timer = 0;

		function again() {
			if (control.signal.aborted) {
				return;
			}
			timer = window.setTimeout(ask, SIGNAL_EVERY_MS);
		}

		function ask() {
			api('GET', '/api/v1/tuner/signal', { signal: control.signal }).then(function (answer) {
				if (control.signal.aborted) {
					return;
				}
				setReading(answer);
				setNoTuner('');
				setProblem(null);
				again();
			}, function (failed) {
				if (control.signal.aborted) {
					return;
				}
				/* Nothing to measure is not a fault. The box answers it as one
				   refusal whether it has no tuner at all or none of them is
				   carrying what plays, and either way the card says so in its
				   own words rather than drawing a red block over a screen that
				   is working. */
				if (failed.problem.type === '/errors/no-tuner') {
					/* Two of them, told apart by the status: a box that has no
					   tuner will never answer this, and one whose tuners are
					   carrying nothing will answer it again as soon as somebody
					   tunes to a channel. */
					setNoTuner(failed.problem.status === 501 ? 'never' : 'now');
					setReading(null);
					setProblem(null);
					again();
					return;
				}
				/* Every other refusal the box wrote, and the card carries it.
				   Nothing here reads the status and makes up a sentence: what
				   the box said is what it refused. */
				setProblem({
					title: failed.problem.title,
					detail: failed.problem.detail,
					href: '',
				});
				again();
			});
		}

		ask();

		return function () {
			control.abort();
			if (timer !== 0) {
				window.clearTimeout(timer);
			}
		};
	}, [watching]);

	/* The two readings are out of the scale that came with them, so both are a
	   division. The bit error count is not: the route says in so many words
	   that it is a count the tuner keeps and not a reading out of a range, so
	   it is drawn as the number it is. */
	const scale = reading ? reading.full_scale : 0;
	const rows = (all.data && all.data.items) || [];

	return html`<section class="now-card now-double" ref=${box} aria-label=${t(text, 'now.card.reception')}>
		<h2 class="now-card-title">${t(text, 'now.card.reception')}</h2>
		${problem
			? html`<div class="now-card-bad note bad" role="alert">
				<h3>${problem.title}</h3>
				${problem.detail ? html`<p>${problem.detail}</p>` : null}
			</div>`
			: reading
				? html`<p class="now-signal-head">
					<span class="mono">${t(text, 'now.signal.tuner', { adapter: reading.adapter, number: reading.number })}</span>
					<${Dot}
						kind=${reading.locked ? 'running' : 'warning'}
						word=${t(text, reading.locked ? 'now.signal.locked' : 'now.signal.unlocked')} />
				</p>
				<div class="now-gauges">
					<${Meter}
						label=${t(text, 'now.signal.strength')}
						value=${ratio(reading.strength, scale)}
						max=${100}
						text=${percent(reading.strength, scale)} />
					<${Meter}
						label=${t(text, 'now.signal.snr')}
						value=${ratio(reading.snr, scale)}
						max=${100}
						text=${percent(reading.snr, scale)} />
					<div class="now-figure">
						<span class="now-meter-label">${t(text, 'now.signal.ber')}</span>
						<span class="now-meter-value">${reading.bit_error_rate}</span>
						<span class="now-hint">${t(text, 'now.signal.count')}</span>
					</div>
				</div>
				<p class="now-hint mono">
					${t(text, 'now.signal.scale', { scale: reading.full_scale })}
				</p>`
				: (/* A box with no tuner says so once, under the list where its
				      tuners would stand. Said here as well it is one sentence
				      twice on one card. */
				   noTuner === 'never' && rows.length === 0
					? null
					: html`<p class="now-empty">${t(text, noTuner === 'never' ? 'now.signal.notuner' : 'now.signal.none')}</p>`)}
		<h3 class="now-sub">${t(text, 'now.card.tuners')}</h3>
		${rows.length === 0
			? html`<p class="now-empty">${t(text, 'now.tuner.none')}</p>`
			: html`<ul class="now-tuners">
				${rows.map(function (row) {
					const kinds = [];
					if (row.satellite) { kinds.push(t(text, 'now.tuner.sat')); }
					if (row.cable) { kinds.push(t(text, 'now.tuner.cable')); }
					if (row.terrestrial) { kinds.push(t(text, 'now.tuner.terrestrial')); }
					return html`<li key=${row.adapter + '/' + row.number} class="now-tuner">
						<span class="mono">${row.adapter}/${row.number}</span>
						<span class="now-tuner-name">${row.name}</span>
						<span class="now-tuner-kinds">${kinds.join(', ')}</span>
						<${/* in_use is a use count and not a signal lock (CFrontend::Locked),
						      so it says something holds this tuner, which on a box with two
						      of them is what a recording looks like. Whether what is held
						      receives anything is the reading above, and it exists for the
						      live one alone. */
						  Dot}
							kind=${row.live ? 'running' : (row.in_use ? 'warning' : '')}
							word=${t(text, row.live ? 'now.tuner.live' : (row.in_use ? 'now.tuner.busy' : 'now.tuner.free'))} />
					</li>`;
				})}
			</ul>`}
		${/* What the line says is that the reading is taken again while this
		     card is open, so it stands only where there is one to take. */
		  reading ? html`<p class="now-hint">${t(text, 'now.signal.resting')}</p>` : null}
	</section>`;
}

/**
 * The four things the old interface kept a page of its own for: the picture
 * the box puts out, what it does with a 4:3 picture, reading the channel lists
 * again, and putting the tuner back to where it started.
 *
 * @returns {Web.Drawn}
 */
function Quick() {
	const schema = useResource('GET', '/api/v1/settings/schema');
	const video = useResource('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
	const [asking, setAsking] = useState(false);
	const modes = settingChoices(schema.data, 'video_Mode');
	const mode = settingValue(video.data, 'video_Mode');
	const aspect = settingValue(video.data, 'video_43mode');

	/**
	 * @param {string} id
	 * @param {number} value
	 * @returns {void}
	 */
	function writeVideo(id, value) {
		/** @type {Record<string, string>} */
		const body = {};
		body[id] = String(value);
		announce(store.write('PATCH', '/api/v1/settings/{section}', {
			params: { section: 'video' },
			/* The document does not describe this body. The writer builds one
			   out of the parameters a table declares, and this handler reads
			   its own: one member per setting, named as the schema names it.
			   The settings screen carries the same cast for the same reason. */
			body: /** @type {Api.BodyValue<'PATCH /api/v1/settings/{section}'>} */ (/** @type {unknown} */ (body)),
			touches: ['/api/v1/settings/'],
		}), t(text, 'now.quick.written'));
	}

	function reloadChannels() {
		announce(store.write('POST', '/api/v1/channels/reload', {
			touches: ['/api/v1/channels', '/api/v1/bouquets', '/api/v1/epg'],
		}), t(text, 'now.quick.reloaded'));
	}

	function resetTuner() {
		setAsking(false);
		/* Asked of the session before it is sent rather than after it is
		   refused. This one takes the picture away from everybody watching, so
		   a caller that is certainly going to be turned down should be turned
		   down before the box acts and not after. */
		announce(session.requireSystem().then(function () {
			return store.write('POST', '/api/v1/tuner/reset', {
				touches: ['/api/v1/tuner', '/api/v1/channels/current'],
			});
		}), t(text, 'now.quick.reset.sent'));
	}

	return html`<${Card} title=${t(text, 'now.card.quick')} snapshot=${video} double=${true}>
		<div class="now-quick">
			<span class="now-quick-label">${t(text, 'now.quick.mode')}</span>
			${modes.length === 0
				? html`<p class="now-empty">${t(text, 'now.quick.mode.unknown')}</p>`
				: html`<div class="now-quick-set">
					${modes.map(function (one) {
						return html`<${Button}
							key=${one.value}
							primary=${String(one.value) === mode}
							onClick=${function () { writeVideo('video_Mode', one.value); }}>${one.label}<//>`;
					})}
				</div>`}
		</div>
		<div class="now-quick">
			<span class="now-quick-label">${t(text, 'now.quick.43')}</span>
			<div class="now-quick-set">
				${ASPECT_MODES.map(function (one) {
					return html`<${Button}
						key=${one.value}
						primary=${String(one.value) === aspect}
						onClick=${function () { writeVideo('video_43mode', one.value); }}>${t(text, one.key)}<//>`;
				})}
			</div>
		</div>
		<p class="now-buttons">
			<${Button} onClick=${reloadChannels}>${t(text, 'now.quick.reload')}<//>
			<${Button} onClick=${function () { setAsking(true); }}>${t(text, 'now.quick.reset')}<//>
		</p>
		<${Dialog}
			open=${asking}
			title=${t(text, 'now.quick.reset')}
			onCancel=${function () { setAsking(false); }}
			onConfirm=${resetTuner}>
			<p>${t(text, 'now.quick.reset.ask')}</p>
		<//>
	<//>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Overview() {
	return html`<div class="now-grid">
		<${Playing} />
		<${BoxCard} />
		<${Sound} />
		<${Message} />
		<${Recordings} />
		<${Storage} />
		<${Reception} />
		<${Quick} />
	</div>`;
}
