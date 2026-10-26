// What the box is doing, on one screen.
//
// One card leads and the rest stand under it. The leading one is what is
// playing, because that is the answer to the question somebody opens this page
// with. The others are the box's own conditions, each asking the box its own
// question and each failing on its own: a box that turns one of them down
// colours that card and leaves the rest standing.
//
// The cards in one row stand as tall as the tallest of them (now.css), and the
// two that carry a set of readings rather than one figure are given two columns
// instead of one.
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
//
// THE ORDER IS THE PAGE'S UNLESS THIS BROWSER SAYS OTHERWISE. With the switch in
// the header's menu on (app/ui/arrange.js), every card carries a grip and stands
// where this browser last put it (order.js). Off, which is the default, this
// screen is drawn exactly as it was before that switch existed.
import { html, useState, useEffect, useRef, useId } from '../../runtime.js';
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
import { refusalLine } from '../../problem.js';
import { Logo } from '../../ui/logo.js';
import { useRefused } from '../../ui/refused.js';
import * as pagePlayer from '../../ui/playing.js';
import { hrefFor, areaById, entryById, labelOf } from '../../nav.js';
import { Card, Title, Meter, useResource, useOnScreen } from './parts.js';
import { CARDS, keptOrder, keepOrder, isOwn, shifted, dropped, behind } from './order.js';
import { chosenArrange, onArrange } from '../../ui/arrange.js';
import { boxNowSeconds } from '../../boxclock.js';
import { readPlayback, isPlayback, positionAt, timeText, tileActions, kResyncMs } from '../../playback.js';
import { ArchiveDetails, usePlaylist, playHereNow, isHere } from '../recordings/archive.js';
import recText from '../recordings/recordings.text.js';
import { archiveCoverHref } from '../recordings/archive.model.js';
import { useKept } from '../../ui/kept.js';
import { ensureCss } from '../../css.js';
import shell from '../../shell.text.js';
import { sendStandby } from '../../ui/power.js';

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

const kNothingPlaying = '/errors/no-running-channel';

/* The four ways a box can put a 4:3 picture on a 16:9 screen, as the driver
   numbers them (DISPLAY_AR_MODE in the hardware library), in the words of this
   page. Which of them this box offers is the schema's to say, see aspectModes. */
const ASPECT_MODES = [
	{ value: 0, key: 'now.quick.43.panscan' },
	{ value: 1, key: 'now.quick.43.letterbox' },
	{ value: 2, key: 'now.quick.43.full' },
	{ value: 3, key: 'now.quick.43.panscan149' },
];

/**
 * The box's clock, in the unit the box counts in, redrawn now and then so that
 * a bar drawn from it does not stand still.
 *
 * @returns {number}
 */
function useNowSeconds() {
	const [at, setAt] = useState(boxNowSeconds());

	useEffect(function () {
		const timer = window.setInterval(function () {
			setAt(boxNowSeconds());
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
		toast(refusalLine(failed && failed.problem) || t(text, 'now.failed'), 'bad');
	});
}

/**
 * Where the programme on air ends, while that is still ahead of the box's clock,
 * and nought otherwise.
 *
 * @param {{ start: number, duration: number } | null | undefined} on
 * @param {number} from the box's time, in seconds
 * @returns {number}
 */
export function programmeEnd(on, from) {
	return on && on.duration > 0 && on.start + on.duration > from ? on.start + on.duration : 0;
}

/**
 * What pressing record sends. The guide event goes along only with the
 * programme's own end, which the box then keeps as it is.
 *
 * @param {{ id: string, name: string }} playing
 * @param {{ id: string, title: string, start: number, duration: number } | null | undefined} on
 * @param {number} from the box's time of the press, in seconds
 * @param {number} stop
 * @returns {Api.BodyValue<'POST /api/v1/timers'>}
 */
export function recordNowBody(playing, on, from, stop) {
	const ownEnd = stop > 0 && stop === programmeEnd(on, from);
	return {
		kind: 'immediate-record',
		channel_id: playing.id,
		start: from,
		stop: stop,
		title: on ? on.title : playing.name,
		epg_id: ownEnd && on ? on.id : undefined,
		epg_start: ownEnd && on ? on.start : undefined,
	};
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
 * @param {{ items?: readonly { id: string, values?: readonly { value: number, label: string }[], values_from?: string }[], value_lists?: Record<string, readonly { value: number, label: string }[]> } | null} schema
 * @param {string} id
 * @returns {{ value: number, label: string }[]}
 */
export function settingChoices(schema, id) {
	const rows = (schema && schema.items) || [];
	for (const row of rows) {
		if (row.id === id) {
			// A list several rows share is stated once, under the name the row gives.
			const shared = row.values_from && schema && schema.value_lists ? schema.value_lists[row.values_from] : undefined;
			return (row.values || shared || []).slice();
		}
	}
	return [];
}

/**
 * The 4:3 modes this box offers, in the order the schema lists them: the
 * page's own words for the values it knows (key) and the schema's label for any
 * other, and the four it knows while the schema has not answered at all.
 *
 * @param {{ items?: readonly { id: string, values?: readonly { value: number, label: string }[], values_from?: string }[], value_lists?: Record<string, readonly { value: number, label: string }[]> } | null} schema
 * @returns {{ value: number, key: string, label: string }[]}
 */
export function aspectModes(schema) {
	if (!schema) {
		return ASPECT_MODES.map(function (one) {
			return { value: one.value, key: one.key, label: '' };
		});
	}
	return settingChoices(schema, 'video_43mode').map(function (o) {
		const known = ASPECT_MODES.find(function (one) { return one.value === o.value; });
		return { value: o.value, key: known === undefined ? '' : known.key, label: o.label };
	});
}

/* The screen that plays a channel in the browser, as the navigation states it.
   Taken from there and not written out, so that the word on the tile is the
   word in the bars: the screen was renamed and its identifier was not, and a
   second spelling here would have been the one place still saying the old
   name. */
const streaming = entryById(areaById('channels'), 'playback');

/**
 * A recording's cover in the box a channel logo takes, drawn whole and smaller
 * where its shape is not the box's.
 *
 * @param {{ id: string, title: string }} props
 * @returns {Web.Drawn}
 */
function Cover(props) {
	const [broken, refuse] = useRefused(props.id);
	if (broken) {
		return null;
	}
	return html`<span class="logo lg now-cover-box">
		<img
			class="now-cover"
			src=${archiveCoverHref(props.id)}
			alt=${t(text, 'now.playback.cover', { title: props.title })}
			onError=${refuse} />
	</span>`;
}

/** @returns {import('../../ui/actions.js').RowAction[]} */
function noActions() {
	return [];
}

/**
 * A recording or a file the movie player shows, in place of the channel.
 *
 * @param {{ shown: import('../../playback.js').Shown, at: number }} props
 * @returns {Web.Drawn}
 */
function PlaybackShown(props) {
	const shown = props.shown;
	const redraw = useState(0)[1];
	const [details, setDetails] = useKept('now:playback-details', false);
	const recording = shown.source === 'recording';
	const list = usePlaylist(shown.id);
	const [here, setHere] = useState(pagePlayer.current());

	useEffect(function () {
		setHere(pagePlayer.current());
		return pagePlayer.subscribe(setHere);
	}, []);

	useEffect(function () {
		if (shown.state !== 'playing') {
			return undefined;
		}
		const timer = window.setInterval(function () { redraw(function (n) { return n + 1; }); }, 1000);
		return function () { window.clearInterval(timer); };
	}, [shown.state, props.at]);

	useEffect(function () {
		const timer = window.setInterval(function () {
			store.reload('GET', '/api/v1/playback').catch(function () { });
		}, kResyncMs);
		return function () { window.clearInterval(timer); };
	}, []);

	useEffect(function () {
		if (recording) {
			ensureCss('/app/screens/recordings/recordings.css');
		}
	}, [recording]);

	const position = positionAt(shown, Date.now() - props.at);
	const name = shown.title || shown.name;
	const acts = tileActions(shown);

	function stop() {
		announce(session.requireLevel('write').then(function () {
			return store.write('POST', '/api/v1/zap', {
				touches: ['/api/v1/playback', '/api/v1/channels/current', '/api/v1/recordings/archive'],
				body: { channel_id: shown.returnsTo, stop_playback: true },
			});
		}), t(text, 'now.playback.stopped'));
	}

	return html`<div class="now-head" data-playback=${shown.source}>
			${recording ? html`<${Cover} id=${shown.id} title=${name} />` : null}
			<div class="now-head-what">
				<p class="now-channel-name">${name}</p>
				<p class="now-channel-facts">
					<span>${t(text, recording ? 'now.playback.recording' : 'now.playback.file')}</span>
					${shown.channel ? html`<span>${shown.channel}</span>` : null}
				</p>
			</div>
			${shown.paused
				? html`<${Dot} kind="warning" word=${t(text, 'now.playback.paused')} />`
				: html`<${Dot} kind="onair" word=${t(text, 'now.card.playing')} />`}
		</div>
		<div class="now-event">
			${shown.duration > 0
				? html`<${Meter}
					label=${t(text, 'now.playback.progress')}
					value=${position}
					max=${shown.duration}
					text=${t(text, 'now.playback.at', { position: timeText(position), duration: timeText(shown.duration) })} />`
				: html`<p class="now-event-when mono now-playback-at">${timeText(position)}</p>`}
		</div>
		<p class="now-buttons">
			${acts.indexOf('stop') !== -1
				? html`<${Button} primary=${true} class="now-act-stop" onClick=${stop}>${t(text, 'now.playback.stop')}<//>`
				: null}
			${acts.indexOf('info') !== -1
				? html`<${Button} class="now-act-info" onClick=${function () { setDetails(true); }}>${t(text, 'now.playback.info')}<//>`
				: null}
			${acts.indexOf('m3u') !== -1 && list !== null
				? html`<a class="btn now-act-m3u" href=${list.href}>${t(text, 'now.act.m3u')}</a>`
				: null}
			${acts.indexOf('browser') !== -1 && !isHere(here, shown.id)
				? html`<${Button} class="now-act-browser" onClick=${function () { playHereNow(shown.id, name); }}>${t(recText, 'rec.archive.here')}<//>`
				: null}
			${acts.indexOf('archive') !== -1
				? html`<a class="btn now-act-archive" href=${hrefFor('recordings', 'archive')}>${t(text, 'now.playback.archive')}</a>`
				: null}
		</p>
		${recording
			? html`<${ArchiveDetails} id=${details ? shown.id : null} actionsOf=${noActions}
				onClose=${function () { setDetails(false); }} />`
			: null}`;
}

/**
 * What is playing, what is on, what comes after it, and what can be done with
 * it without leaving this screen.
 *
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Playing(props) {
	const at = useNowSeconds();
	const channel = useResource('GET', '/api/v1/channels/current');
	const running = useResource('GET', '/api/v1/recordings');
	const standbyStatus = useResource('GET', '/api/v1/system/standby');
	const playback = useResource('GET', '/api/v1/playback');
	const shown = readPlayback(store.lastAnswer(playback));
	const playingBack = isPlayback(shown);
	const playing = playingBack ? null : store.lastAnswer(channel);
	const standby = standbyStatus.data;
	// The card's own empty state, or waking from standby flashes it as a fault.
	const idle = !!channel.error && channel.error.problem.type === kNothingPlaying;
	const id = playing ? channelId(playing.id) : '';

	/* The guide is asked for only once there is a channel to ask about, and
	   the identifier is the one the guide files a schedule under rather than
	   the one the channel list names: two channels sharing a schedule share
	   that second one, which is the whole reason the answer carries both. */
	const guideId = playing ? channelId(playing.epg_id) || id : '';
	const event = useResource('GET', '/api/v1/epg/current',
		{ query: { channel: guideId } }, guideId !== '');
	// The store keeps the answer before a failed read, and that one may be over.
	const on = event.state === store.FAILED ? null : event.data;

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
	// The quarter hour window also holds what has ended, and the tick may be old.
	const drawn = boxNowSeconds();
	for (const row of items) {
		if ((!on || row.id !== on.id) && row.start + row.duration > drawn) {
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
		   the only place on the screen that wants it. The time is read after
		   signing in, because a sign in can outlast the programme. */
		announce(session.requireWrite().then(function () {
			const from = boxNowSeconds();
			const end = programmeEnd(on, from);
			const ends = end > 0
				? Promise.resolve(end)
				: store.load('GET', '/api/v1/settings/{section}', { params: { section: 'recording' } })
					.then(function (answer) {
						const hours = Number(settingValue(answer, 'record_hours'));
						const runs = Number.isFinite(hours) && hours > 0 ? hours : RECORD_HOURS_FALLBACK;
						return from + runs * 3600;
					}, function () {
						return from + RECORD_HOURS_FALLBACK * 3600;
					});
			return ends.then(function (stop) {
				return store.write('POST', '/api/v1/timers', {
					body: recordNowBody(playing, on, from, stop),
					touches: touchesRecordings,
				});
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
		title=${t(text, standby && standby.on ? 'now.card.standby' : 'now.card.playing')}
		snapshot=${standby && standby.on ? standbyStatus
			: (playingBack ? playback : (idle ? Object.assign({}, channel, { error: null }) : channel))}
		wide=${true}
		lead=${true}
		slot=${props.slot}>
		${standby && standby.on
			? html`<p class="now-empty">${t(text, 'now.standby.state')}</p>`
			: playingBack
			? html`<${PlaybackShown} shown=${shown} at=${playback.at} />`
			: playing
				? html`<div class="now-head">
					<${Logo} channel=${playing} size="lg" />
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
						? html`<${Button} primary=${true} class="now-act-recordstop" onClick=${stopRecording}>${t(text, 'now.act.recordstop')}<//>`
						: html`<${Button} primary=${true} class="now-act-record" onClick=${startRecording}>${t(text, 'now.act.record')}<//>`}
					${shifting
						? html`<${Button} class="now-act-timeshiftstop" onClick=${stopShift}>${t(text, 'now.act.timeshiftstop')}<//>`
						: html`<${Button} class="now-act-timeshift" onClick=${startShift}>${t(text, 'now.act.timeshift')}<//>`}
					${/* The file for a player elsewhere, and the same channel played
					     here, side by side: they are the two ways of watching what
					     is on without the television, and somebody who wants one
					     of them was looking for the other a moment before. */''}
					<a class="btn now-act-m3u" href=${buildUrl('/api/v1/stream/playlist/{id}', { id: playing.id }, null)}>
						${t(text, 'now.act.m3u')}
					</a>
					${streaming
						? html`<a class="btn now-act-browser" href=${hrefFor('channels', 'playback', playing.id)}>${labelOf(streaming)}</a>`
						: null}
					<a class="btn now-act-zap" href=${hrefFor('channels', 'list')}>${t(text, 'now.act.zap')}</a>
				</p>`
				: html`<p class="now-empty">${t(text, 'now.channel.none')}</p>`}
	<//>`;
}

/**
 * How loud the box is, and whether it is making any sound at all.
 *
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Sound(props) {
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
			toast(refusalLine(failed && failed.problem) || t(text, 'now.failed'), 'bad');
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
			toast(refusalLine(failed && failed.problem) || t(text, 'now.failed'), 'bad');
		});
	}

	return html`<${Card} title=${t(text, 'now.card.sound')} snapshot=${sound} slot=${props.slot}>
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
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function BoxCard(props) {
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

	/* The command and its words are the header's power key's (app/ui/power.js),
	   so the two ways of sending it are one. */
	/**
	 * @param {boolean} on
	 * @returns {void}
	 */
	function send(on) {
		setAsking(false);
		sendStandby(on);
	}

	const mode = rest.data;

	return html`<${Card} title=${t(text, 'now.card.box')} snapshot=${info} slot=${props.slot}>
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
					label=${t(text, 'now.box.on')}
					hint=${mode.on ? t(text, 'now.box.standby') : ''}
					checked=${!mode.on}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) {
						if (!e.currentTarget.checked) {
							setAsking(true);
							return;
						}
						send(false);
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
			title=${t(shell, 'shell.power.standby')}
			onCancel=${function () { setAsking(false); }}
			onConfirm=${function () { send(true); }}>
			<p>${t(shell, 'shell.power.ask')}</p>
		<//>
	<//>`;
}

/**
 * Note somebody at a browser puts on the television without walking over.
 *
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Message(props) {
	const [message, setMessage] = useState('');
	const [waits, setWaits] = useState(false);
	const standbyStatus = useResource('GET', '/api/v1/system/standby');
	const standby = standbyStatus.data;

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

	return html`<${Card} title=${t(text, 'now.card.message')} snapshot=none slot=${props.slot}>
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
			disabled=${(standby && standby.on) || message === ''}
			onClick=${sendMessage}>${t(text, 'now.message.send')}<//>
	<//>`;
}

/**
 * What the box is writing to a disc right now, and what it is going to write
 * next.
 *
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Recordings(props) {
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
		empty=${all.data && rows.length === 0 ? t(text, 'now.rec.none') : undefined}
		slot=${props.slot}>
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
 * Only what is mounted from a device node under /dev is listed: the disks and
 * flash a recording can land on. /boot and anything under it is left out, as
 * no recording goes there. Pseudo filesystems, network shares and /boot are on
 * the mount points screen.
 *
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Storage(props) {
	const all = useResource('GET', '/api/v1/storage/mounts');
	const rows = ((all.data && all.data.items) || []).filter(function (/** @type {Api.Mount} */ row) {
		return typeof row.device === 'string' && row.device.startsWith('/dev/')
			&& row.id !== '/boot' && !row.id.startsWith('/boot/');
	}).sort(function (/** @type {Api.Mount} */ a, /** @type {Api.Mount} */ b) {
		return a.id.localeCompare(b.id);
	});

	return html`<${Card} double=${true}
		title=${t(text, 'now.card.storage')}
		snapshot=${all}
		empty=${all.data && rows.length === 0 ? t(text, 'now.store.none') : undefined}
		slot=${props.slot}>
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
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Reception(props) {
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

	const slot = props.slot || null;

	return html`<section
		class="now-card now-double"
		ref=${box}
		aria-label=${t(text, 'now.card.reception')}
		data-card=${slot ? slot.id : null}
		data-drop=${slot && slot.drop ? slot.drop : null}
		data-carried=${slot && slot.carried ? 'yes' : null}>
		<${Title} text=${t(text, 'now.card.reception')} slot=${slot} />
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
 * @param {{ slot?: import('./parts.js').Slot | null }} props
 * @returns {Web.Drawn}
 */
function Quick(props) {
	const schema = useResource('GET', '/api/v1/settings/schema');
	const video = useResource('GET', '/api/v1/settings/{section}', { params: { section: 'video' } });
	const [asking, setAsking] = useState(false);
	// A resolution the TV cannot show leaves a black screen, so it is asked for first.
	const [switching, setSwitching] = useState(/** @type {{ value: number, label: string } | null} */ (null));
	const modes = settingChoices(schema.data, 'video_Mode');
	const aspects = aspectModes(schema.data);
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

	return html`<${Card} title=${t(text, 'now.card.quick')} snapshot=${video} double=${true} slot=${props.slot}>
		<div class="now-quick">
			<span class="now-quick-label">${t(text, 'now.quick.mode')}</span>
			${modes.length === 0
				? html`<p class="now-empty">${t(text, 'now.quick.mode.unknown')}</p>`
				: html`<div class="now-quick-set">
					${modes.map(function (one) {
						return html`<${Button}
							key=${one.value}
							primary=${String(one.value) === mode}
							onClick=${function () {
								if (String(one.value) !== mode)
									setSwitching(one);
							}}>${one.label}<//>`;
					})}
				</div>`}
		</div>
		<div class="now-quick">
			<span class="now-quick-label">${t(text, 'now.quick.43')}</span>
			${aspects.length === 0
				? html`<p class="now-empty">${t(text, 'now.quick.43.none')}</p>`
				: html`<div class="now-quick-set">
					${aspects.map(function (one) {
						return html`<${Button}
							key=${one.value}
							primary=${String(one.value) === aspect}
							onClick=${function () { writeVideo('video_43mode', one.value); }}>${one.key === '' ? one.label : t(text, one.key)}<//>`;
					})}
				</div>`}
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
		<${Dialog}
			open=${switching !== null}
			title=${t(text, 'now.quick.mode')}
			onCancel=${function () { setSwitching(null); }}
			onConfirm=${function () {
				const one = switching;
				setSwitching(null);
				if (one !== null)
					writeVideo('video_Mode', one.value);
			}}>
			<p>${t(text, 'now.quick.mode.ask', { mode: switching === null ? '' : switching.label })}</p>
		<//>
	<//>`;
}

/* Which card each name in the order draws, and what its grip is called. */
/** @type {Record<string, (props: { slot?: import('./parts.js').Slot | null }) => Web.Drawn>} */
const CARD_OF = {
	playing: Playing,
	box: BoxCard,
	sound: Sound,
	message: Message,
	recordings: Recordings,
	storage: Storage,
	reception: Reception,
	quick: Quick,
};

/** @type {Record<string, string>} */
const NAME_OF = {
	playing: 'now.card.playing',
	box: 'now.card.box',
	sound: 'now.card.sound',
	message: 'now.card.message',
	recordings: 'now.card.recording',
	storage: 'now.card.storage',
	reception: 'now.card.reception',
	quick: 'now.card.quick',
};

/* How far the pointer travels before this is a drag rather than a press, the
   same allowance the bouquet screen gives a hand that trembles. */
const SLACK = 4;

/* How close to the top or the bottom of the scrolling pane a card being
   carried has to come before the pane scrolls under it, and by how much each
   frame. On a telephone the cards are one column several screens long, and a
   card that can only be dropped on what is already in view could never be
   carried from the top to the bottom. */
const EDGE = 48;
const EDGE_STEP = 12;

/**
 * Where a card being carried would land, as the grid reads under a point.
 *
 * @typedef {object} Aim
 * @property {string} id the card it would land against
 * @property {boolean} after behind that card rather than in front of it
 * @property {boolean} alone whether that card has its row to itself
 */

/**
 * @typedef {object} Held
 * @property {string} id the card being carried
 * @property {number} x where the press began
 * @property {number} y
 * @property {number} atX where the pointer is now
 * @property {number} atY
 * @property {boolean} moved whether it has travelled further than SLACK
 * @property {Aim | null} aim
 */

/**
 * The cards in the order this browser left them, each with a grip.
 *
 * THE GRIP IS THE ONE PLACE A DRAG BEGINS, for the reason the bouquet screen
 * gives (channels/drag.js): a card that started one wherever it was pressed
 * could not be scrolled past on a telephone, and its sliders could not be
 * moved. AND IT IS THE SECOND WAY AND NEVER THE ONLY ONE: the arrow keys on a
 * grip move its card one place, and the place it lands is read out.
 *
 * The order is written down as soon as a card lands. There is no draft and
 * nothing to save, because nothing about it reaches the box.
 *
 * @returns {Web.Drawn}
 */
function Arranged() {
	const [order, setOrder] = useState(keptOrder);
	const [carry, setCarry] = useState(/** @type {{ id: string, aim: Aim | null } | null} */ (null));
	const [said, setSaid] = useState('');
	const grid = useRef(/** @type {HTMLDivElement | null} */ (null));
	const held = useRef(/** @type {Held | null} */ (null));
	const frame = useRef(0);
	// The card whose grip the keyboard was on, to be given it back once drawn.
	const refocus = useRef('');
	const hint = useId();

	/* A card moved with the keyboard is a node the draw has taken out of the
	   grid and put back somewhere else, and a node taken out loses the focus.
	   Given back here, after the draw, so the next press of the arrow moves the
	   same card again. */
	useEffect(function () {
		const id = refocus.current;
		refocus.current = '';
		if (id === '' || !grid.current) {
			return undefined;
		}
		const grip = grid.current.querySelector('[data-card="' + id + '"] .now-grip');
		if (grip instanceof HTMLElement && document.activeElement !== grip) {
			grip.focus();
		}
		return undefined;
	}, [order]);

	useEffect(function () {
		return function () {
			if (frame.current !== 0) {
				window.cancelAnimationFrame(frame.current);
			}
		};
	}, []);

	/**
	 * @param {string} id
	 * @returns {string}
	 */
	function nameOf(id) {
		return t(text, NAME_OF[id] || 'now.card.box');
	}

	/**
	 * @param {readonly string[]} next
	 * @param {string} id
	 * @returns {void}
	 */
	function land(next, id) {
		if (next === order) {
			return;
		}
		setOrder(next);
		keepOrder(next);
		setSaid(t(text, 'now.arrange.moved', {
			card: nameOf(id), pos: next.indexOf(id) + 1, count: next.length,
		}));
	}

	/**
	 * The card under a point, read off the document, and which side of it.
	 *
	 * Nothing is aimed at that would leave the order as it is, so the line that
	 * says where a card lands is drawn only where letting go would move it.
	 *
	 * @param {string} id the card being carried
	 * @param {number} x
	 * @param {number} y
	 * @returns {Aim | null}
	 */
	function aimAt(id, x, y) {
		const whole = grid.current;
		const under = document.elementFromPoint(x, y);
		const card = under ? under.closest('[data-card]') : null;
		if (!whole || !card || !whole.contains(card)) {
			return null;
		}
		const target = card.getAttribute('data-card') || '';
		if (target === '' || target === id) {
			return null;
		}
		const box = card.getBoundingClientRect();
		// Most of the grid's width, and not all of it: a border is a border.
		const alone = box.width > whole.getBoundingClientRect().width * 0.75;
		const after = behind(box, x, y, alone);
		if (dropped(order, id, target, after) === order) {
			return null;
		}
		return { id: target, after: after, alone: alone };
	}

	/**
	 * Draws where the card would land, and only when that has changed: the
	 * pointer moves a great deal more often than the place does, and every draw
	 * here is a draw of all eight cards.
	 *
	 * @param {Held} now
	 * @param {Aim | null} aim
	 * @returns {void}
	 */
	function aimFor(now, aim) {
		const was = now.aim;
		now.aim = aim;
		if (carry && carry.id === now.id && (was === aim || (was && aim
			&& was.id === aim.id && was.after === aim.after && was.alone === aim.alone))) {
			return;
		}
		setCarry({ id: now.id, aim: aim });
	}

	/**
	 * @returns {void}
	 */
	function stopScrolling() {
		if (frame.current !== 0) {
			window.cancelAnimationFrame(frame.current);
			frame.current = 0;
		}
	}

	/**
	 * Scrolls the pane while a carried card is held against its top or its
	 * bottom, and reads what is under the pointer again as it goes, since a
	 * pointer that holds still over a moving page is over something else.
	 *
	 * @returns {void}
	 */
	function scrollAtEdge() {
		const now = held.current;
		const pane = grid.current ? grid.current.closest('.content') : null;
		if (!now || !now.moved || !pane) {
			stopScrolling();
			return;
		}
		const box = pane.getBoundingClientRect();
		const by = now.atY < box.top + EDGE ? -EDGE_STEP : (now.atY > box.bottom - EDGE ? EDGE_STEP : 0);
		if (by === 0) {
			stopScrolling();
			return;
		}
		if (frame.current !== 0) {
			return;
		}
		/** @returns {void} */
		function step() {
			const still = held.current;
			if (!still) {
				frame.current = 0;
				return;
			}
			const edge = pane ? pane.getBoundingClientRect() : box;
			const again = still.atY < edge.top + EDGE ? -EDGE_STEP : (still.atY > edge.bottom - EDGE ? EDGE_STEP : 0);
			if (again === 0 || !pane) {
				frame.current = 0;
				return;
			}
			pane.scrollBy(0, again);
			aimFor(still, aimAt(still.id, still.atX, still.atY));
			frame.current = window.requestAnimationFrame(step);
		}
		frame.current = window.requestAnimationFrame(step);
	}

	/**
	 * @param {string} id
	 * @param {PointerEvent} event
	 * @returns {void}
	 */
	function grab(id, event) {
		// The main button, or a finger or a pen. The others open menus.
		if (event.pointerType === 'mouse' && event.button !== 0) {
			return;
		}
		const grip = /** @type {Element & { setPointerCapture?: (id: number) => void }} */ (event.currentTarget);
		// Everything after this reaches the grip whatever it is over, which is
		// what makes one gesture out of a press and everything after it.
		if (grip && typeof grip.setPointerCapture === 'function') {
			grip.setPointerCapture(event.pointerId);
		}
		event.preventDefault();
		held.current = {
			id: id, x: event.clientX, y: event.clientY,
			atX: event.clientX, atY: event.clientY, moved: false, aim: null,
		};
	}

	/**
	 * @param {PointerEvent} event
	 * @returns {void}
	 */
	function slide(event) {
		const now = held.current;
		if (!now) {
			return;
		}
		event.preventDefault();
		now.atX = event.clientX;
		now.atY = event.clientY;
		if (!now.moved && Math.abs(now.atX - now.x) <= SLACK && Math.abs(now.atY - now.y) <= SLACK) {
			return;
		}
		now.moved = true;
		aimFor(now, aimAt(now.id, now.atX, now.atY));
		scrollAtEdge();
	}

	/**
	 * @returns {void}
	 */
	function letGo() {
		const done = held.current;
		held.current = null;
		stopScrolling();
		setCarry(null);
		if (!done || !done.moved || !done.aim) {
			return;
		}
		land(dropped(order, done.id, done.aim.id, done.aim.after), done.id);
	}

	/**
	 * @returns {void}
	 */
	function giveUp() {
		held.current = null;
		stopScrolling();
		setCarry(null);
	}

	/**
	 * The arrow keys on a grip: up and left in front of the card before it,
	 * down and right behind the one after it, which in a grid read from the
	 * left and down is the same thing.
	 *
	 * @param {string} id
	 * @param {KeyboardEvent} event
	 * @returns {void}
	 */
	function onKey(id, event) {
		const key = event.key;
		let by = 0;
		if (key === 'ArrowUp' || key === 'ArrowLeft') {
			by = -1;
		} else if (key === 'ArrowDown' || key === 'ArrowRight') {
			by = 1;
		} else if (key === 'Escape' && held.current) {
			event.preventDefault();
			giveUp();
			return;
		} else {
			return;
		}
		event.preventDefault();
		const next = shifted(order, id, by);
		if (next === order) {
			return;
		}
		refocus.current = id;
		land(next, id);
	}

	/**
	 * @param {string} id
	 * @returns {import('./parts.js').Slot}
	 */
	function slotOf(id) {
		const aim = carry ? carry.aim : null;
		/** @type {import('./parts.js').Slot['drop']} */
		let drop = '';
		if (aim && aim.id === id) {
			drop = aim.alone ? (aim.after ? 'below' : 'above') : (aim.after ? 'right' : 'left');
		}
		return {
			id: id,
			drop: drop,
			carried: !!(carry && carry.id === id && carry.aim),
			grip: html`<button
				type="button"
				class="now-grip"
				aria-label=${t(text, 'now.arrange.grip', { card: nameOf(id) })}
				aria-describedby=${hint}
				title=${t(text, 'now.arrange.grip', { card: nameOf(id) })}
				onPointerDown=${function (/** @type {PointerEvent} */ e) { grab(id, e); }}
				onPointerMove=${slide}
				onPointerUp=${letGo}
				onPointerCancel=${giveUp}
				onKeyDown=${function (/** @type {KeyboardEvent} */ e) { onKey(id, e); }}>⠿</button>`,
		};
	}

	function reset() {
		if (isOwn(order)) {
			return;
		}
		setOrder(CARDS);
		keepOrder(CARDS);
		setSaid(t(text, 'now.arrange.reset.done'));
	}

	return html`<div class="now-grid now-arranging" ref=${grid}>
			${order.map(function (id) {
				const Drawn = CARD_OF[id];
				return Drawn ? html`<${Drawn} key=${id} slot=${slotOf(id)} />` : null;
			})}
		</div>
		<div class="now-arrange">
			<p id=${hint} class="now-hint">${t(text, 'now.arrange.hint')}</p>
			<${Button} disabled=${isOwn(order)} onClick=${reset}>${t(text, 'now.arrange.reset')}<//>
			<p class="sr" role="status">${said}</p>
		</div>`;
}

/**
 * The cards as the page orders them, or as this browser has arranged them where
 * it has chosen to (app/ui/arrange.js). The first is the default, and it is the
 * overview exactly as it was drawn before the second existed.
 *
 * @returns {Web.Drawn}
 */
export default function Overview() {
	const [arranging, setArranging] = useState(chosenArrange());

	useEffect(function () {
		return onArrange(setArranging);
	}, []);

	if (arranging) {
		return html`<${Arranged} />`;
	}
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
