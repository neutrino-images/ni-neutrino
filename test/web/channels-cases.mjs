// What the channel list works out for one row, run without a browser.
import * as loader from 'node:module';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('channels-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
	process.exit(1);
}

const kStubs = {
	'/vendor/preact.module.js':
		'export function h() { return null; }\n' +
		'export function render() {}\n' +
		'export function Fragment() { return null; }\n',
	'/vendor/htm.module.js':
		'export default { bind: function () { return function () { return null; }; } };\n',
	'/vendor/hooks.module.js':
		['useState', 'useEffect', 'useLayoutEffect', 'useRef', 'useMemo', 'useCallback', 'useId']
			.map(function (name) { return 'export function ' + name + '() {}\n'; }).join(''),
	'/vendor/preact-router.module.js':
		'export default function Router() { return null; }\n' +
		'export function Link() { return null; }\n' +
		'export function route() {}\n' +
		'export function getCurrentUrl() { return ""; }\n',
};

loader.registerHooks({
	resolve: function (spec, context, next) {
		if (Object.prototype.hasOwnProperty.call(kStubs, spec)) {
			return { url: 'data:text/javascript,' + encodeURIComponent(kStubs[spec]), shortCircuit: true };
		}
		if (spec.indexOf('/vendor/') === 0) {
			throw new Error('channels-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

const list = await import('../../data/ni-web/app/screens/channels/list.js');
const event = await import('../../data/ni-web/app/ui/event.js');
const { t } = await import('../../data/ni-web/app/i18n.js');
const text = (await import('../../data/ni-web/app/screens/channels/list.text.js')).default;

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
		process.stderr.write('channels: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

const channel = /** @type {any} */ ({ id: 'b9b0040200016dcb', name: 'Dummy Eins HD' });
const running = { id: '1', channel_id: channel.id, title: 'Tagesschau', start: 100, duration: 900 };
const later = { id: '2', channel_id: channel.id, title: 'Sport', start: 1000, duration: 900 };

/** @type {unknown[]} */
let opened = [];
/** @param {unknown} event */
function open(event) { opened.push(event); }

// the info action

const on = list.infoAction(channel, { now: running, next: later }, open);
same(on.id, 'info', 'the action is named info');
same(on.mark, 'ℹ︎', 'the mark is the information sign, held to text and not a coloured emoji');
same(!!on.disabled, false, 'a running programme can be opened');
same(on.label, t(text, 'list.act.info', { name: channel.name }), 'the label names the channel');
on.onAct();
same(opened, [running], 'it opens the programme on air and not the next one');

opened = [];
const none = list.infoAction(channel, { now: null, next: later }, open);
same(!!none.disabled, true, 'nothing on air is nothing to open');
same(none.label, t(text, 'list.act.info.none', { name: channel.name }), 'and says there is no programme information');
none.onAct();
same(opened, [], 'a disabled action opens nothing');

const asking = list.infoAction(channel, null, open);
same(!!asking.disabled, true, 'a row the guide has not answered yet cannot open anything');
same(asking.label, t(text, 'list.act.info', { name: channel.name }), 'and does not claim there is nothing');

same(t(text, 'list.act.info.none', { name: 'X' }) !== t(text, 'list.act.info', { name: 'X' }), true,
	'the two labels differ');

// the same mark on the shared event sheet

const shown = /** @type {any} */ ({ id: '3', channel_id: channel.id, title: 'Tatort', start: 200, duration: 900 });
const about = event.eventActions({ event: shown, at: 500, onOpen: open })[0];
same(about.id, 'about', 'the about action comes first where there is somewhere to open it');
same(about.mark, 'ℹ︎', 'and carries the channel list\'s information sign, not a plain letter');
same(about.mark.indexOf('︎') !== -1, true, 'and that sign is held to text, not left to a colour emoji font');

// the record mark, on and off the air: a coloured dot is wrong in a list of plain signs

const recordOnAir = event.eventActions({ event: shown, at: 250, onOpen: open }).find(function (a) { return a.id === 'record'; });
const recordTimer = event.eventActions({ event: shown, at: 2000, onOpen: open }).find(function (a) { return a.id === 'record'; });
same(recordOnAir.mark.indexOf('︎') !== -1, true, 'recording now is held to text, not a coloured record button');
same(recordTimer.mark.indexOf('︎') !== -1, true, 'and so is a timer for later');

if (failed > 0) {
	process.stderr.write('channels: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
process.stdout.write('channels: ' + checked + ' checked\n');
