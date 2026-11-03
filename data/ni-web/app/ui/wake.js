/* A refused zap, mode change or play is asked about once and sent again with
   leave to wake, end the playback or end the recording on the tuner. */
import { html, useState, useEffect, useId } from '../runtime.js';
import * as store from '../store.js';
import { api } from '../api.js';
import { ensureCss } from '../css.js';
import { t } from '../i18n.js';
import { Dialog } from './dialog.js';
import { tvComesAlong, TvSwitch } from './power.js';
import text from './wake.text.js';

const kInStandby = '/errors/box-in-standby';
const kPlaying = '/errors/playback-running';
const kHoldsTuner = '/errors/recording-holds-tuner';

const kCss = '/app/ui/wake.css';

const kZapTouches = ['/api/v1/channels/current', '/api/v1/epg/current'];

const kModeTouches = ['/api/v1/channels', '/api/v1/bouquets', '/api/v1/epg'];

// An ended recording leaves the list later than the answer.
const kGoneTries = 20;
const kGonePause = 250;

/** @typedef {'switch' | 'play'} WakeFor */
/** @typedef {'wake' | 'playback' | 'recording'} Topic */
/**
 * @typedef {{
 *   topic: Topic,
 *   what: WakeFor,
 *   recordings: Api.Recording[],
 *   tv: boolean,
 *   resolve: (said: Said) => void
 * }} Asker tv: whether the question about waking carries the switch for the
 *   television, which it does where the box would switch it on with it
 */
/** @typedef {{ yes: boolean, chosen: number | null, tv: boolean | null }} Said */
/**
 * @typedef {{ wake: boolean, stop: boolean, freed: boolean, tv: boolean | null }} Leave
 *   tv: what was answered about the television, null where nobody was asked
 */

// Oldest first; askers of one topic share a question, except recordings.
/** @type {Asker[]} */
let waiting = [];
/** @type {Set<(shown: Asker | null) => void>} */
const listeners = new Set();

/** @returns {void} */
function announce() {
	const shown = waiting[0] || null;
	listeners.forEach(function (fn) { fn(shown); });
}

/**
 * @param {Topic} topic
 * @param {WakeFor} what
 * @param {Api.Recording[]} recordings
 * @param {boolean} [tv] whether the switch for the television is offered
 * @returns {Promise<Said>} the answer, and the recording chosen for that question
 */
function ask(topic, what, recordings, tv) {
	return new Promise(function (resolve) {
		waiting.push({ topic: topic, what: what, recordings: recordings, tv: !!tv, resolve: resolve });
		announce();
	});
}

/**
 * @param {boolean} yes
 * @param {number | null} [chosen] the recording to end, for that question
 * @param {boolean} [tv] what the switch for the television said, where it was there
 * @returns {void}
 */
export function answer(yes, chosen, tv) {
	const first = waiting[0];
	if (!first) {
		return;
	}
	const done = first.topic === 'recording' ? [first] : waiting.filter(function (one) {
		return one.topic === first.topic;
	});
	waiting = waiting.filter(function (one) { return done.indexOf(one) === -1; });
	announce();
	for (const one of done) {
		one.resolve({
			yes: yes,
			chosen: chosen === undefined ? null : chosen,
			tv: one.tv && tv !== undefined ? tv : null,
		});
	}
}

/**
 * @param {unknown} failed
 * @returns {string}
 */
function typeOf(failed) {
	const problem = failed && typeof failed === 'object'
		? /** @type {{ problem?: Api.Problem }} */ (failed).problem : undefined;
	return problem && problem.type ? problem.type : '';
}

/** @param {number} ms */
function pause(ms) {
	return new Promise(function (resolve) { setTimeout(resolve, ms); });
}

/**
 * @returns {Promise<boolean>} false when whoever was asked said no
 */
async function freeTuner() {
	const running = (await api('GET', '/api/v1/recordings')).items;
	if (running.length === 0) {
		return true;
	}
	const said = await ask('recording', 'switch', running);
	const id = said.chosen;
	if (!said.yes || id === null || !running.some(function (one) { return one.id === id; })) {
		return false;
	}
	try {
		await store.write('DELETE', '/api/v1/recordings/{id}', {
			params: { id: id },
			touches: ['/api/v1/recordings'],
		});
	} catch (failed) {
		if (typeOf(failed) !== '/errors/no-such-recording') {
			throw failed;
		}
	}
	for (let i = 0; i < kGoneTries; i++) {
		const now = (await api('GET', '/api/v1/recordings')).items;
		if (!now.some(function (one) { return one.id === id; })) {
			break;
		}
		await pause(kGonePause);
	}
	return true;
}

/**
 * @param {unknown} failed
 * @param {WakeFor} what
 * @param {Leave} leave
 * @returns {Promise<Leave | null>} null once somebody said no
 */
async function grant(failed, what, leave) {
	const type = typeOf(failed);
	if (type === kHoldsTuner && !leave.freed) {
		return (await freeTuner()) ? { wake: leave.wake, stop: leave.stop, freed: true, tv: leave.tv } : null;
	}
	if (type === kPlaying && !leave.stop) {
		return (await ask('playback', what, [])).yes ? { wake: leave.wake, stop: true, freed: leave.freed, tv: leave.tv } : null;
	}
	if (type === kInStandby && !leave.wake) {
		/* Whether waking takes the television along, read when the question is
		   about to be asked and not before, as the power key reads it. */
		const said = await ask('wake', what, [], await tvComesAlong(false));
		return said.yes ? { wake: true, stop: leave.stop, freed: leave.freed, tv: said.tv } : null;
	}
	throw failed;
}

/**
 * @param {Sender} send
 * @param {WakeFor} what
 * @param {Leave} leave
 * @returns {Promise<boolean>}
 */
async function sendWith(send, what, leave) {
	/** @type {Leave | null} */
	let more = null;
	try {
		await send(leave.wake, leave.stop, leave.tv);
		return true;
	} catch (failed) {
		more = await grant(failed, what, leave);
	}
	return more ? sendWith(send, what, more) : false;
}

/**
 * What a caller of waking sends, as often as it is asked to. tv is what was
 * answered about the television, and null where nobody was asked: then it is
 * left out of the body and the box goes by its CEC setting. See withTv.
 * @typedef {(wake: boolean, stop: boolean, tv: boolean | null) => Promise<unknown>} Sender
 */

/**
 * @param {Sender} send
 * @param {WakeFor} [what]
 * @returns {Promise<boolean>} false when whoever was asked said no
 */
export function waking(send, what) {
	return sendWith(send, what || 'switch', { wake: false, stop: false, freed: false, tv: null });
}

/**
 * A body with the answer about the television written in, where there was
 * one. The routes take it as cec beside wake, and false there is a box woken
 * with the television left dark.
 * @template {Record<string, unknown>} B
 * @param {B} body
 * @param {boolean} wake
 * @param {boolean | null} tv
 * @returns {B & { cec?: boolean }}
 */
export function withTv(body, wake, tv) {
	return wake && tv !== null ? Object.assign({}, body, { cec: tv }) : body;
}

/**
 * @param {string} id the channel, hexadecimal
 * @returns {Promise<boolean>} as waking
 */
export function zap(id) {
	return waking(function (wake, stop, tv) {
		return store.write('POST', '/api/v1/zap', {
			touches: kZapTouches,
			body: withTv(stop ? { channel_id: id, wake: wake, stop_playback: true } : { channel_id: id, wake: wake }, wake, tv),
		});
	});
}

/**
 * A mode change leaves the movie player playing.
 *
 * @param {'tv' | 'radio'} mode
 * @returns {Promise<boolean>} as waking
 */
export function switchMode(mode) {
	return waking(function (wake, _stop, tv) {
		return store.write('POST', '/api/v1/mode', {
			touches: kModeTouches,
			body: withTv({ mode: mode, wake: wake }, wake, tv),
		});
	});
}

/**
 * @param {Api.Recording} one
 * @returns {string}
 */
function nameOf(one) {
	if (one.timeshift) {
		return t(text, 'wake.recording.shift');
	}
	return one.title || t(text, 'wake.recording.untitled');
}

/**
 * @param {{ shown: Asker, chosen: number | null, choose: (id: number) => void }} props
 * @returns {Web.Drawn}
 */
function Which(props) {
	const group = useId();
	useEffect(function () { ensureCss(kCss); }, []);
	return html`<fieldset class="text-size wake-which">
		<legend>${t(text, 'wake.recording.which')}</legend>
		${props.shown.recordings.map(function (one) {
			return html`<label key=${one.id}>
				<input
					type="radio"
					name=${group}
					value=${one.id}
					checked=${one.id === props.chosen}
					onChange=${function () { props.choose(one.id); }} />
				<span>${nameOf(one)}</span>
			</label>`;
		})}
	</fieldset>`;
}

/** @returns {Web.Drawn} */
export function WakeQuestion() {
	const [shown, setShown] = useState(waiting[0] || null);
	const [picked, setPicked] = useState(/** @type {{ of: Asker | null, id: number | null }} */ ({ of: null, id: null }));
	// What the switch for the television says, for the question it was moved on.
	const [moved, setMoved] = useState(/** @type {{ of: Asker | null, tv: boolean }} */ ({ of: null, tv: true }));

	useEffect(function () {
		listeners.add(setShown);
		return function () { listeners.delete(setShown); };
	}, []);

	const topic = shown ? shown.topic : 'wake';
	const play = !!shown && shown.what === 'play';
	const recordings = shown ? shown.recordings : [];
	const first = recordings[0];
	const chosen = picked.of === shown && picked.of !== null ? picked.id : (first ? first.id : null);
	/* Ticked as the box is set, which is why it is offered at all: only where
	   waking would switch the television on. */
	const tvOffered = !!shown && topic === 'wake' && shown.tv;
	const tv = moved.of === shown && moved.of !== null ? moved.tv : true;

	/** @type {Record<Topic, string>} */
	const asked = {
		wake: play ? 'wake.ask.play' : 'wake.ask',
		playback: play ? 'wake.playback.ask.play' : 'wake.playback.ask',
		recording: 'wake.recording.ask',
	};

	return html`<${Dialog}
		open=${!!shown}
		title=${t(text, topic === 'wake' ? 'wake.title' : 'wake.' + topic + '.title')}
		confirmLabel=${t(text, topic === 'wake' ? 'wake.confirm' : 'wake.' + topic + '.confirm')}
		onCancel=${function () { answer(false); }}
		onConfirm=${function () { answer(true, chosen, tvOffered ? tv : undefined); }}>
		<p>${t(text, asked[topic])}</p>
		${tvOffered ? html`<${TvSwitch}
			on=${false}
			checked=${tv}
			onChange=${function (/** @type {boolean} */ checked) { setMoved({ of: shown, tv: checked }); }} />` : null}
		${recordings.length > 1 ? html`<${Which} shown=${shown} chosen=${chosen} choose=${function (/** @type {number} */ id) { setPicked({ of: shown, id: id }); }} />` : null}
	<//>`;
}
