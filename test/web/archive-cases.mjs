// What the archive part reads out of the box's answers and sends back, run without a browser.
import * as loader from 'node:module';

if (typeof loader.registerHooks !== 'function') {
	process.stderr.write('archive-cases.mjs: this node cannot register a resolver, and the page names its runtime by an address only a server resolves\n');
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
			throw new Error('archive-cases.mjs: no stub for the runtime module ' + spec);
		}
		return next(spec, context);
	},
});

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
		process.stderr.write('archive: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

const model = await import('../../data/ni-web/app/screens/recordings/archive.model.js');

same(model.readArchive({ items: [{ id: '0123456789abcdef', title: 'Tatort', channel: 'Das Erste HD', channel_id: '0',
	start: 1790000000, duration: 5400, size: 1880, playing: false }], total: 17, next_offset: 15 }),
	{ items: [{ id: '0123456789abcdef', title: 'Tatort', channel: 'Das Erste HD', start: 1790000000, duration: 5400,
		size: 1880, playing: false }], total: 17, next: 15 },
	'a page is read with its next offset');
same(model.readArchive({ items: [{ id: 'nope' }, { id: 'fedcba9876543210', title: 7 }], total: 2 }).items.map(function (i) { return i.id; }),
	['fedcba9876543210'], 'a row without a proper id is left out, a wrong title read as empty');
same(model.readArchive({ items: [], total: 0 }).next, -1, 'the last page has no next');
same(model.readArchive('nonsense'), { items: [], total: 0, next: -1 }, 'nothing out of something that is not an answer');
same(model.archiveFileHref('0123456789abcdef'), '/api/v1/recordings/archive/0123456789abcdef/file', 'the file address');
same(model.archivePlaylistHref('0123456789abcdef', 'tok en'),
	'/api/v1/recordings/archive/0123456789abcdef/playlist.m3u?token=tok%20en', 'the playlist address carries the token');
same(model.archivePlaylistHref('0123456789abcdef', 'dev-token-123'),
	'/api/v1/recordings/archive/0123456789abcdef/playlist.m3u?token=dev-token-123', 'a signed in owner hands VLC the token');
same(model.archivePlaylistHref('0123456789abcdef', ''),
	'/api/v1/recordings/archive/0123456789abcdef/playlist.m3u', 'a reader without a sign in gets the bare address');
same(model.archiveQuery('', 0), {}, 'the first page asks nothing extra');
same(model.archiveQuery(' Krimi ', 15), { title: 'Krimi', offset: '15' }, 'filter and offset are sent trimmed');
same(model.archiveQuery('', 0, 'start', 'desc'), {}, 'the route\'s own order asks nothing extra');
same(model.archiveQuery('', 0, 'title', 'asc'), { sort: 'title' }, 'a key in its first order sends the key alone');
same(model.archiveQuery('', 30, 'start', 'asc'), { offset: '30', sort: 'start', order: 'asc' },
	'the start sorted the other way sends the order and keeps the offset');
same(model.archiveQuery('Krimi', 0, 'size', 'asc'), { title: 'Krimi', sort: 'size', order: 'asc' },
	'filter, key and order together');
same(model.archiveQuery('', 0, 'channel', 'desc'), { sort: 'channel', order: 'desc' }, 'a name sorted from Z');
same(model.kArchiveSortKeys, ['start', 'title', 'channel', 'duration', 'size'], 'the keys the route takes');
same(model.kArchiveSortKeys.map(model.archiveFirstOrder), ['desc', 'asc', 'asc', 'desc', 'desc'],
	'newest, longest and largest first, names from A');
same(model.nextArchiveSort({ column: 'start', dir: 'desc' }, 'start'), { column: 'start', dir: 'asc' },
	'the active key flips');
same(model.nextArchiveSort({ column: 'title', dir: 'desc' }, 'title'), { column: 'title', dir: 'asc' },
	'and flips back');
same(model.nextArchiveSort({ column: 'title', dir: 'asc' }, 'size'), { column: 'size', dir: 'desc' },
	'another key starts in its own first order');
same(model.nextArchiveSort({ column: 'size', dir: 'desc' }, 'channel'), { column: 'channel', dir: 'asc' },
	'a name starts from A');
same(model.onlyKeys({ a: 1, b: 2, c: 3 }, ['a', 'c']), { a: 1, c: 3 }, 'answers of queries left behind are let go');
same(model.onlyKeys({ a: 1 }, ['a', 'b']), { a: 1 }, 'a query not answered yet stays absent');
same(model.lengthWords(5400), '1:30 h', 'an hour and a half');
same(model.lengthWords(2700), '45 min', 'under an hour');
same(model.lengthWords(0), '', 'no length');
same(model.readArchiveDetails({ id: '0123456789abcdef', title: 'Tatort', channel: 'Das Erste HD', channel_id: '2dfdc1c35',
	start: 1790000000, duration: 5400, size: 1880, playing: true, description: 'Krimi', long_description: 'Eins\nZwei',
	genre: 16, genre_minor: 1, series: 'Tatort', country: 'DE', year: 2025, rating: 81, quality: 2, age: 12,
	audio: ['Deutsch', 7, 'Englisch'], cover: true }),
	{ item: { id: '0123456789abcdef', title: 'Tatort', channel: 'Das Erste HD', start: 1790000000, duration: 5400,
		size: 1880, playing: true }, description: 'Krimi', longDescription: 'Eins\nZwei', genre: 16, series: 'Tatort',
		country: 'DE', year: 2025, rating: 81, quality: 2, age: 12, audio: ['Deutsch', 'Englisch'], cover: true },
	'the details are read whole, a track name that is no text left out');
same(model.readArchiveDetails({ id: 'fedcba9876543210', title: 'Bare', cover: false }),
	{ item: { id: 'fedcba9876543210', title: 'Bare', channel: '', start: 0, duration: 0, size: 0, playing: false },
		description: '', longDescription: '', genre: 0, series: '', country: '', year: 0, rating: 0, quality: 0, age: 0,
		audio: [], cover: false },
	'what the box left out reads as empty');
const odd = model.readArchiveDetails({ id: 'fedcba9876543210', title: 'Odd', rating: 101, quality: 4, age: 21 });
same(odd && [odd.rating, odd.quality, odd.age], [0, 0, 0], 'a rating past 100, more than 3 stars or an age past 18 is left out');
const locked = model.readArchiveDetails({ id: 'fedcba9876543210', title: 'Locked', rating: 100, quality: 3, age: 99 });
same(locked && [locked.rating, locked.quality, locked.age], [100, 3, model.kAlwaysLocked],
	'the highest of each is kept, and 99 is the age that always locks');
same(model.kAlwaysLocked, 99, 'always locked is the movie browser\'s 99');
same(model.readArchiveDetails({ id: '../x', title: 'Bad' }), null, 'no details without a proper id');
same(model.readArchiveDetails('nonsense'), null, 'no details out of something that is not an answer');
same(model.archiveCoverHref('0123456789abcdef'), '/api/v1/recordings/archive/0123456789abcdef/cover', 'the cover address');
same(model.paragraphsOf('Eins\n\n  Zwei  \r\nDrei'), ['Eins', 'Zwei', 'Drei'], 'a long text is its lines, blank ones dropped');
same(model.paragraphsOf(''), [], 'no long text, no paragraph');

// Every refusal a play on the TV hands back has words of the page's own, in both languages.
const words = (await import('../../data/ni-web/app/screens/recordings/recordings.text.js')).default;
const tv = model.kTvRefusals || {};
for (const code of ['recording-playing', 'mode-unavailable', 'playback-running', 'box-in-standby']) {
	same(typeof tv[code], 'string', 'a play refused with ' + code + ' says so in words');
	same([typeof words.de[tv[code]], typeof words.en[tv[code]]], ['string', 'string'],
		'the words for ' + code + ' are there in German and English');
}

// A refusal with no words of its own is said by its status, never in the box's English.
const refusalKey = model.refusalKey;
const kRaw = { title: 'Forbidden', detail: 'this endpoint is not open to this caller' };
same([403, 401, 409, 404, 500, 0].map(function (status) { return refusalKey({ problem: Object.assign({ status: status }, kRaw) }); }),
	['rec.refused.forbidden', 'rec.refused.signin', 'rec.refused.conflict', 'rec.refused.other', 'rec.refused.box', 'rec.refused.none'],
	'a refusal is one of this area\'s own sentences, chosen by its status');
same(refusalKey(new Error('offline')), 'rec.refused.none', 'no answer is the box not answering');
for (const key of ['rec.refused.forbidden', 'rec.refused.signin', 'rec.refused.conflict', 'rec.refused.other', 'rec.refused.box', 'rec.refused.none'])
	same([typeof words.de[key], typeof words.en[key]], ['string', 'string'], key + ' is there in German and English');

// verdict

const FLOOR = 44;
if (checked < FLOOR) {
	process.stderr.write('archive-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('archive-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-archive.sh: ' + checked + ' assertions over what the archive reads and addresses\n');
