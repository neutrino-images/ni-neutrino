/* What a timer is, in the shapes the two screens of this destination draw it in,
   and the rules about it that have to be known before anything is sent.

   Here rather than in either screen because both draw the same eight kinds and
   the same fourteen fields, and two copies of that table would run apart the
   first time the daemon gains one.

   Nothing in this file reaches the network except through the store, and every
   function below the hooks is pure. */

import { useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';

/* The eight kinds the box makes, in the order the server declares them
   (src/httpd/ep/ep_timers.cpp), and beside each one the fields it actually carries.

   Not cosmetic. The daemon is handed one block per kind and reads exactly the
   bytes of that kind's block (src/coreapi/box/channelsource_real.cpp), so a value
   written into a field the kind has no block for is a value nothing ever reads
   back, and a form that offered it would lie about what it saved. Standby is the
   plain case: standby_on is read for that kind and for no other. */
export const KINDS = [
	{
		id: 'shutdown', channel: false, start: true, duration: false,
		title: '', repeat: true, announce: true, standby: false, dir: false,
	},
	{
		id: 'zapto', channel: true, start: true, duration: false,
		title: 'form.title.programme', repeat: true, announce: true, standby: false, dir: false,
	},
	{
		id: 'standby', channel: false, start: true, duration: false,
		title: '', repeat: true, announce: true, standby: true, dir: false,
	},
	{
		id: 'record', channel: true, start: true, duration: true,
		title: 'form.title.programme', repeat: true, announce: true, standby: false, dir: true,
	},
	{
		id: 'remind', channel: false, start: true, duration: false,
		title: 'form.title.words', repeat: true, announce: true, standby: false, dir: false,
	},
	{
		id: 'sleeptimer', channel: false, start: true, duration: false,
		title: '', repeat: true, announce: true, standby: false, dir: false,
	},
	{
		id: 'exec-plugin', channel: false, start: true, duration: false,
		title: 'form.title.plugin', repeat: true, announce: true, standby: false, dir: false,
	},
	/* The one kind with no start and no repeat of its own. It means now, and
	   the box builds it as a recording whose start has come, so a repeat on it
	   would name occurrences of a thing that has already happened. */
	{
		id: 'immediate-record', channel: true, start: false, duration: true,
		title: 'form.title.programme', repeat: false, announce: false, standby: false, dir: true,
	},
];

/**
 * @param {string} id
 * @returns {{ id: string, channel: boolean, start: boolean, duration: boolean, title: string, repeat: boolean, announce: boolean, standby: boolean, dir: boolean } | null}
 */
export function kindOf(id) {
	for (const kind of KINDS) {
		if (kind.id === id)
			return kind;
	}
	return null;
}

/* A kind the read side kept although this build has no word for it: the server
   answers the number itself under the same member, deliberately, so that a
   timer file written by another image survives being read (ep_timers.cpp,
   kindName). Such a timer is shown and is not offered for changing, because
   nothing here knows which fields it has. */

/**
 * @param {string} kind
 * @returns {boolean}
 */
export function isKnownKind(kind) {
	return kindOf(kind) !== null;
}

/* The repeats, in the daemon's own numbering, which reaches this page unchanged
   (src/httpd/ep/ep_timers.cpp). Nought to six are the plain ones; from the weekday
   flag upwards the days are one bit each from the ninth up, Monday first
   (lib/timerdclient/timerdtypes.h). */
export const REPEAT_ONCE = 0;
export const REPEAT_WEEKDAYS = 0x100;
const WEEKDAY_SHIFT = 9;

// The plain repeats as the form offers them, in the daemon's order, so that the
// index is the number.
export const PLAIN_REPEATS = ['once', 'daily', 'weekly', 'biweekly', 'fourweekly', 'monthly', 'byevent'];

/**
 * Which of the choices a stored repeat is, as the form names them.
 * @param {number} repeat
 * @returns {string}
 */
export function repeatChoice(repeat) {
	if (!Number.isFinite(repeat) || repeat < 0)
		return 'once';
	if (repeat >= REPEAT_WEEKDAYS)
		return 'weekdays';
	const name = PLAIN_REPEATS[repeat];
	return name === undefined ? 'once' : name;
}

/**
 * Which days a stored repeat runs on, as seven flags, Monday first.
 * @param {number} repeat
 * @returns {boolean[]}
 */
export function repeatDays(repeat) {
	const out = [];
	const set = Number.isFinite(repeat) && repeat >= REPEAT_WEEKDAYS;
	for (let day = 0; day < 7; day++)
		out.push(set && (repeat & (1 << (day + WEEKDAY_SHIFT))) !== 0);
	return out;
}

/**
 * The number to send for a choice and a set of days.
 * @param {string} choice
 * @param {boolean[]} days
 * @returns {number}
 */
export function repeatValue(choice, days) {
	if (choice !== 'weekdays') {
		const at = PLAIN_REPEATS.indexOf(choice);
		return at < 0 ? REPEAT_ONCE : at;
	}
	let value = REPEAT_WEEKDAYS;
	let day = 0;
	for (const on of days) {
		if (on)
			value |= 1 << (day + WEEKDAY_SHIFT);
		day++;
	}
	return value;
}

/* ------------------------------------------------------------------- time */

/**
 * @param {number} value
 * @returns {string}
 */
function pad(value) {
	return value < 10 ? '0' + value : String(value);
}

/**
 * A moment the box sent, as the value the control for it takes, in the clock of
 * whoever is looking at it. An ISO string would be UTC and the control reads its
 * value as local, so the two would differ by the offset and nobody would see why.
 * @param {number} seconds
 * @returns {string}
 */
export function momentInput(seconds) {
	if (!Number.isFinite(seconds) || seconds <= 0)
		return '';
	const at = new Date(seconds * 1000);
	return at.getFullYear() + '-' + pad(at.getMonth() + 1) + '-' + pad(at.getDate()) +
		'T' + pad(at.getHours()) + ':' + pad(at.getMinutes());
}

/**
 * And back. Nought for anything that is not a moment, which is what an empty
 * control and a half typed one both are.
 * @param {string} value
 * @returns {number}
 */
export function momentSeconds(value) {
	const text = String(value === undefined || value === null ? '' : value);
	const halves = text.split('T');
	if (halves.length !== 2)
		return 0;
	const date = String(halves[0]).split('-');
	const time = String(halves[1]).split(':');
	if (date.length !== 3 || time.length < 2)
		return 0;
	const year = Number(date[0]);
	const month = Number(date[1]);
	const day = Number(date[2]);
	const hour = Number(time[0]);
	const minute = Number(time[1]);
	if (!Number.isFinite(year) || !Number.isFinite(month) || !Number.isFinite(day))
		return 0;
	if (!Number.isFinite(hour) || !Number.isFinite(minute))
		return 0;
	const at = new Date(year, month - 1, day, hour, minute, 0, 0);
	const seconds = Math.floor(at.getTime() / 1000);
	return Number.isFinite(seconds) && seconds > 0 ? seconds : 0;
}

/* The box refuses a one-off that begins before the current minute, and it
   rounds down to that minute rather than to the second (src/coreapi/timers.cpp,
   startOfMinute), because a caller building a moment out of separate fields has
   no seconds to give. The same rounding is done here so that the two answer the
   same thing about the same moment. */

/**
 * @param {number} seconds
 * @returns {number}
 */
export function startOfMinute(seconds) {
	return seconds - (seconds % 60);
}

/* ------------------------------------------------------------------- form */

/**
 * @typedef {object} Draft
 * @property {string} kind
 * @property {string} mode
 * @property {string} channel_id
 * @property {string} start
 * @property {string} minutes
 * @property {string} title
 * @property {string} repeat
 * @property {boolean[]} days
 * @property {string} count
 * @property {string} announce
 * @property {boolean} standby_on
 * @property {string} recording_dir
 * @property {string} epg_id
 * @property {number} epg_start
 */

// How long a recording lasts when nothing else said. A number, and any number
// would do: what matters is that the field cannot be empty, not that two hours
// is right for the programme somebody has in mind.
export const DEFAULT_MINUTES = 120;

/**
 * An empty draft, for the kind asked for.
 * @param {string} kind
 * @param {number} now
 * @returns {Draft}
 */
export function emptyDraft(kind, now) {
	const wanted = isKnownKind(kind) ? kind : 'record';
	const at = kindOf(wanted);
	return {
		kind: wanted,
		mode: 'tv',
		channel_id: '',
		start: momentInput(startOfMinute(now) + 60),
		minutes: at !== null && at.duration ? String(DEFAULT_MINUTES) : '',
		title: '',
		repeat: 'once',
		days: [false, false, false, false, false, false, false],
		count: '0',
		announce: '',
		standby_on: false,
		recording_dir: '',
		epg_id: '',
		epg_start: 0,
	};
}

/**
 * A timer the box holds, as a draft the form can edit.
 * @param {Api.Timer} timer
 * @returns {Draft}
 */
export function draftOf(timer) {
	const minutes = timer.stop > timer.start ? Math.round((timer.stop - timer.start) / 60) : 0;
	return {
		kind: timer.kind,
		mode: 'tv',
		channel_id: timer.channel_id,
		start: momentInput(timer.start),
		minutes: minutes > 0 ? String(minutes) : '',
		title: timer.title,
		repeat: repeatChoice(timer.repeat),
		days: repeatDays(timer.repeat),
		count: String(timer.repeat_count),
		// Kept as the minutes before the start it is, which is how it is offered,
		// because a second moment beside the start is two fields that have to be
		// dragged along together.
		announce: timer.announce > 0 && timer.announce <= timer.start
			? String(Math.round((timer.start - timer.announce) / 60))
			: '',
		standby_on: timer.standby_on,
		recording_dir: timer.recording_dir,
		epg_id: timer.epg_id,
		epg_start: timer.epg_start,
	};
}

/**
 * A whole number a field was typed into, and nought for anything else. An empty
 * field is nought and not NaN, because nought is a number the rules below can
 * say something about and NaN is one every comparison answers false to.
 * @param {string} value
 * @returns {number}
 */
export function wholeNumber(value) {
	const text = String(value === undefined || value === null ? '' : value).trim();
	if (text === '')
		return 0;
	const number = Number(text);
	return Number.isFinite(number) ? Math.trunc(number) : 0;
}

/**
 * When the timer begins, in seconds. An immediate recording begins now and says
 * so rather than carrying a field somebody could push into next week.
 * @param {Draft} draft
 * @param {number} now
 * @returns {number}
 */
export function startSeconds(draft, now) {
	if (draft.kind === 'immediate-record')
		return now;
	return momentSeconds(draft.start);
}

/**
 * And when it stops, which for the two recording kinds is the duration the form
 * insisted on, and for everything else is nought: those have nothing to stop.
 * @param {Draft} draft
 * @param {number} now
 * @returns {number}
 */
export function stopSeconds(draft, now) {
	const at = kindOf(draft.kind);
	if (at === null || !at.duration)
		return 0;
	const minutes = wholeNumber(draft.minutes);
	if (minutes <= 0)
		return 0;
	return startSeconds(draft, now) + minutes * 60;
}

/* WHAT THE FORM REFUSES TO SEND, AND WHY IT IS THESE FOUR.

   The first three are the box's own rules, stated here so the person is told
   before the send rather than by a refusal afterwards. Nothing here lets anything
   past that the box would turn down, and everything here it would turn down too
   (src/coreapi/timers.cpp).

   THE DURATION IS THE ONE THAT MATTERS. A recording is the only kind that has an
   end, and a recording that reaches the daemon without one is a row that fires,
   starts writing, and can then be stopped by nothing: the timer it was made from
   has no end for the daemon to act on. So this is not a default somebody can
   clear, it is a value without which nothing is sent at all.

   The fourth is not the box's: a weekday repeat with no day set is a timer that
   matches no day and therefore never runs. The daemon takes it and files it. */

/**
 * The keys of what is wrong with a draft, empty when nothing is.
 * @param {Draft} draft
 * @param {number} now
 * @returns {string[]}
 */
export function draftProblems(draft, now) {
	const out = [];
	const at = kindOf(draft.kind);
	if (at === null) {
		out.push('form.bad.kind');
		return out;
	}

	if (at.channel && draft.channel_id === '')
		out.push('form.bad.channel');

	const start = startSeconds(draft, now);
	if (start <= 0)
		out.push('form.bad.start');

	if (at.duration && wholeNumber(draft.minutes) <= 0)
		out.push('form.bad.duration');

	if (at.repeat && draft.repeat === 'weekdays') {
		let any = false;
		for (const on of draft.days) {
			if (on)
				any = true;
		}
		if (!any)
			out.push('form.bad.noday');
	}

	// Asked of a one-off only, as the box asks it: a repeating timer whose first
	// occurrence is behind us is the ordinary way to enter one, and the daemon
	// moves it to the next.
	const repeats = at.repeat ? repeatValue(draft.repeat, draft.days) : REPEAT_ONCE;
	if (repeats === REPEAT_ONCE && start > 0 && start < startOfMinute(now))
		out.push('form.bad.past');

	return out;
}

/**
 * The two bodies the timer routes declare, named out of the document so that a
 * member the daemon gains is a member these two are held to without anybody
 * writing it down a second time.
 * @typedef {Api.BodyValue<'POST /api/v1/timers'>} CreateBody
 * @typedef {NonNullable<Api.BodyValue<'PATCH /api/v1/timers/{id}'>>} ChangeBody
 */

/**
 * What a new timer is sent as. Only the members the kind carries, because the
 * rest are values the daemon never reads back.
 * @param {Draft} draft
 * @param {number} now
 * @returns {CreateBody}
 */
export function createBody(draft, now) {
	const at = kindOf(draft.kind);
	/** @type {CreateBody} */
	const body = {
		/* The kind as it stands. A kind this build has no word for reaches
		   the draft as the number the daemon calls it, which the read side
		   keeps on purpose, and it goes out that way and is refused by the
		   box, which is the one place that decides which kinds there are. The
		   eight a person can pick are the eight KINDS names. */
		kind: /** @type {CreateBody['kind']} */ (draft.kind),
		start: startSeconds(draft, now),
	};
	if (at === null)
		return body;

	if (at.channel)
		body.channel_id = draft.channel_id;
	if (at.duration)
		body.stop = stopSeconds(draft, now);
	if (at.title !== '')
		body.title = draft.title;
	if (at.repeat) {
		body.repeat = repeatValue(draft.repeat, draft.days);
		body.repeat_count = wholeNumber(draft.count);
	}
	if (at.announce && wholeNumber(draft.announce) > 0)
		body.announce = startSeconds(draft, now) - wholeNumber(draft.announce) * 60;
	if (at.standby)
		body.standby_on = draft.standby_on;
	if (at.dir)
		body.recording_dir = draft.recording_dir;
	/* Carried along whole, and this is the path that matters in daily use: a
	   timer made out of a guide entry knows which entry it came from, and the
	   box keeps the pair in fields of its own. */
	if (draft.epg_id !== '' && draft.epg_start > 0) {
		body.epg_id = draft.epg_id;
		body.epg_start = draft.epg_start;
	}

	/* THE TWO THINGS A RECORDING PICKED OUT OF THE GUIDE GETS, and a recording
	   somebody typed the times of does not.

	   The box draws the same line at its own screens. Times typed into its
	   timer screen are taken as they stand (src/gui/timerlist.cpp), while
	   pressing record on a guide entry asks for both (src/gui/epgview.cpp).
	   The two are different acts: one names a window, the other names a
	   programme, and a programme is worth a margin at each end and worth
	   following when the broadcast moves. Without this the same press gave a
	   bare window here and a margin at the television.

	   Only where the guide entry came along, which is what tells the two acts
	   apart, and only for a recording: the daemon reads neither on any other
	   kind, and an immediate recording begins now, where a margin in front of
	   it would be a start already behind us.

	   Following is asked for only while the programme is still ahead. There is
	   nothing to follow once it has begun, and the box stops asking three
	   minutes earlier still; a timer made inside those three minutes is about
	   a programme starting now, so the difference changes nothing. */
	if (draft.kind === 'record' && draft.epg_id !== '' && draft.epg_start > 0) {
		body.recording_safety = true;
		body.auto_adjust = draft.epg_start > now;
	}
	return body;
}

/**
 * What a change is sent as. The kind, the channel and the guide entry are not in
 * it: the daemon's protocol carries no way to move any of them, so a route that
 * took them would answer ok and change nothing (src/httpd/ep/ep_timers.cpp).
 * @param {Draft} draft
 * @param {number} now
 * @returns {ChangeBody}
 */
export function changeBody(draft, now) {
	const at = kindOf(draft.kind);
	/** @type {ChangeBody} */
	const body = { start: startSeconds(draft, now) };
	if (at === null)
		return body;

	if (at.duration)
		body.stop = stopSeconds(draft, now);
	if (at.title !== '')
		body.title = draft.title;
	if (at.repeat) {
		body.repeat = repeatValue(draft.repeat, draft.days);
		body.repeat_count = wholeNumber(draft.count);
	}
	if (at.announce)
		body.announce = wholeNumber(draft.announce) > 0
			? startSeconds(draft, now) - wholeNumber(draft.announce) * 60
			: 0;
	if (at.standby)
		body.standby_on = draft.standby_on;
	return body;
}

/* --------------------------------------------------------------- the page */

/**
 * One answer out of the store, watched for as long as the screen is drawn. The
 * key is what decides when the watching moves to another address: the options are
 * a fresh object on every draw and comparing those would resubscribe on every one
 * of them.
 * @param {string} method
 * @param {string} path
 * @param {Api.Call<string, 'json'> | null} [options]
 * @param {string} [key]
 * @returns {Web.Snapshot<any>}
 */
export function useAnswer(method, path, options, key) {
	const [shot, setShot] = useState(function () { return store.read(method, path, options); });
	useEffect(function () {
		/* Nothing and null are the same question to the store, and watching is
		   the one door of it that wants to be told which. */
		return store.watch(method, path, options || null, setShot);
	}, [method, path, key === undefined ? '' : key]);
	return shot;
}

