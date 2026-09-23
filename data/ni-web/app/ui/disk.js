/* A file on a disk of this box, and everything about one that is worked out
   rather than acted on.

   THE THIRD ROAD, BESIDE player.js AND webtv.js. Those two say what the box
   would offer for a channel; this one says what came of a file. What holds the
   element and the stream is app/ui/playing.js for all three, and it reaches this
   through import() when somebody plays a file and never otherwise.

   WHY THERE IS ANYTHING TO WORK OUT AT ALL. A recording of this box is a
   transport stream, and no browser opens one. What opens it is the demuxer the
   page already loads for live television: it takes the container apart and hands
   the browser fragmented MP4, so the pictures inside it are the browser's
   business and the container is not.

   What the demuxer cannot do is decode. A recording from a standard definition
   channel is MPEG-2, the demuxer does not even carry it across, and no browser
   has decoded it for years. That is not a defeat, it is the answer, and the
   screen says it in words with the address beside it.

   NOTHING HERE TOUCHES THE ELEMENT OR THE PAGE, apart from the one function that
   asks the browser about itself, and that is what lets test/web/check-web-
   fileplay.sh drive all of it without a browser. */

/**
 * What a media element would be asked about, out of what the demuxer read.
 *
 * The demuxer states a type of its own that names the container it read, which is
 * not what it hands the browser. What it hands over is fragmented MP4, so that is
 * the question, and the codecs it named are the answer to it.
 *
 * @param {{ hasVideo?: boolean, videoCodec?: string, audioCodec?: string } | null} info
 * @returns {string} empty where the demuxer read no picture at all
 */
export function codecsOf(info) {
	if (!info || info.hasVideo !== true || !info.videoCodec) {
		return '';
	}
	return info.audioCodec ? info.videoCodec + ',' + info.audioCodec : info.videoCodec;
}

/**
 * Whether this browser decodes that. The one function here that asks the browser
 * about itself, for the reason written over the same question in app/ui/player.js:
 * a table of browsers written into a page is wrong by the time somebody reads it.
 *
 * @param {string} codecs
 * @returns {boolean}
 */
export function browserAsks(codecs) {
	const source = /** @type {any} */ (window).MediaSource;
	if (!source || typeof source.isTypeSupported !== 'function') {
		return false;
	}
	return source.isTypeSupported('video/mp4; codecs="' + codecs + '"') === true;
}

/**
 * What the demuxer turned out to be handing over, and whether that is playable
 * here.
 *
 * @param {{ hasVideo?: boolean, videoCodec?: string, audioCodec?: string } | null} info
 * @param {(codecs: string) => boolean} plays what the browser answers
 * @returns {{ why: string, codec: string }} why is empty where nothing is wrong
 */
export function verdictOf(info, plays) {
	const codecs = codecsOf(info);
	if (codecs === '') {
		return { why: 'picture', codec: '' };
	}
	if (!plays(codecs)) {
		return { why: 'codec', codec: String(info && info.videoCodec) };
	}
	return { why: '', codec: String(info && info.videoCodec) };
}

/* How near the end of what has arrived still counts as inside it, and how far
   back from the frontier a jump that overshot is put. A landing exactly on the
   frontier is a landing on nothing: the browser has that instant and not the one
   after it. */
const kInside = 0.5;
const kBack = 1;

/**
 * Where a jump may land.
 *
 * Inside what has arrived it lands where it was asked to. Past it there is
 * nothing to fetch the missing part with, so it lands just short of the frontier
 * instead, which goes on playing. Asked for while nothing at all has arrived it
 * is left alone: there is nowhere to hold it to, and a jump turned into a second
 * jump is a loop.
 *
 * @param {number} at
 * @param {ReadonlyArray<{ start: number, end: number }>} arrived
 * @returns {number}
 */
export function landing(at, arrived) {
	let last = -1;
	for (const one of arrived) {
		if (at >= one.start && at < one.end - kInside) {
			return at;
		}
		if (one.end > last) {
			last = one.end;
		}
	}
	const back = last - kBack;
	return back > 0 ? back : at;
}

/**
 * What the element says has arrived, as plain numbers.
 *
 * @param {HTMLMediaElement} media
 * @returns {Array<{ start: number, end: number }>}
 */
export function arrivedIn(media) {
	/** @type {Array<{ start: number, end: number }>} */
	const out = [];
	const ranges = media.buffered;
	for (let i = 0; i < ranges.length; i++) {
		out.push({ start: ranges.start(i), end: ranges.end(i) });
	}
	return out;
}
