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
same(model.tileActions(recording), ['stop', 'info', 'archive'], 'a recording offers stop, details and the archive');
same(model.tileActions(file), [], 'a file with no channel to go back to offers nothing');
same(model.tileActions(Object.assign({}, file, { returnsTo: '2b66' })), ['stop'], 'a file offers stop alone');
same(model.tileActions(model.readPlayback({ source: 'none' })), [], 'nothing playing offers nothing');
same(model.kResyncMs, 15000, 'a playback is read again every fifteen seconds');

// verdict

const FLOOR = 19;
if (checked < FLOOR) {
	process.stderr.write('now-cases.mjs: only ' + checked + ' assertions ran, and there are ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('now-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
console.log('now: ' + checked + ' checked');
