// The dragging gesture of the bouquet screen, driven without a browser.
//
// The gesture is the one thing on these pages that a person performs rather than
// presses, and the one that used to work with a mouse and with nothing else. What it
// is made of is a machine that is handed pointer events and a handful of
// arrangements over lists, and neither reaches the document: the row under a point
// arrives as an argument.
//
// What is NOT checked here is the part that does reach the document, which is
// spotFromPoint. Reading elementFromPoint needs a layout and there is none; the fake
// below stands in for it, and what that fake is held to is the shape the real one
// answers with.
import {
	aimFor, placed, stepsFor, dragging
} from '../../data/ni-web/app/screens/channels/drag.js';

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
		process.stderr.write('drag: ' + what + '\n');
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

/** A row of the fake document, forty pixels tall, stacked from the top. */
function rows(zone, ids, from) {
	return ids.map(function (id, at) {
		return { zone: zone, id: id, top: from + at * 40, height: 40 };
	});
}

/**
 * A document made of two lists, read the way spotFromPoint reads the real one:
 * the left column is 0 to 200 across, the middle one from 200.
 *
 * @param {string[]} bouquets
 * @param {string[]} members
 */
function pageOf(bouquets, members) {
	const left = rows('bouquets', bouquets, 100);
	const middle = rows('members', members, 100);
	return function (x, y) {
		const column = x < 200 ? left : middle;
		for (const row of column) {
			if (y >= row.top && y < row.top + row.height)
				return row;
		}
		return null;
	};
}

/**
 * The same document with the middle list holding nothing.
 *
 * A list that draws no row has no row to answer with, so what the screen puts there
 * instead is one place the height of where the rows would be, naming no row at all
 * (bouquets.js, bq-nothing). This is that shape, because the shape is the whole of
 * what this file may know about the document.
 *
 * @param {string[]} bouquets
 */
function pageWithNothingInTheMiddle(bouquets) {
	const left = rows('bouquets', bouquets, 100);
	const nothing = { zone: 'members', id: '', top: 100, height: 108 };
	return function (x, y) {
		if (x >= 200)
			return (y >= nothing.top && y < nothing.top + nothing.height) ? nothing : null;
		for (const row of left) {
			if (y >= row.top && y < row.top + row.height)
				return row;
		}
		return null;
	};
}

// ------------------------------------------------------------------ aiming

const carryBouquet = { kind: 'bouquet', id: 'Favoriten', label: 'Favoriten' };
const carryMember = { kind: 'member', id: 'c2', label: 'ZDF' };
const carryPool = { kind: 'pool', id: 'c9', label: 'Arte' };

const bqRow = { zone: 'bouquets', id: 'Sport', top: 100, height: 40 };
const bqSelf = { zone: 'bouquets', id: 'Favoriten', top: 140, height: 40 };
const bqShown = { zone: 'bouquets', id: 'Gezeigt', top: 180, height: 40 };
const memRow = { zone: 'members', id: 'c5', top: 100, height: 40 };
const memSelf = { zone: 'members', id: 'c2', top: 140, height: 40 };

same(aimFor(carryBouquet, bqRow, 105, 'Gezeigt'), { zone: 'bouquets', id: 'Sport', after: false },
	'a bouquet over the upper half of another lands in front of it');
same(aimFor(carryBouquet, bqRow, 135, 'Gezeigt'), { zone: 'bouquets', id: 'Sport', after: true },
	'and over the lower half behind it');
is(aimFor(carryBouquet, bqSelf, 145, 'Gezeigt') === null,
	'a bouquet over itself aims at nothing');
is(aimFor(carryBouquet, memRow, 105, 'Gezeigt') === null,
	'a bouquet does not aim into the middle list');
is(aimFor(carryBouquet, null, 105, 'Gezeigt') === null,
	'and over nothing at all it aims at nothing');

same(aimFor(carryMember, memRow, 105, 'Gezeigt'), { zone: 'members', id: 'c5', after: false },
	'a member over another member lands beside it');
is(aimFor(carryMember, memSelf, 145, 'Gezeigt') === null,
	'a member over itself aims at nothing');
same(aimFor(carryMember, bqRow, 105, 'Gezeigt'), { zone: 'handover', id: 'Sport', after: false },
	'a member over another bouquet is handed to it');
is(aimFor(carryMember, bqShown, 185, 'Gezeigt') === null,
	'and over the bouquet it is already in it aims at nothing');
same(aimFor(carryPool, memRow, 135, 'Gezeigt'), { zone: 'members', id: 'c5', after: true },
	'a channel out of the pool lands beside a member');
same(aimFor(carryPool, bqRow, 105, 'Gezeigt'), { zone: 'handover', id: 'Sport', after: false },
	'and can be handed straight to another bouquet');

/* A half that holds nothing. It answers as one place and names no row, which
   is the whole reason it is a case of its own: the aim carries that same empty
   name onwards, and an entry put beside a row no list holds lands at the end
   of it (placed, below). */
const memNone = { zone: 'members', id: '', top: 100, height: 108 };
same(aimFor(carryPool, memNone, 130, 'Gezeigt'), { zone: 'members', id: '', after: false },
	'a channel out of the pool aims at a half that holds nothing');
same(aimFor(carryPool, memNone, 200, 'Gezeigt'), { zone: 'members', id: '', after: false },
	'and low over it aims at the same place, since a list with no row has no side to be on');
is(aimFor(carryBouquet, memNone, 130, 'Gezeigt') === null,
	'a bouquet does not aim into an empty half either');
/* And the left column, which has an empty state of its own and deliberately
   carries no target in it: a box that holds no bouquet holds nothing to hand a
   channel to, and the selection is empty as well, so the one rule that is
   already here refuses it. */
is(aimFor(carryPool, { zone: 'bouquets', id: '', top: 100, height: 108 }, 130, '') === null,
	'and nothing is handed to a list of bouquets that holds none');

// --------------------------------------------------------------- arranging

const list = ['a', 'b', 'c', 'd'];
same(placed(list, 'd', 'b', false), ['a', 'd', 'b', 'c'], 'moved in front of a row further up');
same(placed(list, 'a', 'c', true), ['b', 'c', 'a', 'd'], 'moved behind a row further down');
same(placed(list, 'x', 'b', false), ['a', 'x', 'b', 'c', 'd'], 'one the list does not hold is added');
same(placed(list, 'x', 'gone', true), ['a', 'b', 'c', 'd', 'x'], 'and lands at the end where the target went');
same(placed([], 'x', '', false), ['x'], 'and into a list that holds nothing at all it simply goes');
is(placed(list, 'b', 'b', false) === list, 'a rearrangement onto itself hands the list back as it is');
is(placed(list, 'b', 'c', false) === list, 'and so does one that changes nothing');
is(list.join('') === 'abcd', 'nothing above changed the list it was handed');

// ------------------------------------------------------------------ steps

/**
 * @param {string[]} from
 * @param {Array<{ name: string, direction: string }>} steps
 */
function walked(from, steps) {
	const cur = from.slice();
	for (const step of steps) {
		const at = cur.indexOf(step.name);
		const to = step.direction === 'up' ? at - 1 : at + 1;
		is(at !== -1, 'a step names a bouquet the order holds');
		is(to >= 0 && to < cur.length, 'and never off either end, which the box refuses');
		const swap = cur[to];
		cur[to] = cur[at];
		cur[at] = swap;
	}
	return cur;
}

same(stepsFor(['a', 'b', 'c'], ['a', 'b', 'c']), [], 'an order that did not change costs no writes');
same(stepsFor(['a', 'b', 'c'], ['c', 'a', 'b']), [{ name: 'c', direction: 'up' }, { name: 'c', direction: 'up' }],
	'the last brought to the front is two single moves');
same(walked(['a', 'b', 'c', 'd'], stepsFor(['a', 'b', 'c', 'd'], ['d', 'c', 'b', 'a'])), ['d', 'c', 'b', 'a'],
	'walking the steps of a reversal arrives at the reversal');
same(walked(['a', 'b', 'c', 'd', 'e'], stepsFor(['a', 'b', 'c', 'd', 'e'], ['b', 'e', 'a', 'd', 'c'])),
	['b', 'e', 'a', 'd', 'c'], 'and the steps of an arbitrary order arrive at that order');

// ---------------------------------------------------------------- gesture

const find = pageOf(['Gezeigt', 'Sport', 'Kinder'], ['c1', 'c2', 'c3']);

// A press on the handle that goes nowhere is a press and not a drag.
let drag = dragging(find);
drag.start(carryMember, { clientX: 260, clientY: 150 });
is(drag.held().moved === false, 'a gesture begins as a press');
drag.to({ clientX: 262, clientY: 152 }, 'Gezeigt');
is(drag.held().moved === false, 'two pixels of tremor is still a press');
is(drag.held().aim === null, 'and a press aims at nothing even over another row');
is(drag.drop() === null, 'letting go of a press moves nothing');
is(drag.held() === null, 'and the gesture is over');

// The same press carried far enough is a drag, and it lands.
drag = dragging(find);
drag.start(carryMember, { clientX: 260, clientY: 150 });
drag.to({ clientX: 262, clientY: 110 }, 'Gezeigt');
is(drag.held().moved === true, 'forty pixels is a drag');
same(drag.held().aim, { zone: 'members', id: 'c1', after: false },
	'over the upper half of the first member it lands in front of it');
drag.to({ clientX: 262, clientY: 130 }, 'Gezeigt');
same(drag.held().aim, { zone: 'members', id: 'c1', after: true },
	'and the aim follows the pointer within one row');
const landed = drag.drop();
same(landed && landed.aim, { zone: 'members', id: 'c1', after: true }, 'letting go answers where it landed');
same(placed(['c1', 'c2', 'c3'], landed.grip.id, landed.aim.id, landed.aim.after), ['c1', 'c2', 'c3'],
	'and behind the row above it is where it already was');

// Across the columns: out of the middle list onto another bouquet.
drag = dragging(find);
drag.start(carryMember, { clientX: 260, clientY: 150 });
drag.to({ clientX: 60, clientY: 150 }, 'Gezeigt');
same(drag.held().aim, { zone: 'handover', id: 'Sport', after: false },
	'carried across to the left column it aims at the bouquet under it');
is(drag.held().x === 60 && drag.held().y === 150, 'and the label it carries is where the pointer is');
const handed = drag.drop();
is(handed !== null && handed.aim.zone === 'handover', 'letting go there hands the channel over');

// Out of the pool into the middle list, which is the way a channel is added.
drag = dragging(find);
drag.start(carryPool, { clientX: 500, clientY: 300 });
drag.to({ clientX: 260, clientY: 185 }, 'Gezeigt');
same(drag.held().aim, { zone: 'members', id: 'c3', after: false }, 'out of the pool it aims into the middle list');
const added = drag.drop();
same(placed(['c1', 'c2', 'c3'], added.grip.id, added.aim.id, added.aim.after), ['c1', 'c2', 'c9', 'c3'],
	'and letting go adds it where the line was drawn');

/* Out of the pool into a half that holds nothing, which is the gesture that
   was not possible at all: the rows carried the only thing the document said a
   list with, so a half with no row was a half with nothing under the pointer,
   and a bouquet somebody had just made could be filled by the keyboard and by
   the button beside each channel and in no other way. */
const findNothing = pageWithNothingInTheMiddle(['Gezeigt', 'Sport', 'Kinder']);
drag = dragging(findNothing);
drag.start(carryPool, { clientX: 500, clientY: 300 });
drag.to({ clientX: 260, clientY: 150 }, 'Gezeigt');
is(drag.held().aim !== null, 'a half that holds nothing is a place a drag can be aimed at');
same(drag.held().aim, { zone: 'members', id: '', after: false },
	'and what it aims at is the half itself and not a row of it');
const filled = drag.drop();
is(filled !== null, 'letting go over it is a drop and not a gesture that ended nowhere');
/* Guarded, because a gesture that ended nowhere is exactly what this block is
   here to catch and a check that falls over instead of reporting is a check
   whose next assertions never run. */
const into = filled || { grip: { id: '' }, aim: { id: '', after: false } };
same(placed([], into.grip.id, into.aim.id, into.aim.after), ['c9'],
	'and the half that held nothing holds the channel that was let go over it');
same(placed(['c1'], into.grip.id, into.aim.id, into.aim.after), ['c1', 'c9'],
	'while one that filled up under the gesture takes it at the end');

// A gesture the browser took away, which is what a phone call does.
drag = dragging(find);
drag.start(carryPool, { clientX: 500, clientY: 300 });
drag.to({ clientX: 260, clientY: 185 }, 'Gezeigt');
drag.cancel();
is(drag.held() === null, 'a cancelled gesture is over');
is(drag.drop() === null, 'and letting go after it moves nothing');

// A drag that ends over nothing.
drag = dragging(find);
drag.start(carryMember, { clientX: 260, clientY: 150 });
drag.to({ clientX: 900, clientY: 900 }, 'Gezeigt');
is(drag.held().moved === true, 'a drag that left every list is still a drag');
is(drag.held().aim === null, 'aiming at nothing');
is(drag.drop() === null, 'and letting go there moves nothing');

// A move with no press before it, which is every pointer that crosses the page.
drag = dragging(find);
is(drag.to({ clientX: 260, clientY: 150 }, 'Gezeigt') === null, 'a pointer that is not down carries nothing');
is(drag.drop() === null, 'and letting it up moves nothing');

// ------------------------------------------------------------------ verdict

const FLOOR = 40;
if (checked < FLOOR) {
	process.stderr.write('drag-cases.mjs: only ' + checked + ' assertions ran, and there are more than ' + FLOOR + '\n');
	process.exit(1);
}
if (failed > 0) {
	process.stderr.write('drag-cases.mjs: ' + failed + ' of ' + checked + ' assertions failed\n');
	process.exit(1);
}
process.stdout.write('check-web-drag.sh: ' + checked + ' assertions over the pointer gesture, the arrangements and the single moves it saves as\n');
