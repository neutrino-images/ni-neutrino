// The box's own power key, small, in the header beside the session.
//
// THE SAME COMMAND AS THE SWITCH ON THE BOX CARD AND NOT A SECOND ONE. It
// posts the address that card posts, under the same level, and going into
// standby asks first exactly as the card does, since it takes the picture off
// the television. Waking does not ask. The card stays where it is: it says the
// state in words, and this says it only in a colour and in its name.
//
// IT ASKS FIRST ON EVERY PRESS, not only the first one. A key twenty eight
// pixels wide in the corner a pointer crosses on its way to the destinations
// is a key that gets pressed by accident, and an accident here is a dark
// television in somebody else's room.
//
// THE SENDING AND ITS WORDS LIVE HERE, AND THE CARD BORROWS BOTH. Two copies
// of one command are two that drift, and its sentences stood in now.text.js
// and in shell.text.js word for word. The card imports sendStandby and the
// frame's catalogue, so there is one of each.
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
import { toast } from './toast.js';

/**
 * @param {boolean} on true sends the box into standby, false wakes it
 * @returns {Promise<void>}
 */
export function sendStandby(on) {
	return session.requireSystem().then(function () {
		return store.write('POST', '/api/v1/system/standby', {
			body: { on: on },
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

/**
 * @param {{ standby?: boolean | null }} props null while the box has not said
 * @returns {Web.Drawn}
 */
export function Power(props) {
	const [asking, setAsking] = useState(false);
	const known = props.standby === true || props.standby === false;
	const sleeping = props.standby === true;
	const what = t(text, sleeping ? 'shell.power.wake' : 'shell.power.standby');

	function press() {
		if (!known)
			return;
		if (sleeping) {
			sendStandby(false);
			return;
		}
		setAsking(true);
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
		<${Dialog}
			open=${asking}
			title=${t(text, 'shell.power.standby')}
			confirmLabel=${t(text, 'shell.power.standby')}
			onCancel=${function () { setAsking(false); }}
			onConfirm=${function () { setAsking(false); sendStandby(true); }}>
			<p>${t(text, 'shell.power.ask')}</p>
		<//>
	</span>`;
}
