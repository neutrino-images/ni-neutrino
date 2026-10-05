// The words for GET /api/v1/ai/guides, keyed on the box's fixed ids.
import { t } from '../app/i18n.js';
import text from './guidewords.text.js';

const kMaxSteps = 9;

/** @param {string} prefix @param {Record<string, string>} values @returns {string[]} */
function stepsUnder(prefix, values) {
	/** @type {string[]} */
	const out = [];
	for (let i = 1; i <= kMaxSteps; ++i) {
		const key = prefix + i;
		const line = t(text, key, values);
		if (line === key)
			break;
		out.push(line);
	}
	return out;
}

/**
 * A tunnel's own steps, then the two every tunnel shares; empty for an unknown id.
 * @param {string} id
 * @param {{ host: string, url: string }} values
 * @returns {string[]}
 */
export function tunnelSteps(id, values) {
	const own = stepsUnder('ai.tunnel.' + id + '.step', values);
	if (own.length === 0)
		return own;
	return own.concat([t(text, 'ai.tunnel.step.proxy', values), t(text, 'ai.tunnel.step.check', values)]);
}

/**
 * How to get the fixed name with DynDNS: a step of the own-address way, without a tunnel's checks.
 * @param {{ host: string, url: string }} values
 * @returns {string[]}
 */
export function nameSteps(values) {
	return stepsUnder('ai.tunnel.dyndns.step', values);
}

/** @param {string} id @param {{ url: string }} values @returns {string[]} */
export function clientSteps(id, values) {
	return stepsUnder('ai.client.' + id + '.step', values);
}

/** @param {string} id @returns {string} */
export function tunnelTitle(id) { return t(text, 'ai.tunnel.' + id + '.title'); }

/** @param {string} id @returns {string} */
export function fileText(id) { return t(text, 'ai.tunnel.' + id + '.file'); }

/** @param {string} id @returns {string} */
export function clientTitle(id) { return t(text, 'ai.client.' + id + '.title'); }

/** @param {string} id @returns {string} where a client's snippet goes */
export function clientFile(id) { return t(text, 'ai.client.' + id + '.file'); }

/** @param {string} id @returns {string} */
export function warningText(id) { return t(text, 'ai.warning.' + id); }

/** @param {string} needs public or token @param {boolean} ready @returns {string} */
export function noteText(needs, ready) { return t(text, 'ai.note.' + needs + (ready ? '.ready' : '.waiting')); }

/** @returns {string} */
export function tokenText() { return t(text, 'ai.tokens'); }

/** @param {string} code @returns {string} */
export function errorText(code) { return t(text, 'ai.error.' + code); }
