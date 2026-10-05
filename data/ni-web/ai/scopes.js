// Levels include each other, so a choice stays cumulative: write without read is a promise the box does not keep.

export const LEVELS = ['read', 'write', 'system'];
export const OFFLINE = 'offline_access';
export const KNOWN = LEVELS.concat([OFFLINE]);

/**
 * @param {readonly string[]} list
 * @returns {string[]} the known scopes in it, in level order, each once
 */
export function canonical(list) {
	return KNOWN.filter(function (one) { return list.indexOf(one) !== -1; });
}

/**
 * @param {readonly string[]} chosen
 * @param {string} scope
 * @param {boolean} on
 * @param {readonly string[]} offered
 * @returns {string[]}
 */
export function toggle(chosen, scope, on, offered) {
	const rank = LEVELS.indexOf(scope);
	/** @type {string[]} */
	let next = chosen.slice();
	if (rank === -1) {
		next = on ? next.concat([scope]) : next.filter(function (one) { return one !== scope; });
	} else if (on) {
		next = next.concat(LEVELS.slice(0, rank + 1));
	} else {
		next = next.filter(function (one) {
			const r = LEVELS.indexOf(one);
			return r === -1 || r < rank;
		});
	}
	return canonical(next).filter(function (one) { return offered.indexOf(one) !== -1; });
}

/**
 * @param {readonly string[]} chosen
 * @param {readonly string[]} offered
 * @returns {string}
 */
export function scopeString(chosen, offered) {
	return canonical(chosen).filter(function (one) { return offered.indexOf(one) !== -1; }).join(' ');
}

/**
 * @param {readonly string[]} chosen
 * @returns {boolean}
 */
export function hasLevel(chosen) {
	return chosen.some(function (one) { return LEVELS.indexOf(one) !== -1; });
}
