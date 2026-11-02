import { visibleAreas, countMark } from '../../data/ni-web/app/nav.js';
import { setTimers } from '../../data/ni-web/app/screens/timers/nav.js';

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
		process.stderr.write('nav: ' + what + '\n');
	}
}

/** @param {readonly { id: string }[]} list */
function ids(list) {
	return list.map(function (a) { return a.id; }).join(',');
}

const all = [
	{ id: 'plain', items: [] },
	{ id: 'doc', needs: 'api-doc', items: [] },
	{ id: 'ai', needs: 'mcp', items: [] }
];

is(ids(visibleAreas(all, null)) === 'plain,doc', 'before the box has said, an area needing AI access is held back');
is(ids(visibleAreas(all, { apiDoc: true, mcp: true })) === 'plain,doc,ai', 'a build with both offers both');
is(ids(visibleAreas(all, { apiDoc: true, mcp: false })) === 'plain,doc', 'a build without AI access does not offer it');
is(ids(visibleAreas(all, { apiDoc: false, mcp: true })) === 'plain,ai', 'the two switches are read apart');

// The number beside the timers in the bar.
is(countMark(0) === '', 'no timers set draws no number, not a nought');
is(countMark(null) === '' && countMark(undefined) === '', 'a box that has not said draws no number');
is(countMark(-1) === '' && countMark(NaN) === '', 'a count that is no count draws nothing');
is(countMark(1) === '1' && countMark(42) === '42', 'a count is drawn as its figures');
is(countMark(99) === '99' && countMark(100) === '99+', 'two figures at most');

is(setTimers(null) === 0 && setTimers([]) === 0, 'no list and an empty list are no timers');
is(setTimers([{ state: 0 }, { state: 1 }, { state: 2 }]) === 3, 'scheduled, announced and running are set');
is(setTimers([{ state: 0 }, { state: 3 }, { state: 4 }]) === 1, 'finished and stopped are not counted');
is(setTimers([{}]) === 1, 'a timer without a state is counted, since it is in the list');

if (checked !== 13) {
	process.stderr.write('nav: ran ' + checked + ' checks where there are 13\n');
	process.exit(1);
}
if (failed)
	process.exit(1);
console.log('nav: ' + checked + ' checks');
