// The sums and the rule the floating player is built on, driven without a browser.
//
// What plays is a module and not a screen, because a media element that changes place
// in the tree is taken down and a stream taken down is gone. Almost all of that module
// reaches the document: it makes an element, it moves the element, it hands the element
// to a demuxer. None of that can be driven here and none of it is tried.
//
// What is driven is the part that decides rather than acts: which of the two slots
// carries the element, where a dragged window ends up once the room it is in has had
// its say, and that starting anything stops whatever was playing whichever kind either
// of them is. The first two are arithmetic and both are what a person notices when
// they are wrong. The third is the rule that used to be two screens remembering to
// call each other.
//
// The module is imported for real and not copied, so a rule rewritten in it is a rule
// rewritten here. That is also why app/ui/playing.js reaches player.js, webtv.js and
// disk.js through import() and not from the top of the file: an import of the runtime
// at the top would need a server to resolve it. Which is also why a start() below gets
// no further than saying what it is about to play, and why that is the whole of what
// is asked of it here.
import {
	slotFor, clamped, sized, start, stop, setSlot, subscribe, ofChannel, ofFile,
} from '../../data/ni-web/app/ui/playing.js';

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
		process.stderr.write('player: ' + what + '\n');
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

// ------------------------------------------------------------------- slots

is(slotFor({ page: true, float: true }) === 'page',
	'the page wins when both are there, or the same channel would be drawn twice');
is(slotFor({ page: true, float: false }) === 'page',
	'the page alone carries the element');
is(slotFor({ page: false, float: true }) === 'float',
	'the window carries it wherever the page does not');
is(slotFor({ page: false, float: false }) === 'none',
	'and with neither there is nowhere to put it');

// ----------------------------------------------------------------- clamping

const room = { w: 1000, h: 800 };
const least = { w: 240, h: 135 };

same(clamped({ x: 100, y: 100, w: 400, h: 225 }, room, least),
	{ x: 100, y: 100, w: 400, h: 225 },
	'a window that fits is handed back as it was asked for');

same(clamped({ x: 0, y: 0, w: 4000, h: 4000 }, room, least),
	{ x: 0, y: 0, w: 1000, h: 800 },
	'a window larger than the room is put on the room');

same(clamped({ x: 900, y: 700, w: 400, h: 225 }, room, least),
	{ x: 600, y: 575, w: 400, h: 225 },
	'a window over the far edge is pushed in and keeps its size');

same(clamped({ x: -300, y: -300, w: 400, h: 225 }, room, least),
	{ x: 0, y: 0, w: 400, h: 225 },
	'and one over the near edge the same way');

same(clamped({ x: 10, y: 10, w: 10, h: 10 }, room, least),
	{ x: 10, y: 10, w: 240, h: 135 },
	'the least size wins against a very small wish');

same(clamped({ x: 0, y: 0, w: 100, h: 100 }, { w: 120, h: 90 }, least),
	{ x: 0, y: 0, w: 240, h: 135 },
	'and against a room smaller than itself, where the window sits at the near edge');

same(clamped({ x: 600, y: 575, w: 400, h: 225 }, room, least),
	{ x: 600, y: 575, w: 400, h: 225 },
	'a window exactly against the far edge is left where it is');

same(clamped({ x: 100, y: 100, w: 400, h: 225 }, { w: 300, h: 800 }, least),
	{ x: 0, y: 100, w: 300, h: 225 },
	'a room that narrowed takes the width and the place, and leaves the height alone');

same(clamped({ x: 200, y: 200, w: 400, h: 900 }, room, least),
	{ x: 200, y: 0, w: 400, h: 800 },
	'a window too tall for the room loses the height and keeps the width');

same(clamped({ x: 0, y: 0, w: 240, h: 135 }, room, least),
	{ x: 0, y: 0, w: 240, h: 135 },
	'a window already at the least size is left at it');

// A drag across the far corner, which is the one that asks all three rules at
// once: written out because a rule that holds on its own can still fail beside
// the others.
const held = clamped({ x: 980, y: 780, w: 400, h: 225 }, room, least);
is(held.w === 400 && held.h === 225, 'a move across the far corner changes no size');
is(held.x + held.w === room.w && held.y + held.h === room.h,
	'and comes to rest against the corner rather than beyond it');

// ------------------------------------------------------------------ resizing

// THE OTHER HALF OF THE SAME ARITHMETIC, AND THE REASON IT IS NOT clamped().
// Dragging the far corner is a statement about that corner and about no other:
// the near one is what the window is anchored by and must not move under the
// hand that is not touching it. clamped() moves rather than shrinks, which is
// right for a window being carried and wrong for one being pulled open, and
// the cases below say the difference in numbers.

same(sized({ x: 100, y: 100, w: 400, h: 225 }, room, least),
	{ x: 100, y: 100, w: 400, h: 225 },
	'a corner pulled to somewhere that fits is granted as it was asked for');

// The window stands at 600 and is pulled 900 wide, which is 500 more than the
// room has left. clamped() would give the width and take the place: the same
// wish through it lands the window at 100, three quarters of the screen away
// from where the hand is.
const pulled = { x: 600, y: 575, w: 900, h: 400 };
same(sized(pulled, room, least),
	{ x: 600, y: 575, w: 400, h: 225 },
	'a corner pulled past the far edge stops at the edge and the near corner stays');
is(clamped(pulled, room, least).x === 100,
	'and that is a different answer from the one clamped gives, which is why there are two');

same(sized({ x: 300, y: 200, w: 10, h: 10 }, room, least),
	{ x: 300, y: 200, w: 240, h: 135 },
	'a corner pulled back through the near one gets the least size and moves nothing');

same(sized({ x: 200, y: 100, w: 800, h: 700 }, room, least),
	{ x: 200, y: 100, w: 800, h: 700 },
	'a corner pulled exactly onto the far edge is left there');

same(sized({ x: 900, y: 700, w: 400, h: 225 }, room, least),
	{ x: 760, y: 665, w: 240, h: 135 },
	'a window so near the far edge that the least size will not fit is pulled back to where it does');

same(sized({ x: 0, y: 0, w: 100, h: 100 }, { w: 120, h: 90 }, least),
	{ x: 0, y: 0, w: 240, h: 135 },
	'and a room smaller than the least size puts it at the near edge, as clamped does');

// ------------------------------------------------------ what is handed out

// THE SLOT THE ELEMENT SITS IN IS PART OF WHAT SUBSCRIBERS ARE TOLD, and the reason
// is not arithmetic. Everything that keeps what it was told keeps it by identity, so
// the same object handed out a second time is an announcement that never arrives.
// That is what left the window on shell level standing as an empty black box over the
// screen that had just taken the element back off it, with a close button that ended
// the stream.
//
// It runs without a document because nothing it touches reaches one: start() says
// what is playing and then asks for the road, the road is an import() with no server
// behind it here, and the element is made on the far side of that.

/** @type {Array<import('../../data/ni-web/app/ui/playing.js').Playing | null>} */
const told = [];
const done = subscribe(function (what) { told.push(what); });

start(ofChannel({ id: '1', name: 'One' }));
is(told.length === 1, 'starting a channel is announced once');
is(told[0] !== null && told[0].slot === 'none',
	'and says the element is in neither slot, because nothing has offered one');

setSlot('page', /** @type {any} */ ({}));
is(told.length === 2, 'a screen offering a place of its own is announced');
is(told[1] !== told[0],
	'as another object, or everything holding the first would throw the second away unread');
is(told[1] !== null && told[1].slot === 'page', 'and the new one says the page carries it');
is(told[0] !== null && told[1] !== null && told[1].source === told[0].source,
	'while the source is the one that was playing, since none of that changed');

setSlot('float', /** @type {any} */ ({}));
is(told.length === 2,
	'the window offering a place while a screen holds one changes nothing and is not announced');

setSlot('page', null);
is(told.length === 3, 'the screen taking its offer back is announced');
is(told[2] !== null && told[2].slot === 'float', 'and hands the element to the window');

setSlot('float', null);
is(told.length === 4 && told[3] !== null && told[3].slot === 'none',
	'and with neither offering it waits in neither');

stop();
is(told.length === 5 && told[4] === null, 'stopping says there is nothing playing');

done();

// --------------------------------------------------- one source and only one

// A CHANNEL AND A FILE GO THROUGH THE SAME DOOR, which is the whole of the rule.
// It used to be written by hand in two places: the screen that plays a recording
// stopped whatever was playing when it started, and listened for a channel
// starting so it could take itself down. Two lines, either of which somebody can
// forget, and forgetting one is two soundtracks at once and a tuner nobody gave
// back. There is one of this module, so there is one source, and that is now a
// property of the code rather than an agreement between screens.

/** @type {Array<import('../../data/ni-web/app/ui/playing.js').Playing | null>} */
const heard = [];
const off = subscribe(function (what) { heard.push(what); });

const kRecording = '/api/v1/storage/file?path=%2Fmedia%2Fsda1%2Fmovies%2FTagesschau.ts';
start(ofFile('Tagesschau.ts', kRecording, { how: 'demuxed', sound: false }));
is(heard.length === 1, 'starting a file is announced once, the way a channel is');
is(heard[0] !== null && heard[0].source.of === 'file', 'and what is playing is a file');
is(heard[0] !== null && heard[0].source.of === 'file' && heard[0].source.href === kRecording,
	'carrying the address the listing handed over');
is(heard[0] !== null && heard[0].source.of === 'file' && heard[0].source.demuxed === true,
	'and the road its name decided, which is worked out beside the listing and not here');
is(heard[0] !== null && heard[0].source.id === kRecording,
	'the address is what tells one file from another, since two rows can carry one name');

start(ofChannel({ id: '2', name: 'Two' }));
is(heard.length === 3, 'a channel started over a file is two announcements and not one');
is(heard[1] === null, 'the first of them is the file stopped, which is what gives the stream up');
is(heard[2] !== null && heard[2].source.of === 'channel',
	'and the second is the channel, so only one of the two is ever playing');

start(ofFile('song.mp3', '/api/v1/storage/file?path=%2Ftmp%2Fsong.mp3', { how: 'here', sound: true }));
is(heard.length === 5 && heard[3] === null,
	'and a file started over a channel gives the tuner back first');
is(heard[4] !== null && heard[4].source.of === 'file' && heard[4].source.sound === true,
	'sound with no picture is carried over as the road decided it, which is what shapes the window');
is(heard[4] !== null && heard[4].source.of === 'file' && heard[4].source.demuxed === false,
	'and a file the browser opens itself takes no demuxer');

stop();
off();

// --------------------------------------------------- what a source is made of

// What the window reads off one, and nothing else. Two members, because a
// recording is named after the channel it came off and the name alone therefore
// says neither which of the two it is nor whether there is a picture.
const radio = ofChannel({ id: '3', name: 'Three', kind: 'radio' });
is(radio.of === 'channel' && radio.id === '3' && radio.name === 'Three',
	'a channel is named and told apart by what the channel route calls it');
is(radio.sound === true, 'and radio is sound with no picture');
is(ofChannel({ id: '4', name: 'Four', kind: 'webradio' }).sound === true,
	'as is radio with an address of its own');
is(ofChannel({ id: '5', name: 'Five', kind: 'tv' }).sound === false,
	'and television is not');
is(radio.channel.id === '3',
	'the reading travels with it, because the screen that drew it needs the whole of it back');

const recording = ofFile('Tagesschau.ts', kRecording, { how: 'demuxed', sound: false });
is(recording.of === 'file' && recording.name === 'Tagesschau.ts',
	'a file is named by its leaf, which is what stands in the row somebody pressed');
is(recording.sound === false && recording.demuxed === true,
	'and carries the two things the window and the road need out of what its name decided');

// ------------------------------------------------------------------ verdict

const FLOOR = 50;
if (checked < FLOOR) {
	process.stderr.write('player-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('player-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-player.sh: ' + checked + ' assertions over the slot the element sits in, the room the window is held to, and the one source there can be\n');
