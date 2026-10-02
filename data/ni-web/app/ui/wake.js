/* A zap or a mode change the box refuses in standby, asked about once and sent
   again with wake. One place, so every control asks the same way. */

import { html, useState, useEffect } from '../runtime.js';
import * as store from '../store.js';
import { t } from '../i18n.js';
import { Dialog } from './dialog.js';
import text from './wake.text.js';

const kInStandby = '/errors/box-in-standby';

// A zap leaves the running channel and its guide answer stale.
const kZapTouches = ['/api/v1/channels/current', '/api/v1/epg/current'];

const kModeTouches = ['/api/v1/channels', '/api/v1/bouquets', '/api/v1/epg'];

// Two controls refused at once share one question.
/** @type {Array<(yes: boolean) => void>} */
let waiting = [];
/** @type {Set<(open: boolean) => void>} */
const listeners = new Set();

/** @returns {void} */
function announce() {
	const open = waiting.length > 0;
	listeners.forEach(function (fn) { fn(open); });
}

/** @returns {Promise<boolean>} */
function ask() {
	return new Promise(function (resolve) {
		waiting.push(resolve);
		announce();
	});
}

/**
 * @param {boolean} yes
 * @returns {void}
 */
export function answer(yes) {
	const done = waiting;
	waiting = [];
	announce();
	for (const one of done) {
		one(yes);
	}
}

/**
 * @param {unknown} failed
 * @returns {boolean}
 */
function inStandby(failed) {
	const problem = failed && typeof failed === 'object'
		? /** @type {{ problem?: Api.Problem }} */ (failed).problem : undefined;
	return !!problem && problem.type === kInStandby;
}

/**
 * Sends without leave to wake, and asks for it only when the box says it is in
 * standby.
 *
 * @param {(wake: boolean) => Promise<unknown>} send
 * @returns {Promise<boolean>} true once the box has it, false when whoever was
 *          asked said no
 */
async function waking(send) {
	try {
		await send(false);
		return true;
	} catch (failed) {
		if (!inStandby(failed)) {
			throw failed;
		}
	}
	if (!(await ask())) {
		return false;
	}
	await send(true);
	return true;
}

/**
 * The box, on that channel.
 *
 * @param {string} id the channel, hexadecimal
 * @returns {Promise<boolean>} as waking
 */
export function zap(id) {
	return waking(function (wake) {
		return store.write('POST', '/api/v1/zap', {
			touches: kZapTouches,
			body: { channel_id: id, wake: wake },
		});
	});
}

/**
 * The box, on one of its two lists.
 *
 * @param {'tv' | 'radio'} mode
 * @returns {Promise<boolean>} as waking
 */
export function switchMode(mode) {
	return waking(function (wake) {
		return store.write('POST', '/api/v1/mode', {
			touches: kModeTouches,
			body: { mode: mode, wake: wake },
		});
	});
}

/** @returns {Web.Drawn} */
export function WakeQuestion() {
	const [open, setOpen] = useState(waiting.length > 0);

	useEffect(function () {
		listeners.add(setOpen);
		return function () { listeners.delete(setOpen); };
	}, []);

	return html`<${Dialog}
		open=${open}
		title=${t(text, 'wake.title')}
		confirmLabel=${t(text, 'wake.confirm')}
		onCancel=${function () { answer(false); }}
		onConfirm=${function () { answer(true); }}>
		<p>${t(text, 'wake.ask')}</p>
	<//>`;
}
