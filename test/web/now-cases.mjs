// What the Now tile reads out of GET /api/v1/playback and offers for it, run without a browser.
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
		process.stderr.write('now: ' + what + ': ' + JSON.stringify(got) + ' rather than ' + JSON.stringify(want) + '\n');
	}
}

const model = await import('../../data/ni-web/app/playback.js');

const recording = model.readPlayback({ source: 'recording',
	recording: { id: 'dd27cf8d43d5a28f', title: 'Tatort', channel: 'Das Erste HD' },
	position: 754, duration: 5400, paused: false, state: 'playing',
	returns_to: { id: 'ffffffff8592575f', name: 'ARD Plus Krimi' } });
same(recording, { source: 'recording', id: 'dd27cf8d43d5a28f', title: 'Tatort', channel: 'Das Erste HD', name: '',
	position: 754, duration: 5400, paused: false, state: 'playing', returnsTo: 'ffffffff8592575f' },
	'a recording is read with where it stands and where the box goes back to');
const file = model.readPlayback({ source: 'file', file: { name: 'Urlaub.mkv', title: '' },
	position: 5, duration: 0, paused: true, state: 'paused' });
same([file.source, file.name, file.title, file.paused, file.returnsTo], ['file', 'Urlaub.mkv', '', true, ''],
	'a file is read by its name');
same(model.readPlayback({ source: 'channel', channel: { id: '2b66', name: 'Das Erste HD' }, timeshift: false }).source,
	'channel', 'a channel is a channel');
same(model.readPlayback(null).source, 'none', 'no answer is nothing');
same(model.readPlayback({ source: 'none' }).position, 0, 'nothing stands nowhere');

same([model.isPlayback(recording), model.isPlayback(file), model.isPlayback(model.readPlayback({ source: 'channel' }))],
	[true, true, false], 'a recording and a file are a playback and a channel is not');

same(model.positionAt(recording, 0), 754, 'the answer stands where it was read');
same(model.positionAt(recording, 15999), 769, 'playing it moves on with the clock');
same(model.positionAt(recording, 9999999), 5400, 'and never past its end');
same(model.positionAt(file, 15000), 5, 'paused it stands still');
same(model.positionAt(model.readPlayback({ source: 'file', position: 7, state: 'playing' }), 3000), 10,
	'with no length known it moves on unbounded');
same(model.positionAt(model.readPlayback({ source: 'file', position: 7, state: 'forward' }), 3000), 7,
	'winding it stays where the box last said');

same([model.timeText(0), model.timeText(59), model.timeText(754), model.timeText(5400), model.timeText(-3)],
	['0:00', '0:59', '12:34', '1:30:00', '0:00'], 'a place reads as a clock');

same(model.tileActions(model.readPlayback({ source: 'channel', channel: { id: '2b66', name: 'X' } })),
	[], 'a channel is drawn with its own actions, none of these');
same(model.tileActions(recording), ['stop', 'info', 'm3u', 'browser', 'archive'],
	'a recording offers stop, details, its playlist, the browser and the archive');
same(model.tileActions(file), [], 'a file with no channel to go back to offers nothing');
same(model.tileActions(Object.assign({}, file, { returnsTo: '2b66' })), ['stop'], 'a file offers stop alone');
same(model.tileActions(model.readPlayback({ source: 'none' })), [], 'nothing playing offers nothing');
same(model.kResyncMs, 15000, 'a playback is read again every fifteen seconds');

// The order of the overview's cards, while this browser moves them.
const order = await import('../../data/ni-web/app/screens/now/order.js');
const own = order.CARDS.slice();

same(order.orderOf(null), own, 'nothing kept is the page\'s own order');
same(order.orderOf(''), own, 'an empty line is the page\'s own order');
same(order.orderOf('quick,playing,box,sound,message,recordings,storage,reception'),
	['quick', 'playing', 'box', 'sound', 'message', 'recordings', 'storage', 'reception'],
	'a whole line is read as it stands');
same(order.orderOf('quick, nonsense ,quick,playing'),
	['quick', 'playing', 'box', 'sound', 'message', 'recordings', 'storage', 'reception'],
	'an unknown name and a second mention are dropped, and the rest follow their own neighbours');
same(order.orderOf('quick,sound,box,playing,recordings,storage,reception'),
	['quick', 'sound', 'message', 'box', 'playing', 'recordings', 'storage', 'reception'],
	'a card the line does not name goes in behind the one it follows in the page');
same(order.orderOf('sound,playing'),
	['sound', 'message', 'recordings', 'storage', 'reception', 'quick', 'playing', 'box'],
	'and so does every one of several, each behind the one before it');
same(order.orderOf('nonsense'), own, 'a line naming nothing this page draws is the page\'s own order');
same(order.orderOf(own.join(',')) === order.CARDS, true, 'the page\'s own order read back is the page\'s own order');

same([order.isOwn(own), order.isOwn(own.slice().reverse()), order.isOwn(own.slice(1))], [true, false, false],
	'only all eight in the page\'s order are the page\'s own order');

same(order.shifted(own, 'box', -1), ['box', 'playing', 'sound', 'message', 'recordings', 'storage', 'reception', 'quick'],
	'a card moves one place forward');
same(order.shifted(own, 'box', 1), ['playing', 'sound', 'box', 'message', 'recordings', 'storage', 'reception', 'quick'],
	'and one place back');
same([order.shifted(own, 'playing', -1) === own, order.shifted(own, 'quick', 1) === own, order.shifted(own, 'gone', 1) === own],
	[true, true, true], 'past either end, or for a card that is not there, the list is handed back as it is');

same(order.dropped(own, 'quick', 'playing', false),
	['quick', 'playing', 'box', 'sound', 'message', 'recordings', 'storage', 'reception'],
	'a card dropped in front of another lands there');
same(order.dropped(own, 'playing', 'storage', true),
	['box', 'sound', 'message', 'recordings', 'storage', 'playing', 'reception', 'quick'],
	'and behind it');
same(order.dropped(own, 'box', 'sound', false) === own, true, 'a drop that changes nothing changes nothing');

const box = { left: 100, top: 200, width: 300, height: 100 };
same([order.behind(box, 120, 280, true), order.behind(box, 380, 220, true)], [true, false],
	'a card alone in its row is split across, top and bottom');
same([order.behind(box, 120, 280, false), order.behind(box, 380, 220, false)], [false, true],
	'one that shares its row is split down, left and right');

// verdict

const FLOOR = 36;
if (checked < FLOOR) {
	process.stderr.write('now-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('now-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
console.log('now: ' + checked + ' checked');
