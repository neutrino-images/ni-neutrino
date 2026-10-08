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

const { Control, Row } = await import('../../data/ni-web/app/screens/settings/rows.js');
const { rowOf, changed, numberFault } = await import('../../data/ni-web/app/screens/settings/model.js');
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

// The quick 4:3 buttons offer what the schema lists for the setting, in the
// page's own words, and all four before the schema has answered.
/** @param {number[]} values */
function schemaWith(values) {
	return { items: [
		{ id: 'video_Mode', values: [{ value: 9, label: '1080i 50Hz' }] },
		{ id: 'video_43mode', values: values.map(function (v) { return { value: v, label: String(v) }; }) },
	] };
}
same(aspectModes(schemaWith([0, 3, 1, 2])).map(function (one) { return one.value; }), [0, 1, 2, 3],
	'every mode the box lists, in the order of the page');
same(aspectModes(schemaWith([0, 1, 2])).map(function (one) { return one.key; }),
	['now.quick.43.panscan', 'now.quick.43.letterbox', 'now.quick.43.full'], 'a mode the box does not list is left out');
same(aspectModes(null).length, 4, 'all four before the schema answers');
same(aspectModes({ items: [] }), [], 'none where the schema answered without the setting');

// The choices a setting offers are the values the schema lists for it.
same(settingChoices({ items: [{ id: 'video_Mode', values: [{ value: 9, label: '1080i 50Hz' }] }] }, 'video_Mode'),
	[{ value: 9, label: '1080i 50Hz' }], 'a setting\'s choices are its values');

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

if (failed > 0) {
	process.stderr.write('settingsrows: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('settingsrows: ' + checked + ' checks passed\n');
