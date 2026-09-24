/* One stream for the whole page.

   The box hands out at most eight of them, and a second tab is already the second, so a
   page that opened one per screen would be out of them with three tabs open.

   What an event does here is invalidate, and nothing else. It carries a type, a channel
   and a small value, which says that something under a piece of the address has changed
   and never what it now is. So the table below maps a type to the prefixes that stopped
   being true, the store throws those away, and whatever is on the screen asks again.

   The heartbeat is a comment frame and never reaches this file. That is what it is for: it
   keeps whatever is in between from closing an idle connection. */

import * as store from './store.js';
import * as session from './session.js';

const kStreamPath = '/api/v1/events';

/* Every type the box names (src/httpd/events.cpp, typeName), and under each the addresses
   it makes stale. Named one by one and not derived: the tie between "the box zapped" and
   "what /api/v1/epg/current answers" is knowledge about this API, and the place to write it
   down is one table rather than twenty five screens. A type the box adds without a line
   here invalidates nothing, which shows itself as a screen that will not refresh. */
/** @type {Record<string, readonly string[]>} */
const kStale = {
	'zap': ['/api/v1/channels/current', '/api/v1/epg/current', '/api/v1/tuner'],
	'mode': ['/api/v1/channels', '/api/v1/bouquets', '/api/v1/epg'],
	// The route about standby is among these because the box now says this
	// once the mode has moved, and no longer when the command arrived.
	'standby': ['/api/v1/system/standby', '/api/v1/system/info', '/api/v1/tuner'],
	'volume': ['/api/v1/osd/volume'],
	'mute': ['/api/v1/osd/volume'],
	'record-start': ['/api/v1/recordings', '/api/v1/timers', '/api/v1/tuner'],
	'record-stop': ['/api/v1/recordings', '/api/v1/timers', '/api/v1/tuner'],
	'timer-changed': ['/api/v1/timers'],
	'epg-updated': ['/api/v1/epg'],
	'bouquets-changed': ['/api/v1/bouquets', '/api/v1/channels'],
	'settings-changed': ['/api/v1/settings/'],
};

export const types = Object.keys(kStale);

/**
 * One event off the stream, in the shape a listener is handed. Every member is
 * always there, which is what spares twenty five screens the same four tests.
 *
 * @typedef {object} StreamEvent
 * @property {string} type
 * @property {string} channel_id
 * @property {number} value
 * @property {string} text
 */

/**
 * What the page says about the box being there.
 *
 * @typedef {object} StreamStatus
 * @property {boolean} reachable whether the box answered at all, which a
 *           refusal counts as: a box that says no is a box that is there
 * @property {boolean} carrying whether a stream is open right now, which is a
 *           different question from the one above and the only one that
 *           answers "is this page being told anything"
 * @property {boolean} denied whether the refusal was a refusal and not an absence
 * @property {number} failures
 * @property {number} retryMs the last gap, measured, and nought before one was
 */

/** @typedef {(event: StreamEvent, type: string) => void} StreamListener */

/** @type {EventSource | null} */
let source = null;
/* Set while the document this page is in has been put aside, and the one thing that keeps
   the stream shut for as long as it has.

   A browser keeps the documents somebody has left, and a document it keeps keeps its
   connections: a browser gives a host six, a stream holds one of them for as long as it is
   open, and a page left without closing it leaves that connection standing inside the
   document the browser put away.

   Measured. A walk of nine documents: each of the first six arrived in about forty
   milliseconds, six connections stood open against the box at the end of them, and the
   seventh navigation never started at all.

   Not on visibilitychange, which is a different thing said in a similar way: a page in a
   tab nobody is looking at is still a page somebody left a recording running on. */
let gone = false;
/** @type {Array<{ type: string, listener: StreamListener }>} */
let listeners = [];
/** @type {Array<(status: StreamStatus) => void>} */
let watchers = [];
let failures = 0;
let failedAt = 0;

/* What the page says about the box being there.

   reachable is what the bar is drawn from, and it is not simply "the stream is closed": a
   stream that drops once and comes back inside its retry is normal on a box doing something
   heavy, and a bar that flashed for it would train everybody to ignore it.

   retryMs is measured and not read off the stream. The server names the wait itself, the
   browser obeys it, and no browser exposes the number to a script. Nought means it has not
   been observed yet. */
const status = {
	reachable: true,
	carrying: false,
	denied: false,
	failures: 0,
	retryMs: 0,
};

/** @returns {StreamStatus} */
export function streamStatus() {
	return {
		reachable: status.reachable,
		carrying: status.carrying,
		denied: status.denied,
		failures: status.failures,
		retryMs: status.retryMs,
	};
}

/**
 * @param {(status: StreamStatus) => void} watcher
 * @returns {() => void} stops the watching
 */
export function watchStatus(watcher) {
	watchers.push(watcher);
	watcher(streamStatus());
	return function () {
		const at = watchers.indexOf(watcher);
		if (at !== -1) {
			watchers.splice(at, 1);
		}
	};
}

/** @returns {void} */
function announceStatus() {
	const now = streamStatus();
	for (const watcher of watchers.slice()) {
		watcher(now);
	}
}

/* Every event, whatever its type, for the one place that watches the box as a whole rather
   than one thing about it. This is the shell's seam; a screen wants one type and uses on()
   below. */
/**
 * @param {StreamListener} listener
 * @returns {() => void} stops the listening
 */
export function subscribe(listener) {
	return on('*', listener);
}

/* For the few places that want the event itself rather than the reload it causes: the bar
   at the top follows a zap without asking, and the volume slider follows a change made on
   the box itself. Everything else should be watching the store. */
/**
 * @param {string} type one of types, or '*' for every one of them
 * @param {StreamListener} listener
 * @returns {() => void} stops the listening
 */
export function on(type, listener) {
	const entry = { type: type, listener: listener };
	listeners.push(entry);
	return function () {
		const at = listeners.indexOf(entry);
		if (at !== -1) {
			listeners.splice(at, 1);
		}
	};
}

/**
 * @param {string} type
 * @param {StreamEvent} event
 * @returns {void}
 */
function deliver(type, event) {
	for (const entry of listeners) {
		if (entry.type === type || entry.type === '*') {
			entry.listener(event, type);
		}
	}
}

/**
 * @param {string} type
 * @param {MessageEvent<string>} message
 * @returns {void}
 */
function received(type, message) {
	/** @type {Record<string, unknown> | null} */
	let data = null;
	try {
		// Bytes off the network, and read member by member below rather than
		// taken whole.
		data = /** @type {Record<string, unknown> | null} */ (JSON.parse(message.data));
	} catch (e) {
		// An event whose body cannot be read still says its type, and the type
		// is what invalidates. Dropping the whole event over its body would
		// leave the screen stale for the sake of a field nobody may want.
		data = null;
	}
	/** @type {StreamEvent} */
	const event = {
		type: type,
		channel_id: (data && typeof data['channel_id'] === 'string') ? data['channel_id'] : '',
		value: (data && typeof data['value'] === 'number' && Number.isFinite(data['value'])) ? data['value'] : 0,
		text: (data && typeof data['text'] === 'string') ? data['text'] : '',
	};
	for (const prefix of kStale[type] || []) {
		store.invalidate(prefix);
	}
	deliver(type, event);
}

/** @returns {void} */
function opened() {
	const wasDown = !status.reachable;
	if (failedAt !== 0) {
		status.retryMs = Date.now() - failedAt;
		failedAt = 0;
	}
	failures = 0;
	status.failures = 0;
	status.reachable = true;
	status.carrying = true;
	status.denied = false;
	// The box had one to give after all, so the next time it has none the wait
	// starts over from short rather than from wherever this round ended up.
	retryMs = 0;
	announceStatus();
	/* Everything is forgotten and what is visible is asked again. Whatever happened while
	   the stream was down happened, and there is no way to find out what: the stream carries
	   no identifier, so there is nothing to resume from. Keeping what was in the store would
	   be keeping a picture of the box from before the gap, which is worse than an empty one
	   because it looks current. Only on a stream that comes back, not on the first one. */
	if (wasDown) {
		store.clear();
	}
}

/* How long to wait before opening the stream again after the box closed it on a caller who
   is allowed to read.

   Not the browser's own retry: the browser only does that for a connection it is still
   holding, and this is the case where it gave up for good. Growing, because the reason is
   that the box has no stream left to give. The first wait is longer than it looks like it
   needs to be because the box lets a finished stream go on a heartbeat and not when the
   connection drops. */
const kFirstRetryMs = 5000;
const kLastRetryMs = 60000;
let retryMs = 0;
let retryTimer = 0;

/** @returns {void} */
function retryLater() {
	if (gone || retryTimer !== 0) {
		return;
	}
	retryMs = retryMs === 0 ? kFirstRetryMs : Math.min(retryMs * 2, kLastRetryMs);
	retryTimer = setTimeout(function () {
		retryTimer = 0;
		open();
	}, retryMs);
}

/* Cancels a wait, and deliberately does not forget how long it had grown to. The failure
   path stops the stream before it asks for another wait, so a reset here would hand out the
   first wait every time. Where it does start over is a stream that opened. */
/** @returns {void} */
function stopRetrying() {
	if (retryTimer !== 0) {
		clearTimeout(retryTimer);
		retryTimer = 0;
	}
}

/** @returns {void} */
function failed() {
	/* Two states arrive here as one. CONNECTING means the browser is going to try again by
	   itself, after the wait the server named. CLOSED means it is not.

	   Which answer it was cannot be read here: an EventSource hands its error over with no
	   status on it, and the box sends more than one answer this way, the refusal to a caller
	   that may not read and the refusal to one that arrived when every stream was taken.
	   Reading both as the first drew a page that said it was not allowed to read, on a
	   session that was, and then never asked again. */
	const closed = source === null || source.readyState === 2;
	status.carrying = false;
	if (failedAt === 0) {
		failedAt = Date.now();
	}
	failures++;
	status.failures = failures;
	if (closed) {
		status.reachable = true;
		stop();
		if (session.state().checked && !session.atLeast('read')) {
			status.denied = true;
		} else {
			retryLater();
		}
	} else if (failures > 1) {
		status.reachable = false;
	}
	announceStatus();
}

/* Opens the one stream, if it is not open and if this caller may read at all. Asked of the
   session rather than tried and retried: a caller below Read is refused, and a refusal is
   not a stream, so the browser closes it for good and the page would sit there with a bar
   saying the box is away while the box is answering everything else. */
/** @returns {void} */
export function start() {
	tieToDocument();
	tieToSession();
	open();
}

/** @returns {void} */
function open() {
	/* First, and not after the test below. Between the document being told it is going and the
	   document actually going, the session can still answer, and the one thing that answer does
	   is open a stream. A document on its way out that opens a connection leaves it behind with
	   no one left to close it. */
	if (gone) {
		return;
	}
	if (source !== null) {
		return;
	}
	if (session.state().checked && !session.atLeast('read')) {
		status.denied = true;
		announceStatus();
		return;
	}
	const opening = new EventSource(kStreamPath);
	source = opening;
	opening.onopen = opened;
	opening.onerror = failed;
	for (const type of types) {
		opening.addEventListener(type, function (message) {
			received(type, /** @type {MessageEvent<string>} */ (message));
		});
	}
}

/** @returns {void} */
export function stop() {
	/* Before the test below, because a stream waiting to be opened again is a
	   stream to stop just as much as an open one, and at that moment there is
	   no source to find. */
	stopRetrying();
	status.carrying = false;
	if (source === null) {
		return;
	}
	source.onopen = null;
	source.onerror = null;
	source.close();
	source = null;
}

/* The document is being left: a navigation away from it, a tab being closed, or the browser
   putting it away whole. One event for all three, and pagehide rather than unload or
   beforeunload on purpose: a listener on either of those is itself a reason a browser
   refuses to keep a document. */
/** @returns {void} */
function documentGone() {
	gone = true;
	stop();
}

/* The same document, shown again, which is the browser handing back the one it kept whole:
   every timer, every variable and everything this store holds are as they were. persisted
   is the browser saying which of the two kinds of pageshow this is. */
/**
 * @param {boolean} persisted whether the browser kept this document whole
 * @returns {void}
 */
function documentShown(persisted) {
	if (!gone) {
		return;
	}
	gone = false;
	open();
	/* Everything is forgotten and what is visible is asked again, for the reason opened() gives
	   at length: whatever the box did while this document was away cannot be asked for
	   afterwards. opened() cannot do it here, because nothing failed and by its reckoning the
	   box was never away. */
	if (persisted) {
		store.clear();
	}
}

/* The stream and the session are tied together in one direction: signing in can only grant
   more than was granted before, so a stream that was refused is worth opening again the
   moment somebody signs in. Signing out does not close it, a box on its own network going
   on answering a caller holding nothing.

   Tied from inside start and not at the top of this file, so importing this module does
   nothing at all. */
let tied = false;

/* Tied from inside start for the reason the session is, and once: a second
   pair of listeners would close a stream that was already closed and ask the
   box for everything twice. */
let watched = false;

/** @returns {void} */
function tieToDocument() {
	if (watched) {
		return;
	}
	watched = true;
	window.addEventListener('pagehide', documentGone);
	window.addEventListener('pageshow', function (event) {
		documentShown(!!(/** @type {PageTransitionEvent} */ (event)).persisted);
	});
}

/** @returns {void} */
function tieToSession() {
	if (tied) {
		return;
	}
	tied = true;
	session.subscribe(function (now) {
		if (source === null && now.checked && session.atLeast('read')) {
			status.denied = false;
			open();
		}
	});
}
