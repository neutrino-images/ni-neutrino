/* The document the server writes about itself, read.
 *
 * Every screen of this destination draws out of the one answer, so the reading
 * of it is here and the drawing is next door: what these functions decide can
 * be asserted with no box and no browser, which is the whole reason they are
 * not written inside the screens.
 *
 * NOTHING HERE TRUSTS THE SHAPE. The document is generated from the route
 * tables and is right today, and it is still an answer off a network: a member
 * that is not there is a group that is not drawn, never a read off undefined.
 *
 * WHY IT IS READ THROUGH A CAST WHERE IT IS FETCHED. The route that hands the
 * document out declares no shape for it (src/httpd/doc/openapi.cpp), because an
 * OpenAPI document is not a thing the field tables can describe, so what the
 * generated client knows about that address is that it answers nothing. This
 * file is the shape, stated once, and the cast is at the one line that fetches.
 */

/**
 * @typedef {object} Operation
 * @property {string} method as a caller writes it, in upper case
 * @property {string} path the template, exactly as the document spells it
 * @property {string} group the tag the document files it under
 * @property {string} summary what the route says it does, empty for one that
 *           says nothing
 * @property {string} level what a caller has to carry, empty where the
 *           document does not say
 */

/**
 * @typedef {object} Group
 * @property {string} id
 * @property {string} about what the document says the group is, empty for one
 *           the document says nothing about
 * @property {Operation[]} operations
 */

/* The methods in the order a reader expects them, which is the order they do
   less and then more in. A method outside this list sorts after all of them
   rather than being dropped, because a server that grew one is a server this
   page should still list whole. */
const kMethodOrder = ['GET', 'HEAD', 'OPTIONS', 'POST', 'PUT', 'PATCH', 'DELETE'];

/**
 * @param {string} method
 * @returns {number}
 */
function methodRank(method) {
	const at = kMethodOrder.indexOf(method);
	return at === -1 ? kMethodOrder.length : at;
}

/**
 * Every route the document names, in the document's own order of paths and by
 * method inside one path.
 *
 * @param {unknown} answer
 * @returns {Operation[]}
 */
export function operationsOf(answer) {
	/** @type {Operation[]} */
	const out = [];
	if (!answer || typeof answer !== 'object')
		return out;
	const paths = /** @type {{ paths?: unknown }} */ (answer).paths;
	if (!paths || typeof paths !== 'object')
		return out;

	for (const path of Object.keys(paths)) {
		const byMethod = /** @type {Record<string, unknown>} */ (
			/** @type {Record<string, unknown>} */ (paths)[path]);
		if (!byMethod || typeof byMethod !== 'object')
			continue;
		/** @type {Operation[]} */
		const here = [];
		for (const method of Object.keys(byMethod)) {
			const one = /** @type {{ tags?: unknown, summary?: unknown, 'x-auth-level'?: unknown }} */ (
				byMethod[method]);
			if (!one || typeof one !== 'object')
				continue;
			const tags = Array.isArray(one.tags) ? one.tags : [];
			const level = one['x-auth-level'];
			here.push({
				method: method.toUpperCase(),
				path: path,
				// The first tag and not all of them: the document files a route
				// under one group, and a second tag would be a route that is in
				// two places on this page and counted twice on the next.
				group: typeof tags[0] === 'string' ? tags[0] : '',
				summary: typeof one.summary === 'string' ? one.summary : '',
				level: typeof level === 'string' ? level : ''
			});
		}
		here.sort(function (a, b) { return methodRank(a.method) - methodRank(b.method); });
		for (const one of here)
			out.push(one);
	}
	return out;
}

/**
 * What the document says each of its groups is, keyed by the group's name.
 *
 * @param {unknown} answer
 * @returns {Record<string, string>}
 */
export function aboutGroups(answer) {
	/** @type {Record<string, string>} */
	const out = {};
	if (!answer || typeof answer !== 'object')
		return out;
	const tags = /** @type {{ tags?: unknown }} */ (answer).tags;
	if (!Array.isArray(tags))
		return out;
	for (const one of tags) {
		if (!one || typeof one !== 'object')
			continue;
		const said = /** @type {{ name?: unknown, description?: unknown }} */ (one);
		if (typeof said.name !== 'string')
			continue;
		out[said.name] = typeof said.description === 'string' ? said.description : '';
	}
	return out;
}

/**
 * The routes gathered under the groups the document declares, in the order the
 * document declares them, with anything filed under a group it never declared
 * after them.
 *
 * @param {unknown} answer
 * @returns {Group[]}
 */
export function groupsOf(answer) {
	const operations = operationsOf(answer);
	const about = aboutGroups(answer);

	/** @type {Record<string, Operation[]>} */
	const held = {};
	/** @type {string[]} */
	const order = [];
	for (const name of Object.keys(about)) {
		held[name] = [];
		order.push(name);
	}
	for (const one of operations) {
		let here = held[one.group];
		if (here === undefined) {
			here = [];
			held[one.group] = here;
			order.push(one.group);
		}
		here.push(one);
	}

	/** @type {Group[]} */
	const out = [];
	for (const name of order) {
		const rows = held[name] || [];
		// A group the document declares and files nothing under is not drawn:
		// a heading with nothing beneath it says something went missing.
		if (rows.length === 0)
			continue;
		out.push({ id: name, about: about[name] || '', operations: rows });
	}
	return out;
}

/**
 * What a route's level is filed under, which is a name of this page's own
 * where the document says nothing: a route whose level is unstated is still a
 * row somebody has to be able to find, and the empty string is not something a
 * filter can be set to.
 *
 * @param {Operation} one
 * @returns {string}
 */
export function levelOf(one) {
	return one.level === '' ? 'unsaid' : one.level;
}

/**
 * How many routes carry each level, the levels in the order they grant more.
 *
 * @param {readonly Operation[]} operations
 * @returns {{ id: string, count: number }[]}
 */
export function byLevel(operations) {
	const order = ['public', 'read', 'write', 'system'];
	/** @type {Record<string, number>} */
	const counts = {};
	/** @type {string[]} */
	const seen = [];
	for (const one of operations) {
		const id = levelOf(one);
		if (counts[id] === undefined) {
			counts[id] = 0;
			seen.push(id);
		}
		counts[id] = (counts[id] || 0) + 1;
	}
	seen.sort(function (a, b) {
		const ra = order.indexOf(a);
		const rb = order.indexOf(b);
		return (ra === -1 ? order.length : ra) - (rb === -1 ? order.length : rb);
	});
	return seen.map(function (id) { return { id: id, count: counts[id] || 0 }; });
}

/**
 * The routes left after the three things the screen asks about: a word, a
 * group and a level.
 *
 * The word is matched on the address and on what the route says it does,
 * because a person looking for the way to stop a recording knows the word and
 * not the address. The other two are exact, and an empty one of them is not a
 * filter at all.
 *
 * @param {readonly Operation[]} operations
 * @param {string} query
 * @param {string} group empty for every group
 * @param {string} level empty for every level
 * @returns {Operation[]}
 */
export function matching(operations, query, group, level) {
	const wanted = query.trim().toLowerCase();
	return operations.filter(function (one) {
		if (group !== '' && one.group !== group)
			return false;
		if (level !== '' && levelOf(one) !== level)
			return false;
		if (wanted === '')
			return true;
		return one.path.toLowerCase().indexOf(wanted) !== -1
			|| one.summary.toLowerCase().indexOf(wanted) !== -1
			|| one.method.toLowerCase() === wanted;
	});
}
