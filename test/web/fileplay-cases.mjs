// What the page decides about a file on a disk of this box, driven without a
// browser.
//
// Three questions, and playing a recording is the whole of what depends on them.
// Which road a name takes, which is arithmetic over the last dot in it
// (app/screens/files/paths.js). What the demuxer turned out to be handing the
// browser, once it says so, and whether this browser decodes that. And where a
// jump may land, which is the one place a control is stopped from promising
// something the file cannot do. The last two are app/ui/disk.js, which is the
// road app/ui/playing.js takes for a file the way it takes app/ui/player.js for
// a channel.
//
// None of the three is visible in a type or in a compiler. A road that changed is
// a recording refused that plays, or a black rectangle where a sentence belongs,
// and both of those are reported by telephone rather than by a build.
//
// NOTHING HERE IS STUBBED, and that is worth a sentence because it used to be.
// The deciding lived in a screen, the screen imports the runtime, and the runtime
// is named by an address only a server resolves, so the four runtime modules had
// to be faked before any of this could be read. They are separated now: the
// modules below import nothing at all, so an import of the runtime finding its
// way into either of them is this file failing loudly rather than this file
// quietly resolving a stand-in.
import { fileRoad, mediaKind } from '../../data/ni-web/app/screens/files/paths.js';
import { codecsOf, verdictOf, landing } from '../../data/ni-web/app/ui/disk.js';
import words from '../../data/ni-web/app/screens/files/files.text.js';

// --------------------------------------------------------------- the tally

let checked = 0;
let failed = 0;

/**
 * @param {boolean} ok
 * @param {string} what
 */
function is(ok, what) {
	checked++;
	if (!ok) {
		failed++;
		process.stderr.write('fileplay: ' + what + '\n');
	}
}

/**
 * @param {unknown} got
 * @param {unknown} want
 * @param {string} what
 */
function same(got, want, what) {
	is(JSON.stringify(got) === JSON.stringify(want),
		what + ': got ' + JSON.stringify(got) + ', wanted ' + JSON.stringify(want));
}

// ------------------------------------------------------------- which road

/* A recording of this box. The name is all there is to go on, and what it says
   is the container and never the picture inside it. */
same(fileRoad('film.ts'), { how: 'demuxed', sound: false, why: '' },
	'a transport stream goes through the demuxer');
same(fileRoad('film.m2ts'), { how: 'demuxed', sound: false, why: '' },
	'and so does the other spelling of one');
same(fileRoad('/media/sda1/movie/FILM.TS'), { how: 'demuxed', sound: false, why: '' },
	'a whole path and a name shouted are the same name');

/* What no browser and no demuxer here opens. Refused before anything is
   fetched, which is the one refusal that can be made on the name alone. */
same(fileRoad('alt.mpg'), { how: 'none', sound: false, why: 'program' },
	'a program stream is refused on its name');
same(fileRoad('alt.mpeg'), { how: 'none', sound: false, why: 'program' },
	'under either spelling');
same(fileRoad('VTS_01_1.VOB'), { how: 'none', sound: false, why: 'program' },
	'and a disc image track is one of those');

// What the browser opens by itself, which needs nothing of this.
for (const name of ['clip.mp4', 'clip.m4v', 'clip.mkv', 'clip.webm', 'clip.mov']) {
	same(fileRoad(name), { how: 'here', sound: false, why: '' },
		name + ' is handed to the browser as it is');
}
for (const name of ['song.mp3', 'song.m4a', 'song.aac', 'song.flac', 'song.ogg', 'song.wav']) {
	same(fileRoad(name), { how: 'here', sound: true, why: '' },
		name + ' is sound with no picture');
}

// A name nothing is offered for, which is the state of most of a disk.
same(fileRoad('neutrino.conf'), { how: 'none', sound: false, why: 'unknown' },
	'a name this page reads nothing out of takes no road');
same(fileRoad('film.ts.gz'), { how: 'none', sound: false, why: 'unknown' },
	'a recording packed for storage is bytes and not a stream');
same(fileRoad('README'), { how: 'none', sound: false, why: 'unknown' },
	'nor does a name with no dot in it');

/* The row draws a control out of mediaKind and the panel behind it out of
   fileRoad, so a name the first offers and the second has no road for would be a
   control that opens nothing, and a name the second plays and the first hides
   would be a player nobody can reach. */
for (const name of ['film.ts', 'film.m2ts', 'alt.mpg', 'alt.mpeg', 'x.vob', 'clip.mp4',
	'clip.m4v', 'clip.mkv', 'clip.webm', 'clip.mov', 'song.mp3', 'song.m4a', 'song.aac',
	'song.flac', 'song.ogg', 'song.wav', 'neutrino.conf', 'README', 'film.ts.gz']) {
	const offered = mediaKind(name) !== '';
	const road = fileRoad(name);
	is(offered === (road.why !== 'unknown'),
		name + ': the row offers a control exactly where the panel has something to say');
	is(!road.sound || mediaKind(name) === 'audio',
		name + ': sound with no picture is what the listing calls sound');
}

// ------------------------------------------------- what the demuxer handed over

const kH264 = { hasVideo: true, videoCodec: 'avc1.4d401e', hasAudio: true, audioCodec: 'mp4a.40.2' };
const kHevc = { hasVideo: true, videoCodec: 'hvc1.1.6.L93.B0', hasAudio: true, audioCodec: 'mp4a.40.2' };

is(codecsOf(kH264) === 'avc1.4d401e,mp4a.40.2',
	'the picture and the sound together, which is what a media element is asked about');
is(codecsOf({ hasVideo: true, videoCodec: 'avc1.4d401e' }) === 'avc1.4d401e',
	'the picture alone where the demuxer named no sound');
is(codecsOf(null) === '', 'nothing read is nothing to ask about');
is(codecsOf({ hasVideo: false, audioCodec: 'mp4a.40.2' }) === '',
	'and sound with no picture is nothing to ask about either, which is what a recording of an MPEG-2 channel arrives as');
is(codecsOf({ hasVideo: true, videoCodec: '' }) === '',
	'a picture the demuxer could not name is the same answer');

/** A browser that plays all of it. */
function everything() { return true; }
/** One that plays none of it. */
function nothing() { return false; }

same(verdictOf(kH264, everything), { why: '', codec: 'avc1.4d401e' },
	'H.264 out of a transport stream plays, which is what a recording of this box holds');
same(verdictOf(kHevc, nothing), { why: 'codec', codec: 'hvc1.1.6.L93.B0' },
	'a codec the browser turns down is refused under its own name');
same(verdictOf({ hasVideo: false, hasAudio: true, audioCodec: 'mp4a.40.2' }, everything),
	{ why: 'picture', codec: '' },
	'no picture at all is refused whatever the browser says, because there is nothing to ask it about');
same(verdictOf(null, everything), { why: 'picture', codec: '' },
	'and so is nothing read at all');

/* What the browser was asked, which has to be the type it would be handed and
   not the container the demuxer read: mpegts.js states video/mp2t for itself and
   hands over fragmented MP4. */
const asked = [];
verdictOf(kH264, function (codecs) { asked.push(codecs); return true; });
same(asked, ['avc1.4d401e,mp4a.40.2'], 'the browser is asked about the codecs and not about the container');

// ------------------------------------------------------------ where a jump lands

const kArrived = [{ start: 0, end: 180 }];

is(landing(60, kArrived) === 60, 'a jump inside what has arrived lands where it was asked to');
is(landing(0, kArrived) === 0, 'the beginning of it included');
is(landing(179.9, kArrived) === 179, 'the very frontier is not inside it: there is that instant and not the one after');
is(landing(400, kArrived) === 179, 'a jump past the end lands just short of the end, which goes on playing');
is(landing(400, []) === 400,
	'a jump while nothing has arrived is left alone, since a jump turned into a second jump is a loop');
is(landing(400, [{ start: 0, end: 0.4 }]) === 400,
	'and so is one with too little arrived to land in');
is(landing(20, [{ start: 0, end: 10 }, { start: 15, end: 40 }]) === 20,
	'a hole in the middle is two stretches and a jump into the second is inside');
is(landing(12, [{ start: 0, end: 10 }, { start: 15, end: 40 }]) === 39,
	'a jump into the hole lands at the frontier of the last of them');

// --------------------------------------------------------------- and the words

/* Every road and every refusal the module can name has a sentence in both
   languages. A key without one draws itself on screen, which is how the last one
   of these was found. */
for (const how of ['here', 'demuxed']) {
	for (const lang of ['de', 'en']) {
		is(typeof words[lang]['files.play.how.' + how] === 'string' && words[lang]['files.play.how.' + how] !== '',
			'the road ' + how + ' has a ' + lang + ' sentence');
	}
}
for (const why of ['program', 'picture', 'codec', 'browser', 'load', 'stream', 'format']) {
	for (const lang of ['de', 'en']) {
		is(typeof words[lang]['files.play.no.' + why] === 'string' && words[lang]['files.play.no.' + why] !== '',
			'the refusal ' + why + ' has a ' + lang + ' sentence');
	}
}
for (const lang of ['de', 'en']) {
	is(String(words[lang]['files.play.no.codec']).indexOf('{codec}') >= 0,
		'the ' + lang + ' refusal that is about one codec names it');
}

// ------------------------------------------------------------------ verdict

const FLOOR = 80;
if (checked < FLOOR) {
	process.stderr.write('fileplay-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('fileplay-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-fileplay.sh: ' + checked + ' assertions over the roads, the codecs, the jumps and the words\n');
