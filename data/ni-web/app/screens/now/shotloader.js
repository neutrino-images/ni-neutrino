// The television picture, fetched one capture at a time.
//
// The box refuses a second capture while one runs (409) rather than queueing it,
// so asks made meanwhile are folded into one after it, and a refusal leaves the
// last picture on screen. No imports, so test/web/shot-cases.mjs runs it as is.

// How long after a refused or failed capture it is tried once more.
export const RETRY_MS = 1500;

// The framebuffer read has no deadline of its own; a capture that has not
// answered by then counts as failed so later asks are not held behind it.
export const GIVE_UP_MS = 20000;

/**
 * @typedef {{ onload: (() => void) | null, onerror: (() => void) | null, src: string }} ShotImage
 */

/**
 * @typedef {Object} ShotLoaderOptions
 * @property {(n: number) => string} address the capture's address; n is new for every fetch
 * @property {(src: string, n: number) => void} shown a capture arrived
 * @property {() => void} lost nothing has arrived yet and the capture and its retry both failed
 * @property {() => ShotImage} [image] makes the object a capture loads into
 * @property {(fn: () => void, ms: number) => number} [later]
 * @property {(id: number) => void} [cancel]
 */

/**
 * @param {ShotLoaderOptions} o
 * @returns {{ want: () => void, stop: () => void }}
 */
export function shotLoader(o) {
	const image = o.image || function () { return /** @type {ShotImage} */ (new window.Image()); };
	const later = o.later || function (fn, ms) { return window.setTimeout(fn, ms); };
	const cancel = o.cancel || function (id) { window.clearTimeout(id); };
	let n = 0;
	let busy = false;
	let again = false;
	let ever = false;
	let retry = 0;
	let watch = 0;
	let stopped = false;

	/**
	 * @param {boolean} isRetry
	 * @returns {void}
	 */
	function send(isRetry) {
		busy = true;
		n += 1;
		const at = n;
		const src = o.address(at);
		const img = image();

		/**
		 * @param {boolean} arrived
		 * @returns {void}
		 */
		function settle(arrived) {
			img.onload = img.onerror = null;
			if (watch) {
				cancel(watch);
				watch = 0;
			}
			if (stopped)
				return;
			busy = false;
			if (arrived) {
				ever = true;
				o.shown(src, at);
				next();
				return;
			}
			if (again) {
				next();
				return;
			}
			if (!isRetry) {
				retry = later(function () {
					retry = 0;
					if (!stopped && !busy)
						send(true);
				}, RETRY_MS);
				return;
			}
			if (!ever)
				o.lost();
		}

		img.onload = function () { settle(true); };
		img.onerror = function () { settle(false); };
		watch = later(function () {
			watch = 0;
			// A late answer from this one is ignored.
			if (img.onerror)
				settle(false);
		}, GIVE_UP_MS);
		img.src = src;
	}

	function next() {
		if (!again)
			return;
		again = false;
		send(false);
	}

	return {
		want: function () {
			if (stopped)
				return;
			if (busy) {
				again = true;
				return;
			}
			// A fresh ask replaces a pending retry.
			if (retry) {
				cancel(retry);
				retry = 0;
			}
			send(false);
		},
		stop: function () {
			stopped = true;
			if (retry) {
				cancel(retry);
				retry = 0;
			}
			if (watch) {
				cancel(watch);
				watch = 0;
			}
		},
	};
}

/**
 * One full size picture for saving, tried once more after RETRY_MS if the box
 * was busy taking another. Any other failure, and a second refusal, is thrown.
 *
 * @template T
 * @param {() => Promise<T>} get
 * @param {(fn: () => void, ms: number) => unknown} [later]
 * @returns {Promise<T>}
 */
export function pictureForSaving(get, later) {
	const wait = later || function (fn, ms) { return window.setTimeout(fn, ms); };
	return get().catch(function (/** @type {unknown} */ e) {
		const status = e && typeof e === 'object' && 'status' in e ? /** @type {{ status: unknown }} */ (e).status : 0;
		if (status !== 409)
			throw e;
		return new Promise(function (resolve) { wait(function () { resolve(undefined); }, RETRY_MS); }).then(get);
	});
}
