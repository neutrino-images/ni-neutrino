/* The form, worked out from what the box says about itself.

   Nothing in this file knows the name of a single setting. What it knows is the shape the
   box declares them in: an identifier, a kind, a section, the text to put beside it,
   bounds for a number, the set a choice offers, the value it falls back to, whether the
   box has to be restarted, whether it is a credential, and the comparisons that decide
   whether it is worth showing. So a setting added to the tables in
   src/coreapi/settings/settingstable_*.cpp appears here without a line written for it.

   Five things that look like details and are not. A row the box lacks the hardware for,
   which the schema marks not available, is not drawn: every write of it is refused. A row
   without a label is not drawn either: the
   endpoint leaves the member out where the box offers the setting on no screen of its own,
   and a form that fell back to the identifier would put start_volume on screen as a word.
   A hint is not drawn either, because what arrives under that name is the name of a text
   and not the text. An empty set of choices means the box could not be asked at the moment
   the schema was read, and a write is refused for as long as that lasts, so the row is
   shown and locked, as is a row the box's parental lock holds. A credential has no value,
   and no value is not the empty value: this file carries null for them everywhere a value
   would otherwise be. */

/**
 * @typedef {Object} Row
 * @property {string} id
 * @property {'bool'|'int'|'string'|'enum'|'key'} type
 * @property {string} label
 * @property {string} section
 * @property {boolean} secret
 * @property {boolean} needsRestart
 * @property {string} fallback what the box falls back to, empty for a credential
 * @property {number|null} min
 * @property {number|null} max
 * @property {string} unit the name of the text that follows the number, empty for none
 * @property {number|null} channels how many channels a colour has, null for any other row
 * @property {{ value: number, label: string, text?: string }[]} choices text is what a string row stores for the entry, absent for a number
 * @property {boolean} listed an int whose choices are every number it takes, drawn as that list
 * @property {boolean} locked whether no write of it can land, for either reason below
 * @property {boolean} held whether the box's parental lock holds it
 * @property {string} pair the key of the setting this one is written with, empty for none
 * @property {''|'both'|'id'} pairWrites both: neither is taken without the other. id: the identifier alone is taken and the box fills the name
 * @property {''|'box'|'follows'} readOnly box: only the box itself sets it. follows: it follows another row of the page
 * @property {''|'tv'|'radio'} channelKind the kind of channel the box says the row names, empty where it says nothing
 * @property {''|'tv'|'radio'} picker the kind of channel the row is chosen from, empty for any other row
 * @property {string} partner for a pair written whole, the key of the member drawn together with this one
 * @property {string} textKind the sort of text a string holds, empty where the box states no rule
 * @property {number} minLength fewest bytes of a string, 0 for none
 * @property {number} maxLength most bytes of a string, 0 for no limit
 * @property {string} allowedChars the only characters a string takes, empty for any
 * @property {import('./model.js').Condition[]} conditions
 */

/**
 * One comparison against another setting. text is the placeholder text-valid holds the
 * setting's text against, and empty for every other operator.
 *
 * @typedef {Object} Comparison
 * @property {string} key
 * @property {string} op
 * @property {number[]} values
 * @property {string} text
 */

/**
 * A comparison, or a group that holds when any of its comparisons does.
 *
 * @typedef {Comparison | { any: Comparison[] }} Condition
 */

/** what the schema calls a setting, before this file has read it */
/** @typedef {{ id?: unknown, type?: unknown, section?: unknown, label?: unknown, min?: unknown, max?: unknown, unit?: unknown, channels?: unknown, values?: unknown, values_from?: unknown, listed?: unknown, default?: unknown, needs_restart?: unknown, secret?: unknown, locked?: unknown, available?: unknown, conditions?: unknown, pair?: unknown, pair_writes?: unknown, channel_kind?: unknown, text_kind?: unknown, min_length?: unknown, max_length?: unknown, allowed_chars?: unknown }} Declared */

const kTypes = ['bool', 'int', 'string', 'enum', 'key', 'color'];

/**
 * One declared setting, as this screen reads it.
 *
 * Undrawable rows are answered as null rather than repaired: a row with no label is one the
 * box deliberately left unnamed, one not available is one this box lacks, and a kind this
 * page has never heard of is a server newer than this file.
 *
 * A list the box stated once for several rows is looked up by the name the row gives, in the
 * lists of the answer. A member the answer leaves out is read as its default: not locked, not
 * secret, not a path, no restart, available, no conditions.
 *
 * @param {Declared} declared
 * @param {Record<string, unknown>} [lists]
 * @returns {Row | null}
 */
export function rowOf(declared, lists) {
	if (!declared || typeof declared !== 'object')
		return null;

	const id = typeof declared.id === 'string' ? declared.id : '';
	const label = typeof declared.label === 'string' ? declared.label : '';
	const section = typeof declared.section === 'string' ? declared.section : '';
	const type = typeof declared.type === 'string' ? declared.type : '';
	const pair = typeof declared.pair === 'string' ? declared.pair : '';
	const pairWrites = declared.pair_writes === 'both' || declared.pair_writes === 'id' ? declared.pair_writes : '';
	/* The identifier of a pair written by id carries no label of its own: the name beside
	   it does, and rowsOf lends it to the identifier. Kept here, dropped there if the name
	   is not on the page. */
	const lent = label === '' && pair !== '' && pairWrites === 'id';
	if (id === '' || (label === '' && !lent) || kTypes.indexOf(type) === -1)
		return null;
	// Only an explicit no: a server older than the member says nothing about it.
	if (declared.available === false)
		return null;

	/* A colour is drawn as text. A key keeps its own kind: its names are a list of their
	   own, put on the row by withKeyNames, and until that arrives it is a number. */
	const kind = /** @type {'bool'|'int'|'string'|'enum'|'key'} */ (type === 'color' ? 'string' : type);
	const shared = typeof declared.values_from === 'string' && lists ? lists[declared.values_from] : undefined;
	const offered = Array.isArray(declared.values) ? declared.values : Array.isArray(shared) ? shared : [];
	/** @type {{ value: number, label: string, text?: string }[]} */
	const choices = [];
	for (const one of offered) {
		if (!one || typeof one !== 'object')
			continue;
		// A string row's entry stands for its text and has no number to read.
		if (kind === 'string') {
			const stands = /** @type {{ text?: unknown, label?: unknown }} */ (one);
			if (typeof stands.text === 'string')
				choices.push({ value: 0, text: stands.text, label: typeof stands.label === 'string' && stands.label !== '' ? stands.label : stands.text });
			continue;
		}
		const value = Number(/** @type {{ value?: unknown }} */ (one).value);
		if (!Number.isFinite(value))
			continue;
		const said = /** @type {{ label?: unknown }} */ (one).label;
		choices.push({ value: value, label: typeof said === 'string' ? said : String(value) });
	}

	const held = declared.locked === true;
	return {
		id: id,
		type: kind,
		label: label,
		section: section,
		secret: declared.secret === true,
		needsRestart: declared.needs_restart === true,
		fallback: typeof declared['default'] === 'string' ? declared['default'] : '',
		min: (kind === 'int' || kind === 'key') && Number.isFinite(Number(declared.min)) ? Number(declared.min) : null,
		max: (kind === 'int' || kind === 'key') && Number.isFinite(Number(declared.max)) ? Number(declared.max) : null,
		unit: kind === 'int' && typeof declared.unit === 'string' ? declared.unit : '',
		channels: type === 'color' && (declared.channels === 3 || declared.channels === 4) ? declared.channels : null,
		choices: choices,
		listed: kind === 'int' && declared.listed === true && choices.length > 0,
		// Only a choice can lack its values. Every other kind states what it
		// takes in the row itself, so there is nothing the box could have
		// failed to answer.
		locked: held || (kind === 'enum' && choices.length === 0),
		held: held,
		pair: pair,
		pairWrites: pairWrites,
		readOnly: '',
		channelKind: channelKindOf(declared),
		picker: '',
		partner: '',
		textKind: kind === 'string' && typeof declared.text_kind === 'string' ? declared.text_kind : '',
		minLength: kind === 'string' && Number.isFinite(Number(declared.min_length)) ? Number(declared.min_length) : 0,
		maxLength: kind === 'string' && Number.isFinite(Number(declared.max_length)) ? Number(declared.max_length) : 0,
		allowedChars: kind === 'string' && typeof declared.allowed_chars === 'string' ? declared.allowed_chars : '',
		conditions: conditionsOf(declared.conditions),
	};
}

/**
 * The kind of channel a row names when the box says so, in either member it may use.
 *
 * @param {Declared} declared
 * @returns {''|'tv'|'radio'}
 */
function channelKindOf(declared) {
	for (const one of [declared.channel_kind, declared.channels]) {
		if (one === 'tv' || one === 'radio')
			return one;
	}
	return '';
}

/**
 * @param {unknown} one
 * @returns {Comparison | null}
 */
function comparisonOf(one) {
	if (!one || typeof one !== 'object')
		return null;
	const said = /** @type {{ key?: unknown, op?: unknown, values?: unknown, text?: unknown }} */ (one);
	const key = typeof said.key === 'string' ? said.key : '';
	const op = typeof said.op === 'string' ? said.op : '';
	if (key === '' || op === '')
		return null;
	/** @type {number[]} */
	const numbers = [];
	if (Array.isArray(said.values)) {
		for (const value of said.values) {
			const n = Number(value);
			if (Number.isFinite(n))
				numbers.push(n);
		}
	}
	return { key: key, op: op, values: numbers, text: typeof said.text === 'string' ? said.text : '' };
}

/**
 * A member this page cannot read is kept as a comparison naming no setting, which holds and
 * so makes its group hold, the way the box answers a member it cannot read. An empty group
 * is dropped, which leaves the row shown: the box answers that the same way.
 *
 * @param {unknown} declared
 * @returns {Condition[]}
 */
function conditionsOf(declared) {
	/** @type {Condition[]} */
	const out = [];
	if (!Array.isArray(declared))
		return out;
	for (const one of declared) {
		const group = one && typeof one === 'object' ? /** @type {{ any?: unknown }} */ (one).any : undefined;
		if (!Array.isArray(group)) {
			const comparison = comparisonOf(one);
			if (comparison !== null)
				out.push(comparison);
			continue;
		}
		/** @type {Comparison[]} */
		const any = [];
		for (const member of group) {
			const comparison = comparisonOf(member);
			any.push(comparison !== null ? comparison : { key: '', op: '', values: [], text: '' });
		}
		if (any.length > 0)
			out.push({ any: any });
	}
	return out;
}

/**
 * Every drawable row of the whole schema, in the order the box states them.
 *
 * @param {{ items?: unknown, value_lists?: unknown } | null} answer
 * @returns {Row[]}
 */
export function rowsOf(answer) {
	/** @type {Row[]} */
	const out = [];
	const items = answer && Array.isArray(answer.items) ? answer.items : [];
	const lists = answer && answer.value_lists && typeof answer.value_lists === 'object' ? /** @type {Record<string, unknown>} */ (answer.value_lists) : {};
	for (const one of items) {
		const row = rowOf(/** @type {Declared} */ (one), lists);
		if (row !== null)
			out.push(row);
	}
	return withPairs(out);
}

/**
 * The rows the box says are written together, drawn the way each pair can be edited here.
 *
 * A pair written by identifier is a channel: the identifier is chosen from the channel
 * list, the name beside it is the box's to fill and is shown as it stands. A pair written
 * whole is a place the page has no lookup for, so it is one line the box sets and the
 * second member is not drawn on its own. A server that states no pair leaves every row as
 * it was, which is also what it does today.
 *
 * @param {Row[]} rows
 * @returns {Row[]}
 */
function withPairs(rows) {
	/** @type {Record<string, number>} */
	const at = {};
	rows.forEach(function (row, i) { at[row.id] = i; });

	/** @type {Row[]} */
	const out = [];
	rows.forEach(function (row, i) {
		if (row.pairWrites === '' || row.pair === '' || at[row.pair] === undefined) {
			if (row.label !== '')
				out.push(row);
			return;
		}
		const partner = /** @type {Row} */ (rows[/** @type {number} */ (at[row.pair])]);
		if (row.pairWrites === 'both') {
			// The later member is part of the first one's line.
			if (/** @type {number} */ (at[row.pair]) < i)
				return;
			out.push(Object.assign({}, row, { readOnly: /** @type {'box'} */ ('box'), partner: row.pair }));
			return;
		}
		// By identifier. The member without a label is the identifier.
		if (row.label === '') {
			if (partner.label === '')
				return;
			out.push(Object.assign({}, row, {
				label: partner.label,
				// The box's own statement first; the name is the fallback for one that makes none.
				picker: row.channelKind !== '' ? row.channelKind : /** @type {'tv'|'radio'} */ (row.id.indexOf('radio') === -1 ? 'tv' : 'radio'),
			}));
			return;
		}
		if (partner.label === '')
			out.push(Object.assign({}, row, { readOnly: /** @type {'follows'} */ ('follows') }));
		else
			out.push(row);
	});
	return out;
}

/**
 * The settings the conditions of a row read.
 *
 * @param {Row} row
 * @returns {string[]}
 */
export function conditionKeys(row) {
	/** @type {string[]} */
	const out = [];
	for (const condition of row.conditions) {
		for (const one of 'any' in condition ? condition.any : [condition])
			out.push(one.key);
	}
	return out;
}

/**
 * The text a pair written whole is drawn with: its members' values on one line.
 *
 * @param {Row} row
 * @param {Record<string, string>} values
 * @returns {string}
 */
export function lineOf(row, values) {
	const parts = [row.id, row.partner].map(function (id) {
		return id === '' || values[id] === undefined ? '' : /** @type {string} */ (values[id]);
	});
	return parts.filter(function (one) { return one !== ''; }).join(', ');
}

/**
 * The key rows with the names the box gives its keys as their choices. A row whose list
 * has not arrived keeps none and is drawn as the number it is stored as.
 *
 * @param {Row[]} rows
 * @param {{ items?: unknown } | null} answer GET /api/v1/settings/keys
 * @returns {Row[]}
 */
export function withKeyNames(rows, answer) {
	const items = answer && Array.isArray(answer.items) ? answer.items : [];
	/** @type {{ value: number, label: string }[]} */
	const names = [];
	for (const one of items) {
		if (!one || typeof one !== 'object')
			continue;
		const code = Number(/** @type {{ code?: unknown }} */ (one).code);
		const name = /** @type {{ name?: unknown }} */ (one).name;
		if (Number.isFinite(code) && typeof name === 'string' && name !== '')
			names.push({ value: code, label: name });
	}
	if (names.length === 0)
		return rows;
	return rows.map(function (row) {
		return row.type === 'key' ? Object.assign({}, row, { choices: names }) : row;
	});
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
 * Whether one condition holds, a group when any one of its comparisons does.
 *
 * A comparison this cannot carry out is answered true. It names a setting whose value is
 * not in front of this screen, which happens when it lives on another page or is a
 * credential, and hiding a field on a fact nobody has is how a setting becomes unreachable
 * with nothing on screen to say why.
 *
 * @param {Condition} condition
 * @param {Record<string, string>} values
 * @returns {boolean}
 */
export function conditionHolds(condition, values) {
	if ('any' in condition)
		return condition.any.some(function (one) { return comparisonHolds(one, values); });
	return comparisonHolds(condition, values);
}

/**
 * @param {Comparison} condition
 * @param {Record<string, string>} values
 * @returns {boolean}
 */
function comparisonHolds(condition, values) {
	// A member kept unread names no setting, and no value is held under no name.
	const held = values[condition.key];
	if (held === undefined)
		return true;
	// An empty placeholder adds nothing to the test for an empty text.
	if (condition.op === 'text-valid')
		return held !== '' && held !== condition.text;
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
		// A locked row refuses every write, so a value for one is left out
		// rather than sent to be turned down.
		if (row.locked || row.readOnly)
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
 * The value an int row names in words, when that is what the text holds, such as
 * "off" or "last used". It may lie at or outside the bounds of the row.
 *
 * @param {Row} row
 * @param {string} text
 * @returns {{ value: number, label: string } | null}
 */
export function namedValue(row, text) {
	if (row.type !== 'int' || !/^-?[0-9]+$/.test(text))
		return null;
	const value = Number(text);
	for (const choice of row.choices) {
		if (choice.value === value)
			return choice;
	}
	return null;
}

/**
 * What a number field says when what is in it is not a number the row takes.
 * Null when there is nothing to say. The row's named value is one it takes,
 * wherever it lies.
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
	if (namedValue(row, text) !== null)
		return null;
	const value = Number(text);
	if (row.min !== null && value < row.min)
		return 'range';
	if (row.max !== null && value > row.max)
		return 'range';
	return null;
}

/**
 * What a text field says when what is in it is not a text the row takes, by the rule the
 * box states for it. Bytes, as the box counts them. Null when there is nothing to say.
 *
 * @param {Row} row
 * @param {string} text
 * @returns {'short'|'long'|'chars'|null}
 */
export function textFault(row, text) {
	if (row.type !== 'string' || row.textKind === '' || row.choices.length > 0)
		return null;
	const size = typeof TextEncoder === 'function' ? new TextEncoder().encode(text).length : text.length;
	if (size < row.minLength)
		return 'short';
	if (row.maxLength !== 0 && size > row.maxLength)
		return 'long';
	if (row.allowedChars !== '') {
		for (const one of text) {
			if (row.allowedChars.indexOf(one) === -1)
				return 'chars';
		}
	}
	return null;
}

/**
 * Whether what was typed is something the row cannot take, whichever kind it is.
 *
 * @param {Row} row
 * @param {string} text
 * @returns {boolean}
 */
export function hasFault(row, text) {
	if (row.secret && text === '')
		return false;
	return numberFault(row, text) !== null || textFault(row, text) !== null;
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
	return labelFor(row, row.fallback);
}

/**
 * The words a stored value goes by, and the value itself where the row has none for it.
 *
 * @param {Row} row
 * @param {string} value
 * @returns {string}
 */
export function labelFor(row, value) {
	if (row.type === 'string') {
		for (const choice of row.choices) {
			if (choice.text === value)
				return choice.label;
		}
		return value;
	}
	if (row.type !== 'enum' && row.type !== 'key' && !(row.type === 'int' && row.choices.length > 0))
		return value;
	const wanted = Number(value);
	for (const choice of row.choices) {
		if (choice.value === wanted)
			return choice.label;
	}
	return value;
}

/* Settings whose write can leave the box unusable until somebody is at it: a picture the
   TV cannot show is a black screen, and a remote control of the wrong kind answers to no
   key. Named here because the box states no such thing about a row, and the text to ask
   with is this page's. */
const kRisky = {
	video_Mode: 'settings.risk.video_Mode',
	remote_control_hardware: 'settings.risk.remote_control_hardware'
};

/**
 * The questions to put before a write: one text name per risky setting the body carries.
 *
 * @param {Record<string, string>} body
 * @returns {string[]}
 */
export function risksOf(body) {
	/** @type {string[]} */
	const out = [];
	for (const id of Object.keys(kRisky)) {
		if (Object.prototype.hasOwnProperty.call(body, id))
			out.push(/** @type {string} */ (/** @type {Record<string, string>} */ (kRisky)[id]));
	}
	return out;
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
