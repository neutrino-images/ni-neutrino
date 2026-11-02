// The box's own power key, small, in the header beside the session.
//
// THE SAME COMMAND AS THE SWITCH ON THE BOX CARD AND NOT A SECOND ONE. It
// posts the address that card posts, under the same level, and going into
// standby asks first exactly as the card does, since it takes the picture off
// the television. Waking asks only where the box would switch the television
// on with it (see useStandby below). The card stays where it is: it says the
// state in words, and this says it only in a colour and in its name.
//
// IT ASKS FIRST ON EVERY PRESS, not only the first one. A key twenty eight
// pixels wide in the corner a pointer crosses on its way to the destinations
// is a key that gets pressed by accident, and an accident here is a dark
// television in somebody else's room.
//
// THE SENDING, ITS QUESTION AND ITS WORDS LIVE HERE, AND THE CARD BORROWS
// THEM. Two copies of one command are two that drift, and its sentences stood
// in now.text.js and in shell.text.js word for word. The card draws
// useStandby, so there is one of each.
//
// What the box is in comes from the frame (app/main.js), which already watches
// the standby address for the chip beside this; nothing here asks for it a
// second time. Before the first answer the key is drawn and cannot be pressed,
// because a key that guesses is one that sends the box the wrong way.
import { html, useState } from '../runtime.js';
import * as store from '../store.js';
import * as session from '../session.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { refusalLine } from '../problem.js';
import { Dialog } from './dialog.js';
import { Switch } from './switch.js';
import { toast } from './toast.js';

/**
 * @param {boolean} on true sends the box into standby, false wakes it
 * @param {boolean} [tv] whether the television follows over HDMI-CEC, where
 *        somebody was asked; left out, the box's own CEC settings decide
 * @returns {Promise<void>}
 */
export function sendStandby(on, tv) {
	return session.requireSystem().then(function () {
		return store.write('POST', '/api/v1/system/standby', {
			body: tv === undefined ? { on: on } : { on: on, cec: tv },
			touches: ['/api/v1/system/info', '/api/v1/tuner'],
		});
	}).then(function () {
		toast(t(text, on ? 'shell.power.sent.standby' : 'shell.power.sent.wake'), '');
	}, function (failed) {
		/* A sign in sheet that was closed is no refusal by the box. */
		if (failed && failed.aborted === true)
			return;
		toast(refusalLine(failed && failed.problem) || t(text, 'shell.power.failed'), 'bad');
	});
}

/* WHETHER THE TELEVISION GOES WITH THE BOX, which is the box's setting and
   not this page's: HDMI-CEC switched on at all, and then the one of its two
   switches that is about this direction. hdmi_cec_standby switches the
   television off with the box, hdmi_cec_view_on switches it on with it
   (src/coreapi/settings/settingstable_video.cpp). A box that cannot do CEC
   has no such rows, and an empty value is a no. */
/**
 * @param {{ items?: readonly { id: string, value: string }[] } | null | undefined} video
 *        the box's video section
 * @param {boolean} on the direction, as sendStandby takes it
 * @returns {boolean}
 */
export function tvFollows(video, on) {
	/** @type {Record<string, string>} */
	const value = {};
	for (const row of (video && video.items) || [])
		value[row.id] = String(row.value);
	const mode = value.hdmi_cec_mode;
	if (mode === undefined || mode === '' || mode === '0')
		return false;
	return value[on ? 'hdmi_cec_standby' : 'hdmi_cec_view_on'] === '1';
}

/* THE QUESTION BEFORE THE BOX GOES EITHER WAY, as one piece both the key in
   the header and the switch on the box card draw.

   Going into standby always asks, as it always did. Waking asks only where
   the box would switch the television on with it: that is a second room's
   television coming on at night, and whoever wakes the box from a browser
   may want only the box. Where the television follows, the question carries
   a switch saying so, ticked as the box is set, and what it says is sent for
   this one change and nothing more. The setting under video stays as it is.

   Read at the press and not before, because the setting can have changed
   since the page opened, and after the sign in, because the section is not
   read without one and the command is not sent without one either. A box
   that does not answer is taken as one whose television does not follow:
   then the question is the one it always was and the box decides. */
/* WHAT A SCREEN MAY CHANGE ABOUT IT, AND ONLY FOR THE REMOTE. The handset
   presses the box's power key rather than posting the standby address, because
   the box decides what its power key does. So where nobody was asked about the
   television, plain is what is sent, and the key goes as it always went. Where
   somebody was asked, only the standby address can carry the answer, so that
   is what is sent then: a key has no way to say "and leave the television".
   gate is the sign in asked for before the settings are read, which for keys
   is writing and not the system level the address wants; the address asks for
   its own when it is the one that is sent. */
/**
 * @param {{ plain?: (on: boolean) => void, gate?: () => Promise<unknown> }} [how]
 * @returns {{ request: (on: boolean) => void, question: Web.Drawn }}
 */
export function useStandby(how) {
	const plain = (how && how.plain) || function (/** @type {boolean} */ on) { sendStandby(on); };
	const gate = (how && how.gate) || session.requireSystem;
	const [asking, setAsking] = useState(/** @type {{ on: boolean, tv: boolean } | null} */ (null));
	const [tv, setTv] = useState(true);

	/**
	 * @param {boolean} on
	 * @returns {void}
	 */
	function request(on) {
		gate().then(function () {
			return store.load('GET', '/api/v1/settings/{section}', { params: { section: 'video' } }).then(function (video) {
				return tvFollows(/** @type {any} */ (video), on);
			}, function () { return false; });
		}).then(function (follows) {
			if (!on && !follows) {
				plain(false);
				return;
			}
			setTv(true);
			setAsking({ on: on, tv: follows });
		}, function () { /* The sign in was closed; nothing was asked for. */ });
	}

	const on = asking ? asking.on : true;
	const title = t(text, on ? 'shell.power.standby' : 'shell.power.wake');

	const question = html`<${Dialog}
		open=${asking !== null}
		title=${title}
		confirmLabel=${title}
		onCancel=${function () { setAsking(null); }}
		onConfirm=${function () {
			const was = asking;
			setAsking(null);
			if (was && was.tv)
				sendStandby(was.on, tv);
			else if (was)
				plain(was.on);
		}}>
		<p>${t(text, on ? 'shell.power.ask' : 'shell.power.wake.ask')}</p>
		${asking && asking.tv ? html`<${Switch}
			label=${t(text, on ? 'shell.power.cec.off' : 'shell.power.cec.on')}
			hint=${t(text, 'shell.power.cec.once')}
			checked=${tv}
			onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { setTv(e.currentTarget.checked); }} />` : null}
	<//>`;

	return { request: request, question: question };
}

/**
 * @param {{ standby?: boolean | null }} props null while the box has not said
 * @returns {Web.Drawn}
 */
export function Power(props) {
	const standby = useStandby();
	const known = props.standby === true || props.standby === false;
	const sleeping = props.standby === true;
	const what = t(text, sleeping ? 'shell.power.wake' : 'shell.power.standby');

	function press() {
		if (!known)
			return;
		standby.request(!sleeping);
	}

	return html`<span class="power">
		<button
			type="button"
			class="power-key"
			data-standby=${known ? (sleeping ? 'on' : 'off') : null}
			disabled=${!known}
			title=${what}
			aria-label=${what}
			onClick=${press}>
			<svg viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="M12 3.5v8" /><path d="M7.4 6.6a7.5 7.5 0 1 0 9.2 0" /></svg>
		</button>
		${standby.question}
	</span>`;
}
