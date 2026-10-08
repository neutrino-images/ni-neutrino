// What the settings page draws for an enum row whose stored value the box does not offer.
//
// A neutrino.conf written on another box can hold a value this one never lists. The
// select must still show it, as an extra option that cannot be chosen, rather than
// draw empty. The element factory and the template tag are the real ones, so the
// tree is the one the page builds; only the hooks are stubbed.
import * as loader from 'node:module';
import { pathToFileURL } from 'node:url';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { readFileSync, readdirSync } from 'node:fs';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('settingsrows-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const here = dirname(fileURLToPath(import.meta.url));
const kReal = {
	'/vendor/preact.module.js': pathToFileURL(resolve(here, 'node_modules/preact/dist/preact.module.js')).href,
	'/vendor/htm.module.js': pathToFileURL(resolve(here, 'node_modules/htm/dist/htm.module.js')).href,
};
const kStubs = {
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback']
			.map(function (name) { return 'export function ' + name + '() {}\n'; }).join('') +
		'export function useId() { return "id"; }\n',
	'/vendor/preact-router.module.js':
		'export default function Router() { return null; }\n' +
		'export function Link() { return null; }\n' +
		'export function route() {}\n' +
		'export function getCurrentUrl() { return ""; }\n',
};

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (Object.prototype.hasOwnProperty.call(kReal, spec))
			return { url: kReal[spec], shortCircuit: true };
		if (Object.prototype.hasOwnProperty.call(kStubs, spec))
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		if (spec.indexOf('/vendor/') === 0)
			throw new Error('settingsrows-cases.mjs: no stub for the runtime module ' + spec);
		return next(spec, context);
	},
});

globalThis.document = /** @type {any} */ ({ documentElement: {} });

const { Control, Row, ChannelPick, ChannelSelect, ReadOnlyRow, channelChoices, faultText } = await import('../../data/ni-web/app/screens/settings/rows.js');
const { conditionKeys, rowOf, rowsOf, changed, numberFault, textFault, hasFault, isVisible, withKeyNames, lineOf, risksOf } = await import('../../data/ni-web/app/screens/settings/model.js');
const { outcomeOf, refusedOfSent, refusalText, applyFailedText } = await import('../../data/ni-web/app/screens/settings/refusal.js');
const { default: words } = await import('../../data/ni-web/app/screens/settings/settings.text.js');
const { setLanguage } = await import('../../data/ni-web/app/i18n.js');
const { aspectModes } = await import('../../data/ni-web/app/screens/now/overview.js');
const { settingChoices } = await import('../../data/ni-web/app/screens/now/overview.js');

let checked = 0;
let failed = 0;

/**
 * @param {unknown} got
 * @param {unknown} want
 * @param {string} what
 */
function same(got, want, what) {
	checked++;
	if (JSON.stringify(got) !== JSON.stringify(want)) {
		failed++;
		process.stderr.write('settingsrows: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

/**
 * Expands components until only elements are left, and collects those named tag.
 * @param {any} node
 * @param {string} tag
 * @param {any[]} into
 * @returns {any[]}
 */
function find(node, tag, into) {
	if (node === null || typeof node !== 'object')
		return into;
	if (Array.isArray(node)) {
		node.forEach(function (n) { find(n, tag, into); });
		return into;
	}
	if (typeof node.type === 'function') {
		find(node.type(node.props), tag, into);
		return into;
	}
	if (node.type === tag)
		into.push(node);
	find(node.props && node.props.children, tag, into);
	return into;
}

/** @param {string} value */
function drawn(value) {
	const row = {
		id: 'x.mode', label: 'Mode', type: 'enum', secret: false, locked: false, needsRestart: false,
		choices: [0, 1, 2].map(function (n) { return { value: n, label: 'choice ' + n }; }),
	};
	const tree = Control(/** @type {any} */ ({ row: row, value: value, onChange: function () {}, onClear: function () {} }));
	const select = find(tree, 'select', [])[0];
	return {
		selected: select.props.value,
		options: find(tree, 'option', []).map(function (o) {
			return { value: o.props.value, disabled: o.props.disabled === true, label: o.props.children };
		}),
	};
}

setLanguage('en');

const unlisted = drawn('3');
same(unlisted.options.length, 4, 'the stored value is added to the three choices');
same(unlisted.options[3], { value: '3', disabled: true, label: '3, not available on this box' }, 'the extra option is disabled and says why');
same(unlisted.selected, '3', 'the extra option is the selected one');
same(unlisted.options.slice(0, 3).some(function (o) { return o.disabled; }), false, 'the offered choices stay choosable');

const listed = drawn('1');
same(listed.options.length, 3, 'a listed value adds nothing');
same(listed.selected, '1', 'a listed value is selected as before');

same(drawn('').options.length, 3, 'an empty value adds nothing');

setLanguage('de');
same(drawn('3').options[3].label, '3, auf dieser Box nicht verfügbar', 'the German text');

// A row the schema reports locked is drawn read only, even with its choices there,
// and an edit of it is never sent.
setLanguage('en');
/** @param {boolean} locked */
function declared(locked) {
	return rowOf({
		id: 'x.age', label: 'Age', type: 'enum', section: 'x', locked: locked,
		values: [{ value: 12, label: '12' }, { value: 18, label: '18' }], conditions: [],
	});
}
const held = /** @type {any} */ (declared(true));
same([held.locked, held.held], [true, true], 'a locked schema row is held and locked');
const heldTree = Control(/** @type {any} */ ({ row: held, value: '18', onChange: function () {}, onClear: function () {} }));
same(find(heldTree, 'select', []).length, 0, 'a held row offers no chooser');
same(find(heldTree, 'span', []).map(function (n) { return n.props.children; })[1],
	'The parental lock of this box fixes this, so it cannot be changed here.', 'a held row says why');
same(changed([held], { 'x.age': '18' }, { 'x.age': '12' }), {}, 'an edit of a held row is not sent');
const free = /** @type {any} */ (declared(false));
same([free.locked, free.held], [false, false], 'an unlocked schema row is neither');
same(changed([free], { 'x.age': '18' }, { 'x.age': '12' }), { 'x.age': '12' }, 'an edit of an unlocked row is sent');

// A held row drifting from its default is marked but offered no way back, which
// would be an edit that is never sent.
/** @param {any} row */
function backButtons(row) {
	const tree = Row(/** @type {any} */ ({ row: row, value: '18', drifts: true, place: '',
		onChange: function () {}, onClear: function () {}, onRevert: function () {} }));
	return find(tree, 'button', []).length;
}
same(backButtons(held), 0, 'a drifting held row has no put back button');
same(backButtons(free), 1, 'a drifting unlocked row keeps its put back button');

// A row this box lacks is not drawn; one that says nothing about it is.
/** @param {unknown} available */
function onBox(available) {
	/** @type {any} */
	const said = { id: 'x.fan', label: 'Fan', type: 'int', section: 'x', min: 1, max: 14, conditions: [] };
	if (available !== undefined)
		said.available = available;
	return rowOf(said);
}
same(onBox(false), null, 'a row the box lacks is not drawn');
same(onBox(true) !== null, true, 'an available row is drawn');
same(onBox(undefined) !== null, true, 'a row from a server that does not say is drawn');

// A key is a choice among the names the box gives its keys, and the number it is stored as
// until those have arrived. A stored code with no name is one more entry and stays choosable.
const keyRow = /** @type {any} */ (rowOf({ id: 'x.key', label: 'Key', type: 'key', section: 'x', min: -2, max: 100,
	default: '5', conditions: [] }));
same(keyRow === null ? null : [keyRow.type, keyRow.min, keyRow.max, keyRow.choices.length], ['key', -2, 100, 0], 'a key row keeps its kind and its bounds');
const keyNames = { items: [{ code: -2, name: 'none' }, { code: 5, name: 'ok' }, { code: 1029, name: 'ok (long)' }, { code: 'x', name: 'junk' }, { code: 7, name: '' }] };
const withNames = /** @type {any} */ (withKeyNames([keyRow], keyNames)[0]);
same(withNames.choices, [{ value: -2, label: 'none' }, { value: 5, label: 'ok' }, { value: 1029, label: 'ok (long)' }], 'the names become the choices and a malformed entry is left out');
same(withKeyNames([keyRow], null)[0].choices, [], 'no list leaves the row without choices');
same(withKeyNames([keyRow], { items: [] })[0].choices, [], 'an empty list leaves the row without choices');
/** @param {any} row @param {string} value */
function keyDrawn(row, value) {
	const tree = Control(/** @type {any} */ ({ row: row, value: value, onChange: function () {}, onClear: function () {} }));
	const select = find(tree, 'select', [])[0];
	return {
		inputs: find(tree, 'input', []).map(function (i) { return i.props.type; }),
		selected: select ? select.props.value : null,
		options: find(tree, 'option', []).map(function (o) {
			return { value: o.props.value, disabled: o.props.disabled === true, label: o.props.children };
		}),
	};
}
same(keyDrawn(keyRow, '5').inputs, ['number'], 'a key without names is a number field');
const named5 = keyDrawn(withNames, '5');
same([named5.inputs, named5.selected, named5.options.length], [[], '5', 3], 'a key with names is a choice with the stored key selected');
same(named5.options[1], { value: '5', disabled: false, label: 'ok' }, 'the choice carries the name');
const unnamed = keyDrawn(withNames, '77');
same(unnamed.options[3], { value: '77', disabled: false, label: '77, no name' }, 'a stored code with no name is one more choosable entry');
same(unnamed.selected, '77', 'and it is the selected one, so the stored value is not lost');
setLanguage('de');
same(keyDrawn(withNames, '77').options[3].label, '77, ohne Namen', 'the German text of the extra entry');
setLanguage('en');
same(keyDrawn(withNames, '-2').options.length, 3, 'a named code adds nothing');

// A colour is drawn as text with the format it takes, from the channels the schema states.
const colorRow = /** @type {any} */ (rowOf({ id: 'x.color', label: 'Colour', type: 'color', section: 'x', default: '#102030', channels: 3, conditions: [] }));
same(colorRow === null ? null : [colorRow.type, colorRow.fallback, colorRow.min, colorRow.channels], ['string', '#102030', null, 3], 'a colour row is drawn as text and knows its channels');
const alphaRow = /** @type {any} */ (rowOf({ id: 'x.colora', label: 'Colour', type: 'color', section: 'x', default: '#10203040', channels: 4, conditions: [] }));
same(alphaRow.channels, 4, 'a colour with an alpha says four');
same(/** @type {any} */ (rowOf({ id: 'x.colorb', label: 'Colour', type: 'color', section: 'x', default: '#102030', channels: 7, conditions: [] })).channels, null, 'a channel count that is not three or four is not believed');
/** @param {any} row */
function colorHint(row) {
	const tree = Control(/** @type {any} */ ({ row: row, value: row.fallback, onChange: function () {}, onClear: function () {} }));
	return find(tree, 'span', []).map(function (n) { return n.props.children; }).filter(function (c) { return typeof c === 'string' && c.indexOf('#rrggbb') !== -1; });
}
same(colorHint(colorRow), ['Colour as #rrggbb'], 'a three channel colour says #rrggbb');
same(colorHint(alphaRow), ['Colour as #rrggbbaa, the last two digits are the transparency'], 'a four channel colour says #rrggbbaa');

// The quick 4:3 buttons offer what the schema lists for the setting, in the
// page's own words, and all four before the schema has answered.
/** @param {number[]} values */
function schemaWith(values) {
	return { items: [
		{ id: 'video_Mode', values: [{ value: 9, label: '1080i 50Hz' }] },
		{ id: 'video_43mode', values: values.map(function (v) { return { value: v, label: String(v) }; }) },
	] };
}
same(aspectModes(schemaWith([0, 3, 1, 2])).map(function (one) { return one.value; }), [0, 3, 1, 2],
	'every mode the box lists, in the order the box lists them');
same(aspectModes(schemaWith([0, 7])).map(function (one) { return [one.value, one.key, one.label]; }),
	[[0, 'now.quick.43.panscan', '0'], [7, '', '7']], 'a value the page has no words for is an extra button with the schema\'s label');
same(aspectModes(schemaWith([0, 1, 2])).map(function (one) { return one.key; }),
	['now.quick.43.panscan', 'now.quick.43.letterbox', 'now.quick.43.full'], 'a mode the box does not list is not offered');
same(aspectModes(null).length, 4, 'all four before the schema answers');
same(aspectModes({ items: [] }), [], 'none where the schema answered without the setting');

// The choices a setting offers are the values the schema lists for it.
same(settingChoices({ items: [{ id: 'video_Mode', values: [{ value: 9, label: '1080i 50Hz' }] }] }, 'video_Mode'),
	[{ value: 9, label: '1080i 50Hz' }], 'a setting\'s choices are its values');

// A list the box stated once for several rows is found under the name the row gives.
const sharedSchema = { items: [
	{ id: 'video_Mode', values_from: 'a1' },
	{ id: 'video_43mode', values_from: 'a2' },
], value_lists: {
	a1: [{ value: 9, label: '1080i 50Hz' }],
	a2: [{ value: 0, label: 'p' }, { value: 3, label: 'x' }],
} };
same(settingChoices(sharedSchema, 'video_Mode'), [{ value: 9, label: '1080i 50Hz' }], 'a setting\'s choices come from the shared list it names');
same(aspectModes(sharedSchema).map(function (one) { return one.value; }), [0, 3], 'the 4:3 buttons come from the shared list too');

// A number row that names a value in words takes it wherever it lies and says the words
// beside the number. The row shape is a declared one: start_volume names -1 at its floor.
/**
 * @param {number} min
 * @param {any[]} values
 */
function intRow(min, values) {
	return /** @type {any} */ (rowOf({
		id: 'x.volume', label: 'Volume', type: 'int', section: 'x', min: min, max: 100,
		values: values, conditions: [],
	}));
}
const named = intRow(-1, [{ value: -1, label: 'Last used' }]);
const below = intRow(0, [{ value: -1, label: 'Last used' }]);
const plain = intRow(0, []);
same(numberFault(named, '-1'), null, 'the named value at the floor is no fault');
same(numberFault(named, '-2'), 'range', 'another number below the floor is a fault');
same(numberFault(below, '-1'), null, 'a named value below the range is no fault');
same(numberFault(below, '101'), 'range', 'a number above the range is a fault');
same(numberFault(plain, '-1'), 'range', 'without a named value the same number is a fault');

/** @param {any} row @param {string} value */
function numberField(row, value) {
	const tree = Control(/** @type {any} */ ({ row: row, value: value, onChange: function () {}, onClear: function () {} }));
	const input = find(tree, 'input', [])[0];
	return {
		type: input.props.type,
		shown: input.props.value,
		notes: find(tree, 'span', []).filter(function (n) { return n.props.class === 'hint'; })
			.map(function (n) { return n.props.children; }),
		faults: find(tree, 'span', []).filter(function (n) { return n.props.class === 'err'; }).length,
	};
}
same(numberField(named, '-1'), { type: 'number', shown: '-1', notes: ['Last used'], faults: 0 }, 'the named value is a number with its words beside it');
same(numberField(named, '50'), { type: 'number', shown: '50', notes: [], faults: 0 }, 'another number has no words');
same(numberField(named, ''), { type: 'number', shown: '', notes: [], faults: 1 }, 'an empty field has no words and a fault');
same(numberField(below, '-1'), { type: 'number', shown: '-1', notes: ['Last used'], faults: 0 }, 'the words show for a value below the range too');
same(numberField(plain, '-1'), { type: 'number', shown: '-1', notes: [], faults: 1 }, 'without a named value it is faulted');

// A number shows its unit beside the input, in the page's own words. The schema names the
// unit by the box's catalog key; a key this page has no word for shows nothing, and the raw
// key never reaches the screen.
/** @param {unknown} unit */
function unitRow(unit) {
	/** @type {any} */
	const said = { id: 'x.hours', label: 'Hours', type: 'int', section: 'x', min: 1, max: 24, conditions: [] };
	if (unit !== undefined)
		said.unit = unit;
	return /** @type {any} */ (rowOf(said));
}
/** @param {any} row @param {string} lang */
function unitShown(row, lang) {
	setLanguage(lang);
	const tree = Control(/** @type {any} */ ({ row: row, value: '4', onChange: function () {}, onClear: function () {} }));
	return find(tree, 'span', []).filter(function (n) { return n.props.class === 'unit'; })
		.map(function (n) { return n.props.children; });
}
same(unitRow('unit.short.hour').unit, 'unit.short.hour', 'the row keeps the unit name the schema gave');
same(unitRow(undefined).unit, '', 'a row without a unit has none');
same(unitShown(unitRow('unit.short.hour'), 'en'), ['h'], 'the unit is shown beside the number');
same(unitShown(unitRow('unit.short.percent'), 'de'), ['%'], 'the unit is shown in the other language too');
same(unitShown(unitRow('unit.short.nonsense'), 'en'), [], 'a unit the page has no word for shows nothing');
same(unitShown(unitRow('unit.short.nonsense'), 'en').concat(unitShown(unitRow('unit.short.nonsense'), 'de')).join(''), '', 'the raw unit name is never shown');
same(unitShown(unitRow(undefined), 'en'), [], 'a number without a unit shows none');
same(unitRow(5).unit, '', 'a unit that is no text is none');
setLanguage('en');

// A text condition and a group are read the way the box reads them.
/** @param {any[]} conditions */
function gated(conditions) {
	return /** @type {any} */ (rowOf({ id: 'x.gated', label: 'Gated', type: 'int', section: 'x', min: 0, max: 9,
		conditions: conditions }));
}
const keyed = gated([{ key: 'k_key', op: 'text-valid', text: 'XXXX' }]);
same(isVisible(keyed, { k_key: 'abcd' }), true, 'a real key holds a text condition');
same(isVisible(keyed, { k_key: 'XXXX' }), false, 'the placeholder does not hold a text condition');
same(isVisible(keyed, { k_key: '' }), false, 'an empty key does not hold a text condition');
same(isVisible(keyed, {}), true, 'a key not in front of the page leaves the row shown');
const anyText = gated([{ key: 'k_key', op: 'text-valid' }]);
same(isVisible(anyText, { k_key: 'XXXX' }), true, 'without a placeholder any text holds');
same(isVisible(anyText, { k_key: '' }), false, 'without a placeholder an empty key fails');
const emptyPattern = gated([{ key: 'k_key', op: 'text-valid', text: '' }]);
same(isVisible(emptyPattern, { k_key: 'abcd' }), true, 'an empty placeholder lets any text hold');
same(isVisible(emptyPattern, { k_key: '' }), false, 'an empty placeholder still fails an empty key');

const either = gated([{ any: [{ key: 'a', op: 'ne', values: [0] }, { key: 'b', op: 'ne', values: [0] }] }]);
same(isVisible(either, { a: '0', b: '1' }), true, 'a group holds when one member holds');
same(isVisible(either, { a: '1', b: '0' }), true, 'a group holds when the other member holds');
same(isVisible(either, { a: '0', b: '0' }), false, 'a group fails when no member holds');
same(isVisible(either, { a: '0' }), true, 'a member not in front of the page holds');

const both = gated([{ any: [{ key: 'a', op: 'ne', values: [0] }, { key: 'b', op: 'ne', values: [0] }] },
	{ key: 'on', op: 'ne', values: [0] }]);
same(isVisible(both, { a: '1', b: '0', on: '1' }), true, 'a group and a comparison both holding');
same(isVisible(both, { a: '1', b: '0', on: '0' }), false, 'a group is one entry of a conjunction');
same(isVisible(gated([{ any: [] }]), { a: '0' }), true, 'an empty group leaves the row shown');
const unread = gated([{ any: [{ key: 'a', op: 'ne', values: [0] }, { op: 'ne', values: [0] }] }]);
same(isVisible(unread, { a: '0' }), true, 'a member the page cannot read makes its group hold');
same(unread.conditions[0].any.length, 2, 'a member the page cannot read is kept');
same(gated([{ any: [{ key: 'a', op: 'ne', values: [0] }] }]).conditions,
	[{ any: [{ key: 'a', op: 'ne', values: [0], text: '' }] }], 'a group is read with its members');

// A string row that names the entries it accepts is drawn as a select over their texts.
const { fallbackLabel } = await import('../../data/ni-web/app/screens/settings/model.js');
/** @param {unknown[]} values */
function localeRow(values) {
	return rowOf(/** @type {any} */ ({
		id: 'x.locale', type: 'string', section: 'x', label: 'Locale', default: 'de',
		values: values,
	}));
}
const withList = localeRow([{ text: 'de', label: 'Deutsch' }, { text: 'fr', key: 'k', label: 'Francais' }, { text: 'it' }, { label: 'no text' }]);
same(withList === null ? null : withList.choices.map(function (c) { return [c.text, c.label]; }),
	[['de', 'Deutsch'], ['fr', 'Francais'], ['it', 'it']], 'a string row keeps the text of each entry, the text standing for a missing label');
same(withList === null ? null : withList.locked, false, 'a string row with entries is not locked');
same(fallbackLabel(/** @type {any} */ (withList)), 'Deutsch', 'the default of a string row is named by its entry');
same(localeRow([]) === null ? null : /** @type {any} */ (localeRow([])).choices, [], 'a string row without entries has no choices');

/** @param {string} value @param {unknown[]} values */
function drawnText(value, values) {
	const tree = Control(/** @type {any} */ ({ row: localeRow(values), value: value, onChange: function () {}, onClear: function () {} }));
	return {
		selects: find(tree, 'select', []).length,
		inputs: find(tree, 'input', []).length,
		options: find(tree, 'option', []).map(function (o) {
			return { value: o.props.value, disabled: o.props.disabled === true, label: o.props.children };
		}),
	};
}
setLanguage('en');
const pickText = drawnText('fr', [{ text: 'de', label: 'Deutsch' }, { text: 'fr', label: 'Francais' }]);
same(pickText.selects, 1, 'a string row with entries is a select');
same(pickText.options.map(function (o) { return o.value; }), ['de', 'fr'], 'the options carry the texts');
same(pickText.options.some(function (o) { return o.disabled; }), false, 'every listed text is choosable');
const absent = drawnText('xx', [{ text: 'de', label: 'Deutsch' }]);
same(absent.options.length, 2, 'a stored text the box does not list is shown');
same(absent.options[1].value, 'xx', 'the extra option is the stored text');
same(absent.options[1].disabled, true, 'the extra option cannot be picked');
const emptyStored = drawnText('', [{ text: 'de', label: 'Deutsch' }]);
same(emptyStored.options.map(function (o) { return o.value; }), ['', 'de'], 'a stored empty text is an option of its own, first');
same(emptyStored.options[0].label, 'automatic', 'and is worded as the box picking');
const plainString = drawnText('fr', []);
same([plainString.selects, plainString.inputs], [0, 1], 'a string row without entries stays a text field');

// A number whose values the box lists in full, such as the tuners it has, is drawn as that
// list, so no number the box lacks can be picked; one that only names a value stays a number.
/** @param {boolean} full @param {string} value */
function tunerDrawn(full, value) {
	/** @type {any} */
	const said = { id: 'x.tuner', label: 'Tuner', type: 'int', section: 'x', min: -1, max: 23, conditions: [],
		values: [{ value: -1, label: 'Off' }, { value: 0, label: '1: DVB-S2' }, { value: 1, label: '2: DVB-C' }] };
	if (full)
		said.listed = true;
	const tree = Control(/** @type {any} */ ({ row: rowOf(said), value: value, onChange: function () {}, onClear: function () {} }));
	return {
		selects: find(tree, 'select', []).length,
		options: find(tree, 'option', []).map(function (o) {
			return { value: o.props.value, disabled: o.props.disabled === true, label: o.props.children };
		}),
	};
}
setLanguage('en');
const tuners = tunerDrawn(true, '0');
same(tuners.selects, 1, 'a listed int is a select');
same(tuners.options.map(function (o) { return [o.value, o.label]; }), [['-1', 'Off'], ['0', '1: DVB-S2'], ['1', '2: DVB-C']], 'its options are the box\'s values and words');
const goneTuner = tunerDrawn(true, '5');
same(goneTuner.options[3], { value: '5', disabled: true, label: '5, not available on this box' }, 'a stored number the box does not list is shown and not choosable');
same(tunerDrawn(false, '0').selects, 0, 'an int that only names values stays a number field');


// ---------------------------------------------------------------- pairs the box states

// A start channel is two rows, an identifier without a label and the name with one. The
// identifier is drawn as a picker under the name's label, and the name is shown as it
// stands and never sent.
setLanguage('en');
/** @param {boolean} paired */
function startChannel(paired) {
	/** @type {any} */
	const id = { id: 'startchanneltv_id', type: 'string', section: 'channel', default: '0', conditions: [] };
	/** @type {any} */
	const name = { id: 'startchanneltv', type: 'string', section: 'channel', label: 'Last TV channel', default: '', conditions: [] };
	/** @type {any} */
	const radio = { id: 'startchannelradio_id', type: 'string', section: 'channel', default: '0', conditions: [] };
	/** @type {any} */
	const radioName = { id: 'startchannelradio', type: 'string', section: 'channel', label: 'Last radio channel', default: '', conditions: [] };
	if (paired) {
		id.pair = 'startchanneltv'; id.pair_writes = 'id';
		name.pair = 'startchanneltv_id'; name.pair_writes = 'id';
		radio.pair = 'startchannelradio'; radio.pair_writes = 'id';
		radioName.pair = 'startchannelradio_id'; radioName.pair_writes = 'id';
	}
	return rowsOf({ items: [id, name, radio, radioName] });
}
const channelRows = /** @type {any[]} */ (startChannel(true));
same(channelRows.map(function (r) { return [r.id, r.label, r.picker, r.readOnly]; }), [
	['startchanneltv_id', 'Last TV channel', 'tv', ''],
	['startchanneltv', 'Last TV channel', '', 'follows'],
	['startchannelradio_id', 'Last radio channel', 'radio', ''],
	['startchannelradio', 'Last radio channel', '', 'follows'],
], 'the identifier is a picker under the name\'s label and the name is read only');
same(changed(channelRows, { startchanneltv_id: '0', startchanneltv: '' }, { startchanneltv_id: 'b9b0040200016dcb', startchanneltv: 'typed' }),
	{ startchanneltv_id: 'b9b0040200016dcb' }, 'only the identifier is sent for a start channel');
const channelTree = Control(/** @type {any} */ ({ row: channelRows[0], value: '0', onChange: function () {}, onClear: function () {} }));
same(channelTree.type === ChannelPick, true, 'the identifier row is drawn as the channel picker');
const nameTree = Control(/** @type {any} */ ({ row: channelRows[1], value: 'Das Erste', onChange: function () {}, onClear: function () {} }));
same([nameTree.type === ReadOnlyRow, find(nameTree, 'p', [])[0].props.children], [true, 'Das Erste'], 'the name row shows the name');
same(startChannel(false).map(function (r) { return /** @type {any} */ (r).id; }), ['startchanneltv', 'startchannelradio'],
	'a box that states no pair draws what it drew before: the names, editable');
same(startChannel(false).every(function (r) { return /** @type {any} */ (r).picker === '' && /** @type {any} */ (r).readOnly === ''; }), true,
	'and neither is a picker nor read only');
same(channelChoices([{ id: 'a1', number: 1, name: 'Eins' }], 'a1').map(function (o) { return o.value; }), ['0', 'a1'], 'a channel in the list is one entry');
same(channelChoices([{ id: 'a1', number: 1, name: 'Eins' }], 'ff').map(function (o) { return o.value; }), ['0', 'a1', 'ff'],
	'a stored channel the list lacks stays an entry');
same(channelChoices([], '0'), [{ value: '0', label: 'no channel' }], 'none is its own entry');

// The kind of channel comes from the box when it says so, and from the name when it does not.
/** @param {string} id @param {any} extra */
function kindOf(id, extra) {
	return /** @type {any[]} */ (rowsOf({ items: [
		Object.assign({ id: id, type: 'string', section: 'c', pair: 'n', pair_writes: 'id', conditions: [] }, extra),
		{ id: 'n', type: 'string', section: 'c', label: 'Name', pair: id, pair_writes: 'id', conditions: [] },
	] }))[0].picker;
}
same([kindOf('tv_id', { channel_kind: 'radio' }), kindOf('radio_id', { channels: 'tv' }), kindOf('radio_id', {}), kindOf('x_id', {})],
	['radio', 'tv', 'radio', 'tv'], 'the picker follows the kind the box states and falls back to the name');

// The weather place is two rows written whole: one read only line, the second not drawn.
const weatherRows = /** @type {any[]} */ (rowsOf({ items: [
	{ id: 'weather_city', type: 'string', section: 'weather', label: 'Location', pair: 'weather_location', pair_writes: 'both', default: '', conditions: [] },
	{ id: 'weather_location', type: 'string', section: 'weather', label: 'Location', pair: 'weather_city', pair_writes: 'both', default: '', conditions: [] },
] }));
same(weatherRows.map(function (r) { return [r.id, r.readOnly]; }), [['weather_city', 'box']], 'a pair written whole is one read only line');
same(lineOf(weatherRows[0], { weather_city: 'Berlin', weather_location: '52.5,13.4' }), 'Berlin, 52.5,13.4', 'its line holds both members');
same(changed(weatherRows, { weather_city: 'a' }, { weather_city: 'b' }), {}, 'and is never sent');

// -------------------------------------------------- a parental lock shows the words
const lockedAge = /** @type {any} */ (rowOf({ id: 'x.age', label: 'Age', type: 'enum', section: 'x', locked: true,
	values: [{ value: 12, label: '12 and over' }, { value: 18, label: '18 and over' }], conditions: [] }));
same(find(Control(/** @type {any} */ ({ row: lockedAge, value: '18', onChange: function () {}, onClear: function () {} })), 'p', [])[0].props.children,
	'18 and over', 'a held choice shows its label and not its number');

// ------------------------------------------------------------------ text limits
const pinRow = /** @type {any} */ (rowOf({ id: 'x.text', label: 'Text', type: 'string', section: 'x', text_kind: 'plain',
	min_length: 2, max_length: 4, allowed_chars: '0123456789abcdef', conditions: [] }));
same([textFault(pinRow, 'a'), textFault(pinRow, 'abcde'), textFault(pinRow, 'xy'), textFault(pinRow, 'ab')], ['short', 'long', 'chars', null],
	'a text is held to the length and characters the box states');
same(hasFault(pinRow, 'xy'), true, 'and a fault holds the save back');
same(faultText(pinRow, 'abcde'), 'At most 4 characters.', 'with the words of the page');
same(textFault(/** @type {any} */ (rowOf({ id: 'x.t', label: 'T', type: 'string', section: 'x', conditions: [] })), 'anything at all'), null,
	'a string with no stated rule takes anything');
const secretText = /** @type {any} */ (rowOf({ id: 'x.s', label: 'S', type: 'string', section: 'x', secret: true, text_kind: 'plain', min_length: 3, conditions: [] }));
same(hasFault(secretText, ''), false, 'an untouched credential field is no fault');

// -------------------------------------------------- what a partly refused write says
const answerResults = { a: { status: 204, code: '' }, b: { status: 409, code: 'setting-condition-not-met', detail: 'not now', depends_on: ['c', 7] },
	c: { status: 409, code: 'setting-locked', detail: 'locked' }, d: { status: 400, code: 'something-new', detail: 'a new thing' }, e: { status: 409, code: 'setting-locked' } };
const outcome = outcomeOf(answerResults);
same(outcome.landed, ['a'], 'a 2xx entry landed');
same(outcome.refused.map(function (r) { return r.key; }), ['b', 'c', 'd', 'e'], 'the others were refused');
same(refusedOfSent(outcome.refused, ['a', 'b', 'c']), 2, 'only the keys that were sent are counted');
/** @param {string} key */
const nameOf = function (key) { return ({ a: 'Alpha', b: 'Beta', c: 'Gamma' })[key] || key; };
same(refusalText(/** @type {any} */ (outcome.refused[0]), nameOf).indexOf('Beta: depends on Gamma'), 0, 'a refused condition names the setting it depends on');
same(refusalText(/** @type {any} */ (outcome.refused[1]), nameOf), 'Gamma: Locked, it cannot be changed right now.', 'a known code is worded by the page');
same(refusalText(/** @type {any} */ (outcome.refused[2]), nameOf), 'd: a new thing', 'an unknown code falls back to the box\'s sentence');
setLanguage('de');
same(refusalText(/** @type {any} */ (outcome.refused[1]), nameOf).indexOf('Gamma: Gesperrt'), 0, 'in German as well');
setLanguage('en');
same(outcomeOf(null), { landed: [], refused: [] }, 'no results is no outcome');

same(applyFailedText({ keys: ['a', 'zz'], detail: 'busy' }, nameOf), 'The box stored Alpha, zz but could not put it in force: busy', 'an apply failure names the settings and the reason');
same(applyFailedText({ keys: ['a'], detail: '' }, nameOf), 'The box stored Alpha but could not put it in force.', 'and works without a reason');


// A refusal naming settings the row's own conditions do not read is the box keeping settings
// together, and "change that one first" is wrong advice for it.
const bConditions = { b: ['c'] };
/** @param {string} key */
const conditionKeysOf = function (key) { return /** @type {Record<string, string[]>} */ (bConditions)[key] || []; };
same(refusalText(/** @type {any} */ (outcome.refused[0]), nameOf, conditionKeysOf).indexOf('Beta: depends on Gamma'), 0,
	'a refusal naming a setting the row\'s condition reads is worded as a dependency');
same(refusalText(/** @type {any} */ (outcome.refused[0]), nameOf, function () { return []; }).indexOf('Beta: does not go together with Gamma'), 0,
	'one naming a setting no condition of the row reads is worded as a pairing');
same(conditionKeys(/** @type {any} */ (rowOf({ id: 'x', label: 'X', type: 'bool', section: 's', conditions: [
	{ key: 'a', op: 'eq', values: [1] }, { any: [{ key: 'b', op: 'ne', values: [0] }, { key: 'c', op: 'ne', values: [0] }] }] }))),
	['a', 'b', 'c'], 'the keys a row\'s conditions read include those in a group');

// The picker hands the identifier, under the identifier\'s key, to the page.
/** @type {any[]} */
const picked = [];
const pickTree = ChannelSelect(/** @type {any} */ ({ row: channelRows[0], value: '0', channels: { items: [{ id: 'b9b0040200016dcb', number: 1, name: 'Eins' }], failed: false },
	onChange: function (/** @type {string} */ id, /** @type {string} */ value) { picked.push([id, value]); } }));
find(pickTree, 'select', [])[0].props.onChange({ currentTarget: { value: 'b9b0040200016dcb' } });
same(picked, [['startchanneltv_id', 'b9b0040200016dcb']], 'choosing a channel reaches the page as the identifier of the row, not the name');

// ------------------------------------------------------------ writes that ask first
same(risksOf({ video_Mode: '5' }), ['settings.risk.video_Mode'], 'a resolution is asked about');
same(risksOf({ remote_control_hardware: '1', other: '2' }), ['settings.risk.remote_control_hardware'], 'so is the remote control');
same(risksOf({ other: '2' }), [], 'nothing else is');

// ------------------------------------------------------ the tables against this page
const kTables = resolve(here, '../../src/coreapi/settings');
const kSources = readdirSync(kTables).filter(function (f) { return /^settingstable_.*\.cpp$/.test(f); })
	.map(function (f) { return readFileSync(resolve(kTables, f), 'utf8'); });
same(kSources.length > 0, true, 'the tables are found');

// Every unit a table uses has a word in both languages, or the number shows without one.
/** @type {Record<string, boolean>} */
const units = {};
for (const source of kSources) {
	for (const found of source.matchAll(/\.unit\("([^"]+)"\)/g))
		units[/** @type {string} */ (found[1])] = true;
}
same(Object.keys(units).length > 0, true, 'the tables use units');
for (const unit of Object.keys(units)) {
	for (const lang of ['de', 'en'])
		same(typeof /** @type {any} */ (words)[lang]['settings.' + unit], 'string', 'the unit ' + unit + ' has a text in ' + lang);
}

// A list row with a label has to be drawn. Reading the tables as text is what finds one
// before a box does; the page draws none today because no list row has a label.
/** @param {string} source @returns {string[]} */
function labelledLists(source) {
	/** @type {string[]} */
	const out = [];
	for (const found of source.matchAll(/listRow\("([^"]+)"\)([\s\S]*?)\.field\(/g)) {
		if (/\.label\(/.test(/** @type {string} */ (found[2])))
			out.push(/** @type {string} */ (found[1]));
	}
	return out;
}
same(labelledLists('listRow("a").section("x").label("k").field(F)\nlistRow("b").section("x").field(F)'), ['a'],
	'the scan finds a labelled list row and not an unlabelled one');
const drawsLists = rowOf({ id: 'x.list', label: 'List', type: 'list', section: 'x', conditions: [] }) !== null;
for (const source of kSources) {
	for (const key of labelledLists(source))
		same(drawsLists, true, 'the list row ' + key + ' has a label, so the page has to draw lists');
}

// The box leaves a member out when it has its default: the page reads that as the default.
const bare = /** @type {any} */ (rowOf({ id: 'x.bare', label: 'Bare', type: 'bool', section: 'x', default: '0' }));
same([bare.secret, bare.locked, bare.held, bare.needsRestart, bare.conditions], [false, false, false, false, []],
	'a row with no secret, locked, needs_restart or conditions member has their defaults');
same(isVisible(bare, {}), true, 'a row with no conditions member is always shown');
same(rowOf({ id: 'x.on', label: 'On', type: 'bool', section: 'x' }) !== null, true, 'a row with no available member is drawn');
const flagged = /** @type {any} */ (rowOf({ id: 'x.flag', label: 'Flag', type: 'bool', section: 'x', locked: true, secret: true, needs_restart: true }));
same([flagged.locked, flagged.secret, flagged.needsRestart], [true, true, true], 'a member that is present and true is read as true');

// A list the box states once is looked up by the name a row gives.
const sharedAnswer = {
	value_lists: { 'list-a': [{ value: 0, key: 'k.off', label: 'Off' }, { value: 1, key: 'k.on', label: 'On' }],
		'list-t': [{ value: 0, text: 'de', label: 'Deutsch' }, { value: 0, text: 'en', label: 'English' }] },
	items: [
		{ id: 'x.one', label: 'One', type: 'enum', section: 'x', values_from: 'list-a' },
		{ id: 'x.two', label: 'Two', type: 'enum', section: 'x', values_from: 'list-a' },
		{ id: 'x.own', label: 'Own', type: 'enum', section: 'x', values: [{ value: 5, label: 'Five' }] },
		{ id: 'x.lang', label: 'Lang', type: 'string', section: 'x', values_from: 'list-t' },
		{ id: 'x.gone', label: 'Gone', type: 'enum', section: 'x', values_from: 'list-missing' },
	],
};
const sharedRows = /** @type {any[]} */ (rowsOf(sharedAnswer));
same(sharedRows[0].choices, [{ value: 0, label: 'Off' }, { value: 1, label: 'On' }], 'values_from gives the row the shared choices');
same(sharedRows[1].choices, sharedRows[0].choices, 'two rows naming one list get the same choices');
same(sharedRows[2].choices, [{ value: 5, label: 'Five' }], 'a row with its own values keeps them');
same(sharedRows[3].choices.map(function (/** @type {any} */ c) { return c.text; }), ['de', 'en'], 'a string row reads shared entries by their text');
same([sharedRows[4].choices.length, sharedRows[4].locked], [0, true], 'a name the answer does not hold leaves a choice with no values, which is held');
same(rowsOf({ items: sharedAnswer.items.slice(0, 1) })[0].choices, [], 'an answer with no value_lists leaves the named list empty');

if (failed > 0) {
	process.stderr.write('settingsrows: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('settingsrows: ' + checked + ' checks passed\n');
