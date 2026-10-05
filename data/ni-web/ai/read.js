// The AI routes may answer untyped; a member of the wrong type reads as empty, never as a throw.

/**
 * @param {unknown} v
 * @returns {Record<string, unknown> | null}
 */
export function obj(v) {
	return (v !== null && typeof v === 'object' && !Array.isArray(v))
		? /** @type {Record<string, unknown>} */ (v) : null;
}

/**
 * @param {unknown} v
 * @returns {string}
 */
export function str(v) {
	return typeof v === 'string' ? v : '';
}

/**
 * @param {unknown} v
 * @returns {number}
 */
export function num(v) {
	return (typeof v === 'number' && isFinite(v)) ? v : 0;
}

/**
 * @param {unknown} v
 * @returns {string[]}
 */
export function strings(v) {
	if (!Array.isArray(v))
		return [];
	return v.filter(function (one) { return typeof one === 'string'; });
}

/**
 * The box spells its code into the problem type as /errors/<code>.
 *
 * @param {unknown} caught
 * @returns {string} the problem code, empty when the box named none
 */
export function codeOf(caught) {
	const said = obj(caught);
	const problem = said ? obj(said.problem) : null;
	const type = problem ? str(problem.type) : '';
	return type.indexOf('/errors/') === 0 ? type.slice(8) : '';
}

/**
 * Which of this area's own sentences stands for a failure: what the box wrote is never drawn.
 *
 * @param {unknown} caught
 * @returns {string} a catalogue key
 */
export function refusalKey(caught) {
	const said = obj(caught);
	const problem = said ? obj(said.problem) : null;
	const status = problem ? num(problem.status) : 0;
	if (status === 401)
		return 'ai.refused.signin';
	if (status === 403)
		return 'ai.refused.forbidden';
	if (status === 409)
		return 'ai.refused.conflict';
	if (status >= 400 && status < 500)
		return 'ai.refused.other';
	if (status >= 500)
		return 'ai.refused.box';
	return 'ai.failed';
}

/**
 * Every key of this area starts with "ai.", and a lookup without words answers its key.
 *
 * @param {string} said
 * @returns {boolean}
 */
export function worded(said) {
	return said !== '' && said.indexOf('ai.') !== 0;
}
