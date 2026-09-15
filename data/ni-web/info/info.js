/* THE DISPLAY AT /info. Not a screen of the application and not reached from
   it: no navigation, no bars, nothing to press. It is meant to be pinned to the
   home screen of a telephone and left standing somewhere in the room, which is
   what the manifest beside it is for.

   It replaces the page the old web interface had at the same address. That one
   read seventeen files under /tmp/lcd through shell calls in a template
   language and fetched itself whole every fifteen seconds. This one asks the
   box what it is doing and then listens: the stream says when the channel
   changed, when a recording began and when the box went to standby, so between
   two of those it costs one round of asking a minute and nothing else.

   IT BORROWS THE APPLICATION'S LAYERS AND NOT ITS LOOK. The one file that
   speaks to the box, the one stream, the words and the formatting are the
   application's, so an address written here is held to the box's own document
   by the same check as every address written there. The colours are its own,
   for the reasons the stylesheet beside this gives.

   NOTHING HERE SIGNS ANYBODY IN. A box grants reading to a caller on its own
   network with no session at all, which is what makes this page work from a
   telephone; a caller from anywhere else is refused, and what this draws then
   is a sentence saying so. There is no sheet to raise on this page, so every
   call turns recovery off rather than leaving a promise waiting on one. */

import { api } from '../app/api.js';
import * as events from '../app/events.js';
import { isOffline } from '../app/problem.js';
import { setLanguage, pickLanguage, t } from '../app/i18n.js';
import { clock, duration, isTimeshift } from '../app/fmt.js';
import text from './info.text.js';

/* The clock, the share of the programme that has gone and the time left are
   worked out here rather than asked for, so they move between two rounds of
   asking. A second, because a clock that turns a minute late on a display
   somebody is looking at is a clock that is wrong. */
const kTickMs = 1000;

/* And the whole state, on its own, for what nothing on the stream announces:
   one programme ending and the next beginning. */
const kAgainMs = 60000;

/* What an event off the stream is answered with. Not at once: a channel change
   arrives as several events a fraction of a second apart, and a round of asking
   per event would be six requests times the number of them. */
const kAfterEventMs = 400;

/* And what the end of a programme is answered with. Longer, because the box
   writes the next entry of the guide a moment after the one before it has run
   out, so a page that asked on the second would be answered with the programme
   that just ended. */
const kAfterEventEndMs = 15000;

/**
 * @param {string} id
 * @returns {HTMLElement}
 */
function node(id) {
	const found = document.getElementById(id);
	if (!found) {
		throw new Error('info: the document carries no element with the identifier ' + id);
	}
	return found;
}

const shown = {
	ecm: node('info-ecm'),
	ci: node('info-ci'),
	timer: node('info-timer'),
	rec: node('info-rec'),
	ts: node('info-ts'),
	logo: /** @type {HTMLImageElement} */ (node('info-logo')),
	name: node('info-name'),
	said: node('info-said'),
	event: node('info-event'),
	bar: node('info-bar'),
	fill: node('info-fill'),
	clock: node('info-clock'),
	left: node('info-left'),
};

/**
 * What the box last said, as this page needs it. Every member is always there,
 * so drawing never has to ask whether a round has happened yet.
 *
 * @typedef {object} Seen
 * @property {boolean} asked whether a round has come back at all
 * @property {boolean} denied whether the box refused this caller
 * @property {boolean} away whether the box did not answer
 * @property {boolean} standby
 * @property {Api.Result<'GET /api/v1/channels/current'> | null} channel
 * @property {Api.Result<'GET /api/v1/epg/current'> | null} event
 * @property {boolean} recording whether the box is writing a recording
 * @property {boolean} shifting whether the box is keeping a shift of what it shows
 * @property {boolean} timer whether a timer is still to fire
 * @property {boolean} softcam whether a softcam is handing over a control word
 * @property {boolean} ci whether a module in a slot is being used
 */

/** @type {Seen} */
let seen = {
	asked: false, denied: false, away: false, standby: false,
	channel: null, event: null, recording: false, shifting: false, timer: false,
	softcam: false, ci: false,
};

/* The picture that answered, by the address it answered for. Held against the
   address rather than as a flag, because a 404 already in the browser's cache
   fires before anything here has run and no second one ever comes for an
   address that has not changed. */
let drawn = '';

/* One answer, with the refusal kept rather than thrown. Several calls go out
   together and one of them failing is an ordinary state of this page: a box
   with no guide for the running channel refuses the guide and plays the
   channel all the same. */
/**
 * @template T
 * @param {Promise<T>} call
 * @returns {Promise<{ value: T | null, status: number, offline: boolean }>}
 */
function settled(call) {
	return call.then(function (value) {
		return { value: value, status: 200, offline: false };
	}, function (/** @type {unknown} */ failed) {
		const why = /** @type {{ status?: unknown } | null} */ (failed);
		return {
			value: null,
			status: (why && typeof why.status === 'number') ? why.status : 0,
			offline: isOffline(failed),
		};
	});
}

/** @returns {number} seconds since the epoch, the way the box counts */
function seconds() {
	return Math.floor(Date.now() / 1000);
}

/* What the box calls itself in, read the way the application reads it, with the
   browser behind it. A box that has never been given a language answers with an
   empty name, and then the telephone decides. */
/** @returns {Promise<void>} */
function readLanguage() {
	return api('GET', '/api/v1/settings/{section}',
		{ params: { section: 'general' }, recover: false }
	).then(function (answer) {
		for (const row of (answer && answer.items) || []) {
			if (row.id === 'language')
				return String(row.value);
		}
		return '';
	}, function () {
		// A box that will not say is a box the browser answers for.
		return '';
	}).then(function (named) {
		setLanguage(pickLanguage(named, navigator.languages || [navigator.language]));
		paint();
	});
}

/* WHAT THE TWO MARKS ON THE LEFT ARE READ OUT OF, and the one answer on this
   page whose shape nothing holds this file to.

   The route is written in another branch of this round and the contract is
   settled: { "softcam": boolean, "ci_module": boolean }. Until it lands, a box
   answers this address with a refusal, and a refusal here is not a fault: both
   marks grey is exactly what a box descrambling nothing looks like, and it is
   also what a caller the box does not grant reading to sees.

   So each member is tested for being true rather than read off a shape. That is
   what a page may do with an answer the document it is held to does not yet
   describe, and it is also the reading that is right for a box older than the
   route. */
/** @returns {Promise<{ softcam: boolean, ci: boolean }>} */
function readDecryption() {
	return api('GET', '/api/v1/system/decryption', { recover: false }).then(
		function (/** @type {unknown} */ answer) {
			const said = /** @type {Record<string, unknown> | null} */ (answer);
			return {
				softcam: !!said && said['softcam'] === true,
				ci: !!said && said['ci_module'] === true,
			};
		},
		function () {
			return { softcam: false, ci: false };
		});
}

/** @returns {Promise<void>} */
async function read() {
	const [standby, channel, recordings, timers, crypt] = await Promise.all([
		settled(api('GET', '/api/v1/system/standby', { recover: false })),
		settled(api('GET', '/api/v1/channels/current', { recover: false })),
		settled(api('GET', '/api/v1/recordings', { recover: false })),
		settled(api('GET', '/api/v1/timers', { recover: false })),
		readDecryption(),
	]);

	/* The guide is asked about a channel, so it is asked after the channel and
	   not beside it. A channel the guide holds nothing for is a refusal, which
	   is a display without a programme on it and not a display that failed. */
	const found = channel.value;
	const event = found
		? await settled(api('GET', '/api/v1/epg/current',
			{ query: { channel: found.id }, recover: false }))
		: null;

	const rows = (recordings.value && recordings.value.items) || [];
	const waiting = (timers.value && timers.value.items) || [];
	const now = seconds();

	seen = {
		asked: true,
		/* Read off the channel and not off whichever call failed first. Every
		   one of them is refused alike to a caller the box does not grant
		   reading to, and this is the one whose absence the page is about. */
		denied: channel.status === 403,
		away: channel.offline,
		standby: !!(standby.value && standby.value.on),
		channel: found,
		event: event ? event.value : null,
		recording: rows.some(function (row) { return !isTimeshift(row); }),
		shifting: rows.some(isTimeshift),
		/* What the display this replaces lit the mark for: a timer that has
		   still to fire and that does something to the picture. A reminder or
		   a plugin run is not what anybody reads that mark as. */
		timer: waiting.some(function (row) {
			return (row.kind === 'record' || row.kind === 'zapto') && row.start > now;
		}),
		softcam: crypt.softcam,
		ci: crypt.ci,
	};

	paint();
}

/**
 * @param {HTMLElement} where
 * @param {string} value
 * @returns {void}
 */
function write(where, value) {
	// Only when it changed. This runs every second, and a write of the same
	// text is still a write the browser lays the page out for.
	if (where.textContent !== value) {
		where.textContent = value;
	}
}

/**
 * @param {HTMLElement} where
 * @param {boolean} on
 * @returns {void}
 */
function mark(where, on) {
	if (on) {
		where.setAttribute('data-on', '');
	} else {
		where.removeAttribute('data-on');
	}
}

/* The sentence under the name, and the empty string while there is a channel to
   name instead. The order is what the box is doing and not what this page asked
   for: a box in standby is in standby whether or not the guide answered. */
/** @returns {string} */
function said() {
	if (seen.denied)
		return t(text, 'info.denied');
	if (seen.away)
		return t(text, 'info.away');
	if (!seen.asked)
		return t(text, 'info.asking');
	if (seen.standby)
		return t(text, 'info.standby');
	if (!seen.channel)
		return t(text, 'info.nothing');
	return '';
}

/* A channel the box is showing, which standby is not. The box goes on naming
   the channel it was last on while it sleeps, and a display drawing it would
   say the box is playing something it is not. */
/** @returns {Api.Result<'GET /api/v1/channels/current'> | null} */
function playing() {
	return (seen.asked && !seen.standby) ? seen.channel : null;
}

/** @returns {void} */
function paint() {
	const channel = playing();
	const event = channel ? seen.event : null;
	const sentence = said();

	/* Every mark straight off what the box said, standby included. A box asleep
	   with a timer recording is exactly the state somebody walks past this
	   display to check. */
	mark(shown.ecm, seen.softcam);
	mark(shown.ci, seen.ci);
	mark(shown.timer, seen.timer);
	mark(shown.rec, seen.recording);
	mark(shown.ts, seen.shifting);

	write(shown.said, sentence);
	shown.said.hidden = sentence === '';

	/* The picture instead of the name where the box has one, which is what the
	   display this replaces did. Nothing the box says about a channel tells
	   whether it has a picture, so the way to find out is to ask for it, and
	   the name stands until the answer is one. */
	const wanted = channel ? '/api/v1/channels/' + encodeURIComponent(channel.id) + '/logo' : '';
	if (shown.logo.getAttribute('data-for') !== wanted) {
		shown.logo.setAttribute('data-for', wanted);
		drawn = '';
		if (wanted === '') {
			shown.logo.removeAttribute('src');
		} else {
			shown.logo.src = wanted;
		}
	}
	const picture = wanted !== '' && drawn === wanted;
	shown.logo.hidden = !picture;

	write(shown.name, channel ? channel.name : '');
	shown.name.hidden = !channel || picture;

	write(shown.event, event ? event.title : (channel ? t(text, 'info.noevent') : ''));
	shown.event.hidden = !channel;

	tick();
}

/* The three values that move on their own: the clock, the share of the
   programme that has gone, and what is left of it. */
/** @returns {void} */
function tick() {
	write(shown.clock, clock(seconds()));

	const event = playing() ? seen.event : null;
	const total = event ? event.duration : 0;
	shown.bar.hidden = !event || total <= 0;
	if (!event || total <= 0) {
		write(shown.left, '');
		return;
	}

	const gone = Math.min(Math.max(seconds() - event.start, 0), total);
	// A tenth of a percent, so a bar a thousand pixels wide moves smoothly and
	// a shorter one is not rewritten for a change nobody can see.
	const width = (Math.round((gone / total) * 1000) / 10) + '%';
	if (shown.fill.style.width !== width) {
		shown.fill.style.width = width;
	}

	write(shown.left, t(text, 'info.left', { time: duration(total - gone) }));

	if (gone >= total) {
		again(kAfterEventEndMs);
	}
}

let pending = 0;
let pendingAt = 0;

/* Another round, at most one waiting, and the soonest of whatever asked for
   one: an event arriving while the end of a programme is being waited out is
   answered at the event's own pace rather than in fifteen seconds. */
/**
 * @param {number} delay
 * @returns {void}
 */
function again(delay) {
	const at = Date.now() + delay;
	if (pending !== 0) {
		if (at >= pendingAt) {
			return;
		}
		clearTimeout(pending);
	}
	pendingAt = at;
	pending = setTimeout(function () {
		pending = 0;
		read();
	}, delay);
}

/** @returns {void} */
function start() {
	shown.logo.addEventListener('load', function () {
		drawn = shown.logo.getAttribute('data-for') || '';
		paint();
	});
	shown.logo.addEventListener('error', function () {
		drawn = '';
		paint();
	});

	/* Every type this display draws something out of. The channel, the guide,
	   the two recording marks and standby; a timer made or dropped moves the
	   third mark on the right. Nothing on the stream announces a softcam
	   starting to deliver, so the two on the left ride on the round that runs
	   anyway. Standby carries its own value, but the rest of the round has to
	   be asked for regardless, so it is one path and not two. */
	for (const type of ['zap', 'standby', 'record-start', 'record-stop', 'timer-changed', 'epg-updated']) {
		events.on(type, function () { again(kAfterEventMs); });
	}
	events.start();

	read();
	readLanguage();
	setInterval(tick, kTickMs);
	setInterval(read, kAgainMs);
}

paint();
start();
