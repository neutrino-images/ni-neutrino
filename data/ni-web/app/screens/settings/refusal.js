/* What the box answered to a write that landed in part, and what the page says of it.
 *
 * The answer is a status for the write as a whole and one entry per setting it names, each
 * with the code the box refused it by. The code is what is worded here, in the language of
 * the page and with the name of the row beside it; the box's own sentence is English and
 * stays a last resort for a code this page has no words for.
 *
 * Nothing here reaches the box.
 */
import { t } from '../../i18n.js';
import text from './settings.text.js';

/**
 * One setting the box turned down.
 *
 * @typedef {Object} Refusal
 * @property {string} key
 * @property {string} code
 * @property {string} detail
 * @property {string[]} dependsOn the settings whose condition failed, empty where the box names none
 */

/**
 * @param {unknown} results the results member of a 207
 * @returns {{ landed: string[], refused: Refusal[] }}
 */
export function outcomeOf(results) {
	/** @type {string[]} */
	const landed = [];
	/** @type {Refusal[]} */
	const refused = [];
	if (results === null || typeof results !== 'object')
		return { landed: landed, refused: refused };
	const all = /** @type {Record<string, { status?: unknown, code?: unknown, detail?: unknown, depends_on?: unknown } | undefined>} */ (results);
	for (const key of Object.keys(all)) {
		const one = all[key];
		const status = one === undefined ? 0 : Number(one.status);
		if (status >= 200 && status < 300) {
			landed.push(key);
			continue;
		}
		refused.push({
			key: key,
			code: one === undefined || typeof one.code !== 'string' ? '' : one.code,
			detail: one === undefined || typeof one.detail !== 'string' ? '' : one.detail,
			dependsOn: one === undefined || !Array.isArray(one.depends_on)
				? [] : one.depends_on.filter(function (id) { return typeof id === 'string'; })
		});
	}
	return { landed: landed, refused: refused };
}

/**
 * How many of the settings that were sent were turned down. A setting the box added on its
 * own account, a coupling, is in the answer and was not sent, so it is not one of the count.
 *
 * @param {Refusal[]} refused
 * @param {string[]} sent
 * @returns {number}
 */
export function refusedOfSent(refused, sent) {
	return refused.filter(function (one) { return sent.indexOf(one.key) !== -1; }).length;
}

/**
 * The line for one refusal.
 *
 * @param {Refusal} one
 * @param {(key: string) => string} nameOf the words of a setting, and its key where the page has none
 * @param {(key: string) => string[]} [conditionKeysOf] the settings a row's own conditions read. A refusal
 *   naming others is the box keeping settings together that no condition ties, and "change that one
 *   first" would be wrong advice for it.
 * @returns {string}
 */
export function refusalText(one, nameOf, conditionKeysOf) {
	const name = nameOf(one.key);
	if (one.code === 'setting-condition-not-met' && one.dependsOn.length > 0) {
		const own = conditionKeysOf === undefined ? null : conditionKeysOf(one.key);
		const byCondition = own === null || one.dependsOn.every(function (key) { return own.indexOf(key) !== -1; });
		return t(text, 'settings.rejected', {
			label: name,
			reason: t(text, byCondition ? 'settings.code.setting-condition-not-met.on' : 'settings.code.setting-condition-not-met.with',
				{ names: one.dependsOn.map(nameOf).join(', ') })
		});
	}
	const known = 'settings.code.' + one.code;
	const words = t(text, known);
	return t(text, 'settings.rejected', {
		label: name,
		reason: words === known ? (one.detail === '' ? one.code : one.detail) : words
	});
}

/**
 * What the box said when it took a value and could not put it in force.
 *
 * @param {{ keys: string[], detail: string }} event
 * @param {(key: string) => string} nameOf
 * @returns {string}
 */
export function applyFailedText(event, nameOf) {
	return t(text, event.detail === '' ? 'settings.applyfailed.bare' : 'settings.applyfailed', {
		names: event.keys.map(nameOf).join(', '),
		detail: event.detail
	});
}
