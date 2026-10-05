// Whether a picture was refused, for whatever draws one the box may not have.
import { useState } from '../runtime.js';

/**
 * Whether the picture drawn for this key was refused, and the handler that
 * says so.
 *
 * Held against the key and not as a flag. A refusal the browser already holds
 * arrives before the first effect runs, so an effect clearing a flag on the
 * way in would throw that answer away, and no second one comes for the same
 * address. Held against the key, it stops applying when the key changes.
 *
 * @param {string} key
 * @returns {[boolean, () => void]}
 */
export function useRefused(key) {
	const [failed, setFailed] = useState(/** @type {string | null} */ (null));
	return [failed === key, function () { setFailed(key); }];
}
