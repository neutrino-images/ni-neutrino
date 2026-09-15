/* The form, worked out from what the box says about itself.

   Nothing in this file knows the name of a single setting. What it knows is the shape the
   box declares them in: an identifier, a kind, a section, the text to put beside it,
   bounds for a number, the set a choice offers, the value it falls back to, whether the
   box has to be restarted, whether it is a credential, and the comparisons that decide
   whether it is worth showing. So a setting added to the tables in
   src/coreapi/settings/settingstable_*.cpp appears here without a line written for it.

   Four things that look like details and are not. A row without a label is not drawn: the
   endpoint leaves the member out where the box offers the setting on no screen of its own,
   and a form that fell back to the identifier would put start_volume on screen as a word.
   A hint is not drawn either, because what arrives under that name is the name of a text
   and not the text. An empty set of choices means the box could not be asked at the moment
   the schema was read, and a write is refused for as long as that lasts, so the row is
   shown and locked. A credential has no value, and no value is not the empty value: this
   file carries null for them everywhere a value would otherwise be. */

/**
 * @typedef {Object} Row
 * @property {string} id
 * @property {'bool'|'int'|'string'|'enum'} type
 * @property {string} label
 * @property {string} section
 * @property {boolean} secret
 * @property {boolean} needsRestart
 * @property {string} fallback what the box falls back to, empty for a credential
 * @property {number|null} min
 * @property {number|null} max
 * @property {{ value: number, label: string }[]} choices
 * @property {boolean} locked whether the box could not state what this takes
 * @property {import('./model.js').Condition[]} conditions
 */

/**
 * @typedef {Object} Condition
 * @property {string} key
 * @property {string} op
 * @property {number[]} values
 */

/** what the schema calls a setting, before this file has read it */
/** @typedef {{ id?: unknown, type?: unknown, section?: unknown, label?: unknown, min?: unknown, max?: unknown, values?: unknown, default?: unknown, needs_restart?: unknown, secret?: unknown, conditions?: unknown }} Declared */

const kTypes = ['bool', 'int', 'string', 'enum'];

/**
 * One declared setting, as this screen reads it.
 *
 * Undrawable rows are answered as null rather than repaired: a row with no label is one the
 * box deliberately left unnamed, and a kind this page has never heard of is a server newer
 * than this file.
 *
 * @param {Declared} declared
 * @returns {Row | null}
 */
export function rowOf(declared) {
	if (!declared || typeof declared !== 'object')
		return null;

	const id = typeof declared.id === 'string' ? declared.id : '';
	const label = typeof declared.label === 'string' ? declared.label : '';
	const section = typeof declared.section === 'string' ? declared.section : '';
	const type = typeof declared.type === 'string' ? declared.type : '';
	if (id === '' || label === '' || kTypes.indexOf(type) === -1)
		return null;

	const kind = /** @type {'bool'|'int'|'string'|'enum'} */ (type);
	const offered = Array.isArray(declared.values) ? declared.values : [];
	/** @type {{ value: number, label: string }[]} */
	const choices = [];
	for (const one of offered) {
		if (!one || typeof one !== 'object')
			continue;
		const value = Number(/** @type {{ value?: unknown }} */ (one).value);
		if (!Number.isFinite(value))
			continue;
		const said = /** @type {{ label?: unknown }} */ (one).label;
		choices.push({ value: value, label: typeof said === 'string' ? said : String(value) });
	}

	return {
		id: id,
		type: kind,
		label: label,
		section: section,
		secret: declared.secret === true,
		needsRestart: declared.needs_restart === true,
		fallback: typeof declared['default'] === 'string' ? declared['default'] : '',
		min: kind === 'int' && Number.isFinite(Number(declared.min)) ? Number(declared.min) : null,
		max: kind === 'int' && Number.isFinite(Number(declared.max)) ? Number(declared.max) : null,
		choices: choices,
		// Only a choice can be locked this way. Every other kind states what it
		// takes in the row itself, so there is nothing the box could have
		// failed to answer.
		locked: kind === 'enum' && choices.length === 0,
		conditions: conditionsOf(declared.conditions),
	};
}

/**
 * @param {unknown} declared
 * @returns {Condition[]}
 */
function conditionsOf(declared) {
	/** @type {Condition[]} */
	const out = [];
	if (!Array.isArray(declared))
		return out;
	for (const one of declared) {
		if (!one || typeof one !== 'object')
			continue;
		const said = /** @type {{ key?: unknown, op?: unknown, values?: unknown }} */ (one);
		const key = typeof said.key === 'string' ? said.key : '';
		const op = typeof said.op === 'string' ? said.op : '';
		if (key === '' || op === '')
			continue;
		/** @type {number[]} */
		const numbers = [];
		if (Array.isArray(said.values)) {
			for (const value of said.values) {
				const n = Number(value);
				if (Number.isFinite(n))
					numbers.push(n);
			}
		}
		out.push({ key: key, op: op, values: numbers });
	}
	return out;
}

/**
 * Every drawable row of the whole schema, in the order the box states them.
 *
 * @param {{ items?: unknown } | null} answer
 * @returns {Row[]}
 */
export function rowsOf(answer) {
	/** @type {Row[]} */
	const out = [];
	const items = answer && Array.isArray(answer.items) ? answer.items : [];
	for (const one of items) {
		const row = rowOf(/** @type {Declared} */ (one));
		if (row !== null)
			out.push(row);
	}
	return out;
}

/**
 * @param {Row[]} rows
 * @param {string} section
 * @returns {Row[]}
 */
export function rowsOfSection(rows, section) {
	return rows.filter(function (row) { return row.section === section; });
}

/**
 * What the box is running on, by identifier.
 *
 * A credential is left out rather than entered as empty text, so everything downstream
 * reads "not known" and nothing reads "known to be nothing". That difference is the whole
 * of what marking a row secret buys.
 *
 * @param {{ items?: unknown } | null} answer
 * @param {Row[]} rows
 * @returns {Record<string, string>}
 */
export function valuesOf(answer, rows) {
	/** @type {Record<string, boolean>} */
	const withheld = {};
	for (const row of rows) {
		if (row.secret)
			withheld[row.id] = true;
	}

	/** @type {Record<string, string>} */
	const out = {};
	const items = answer && Array.isArray(answer.items) ? answer.items : [];
	for (const one of items) {
		if (!one || typeof one !== 'object')
			continue;
		const said = /** @type {{ id?: unknown, value?: unknown }} */ (one);
		if (typeof said.id !== 'string' || said.id === '')
			continue;
		if (withheld[said.id] === true)
			continue;
		out[said.id] = typeof said.value === 'string' ? said.value : '';
	}
	return out;
}

/**
 * What the box holds for one row, and null where it does not say.
 *
 * @param {Row} row
 * @param {Record<string, string>} values
 * @returns {string | null}
 */
export function valueOf(row, values) {
	if (row.secret)
		return null;
	const held = values[row.id];
	return held === undefined ? null : held;
}

/**
 * Whether one comparison holds.
 *
 * A comparison this cannot carry out is answered true. It names a setting whose value is
 * not in front of this screen, which happens when it lives on another page, and hiding a
 * field on a fact nobody has is how a setting becomes unreachable with nothing on screen to
 * say why.
 *
 * @param {Condition} condition
 * @param {Record<string, string>} values
 * @returns {boolean}
 */
export function conditionHolds(condition, values) {
	const held = values[condition.key];
	if (held === undefined)
		return true;
	const value = Number(held);
	if (!Number.isFinite(value))
		return true;

	const first = condition.values.length > 0 ? condition.values[0] : undefined;
	switch (condition.op) {
		case 'eq': return first !== undefined && value === first;
		case 'ne': return first !== undefined && value !== first;
		case 'lt': return first !== undefined && value < first;
		case 'le': return first !== undefined && value <= first;
		case 'gt': return first !== undefined && value > first;
		case 'ge': return first !== undefined && value >= first;
		case 'in': return condition.values.indexOf(value) !== -1;
		// An operator this page has never heard of is a server newer than this
		// file, and the field stays on screen rather than disappearing on a
		// comparison nobody here can carry out.
		default: return true;
	}
}

/**
 * All of them together, which is what the box means by them.
 *
 * @param {Row} row
 * @param {Record<string, string>} values
 * @returns {boolean}
 */
export function isVisible(row, values) {
	for (const condition of row.conditions) {
		if (!conditionHolds(condition, values))
			return false;
	}
	return true;
}

/**
 * What is on screen and what is not, out of a section and what it is set to.
 *
 * The values a condition reads are the ones being edited and not the ones the box last
 * answered, so a field appears the moment the switch above it is turned and not after a
 * save.
 *
 * @param {Row[]} rows
 * @param {Record<string, string>} values
 * @returns {Row[]}
 */
export function shownRows(rows, values) {
	return rows.filter(function (row) { return isVisible(row, values); });
}

/**
 * What a control shows, which is what was typed into it where something was.
 *
 * @param {Row} row
 * @param {Record<string, string>} values
 * @param {Record<string, string>} edits
 * @returns {string}
 */
export function shownValue(row, values, edits) {
	const typed = edits[row.id];
	if (typed !== undefined)
		return typed;
	const held = valueOf(row, values);
	return held === null ? '' : held;
}

/**
 * The values every condition is read against: what the box says, with what has
 * been typed over the top.
 *
 * @param {Record<string, string>} values
 * @param {Record<string, string>} edits
 * @returns {Record<string, string>}
 */
export function effective(values, edits) {
	/** @type {Record<string, string>} */
	const out = {};
	for (const id of Object.keys(values))
		out[id] = /** @type {string} */ (values[id]);
	for (const id of Object.keys(edits))
		out[id] = /** @type {string} */ (edits[id]);
	return out;
}

/**
 * What is sent, and why it is only this.
 *
 * Only what somebody changed, never the section. A write that carries the whole page writes
 * back every value it read, including the one a person at the box changed while this page
 * was open.
 *
 * A credential is in here only when something was typed into it. Its declared value is
 * nothing and it reads as nothing, so a field left alone drops out by the same rule
 * everything else does. That is why the way to empty one is a route of its own.
 *
 * @param {Row[]} rows
 * @param {Record<string, string>} values
 * @param {Record<string, string>} edits
 * @returns {Record<string, string>}
 */
export function changed(rows, values, edits) {
	/** @type {Record<string, string>} */
	const out = {};
	const shown = shownRows(rows, effective(values, edits));
	for (const row of shown) {
		const typed = edits[row.id];
		if (typed === undefined)
			continue;
		// A row the box cannot state the choices of refuses every write, so a
		// value for one is left out rather than sent to be turned down.
		if (row.locked)
			continue;
		const held = valueOf(row, values);
		if (held !== null && typed === held)
			continue;
		if (row.secret && typed === '')
			continue;
		out[row.id] = typed;
	}
	return out;
}

/**
 * @param {Record<string, string>} body
 * @returns {number}
 */
export function countOf(body) {
	return Object.keys(body).length;
}

/**
 * What a number field says when what is in it is not a number the row takes.
 * Null when there is nothing to say.
 *
 * @param {Row} row
 * @param {string} text
 * @returns {'empty'|'notnumber'|'range'|null}
 */
export function numberFault(row, text) {
	if (row.type !== 'int')
		return null;
	if (text === '')
		return 'empty';
	if (!/^-?[0-9]+$/.test(text))
		return 'notnumber';
	const value = Number(text);
	if (row.min !== null && value < row.min)
		return 'range';
	if (row.max !== null && value > row.max)
		return 'range';
	return null;
}

/**
 * Whether what the box holds differs from what it falls back to.
 *
 * Null for a credential, and null is the answer this view prints rather than hides: there
 * is no value to compare, so saying "unchanged" would be a claim about something nobody
 * here can see.
 *
 * @param {Row} row
 * @param {Record<string, string>} values
 * @returns {boolean | null}
 */
export function driftsFromDefault(row, values) {
	const held = valueOf(row, values);
	if (held === null)
		return null;
	return held !== row.fallback;
}

/**
 * How the value the box falls back to is put on screen. A choice is named by the words the
 * box gave it and not by the number it stores, the number being the one thing on this
 * screen nobody outside the program has ever seen.
 *
 * @param {Row} row
 * @returns {string}
 */
export function fallbackLabel(row) {
	if (row.type !== 'enum')
		return row.fallback;
	const wanted = Number(row.fallback);
	for (const choice of row.choices) {
		if (choice.value === wanted)
			return choice.label;
	}
	return row.fallback;
}

/**
 * The search, over the whole schema and not over one section.
 *
 * Over the identifier as well as over the words, because the identifier is what somebody
 * arrives with: it is what stands in the settings file, what a forum post names, and what
 * the old interface put in its URLs.
 *
 * @param {Row[]} rows
 * @param {string} query
 * @returns {Row[]}
 */
export function search(rows, query) {
	const wanted = query.trim().toLowerCase();
	if (wanted === '')
		return [];
	return rows.filter(function (row) {
		return row.id.toLowerCase().indexOf(wanted) !== -1
			|| row.label.toLowerCase().indexOf(wanted) !== -1;
	});
}
