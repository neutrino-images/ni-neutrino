// Where the grid's window opens for the evening, and a day later.
//
// Run on a Berlin clock (check-web-gridtime.sh says so), and checked to be on
// one, because a case about the night the clocks change says nothing on a
// clock that never changes.
import { primetime, dayLater, windowStart, PRIME_HOUR } from '../../data/ni-web/app/screens/epg/grid.model.js';

let checked = 0;
let failed = 0;

/**
 * @param {unknown} got
 * @param {unknown} want
 * @param {string} what
 */
function same(got, want, what) {
	checked++;
	if (got !== want) {
		failed++;
		process.stderr.write('gridtime: ' + what + ': got ' + String(got) + ', wanted ' + String(want) + '\n');
	}
}

/**
 * Seconds since the epoch for a Berlin wall clock, by the offset stated.
 * @param {string} at e.g. '2026-10-27T06:30:00+01:00'
 * @returns {number}
 */
function sec(at) {
	return Date.parse(at) / 1000;
}

// The clock this runs on is the one the cases are written for.
same(new Date(sec('2026-07-01T12:00:00+02:00') * 1000).getHours(), 12, 'the clock is Berlin in summer');
same(new Date(sec('2026-12-01T12:00:00+01:00') * 1000).getHours(), 12, 'the clock is Berlin in winter');
same(PRIME_HOUR, 20, 'the evening begins at eight');

// ------------------------------------------------------------- primetime

same(primetime(sec('2026-10-27T06:30:00+01:00')), sec('2026-10-27T20:00:00+01:00'),
	'a morning window goes to that evening');
same(primetime(sec('2026-10-27T21:00:00+01:00')), sec('2026-10-27T20:00:00+01:00'),
	'a window past eight goes back to that evening, not on to the next');
same(primetime(sec('2026-10-28T00:30:00+01:00')), sec('2026-10-28T20:00:00+01:00'),
	'a window after midnight belongs to the day it is on');
same(primetime(sec('2026-10-27T20:00:00+01:00')), sec('2026-10-27T20:00:00+01:00'),
	'the evening asked for twice stays where it is');
same(primetime(sec('2026-07-01T09:00:00+02:00')), sec('2026-07-01T20:00:00+02:00'),
	'in summer time it is eight on the summer clock');
// The day the clocks go back: twenty five hours long, still an evening at eight.
same(primetime(sec('2026-10-25T01:30:00+02:00')), sec('2026-10-25T20:00:00+01:00'),
	'on the day the clocks go back, eight on the clock of the evening');
// And the day they go forward: twenty three hours long.
same(primetime(sec('2026-03-29T01:30:00+01:00')), sec('2026-03-29T20:00:00+02:00'),
	'on the day the clocks go forward, eight on the clock of the evening');
same(primetime(sec('2026-10-27T06:30:00+01:00')), windowStart(primetime(sec('2026-10-27T06:30:00+01:00'))),
	'the evening opens on a mark of the axis');

// ------------------------------------------------------------- a day later

same(dayLater(sec('2026-10-27T06:30:00+01:00')), sec('2026-10-28T06:30:00+01:00'),
	'an ordinary day is twenty four hours');
same(dayLater(sec('2026-10-27T23:00:00+01:00')), sec('2026-10-28T23:00:00+01:00'),
	'a late window goes to the same hour of the next day');
same(dayLater(sec('2026-10-31T20:00:00+01:00')), sec('2026-11-01T20:00:00+01:00'),
	'the last of a month goes to the first of the next');
same(dayLater(sec('2026-10-24T20:00:00+02:00')), sec('2026-10-25T20:00:00+01:00'),
	'across the clocks going back it is eight again, twenty five hours on');
same(dayLater(sec('2026-03-28T20:00:00+01:00')), sec('2026-03-29T20:00:00+02:00'),
	'across the clocks going forward it is eight again, twenty three hours on');
same(dayLater(primetime(sec('2026-10-27T06:30:00+01:00'))), sec('2026-10-28T20:00:00+01:00'),
	'the evening, then a day: tomorrow evening');

if (failed > 0) {
	process.stderr.write('gridtime: ' + failed + ' of ' + checked + ' cases failed\n');
	process.exit(1);
}
process.stdout.write('gridtime-cases.mjs: ' + checked + ' cases hold\n');
