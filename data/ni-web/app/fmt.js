/* The box speaks in its own units, and every conversion out of them stands here exactly
   once. Nothing in this file reaches the network or holds state.

   The one rule that is not a matter of taste: a channel identifier is a string, from the
   moment it arrives to the moment it is sent back, and never a number. The box writes it
   with %llx and it is up to sixty four bits wide; a browser's number holds fifty three of
   them exactly, so parseInt or Number on the widest identifiers answers a different channel
   from the one that arrived, and does it silently. */

import { text } from './data.text.js';
import { language as chosen } from './i18n.js';

/* The one chosen language of the page, which is i18n's, narrowed to the two this file has
   words for. German for anything else.

   This file held a second one and nothing ever set it, and the units came out right anyway
   only because i18n writes document.documentElement.lang on its way past and this read the
   document. That is not a bridge: i18n returns early without writing the document when the
   language asked for is the one already chosen. */
/** @returns {'de' | 'en'} */
export function language() {
	return chosen() === 'en' ? 'en' : 'de';
}

// The catalogue of the chosen language, for this stream's own words. A key nobody put in
// the catalogue answers as itself rather than as nothing, so a missing word shows up on the
// screen as the key that is missing.
/** @returns {Record<string, string>} */
export function words() {
	return text[language()] || text.de;
}

/**
 * @param {string} key
 * @returns {string} the word, or the key itself so a missing one is reportable
 */
export function word(key) {
	const table = words();
	const found = table[key];
	return (found !== undefined && Object.prototype.hasOwnProperty.call(table, key)) ? found : key;
}

/* ------------------------------------------------------------ identifiers */

const kHexDigits = '0123456789abcdef';

/* One identifier in the one spelling everything here compares: lower case hex with no
   leading zeroes and no 0x in front, which is what the box writes.

   An identifier that is not hexadecimal at all answers empty. That is not a fault to throw
   over: identifiers reach this page out of an address bar. */
/**
 * @param {unknown} value
 * @returns {string} the canonical spelling, or empty for anything that is not
 *          an identifier at all
 */
export function channelId(value) {
	if (typeof value !== 'string') {
		// Numbers are refused rather than converted. A caller holding one has
		// already lost the low bits of a wide identifier, and answering a
		// plausible string for it would hide where that happened.
		return '';
	}
	let hex = value.trim().toLowerCase();
	if (hex.startsWith('0x')) {
		hex = hex.slice(2);
	}
	if (hex === '') {
		return '';
	}
	for (const digit of hex) {
		if (kHexDigits.indexOf(digit) === -1) {
			return '';
		}
	}
	let first = 0;
	while (first < hex.length - 1 && hex[first] === '0') {
		first++;
	}
	return hex.slice(first);
}

// Whether two identifiers name the same channel, whatever spelling each of
// them arrived in.
/**
 * @param {unknown} a
 * @param {unknown} b
 * @returns {boolean}
 */
export function sameChannel(a, b) {
	const left = channelId(a);
	return left !== '' && left === channelId(b);
}

/* ------------------------------------------------------------------ time */

/** @returns {string} */
function locale() {
	return language() === 'en' ? 'en-GB' : 'de-DE';
}

// A time the box sent, which is seconds since the epoch (ParamType::Time), as the clock of
// whoever is looking at it. Nought is drawn as nothing: the box writes it for a time it does
// not have.
/**
 * @param {number} seconds since the epoch, as the box counts
 * @returns {string} the time of day, and empty for a time the box does not have
 */
export function clock(seconds) {
	const at = dateOf(seconds);
	return at === null ? '' : at.toLocaleTimeString(locale(), {
		hour: '2-digit', minute: '2-digit', hour12: false,
	});
}

/**
 * @param {number} seconds since the epoch, as the box counts
 * @returns {string} the date, and empty for a time the box does not have
 */
export function day(seconds) {
	const at = dateOf(seconds);
	return at === null ? '' : at.toLocaleDateString(locale(), {
		weekday: 'short', day: '2-digit', month: '2-digit', year: 'numeric',
	});
}

/**
 * @param {number} seconds since the epoch, as the box counts
 * @returns {string} both, and empty for a time the box does not have
 */
export function dayAndClock(seconds) {
	const at = dateOf(seconds);
	return at === null ? '' : day(seconds) + ' ' + clock(seconds);
}

/**
 * @param {number} seconds
 * @returns {Date | null} null for a time the box does not have, which it
 *          writes as nought
 */
export function dateOf(seconds) {
	if (!Number.isFinite(seconds) || seconds <= 0) {
		return null;
	}
	return new Date(seconds * 1000);
}

/* A span of seconds as hours and minutes, and as minutes and seconds below an hour. Never
   as a clock time: a recording of ninety minutes drawn as 01:30 reads like half past one. */
/**
 * @param {number} seconds
 * @returns {string}
 */
export function duration(seconds) {
	if (!Number.isFinite(seconds) || seconds < 0) {
		return '';
	}
	const whole = Math.floor(seconds);
	const hours = Math.floor(whole / 3600);
	const minutes = Math.floor((whole % 3600) / 60);
	if (hours > 0) {
		return hours + ' ' + word('unit.hour') + ' ' + minutes + ' ' + word('unit.minute');
	}
	if (minutes > 0) {
		return minutes + ' ' + word('unit.minute');
	}
	return whole + ' ' + word('unit.second');
}

/* ---------------------------------------------------------------- numbers */

const kByteUnits = ['unit.byte', 'unit.kibibyte', 'unit.mebibyte', 'unit.gibibyte', 'unit.tebibyte'];

/* Sizes the box measures in bytes, in the multiples the box itself counts in. Powers of two
   and not of ten, because that is what the free space of a filesystem is counted in and a
   recording that fits has to look like it fits. */
/**
 * @param {number} value
 * @returns {string}
 */
export function bytes(value) {
	if (!Number.isFinite(value) || value < 0) {
		return '';
	}
	let size = value;
	let unit = 0;
	while (size >= 1024 && unit < kByteUnits.length - 1) {
		size /= 1024;
		unit++;
	}
	// Whole bytes stay whole: a file of 512 bytes is not 512.0 of anything.
	const digits = (unit === 0 || size >= 100) ? 0 : 1;
	return size.toLocaleString(locale(), {
		minimumFractionDigits: digits, maximumFractionDigits: digits,
	/* The index is bounded by the loop above, so the second half of this can never be
	   reached. It is spelt out rather than asserted away, and spelt as a key nobody put
	   in the catalogue, so a table shortened one day lands on the screen as the name of
	   what went missing. */
	}) + ' ' + word(kByteUnits[unit] || 'unit.unknown');
}

/* A measured value against the scale the same answer states, as a percentage. full_scale
   comes with the reading (src/httpd/ep/ep_tuner.cpp) and is not a constant here: it is the
   driver's, it differs between tuners, and a percentage worked out against a number written
   into this page would be a different percentage on the next box. */
/**
 * @param {number} value
 * @param {number} fullScale the scale the same answer states, which is the
 *        driver's and differs between tuners
 * @returns {number} nought to a hundred, and -1 for a reading there is no
 *          scale for
 */
export function ratio(value, fullScale) {
	if (!Number.isFinite(value) || !Number.isFinite(fullScale) || fullScale <= 0) {
		return -1;
	}
	const percent = Math.round((value / fullScale) * 100);
	return percent < 0 ? 0 : (percent > 100 ? 100 : percent);
}

/**
 * @param {number} value
 * @param {number} fullScale
 * @returns {string}
 */
export function percent(value, fullScale) {
	const share = ratio(value, fullScale);
	return share < 0 ? '' : share + ' %';
}

/* ----------------------------------------------------------------- timers */

const kRepeatWords = [
	'timer.repeat.once',
	'timer.repeat.daily',
	'timer.repeat.weekly',
	'timer.repeat.biweekly',
	'timer.repeat.fourweekly',
	'timer.repeat.monthly',
	'timer.repeat.byevent',
];

const kWeekdayWords = [
	'weekday.mo', 'weekday.tu', 'weekday.we',
	'weekday.th', 'weekday.fr', 'weekday.sa', 'weekday.su',
];

// Anything at or above this is a set of weekdays rather than one of the words above, and the
// days themselves are the bits from nine upwards, Monday first
// (lib/timerdclient/timerdtypes.h, and the reader in timerdclient.cpp that walks
// 1 << (n + 9)).
const kWeekdaysFlag = 0x100;
const kWeekdayShift = 9;

// Which days a repeating timer runs on, as their numbers, Monday being nought.
// Empty for a timer that does not repeat by weekday.
/**
 * @param {number} repeat the daemon's own numbering
 * @returns {number[]} the days, Monday being nought, empty for a timer that
 *          does not repeat by weekday
 */
export function weekdays(repeat) {
	/** @type {number[]} */
	const out = [];
	if (!Number.isFinite(repeat) || repeat < kWeekdaysFlag) {
		return out;
	}
	for (let day = 0; day < 7; day++) {
		if (repeat & (1 << (day + kWeekdayShift))) {
			out.push(day);
		}
	}
	return out;
}

/* How a timer repeats, in words. The daemon's own numbering reaches this page unchanged
   (src/httpd/ep/ep_timers.cpp says so in the document), so the reading of it lives here and not
   in the three screens that draw a timer. */
/**
 * @param {number} repeat
 * @returns {string}
 */
export function timerRepeat(repeat) {
	if (!Number.isFinite(repeat) || repeat < 0) {
		return word('timer.repeat.unknown');
	}
	if (repeat >= kWeekdaysFlag) {
		const days = weekdays(repeat).map(function (day) {
			return word(kWeekdayWords[day] || 'weekday.unknown');
		});
		return days.length === 0 ? word('timer.repeat.weekdays') : days.join(' ');
	}
	if (repeat < kRepeatWords.length) {
		return word(kRepeatWords[repeat] || 'timer.repeat.unknown');
	}
	return word('timer.repeat.unknown');
}

const kStateWords = [
	'timer.state.scheduled',
	'timer.state.preannounce',
	'timer.state.running',
	'timer.state.finished',
	'timer.state.terminated',
];

// What the daemon is doing with a timer, in words, out of the same numbering.
/**
 * @param {number} state
 * @returns {string}
 */
export function timerState(state) {
	if (!Number.isFinite(state) || state < 0 || state >= kStateWords.length) {
		return word('timer.state.unknown');
	}
	return word(kStateWords[state] || 'timer.state.unknown');
}

/* ------------------------------------------------------------- recordings */

/* What a row of the recordings list is, and what mark it is drawn with, both out of one
   sentence so that the screens drawing that list cannot answer differently. Three of them
   did: the corner of the frame left a timeshift out of its count of recordings, the overview
   drew it as a warning and the recordings screen drew it as running, all three off the same
   answer.

   A timeshift is not a recording: it is the box holding what is on now. So it does not
   belong in a count of recordings, and nothing about it is wrong, so the mark it carries is
   the running one and never the warning one. */
/**
 * @param {{ timeshift?: boolean } | null | undefined} one
 * @returns {boolean}
 */
export function isTimeshift(one) {
	return !!(one && one.timeshift);
}

/**
 * @param {{ timeshift?: boolean } | null | undefined} one
 * @returns {string} the mark ui/dot.js draws it with
 */
export function recordingMark(one) {
	return isTimeshift(one) ? 'running' : 'recording';
}
