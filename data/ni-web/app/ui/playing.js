/* What the page is playing, which is one source and outlives every screen.

   ONE SOURCE, AND A SOURCE IS EITHER KIND. A channel of this box or a file on a
   disk of it: both end up as one media element with one stream in it, and
   everything below the road that fills it is the same arithmetic over two slots
   and a rectangle. The rule that only one thing plays at a time is therefore not
   an agreement between screens any more, it is what this module is: start()
   stops whatever was playing before it says what is playing now, and there is
   one of it for the page.

   WHAT THE TWO COST IS NOT THE SAME, and the window says so rather than hiding
   it. A channel holds a tuner and a converter of the box, and the box converts
   at most two sounds at once, so a channel somebody walked away from and forgot
   is taken from whoever wants one next. A file costs one open request. That is
   why app/ui/float.js writes which of the two it is holding beside the name: a
   recording is named after the channel it came off, so the name alone does not
   say it.

   WHY A MODULE AND NOT A COMPONENT. Preact takes a node down the moment it
   changes place in the tree, and a media element taken down loses its stream. A
   source that goes on playing while somebody walks to another screen can
   therefore not be owned by a screen. A screen says where it would like to see
   the element and stops saying so when it goes away.

   THE ELEMENT MOVES AND NEVER RELOADS. Putting it into another parent takes it
   out of the first and puts it into the second in one step, and the standard
   pauses a media element that has left a document only once the task that
   removed it has run to its end. By then this one is somewhere else, so nothing
   pauses. Three calls end a stream, pause, taking the address off and load, and
   all three are in stop() and in no other function here. A source buffer the
   demuxer is filling belongs to the element and not to its place in the page.

   AND IT WAITS SOMEWHERE RATHER THAN NOWHERE. Between two screens there is a
   moment with no slot at all, and an element held outside the document over that
   moment is an element the rule above stops. So it waits in a hidden corner of
   the document instead.

   THREE ROADS, SPLIT THE WAY THE SCREENS SPLIT THEM. A channel the box tunes is
   fetched from the box, demuxed in the browser and played; app/ui/player.js works
   out which of those the box would even offer. A channel with an address of its
   own is fetched through the box, past the tuner, and handed to the playlist
   library by app/ui/webtv.js. A file is fetched off a disk of the box and either
   handed straight over or taken apart first; app/ui/disk.js works out what came
   of that. All three are reached through import() and not from the top of this
   file: everything else here is arithmetic, and a check drives that without a
   browser (test/web/check-web-player.sh). */

import { api } from '../api.js';
import { isAborted } from '../problem.js';
import { channelId } from '../fmt.js';

/**
 * A channel, as much of one as this needs: what a channel list carries, and
 * what a second reading adds once the box has tuned it.
 *
 * @typedef {import('./player.js').Playable & { url?: string }} Channel
 */

/**
 * @typedef {object} ChannelSource
 * @property {'channel'} of
 * @property {string} id what the channel route calls it
 * @property {string} name what a window writes over it
 * @property {boolean} sound sound with no picture, which is an audio element
 * @property {Channel} channel the reading, which grows once the box has tuned it
 */

/**
 * @typedef {object} FileSource
 * @property {'file'} of
 * @property {string} id the address, which is what tells one file from another
 * @property {string} name what a window writes over it
 * @property {boolean} sound sound with no picture, which is an audio element
 * @property {string} href the address the box hands the bytes over at
 * @property {boolean} demuxed whether it is taken apart before the browser sees it
 */

/**
 * One thing to play. Which of the two it is decides the road and nothing else
 * here: the slot, the window and the rules about ending a stream are the same
 * for both.
 *
 * @typedef {ChannelSource | FileSource} Source
 */

/**
 * What is playing, as everything that draws it needs to know.
 *
 * @typedef {object} Playing
 * @property {Source} source
 * @property {string} phase what the element says about itself
 * @property {string} failure which refusal to name, empty while there is none
 * @property {string} said the word the sentence about that refusal needs, in
 *           somebody else's words and never ours: what the browser said about a
 *           channel, the codec the demuxer read out of a file. Empty where there
 *           is none
 * @property {'page' | 'float' | 'none'} slot which of the two slots carries the element
 */

// The demuxer the buildsystem delivers. Published as one script that assigns a
// global rather than as a module, so it is loaded by an element and not by an
// import, which is also what keeps its weight off a page nobody plays anything
// on: it is fetched when somebody presses play and never before.
const kDemuxer = '/vendor/mpegts.js';

/* How long to keep asking what a channel turned out to carry. The box answers
   once it has read the programme map, a second or two after the tuner locks, and
   a channel that has not answered in half a minute is one whose stream never
   arrived either.

   Asked here and not on the screen that pressed play, because the answer matters
   after that screen is gone: a poll that ended with the screen would leave a
   converter running on the box.

   A file needs none of this. Nobody has to tune it to find out what is in it,
   and what it turns out to hold is read off the demuxer while it plays. */
const kWarmWaitMs = 2000;
const kWarmTries = 15;

/* When to ask the element what it is doing. The answer is read off the element
   every time rather than kept beside it, because the browser owns that state
   and a second copy of it goes stale the moment somebody uses the element's
   own controls. */
const kWatched = ['playing', 'waiting', 'canplay', 'pause', 'ended', 'error', 'stalled'];

/** @type {Playing | null} */
let now = null;

/** @type {HTMLMediaElement | null} */
let element = null;

/** @type {any} */
let demux = null;

/** @type {(() => void) | null} */
let letGo = null;

/** @type {Array<(what: Playing | null) => void>} */
const watchers = [];

/** @type {{ page: Element | null, float: Element | null }} */
const slots = { page: null, float: null };

/** @type {HTMLElement | null} */
let corner = null;

/* Which of the slots the element is in. Held here as well as in what is handed
   out, because it outlives the playing: a screen offers its slot and takes it
   back while nothing plays at all.

   IT IS PART OF WHAT IS HANDED OUT AND NOT A QUESTION TO BE ASKED, and that is
   why the window on shell level redraws when a screen takes the element over.
   What is handed out is compared by identity by everything that keeps it, so an
   announcement carrying the same object is one thrown away, and a slot change is
   exactly the case where nothing else about the playing has changed. */
/** @type {'page' | 'float' | 'none'} */
let carried = 'none';

/* Which attempt is the current one. Everything below that comes back from a
   fetch compares against it, because stopping and starting again both happen
   while a module, a demuxer or the first bytes of a stream are still in the
   air. */
let round = 0;

let warmTimer = 0;
let warmLeft = 0;
/** @type {AbortController | null} */
let warming = null;

/**
 * A channel of this box, as something to play.
 *
 * @param {Channel} channel
 * @returns {ChannelSource}
 */
export function ofChannel(channel) {
	return {
		of: 'channel',
		id: channel.id,
		name: channel.name,
		sound: channel.kind === 'radio' || channel.kind === 'webradio',
		channel: channel,
	};
}

/**
 * A file on a disk of this box, as something to play.
 *
 * The road is handed in rather than worked out here. Which road a name takes is
 * arithmetic over the name (app/screens/files/paths.js, fileRoad) and belongs
 * beside the screen that lists the names, which is also the screen that has to
 * decide whether to offer a button at all.
 *
 * @param {string} name the leaf, which is what a window writes
 * @param {string} href the address the box hands the bytes over at
 * @param {{ how: string, sound: boolean }} road which of the three it takes
 * @returns {FileSource}
 */
export function ofFile(name, href, road) {
	return {
		of: 'file',
		id: href,
		name: name,
		sound: road.sound,
		href: href,
		demuxed: road.how === 'demuxed',
	};
}

/**
 * Which of the two slots carries the element.
 *
 * The page wins over the window: the screen that plays the source draws it in
 * its own place, and a window floating over that screen would be the same
 * picture twice.
 *
 * @param {{ page: boolean, float: boolean }} taken
 * @returns {'page' | 'float' | 'none'}
 */
export function slotFor(taken) {
	if (taken.page) {
		return 'page';
	}
	if (taken.float) {
		return 'float';
	}
	return 'none';
}

/**
 * A window held inside the room there is for it.
 *
 * Three rules, in this order: no larger than the room, never smaller than the
 * least it is worth drawing at, and then moved rather than shrunk to get it
 * inside the edges. Moved and not shrunk because a person who dragged a window
 * half off the screen asked for a window in that place, not for a smaller one.
 *
 * @param {{x:number,y:number,w:number,h:number}} want
 * @param {{w:number,h:number}} room
 * @param {{w:number,h:number}} least
 * @returns {{x:number,y:number,w:number,h:number}}
 */
export function clamped(want, room, least) {
	const w = Math.max(least.w, Math.min(want.w, room.w));
	const h = Math.max(least.h, Math.min(want.h, room.h));
	return {
		/* Nought wins where the least size is wider than the room itself,
		   which is a window too big for the screen and is better against the
		   left edge than off it on both sides. */
		x: Math.max(0, Math.min(want.x, room.w - w)),
		y: Math.max(0, Math.min(want.y, room.h - h)),
		w: w,
		h: h,
	};
}

/**
 * A window whose far corner is being pulled, held inside the same room.
 *
 * THE NEAR CORNER DOES NOT MOVE, and that is why this is not clamped(). Pulling
 * the corner at the bottom right is a statement about that corner and no other,
 * so what the room refuses is taken off the size and never off the place: a
 * window at six hundred pulled nine hundred wide in a room of a thousand ends
 * four hundred wide where it stood, and the same wish through clamped() would
 * keep the width and slide the window three quarters of the screen to the left,
 * out from under the hand holding it.
 *
 * The near corner is still held inside the room, because a window dragged
 * against the far edge and then pulled at can otherwise have less room left than
 * the least size needs.
 *
 * @param {{x:number,y:number,w:number,h:number}} want
 * @param {{w:number,h:number}} room
 * @param {{w:number,h:number}} least
 * @returns {{x:number,y:number,w:number,h:number}}
 */
export function sized(want, room, least) {
	const x = Math.max(0, Math.min(want.x, Math.max(0, room.w - least.w)));
	const y = Math.max(0, Math.min(want.y, Math.max(0, room.h - least.h)));
	return {
		x: x,
		y: y,
		w: Math.max(least.w, Math.min(want.w, Math.max(least.w, room.w - x))),
		h: Math.max(least.h, Math.min(want.h, Math.max(least.h, room.h - y))),
	};
}

/**
 * Whether this channel is one the browser is handed an address for.
 *
 * THE SAME QUESTION IS ASKED IN app/screens/channels/list.paging.js, by isWeb,
 * and that is what picks which screen draws the buttons. The two have to answer
 * alike, or a screen would draw one road's controls over the other road's
 * stream.
 *
 * @param {Channel} channel
 * @returns {boolean}
 */
function fetched(channel) {
	return typeof channel.url === 'string' && channel.url !== '';
}

/** @type {Promise<any> | null} */
let loading = null;

/* The demuxer, fetched once for the whole page and only when it is wanted.
   A failed fetch clears the promise, so a person who presses play again after
   a dropped connection gets another attempt rather than the old failure.

   One module for both roads that need it, because a recording is a transport
   stream too. Once for the page and not once per source: it is sixty
   kilobytes. */
/** @returns {Promise<any>} */
export function demuxer() {
	const already = /** @type {any} */ (window).mpegts;
	if (already) {
		return Promise.resolve(already);
	}
	if (!loading) {
		loading = new Promise(function (resolve, reject) {
			const el = document.createElement('script');
			el.src = kDemuxer;
			el.addEventListener('load', function () {
				const got = /** @type {any} */ (window).mpegts;
				if (got) {
					resolve(got);
				} else {
					loading = null;
					reject(new Error(kDemuxer));
				}
			});
			el.addEventListener('error', function () {
				loading = null;
				reject(new Error(kDemuxer));
			});
			document.head.appendChild(el);
		});
	}
	return loading;
}

/** @returns {void} */
function tell() {
	// Over a copy, because a watcher is free to unsubscribe from inside itself.
	for (const watcher of watchers.slice()) {
		watcher(now);
	}
}

/**
 * @param {string} phase
 * @param {string} failure
 * @param {string} said
 * @returns {void}
 */
function settle(phase, failure, said) {
	if (!now) {
		return;
	}
	now = { source: now.source, phase: phase, failure: failure, said: said, slot: carried };
	tell();
}

/**
 * A refusal, with the phase left as the element last stated it: what went
 * wrong and what the element is doing are two answers and not one.
 *
 * @param {string} failure
 * @returns {void}
 */
function blame(failure) {
	if (!now) {
		return;
	}
	settle(now.phase, failure, '');
}

/**
 * A source that turned out to be something nothing here plays: said, and then
 * stopped.
 *
 * Said before it is stopped, because stopping says there is nothing playing and
 * a screen that was only told that has nothing left to explain the refusal with.
 * Stopped at all, because the alternative is a request the box goes on answering
 * for a picture that will never appear.
 *
 * @param {string} failure
 * @param {string} said
 * @returns {void}
 */
function refuse(failure, said) {
	settle('failed', failure, said);
	stop();
}

/**
 * Where the element waits while no slot asks for it. In the document and out
 * of sight, for the reason at the head of this file.
 *
 * @returns {HTMLElement}
 */
function waitingRoom() {
	if (!corner) {
		const box = document.createElement('div');
		box.hidden = true;
		document.body.appendChild(box);
		corner = box;
	}
	return corner;
}

/**
 * The one place that moves the element, and the reason the stream survives a
 * change of screen.
 *
 * @returns {void}
 */
function place() {
	const which = slotFor({ page: slots.page !== null, float: slots.float !== null });
	const changed = which !== carried;
	carried = which;
	if (element) {
		into(which);
	}
	if (!changed) {
		return;
	}
	/* Built afresh rather than written into, because what was handed out last
	   time is what a watcher is holding and comparing against. The same object
	   back with one member changed is a change nobody sees. */
	if (now) {
		now = { source: now.source, phase: now.phase, failure: now.failure, said: now.said, slot: which };
	}
	tell();
}

/**
 * @param {'page' | 'float' | 'none'} which
 * @returns {void}
 */
function into(which) {
	const el = element;
	if (!el) {
		return;
	}
	const host = which === 'none' ? waitingRoom() : slots[which];
	if (!host || el.parentNode === host) {
		return;
	}
	/* One call, and the element is out of the old parent and into the new one
	   before the browser looks again. Nothing here pauses it, takes its address
	   away or calls load, which is what would end the stream; the three that do
	   are in stop().

	   MEASURED, NOT ASSUMED, on Chromium 152: an element moved between two
	   parents this way goes on playing, its time goes on running, and it raises
	   neither loadstart nor emptied, so nothing was fetched twice; an attached
	   MediaSource stays open and keeps the same source buffer. The same
	   measurement is why the waiting room above exists, because the same element
	   taken out of the document reported itself paused a moment later. */
	host.appendChild(el);
}

/**
 * Which slot a screen or the window is offering, or none from it any more.
 *
 * @param {'page' | 'float'} which
 * @param {Element | null} node
 * @returns {void}
 */
export function setSlot(which, node) {
	slots[which] = node;
	place();
}

/**
 * What is playing, or nothing.
 *
 * @returns {Playing | null}
 */
export function current() {
	return now;
}

/**
 * Told whenever that changes, and whenever the element changes hands between the
 * two slots. The second is here because what the window on shell level draws
 * depends on whether a screen has offered a slot of its own, and a screen offers
 * one and takes it back without anything playing differently.
 *
 * Which slot it is, is in what arrives, so a watcher answers that question out
 * of what it was told and never by asking again while it draws.
 *
 * @param {(what: Playing | null) => void} fn
 * @returns {() => void} what ends the subscription
 */
export function subscribe(fn) {
	watchers.push(fn);
	return function () {
		const at = watchers.indexOf(fn);
		if (at >= 0) {
			watchers.splice(at, 1);
		}
	};
}

/** @returns {void} */
function stopWarming() {
	if (warming) {
		warming.abort();
		warming = null;
	}
	if (warmTimer) {
		window.clearTimeout(warmTimer);
		warmTimer = 0;
	}
}

/**
 * Everything this holds, let go of: the demuxer, the request the box still has
 * open, and the element itself.
 *
 * @returns {void}
 */
export function stop() {
	round += 1;
	stopWarming();

	const held = demux;
	demux = null;
	if (held) {
		try {
			held.pause();
			held.unload();
			held.detachMediaElement();
			held.destroy();
		} catch (e) {
			/* A demuxer that is already half gone throws on the rest of the
			   way down, and there is nothing left to do about it. */
		}
	}

	const away = letGo;
	letGo = null;
	if (away) {
		away();
	}

	const el = element;
	element = null;
	if (el) {
		/* The three that end a stream, in the one place that ends one. Taking
		   the address off and loading again is what ends the request the box
		   still has open; left alone, the box goes on fetching, converting or
		   reading a disk for a stream nobody is watching until it notices the
		   socket has gone. */
		el.pause();
		el.removeAttribute('src');
		el.load();
		if (el.parentNode) {
			el.parentNode.removeChild(el);
		}
	}

	if (now) {
		now = null;
		tell();
	}
}

/**
 * Asked again while the box is being made to tune the channel, and only then.
 * The answer arrives some seconds after the stream does, because the box reads
 * the programme map after it has the transponder, so this asks a few times
 * rather than once and gives up rather than asking forever.
 *
 * @param {number} mine
 * @param {typeof import('./player.js')} player
 * @param {string} id
 * @returns {void}
 */
function askAgain(mine, player, id) {
	if (mine !== round) {
		return;
	}
	warmTimer = 0;
	const control = new AbortController();
	warming = control;

	/** @returns {void} */
	function later() {
		if (--warmLeft > 0) {
			warmTimer = window.setTimeout(function () { askAgain(mine, player, id); }, kWarmWaitMs);
		}
	}

	api('GET', '/api/v1/channels/{id}', {
		params: { id: id },
		signal: control.signal,
	}).then(function (one) {
		if (mine !== round) {
			return;
		}
		warming = null;
		/* The channel route carries what this needs and the generated shapes
		   do not yet name it, so it is read through the shape player.js
		   states. */
		const read = /** @type {Channel} */ (/** @type {unknown} */ (one));
		if (read && read.streams_known === true) {
			learned(player, read);
			return;
		}
		later();
	}, function (error) {
		if (mine !== round || isAborted(error)) {
			return;
		}
		warming = null;
		later();
	});
}

/**
 * What the box turned out to be doing, once it has read the channel.
 *
 * @param {typeof import('./player.js')} player
 * @param {Channel} read
 * @returns {void}
 */
function learned(player, read) {
	if (!now) {
		return;
	}
	now = { source: ofChannel(read), phase: now.phase, failure: now.failure, said: now.said, slot: carried };
	tell();
	/* Learned too late to have been said beforehand, which is the whole shape
	   of a cold channel: it was opened, the box read it, and it turns out
	   nothing here can play it. So it is stopped at this point and whoever is
	   watching has the reading to explain it with, which greying the channel
	   out beforehand could not have done honestly. */
	if (player.decide(read).path === 'not-offered') {
		stop();
	}
}

/**
 * The element, and the road that fills it.
 *
 * @param {number} mine
 * @param {Roads} road
 * @param {Source} source
 * @returns {void}
 */
function open(mine, road, source) {
	const player = road.player;
	const decision = source.of === 'channel' && !road.webtv ? player.decide(source.channel) : null;

	if (decision && decision.src === '') {
		/* A channel nothing here can play is explained by the screen and never
		   started, so reaching this means somebody asked for one regardless. */
		stop();
		return;
	}

	const el = /** @type {HTMLMediaElement} */ (document.createElement(source.sound ? 'audio' : 'video'));
	el.className = 'np-media';
	el.controls = true;
	el.preload = 'none';
	if (!source.sound) {
		/** @type {HTMLVideoElement} */ (el).playsInline = true;
	}
	el.setAttribute('aria-label', source.name);
	element = el;
	place();

	/** @returns {void} */
	function follow() {
		const held = now;
		if (element !== el || !held) {
			return;
		}
		if (road.webtv) {
			/* The element's own verdict, which separates a source that could
			   not be fetched from one that answered with something this
			   browser will not play. Those two send somebody looking in
			   completely different places. */
			const fault = player.faultOf(el);
			settle(player.phaseOf(el), fault ? fault.why : '', fault ? fault.said : '');
			return;
		}
		if (source.of === 'file') {
			const fault = player.faultOf(el);
			/* Ours: stop() takes the address off, and the element reports
			   that as a fetch somebody stopped. */
			if (fault && fault.why !== 'aborted') {
				refuse(fault.why === 'network' ? 'stream' : 'format', '');
				return;
			}
		}
		settle(player.phaseOf(el), held.failure, held.said);
	}

	for (const name of kWatched) {
		el.addEventListener(name, follow);
	}

	/** @returns {void} */
	function press() {
		const played = el.play();
		if (played && typeof played.catch === 'function') {
			// A refusal to start is carried by the element, and what is said
			// about it is read off the element.
			played.catch(follow);
		}
	}

	if (source.of === 'file') {
		fromDisk(mine, el, /** @type {typeof import('./disk.js')} */ (road.disk), source, press);
		return;
	}

	if (road.webtv) {
		/* Asked to play when it has something to play and not when the button
		   was pressed: with the library in the way the source arrives on the
		   element some time after this, and a call made now lands on an
		   element with nothing on it. */
		el.addEventListener('canplay', press);
		letGo = road.webtv.attachWebTv(el, channelId(source.channel.id), function (said) {
			/* Two of the three the block reports are already legible off the
			   element and are left to it. The third is the one the element
			   cannot carry: neither address played, and with the library in
			   the way there is no MediaError to read that off. */
			if (element === el && said === 'unplayable') {
				settle('failed', 'format', '');
			}
		});
		return;
	}

	if (!decision) {
		return;
	}

	if (decision.radio) {
		/* Sound alone arrives as ADTS, which every browser plays without
		   anything in front of it. No demuxer is fetched at all. */
		el.src = decision.src;
		press();
	} else {
		through(mine, el, decision, press);
	}

	if (decision.path === 'blind') {
		warmLeft = kWarmTries;
		warmTimer = window.setTimeout(function () { askAgain(mine, player, source.id); }, kWarmWaitMs);
	}
}

/**
 * A picture, through the demuxer, because no browser opens a transport stream on
 * its own.
 *
 * @param {number} mine
 * @param {HTMLMediaElement} el
 * @param {import('./player.js').Decision} decision
 * @param {() => void} press
 * @returns {void}
 */
function through(mine, el, decision, press) {
	demuxer().then(function (mpegts) {
		if (mine !== round || element !== el) {
			return;
		}
		if (!mpegts.isSupported()) {
			blame('browser');
			return;
		}
		const made = mpegts.createPlayer({
			type: 'mpegts',
			isLive: true,
			url: decision.src,
		}, {
			// A live stream has no end to seek to and no length to state, so
			// the parts of the demuxer that look for either are off.
			enableStashBuffer: false,
			liveBufferLatencyChasing: true,
		});
		/* The demuxer fetches the stream itself, so what the box said about the
		   request reaches this page here and nowhere else: an element only ever
		   reports that something went wrong. The two answers worth repeating
		   are the box being full and the box having nothing to convert with,
		   because a person does something different about each. */
		made.on(mpegts.Events.ERROR, function (/** @type {any} */ _type,
		                                       /** @type {any} */ _detail,
		                                       /** @type {any} */ info) {
			const code = info && info.code;
			blame(code === 503 ? 'busy' : code === 501 ? 'converter' : 'stream');
		});
		made.attachMediaElement(el);
		made.load();
		press();
		demux = made;
	}, function () {
		if (mine === round && element === el) {
			blame('load');
		}
	});
}

/**
 * A file off a disk of the box, handed straight over or taken apart first.
 *
 * NOT DECIDE AND THEN PLAY, WHICH IS WHAT A CHANNEL DOES. For a channel the box
 * says what it carries. For a file nobody knows: a name ending in .ts says the
 * container and nothing about the picture inside it, and the box does not read a
 * recording to find out. So this is try and then be honest, and what the demuxer
 * turns out to be handing over is compared with what this browser will decode
 * the moment it says so.
 *
 * @param {number} mine
 * @param {typeof import('./disk.js')} disk
 * @param {HTMLMediaElement} el
 * @param {FileSource} source
 * @param {() => void} press
 * @returns {void}
 */
function fromDisk(mine, el, disk, source, press) {
	/* A JUMP IS HELD INSIDE WHAT HAS ARRIVED. A recording is a bare transport
	   stream with no index in it, so there is nothing to look a position up in:
	   a jump past what has arrived empties the browser's buffer, fetches nothing
	   and leaves a player that says it is running and never moves again.
	   Measured on Chromium 152.

	   Listened for before the demuxer attaches its own listener, so the place is
	   put right before the demuxer reads it and throws the buffer away. */
	el.addEventListener('seeking', function () {
		if (element !== el) {
			return;
		}
		const to = disk.landing(el.currentTime, disk.arrivedIn(el));
		if (to !== el.currentTime) {
			el.currentTime = to;
		}
	});

	if (!source.demuxed) {
		el.src = source.href;
		press();
		return;
	}

	/* NO PICTURE CAME THROUGH. The demuxer carries over the streams it knows and
	   hands the rest to nobody, so an element that has read the head of the
	   stream and has no width is a recording whose picture it could not carry:
	   MPEG-2, or none in the file at all. What the element is handed is
	   fragmented MP4, and the first piece of that carries the size of the
	   picture, so there is no width still to come.

	   ASKED AT loadedmetadata AND NOT ONE EVENT LATER, measured: sound with no
	   picture reaches this point after a moment and reaches canplay only when
	   the whole file has arrived, which on a recording of gigabytes is a refusal
	   that comes after the download it was meant to prevent. */
	el.addEventListener('loadedmetadata', function () {
		if (element === el && /** @type {HTMLVideoElement} */ (el).videoWidth === 0) {
			refuse('picture', '');
		}
	});

	demuxer().then(function (mpegts) {
		if (mine !== round || element !== el) {
			return;
		}
		if (!mpegts.isSupported()) {
			refuse('browser', '');
			return;
		}
		/* Not a live stream, which is what buys the two things a recording
		   needs: the loader stops once it is far enough ahead and picks up again
		   with a byte range, so a recording of five gigabytes is never fetched
		   whole, and what has arrived can be jumped about in. The box answers
		   ranges (src/httpd/server.cpp). */
		const made = mpegts.createPlayer({
			type: 'mpegts',
			isLive: false,
			url: source.href,
		});
		made.on(mpegts.Events.MEDIA_INFO, function (/** @type {any} */ info) {
			const verdict = disk.verdictOf(info, disk.browserAsks);
			if (verdict.why !== '') {
				refuse(verdict.why, verdict.codec);
			}
		});
		made.on(mpegts.Events.ERROR, function (/** @type {any} */ _type,
		                                       /** @type {any} */ _detail,
		                                       /** @type {any} */ _info) {
			/* A turn of the loop later, and this is the measurement that says
			   why: a picture the browser will not decode is reported here first
			   and named by the reading above a moment afterwards, so refusing at
			   once would say the stream broke off about a file whose codec is
			   known one line later. Whichever of the two gets there first is the
			   one that speaks, and this is the vaguer of them. */
			window.setTimeout(function () {
				if (mine === round) {
					refuse('stream', '');
				}
			}, 0);
		});
		made.attachMediaElement(el);
		made.load();
		press();
		demux = made;
	}, function () {
		if (mine === round && element === el) {
			refuse('load', '');
		}
	});
}

/**
 * @typedef {object} Roads
 * @property {typeof import('./player.js')} player
 * @property {typeof import('./webtv.js') | null} webtv null unless the channel has an address of its own
 * @property {typeof import('./disk.js') | null} disk null unless the source is a file
 */

/**
 * The road this source takes, fetched.
 *
 * player.js is on all three of them: what the element says about itself, and
 * what it says went wrong, are the same two questions whatever filled it.
 *
 * @param {Source} source
 * @returns {Promise<Roads>}
 */
function roads(source) {
	if (source.of === 'file') {
		return Promise.all([import('./player.js'), import('./disk.js')]).then(function (both) {
			return { player: both[0], webtv: null, disk: both[1] };
		});
	}
	if (!fetched(source.channel)) {
		return import('./player.js').then(function (player) {
			return { player: player, webtv: null, disk: null };
		});
	}
	return Promise.all([import('./player.js'), import('./webtv.js')]).then(function (both) {
		return { player: both[0], webtv: both[1], disk: null };
	});
}

/**
 * One source, and only one.
 *
 * WHATEVER WAS PLAYING STOPS FIRST, and that is the whole of the rule rather
 * than half of it. A box asked for two channels at once is a box with two tuners
 * fewer; two soundtracks at once is nobody's idea of either of them; and a
 * screen that remembered to stop the other kind and a screen that forgot would
 * differ only in a line nothing fails over. There is one of this module, so
 * there is one source.
 *
 * @param {Source} source
 * @returns {void}
 */
export function start(source) {
	stop();
	const mine = ++round;
	/* Said before anything is fetched, so that whoever pressed the button sees
	   the source it belongs to and not the one that was there. */
	now = { source: source, phase: 'loading', failure: '', said: '', slot: carried };
	tell();

	roads(source).then(function (road) {
		if (mine !== round) {
			return;
		}
		open(mine, road, source);
	}, function () {
		if (mine === round) {
			settle('failed', 'load', '');
		}
	});
}
