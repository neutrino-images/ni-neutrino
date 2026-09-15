/* Dragging one row onto another, with a mouse, a finger or a pen.
 *
 * WHY THIS IS NOT THE BROWSER'S OWN DRAG. draggable, dragstart, dragover and drop
 * are a mouse protocol: on a touch screen they never fire at all, so the editor
 * this replaces could be operated with a mouse and with a keyboard and in no
 * other way. Pointer events are the one set every input raises.
 *
 * THE GESTURE STARTS ON THE HANDLE AND NOWHERE ELSE. A list whose whole row begins
 * a drag is a list nobody can scroll on a telephone. So the handle carries
 * touch-action: none and takes the gesture, and the row around it carries pan-y
 * and keeps the scroll.
 *
 * AND IT IS THE SECOND WAY AND NEVER THE ONLY ONE. Everything below can also be
 * done with the keyboard, on the same rows.
 *
 * Nothing in this file reaches the document except spotFromPoint, which is handed
 * to the machine below rather than called by it, so the gesture can be driven
 * without a browser and is (test/web/check-web-drag.sh).
 */

/**
 * What was taken hold of.
 *
 * @typedef {object} Grip
 * @property {'bouquet' | 'member' | 'pool'} kind
 * @property {string} id a bouquet's name, or a channel's identifier
 * @property {string} label what the thing being carried is called
 */

/**
 * A row under the pointer, as the document describes it.
 *
 * @typedef {object} Spot
 * @property {'bouquets' | 'members'} zone
 * @property {string} id which row, or nothing at all for a list that draws no
 *           row and offers the place they would be in instead
 * @property {number} top
 * @property {number} height
 */

/**
 * Where letting go now would put it.
 *
 * @typedef {object} Aim
 * @property {'bouquets' | 'members' | 'handover'} zone
 * @property {string} id
 * @property {boolean} after whether it lands behind that row rather than in
 *           front of it, which a drop onto a whole bouquet and a drop into a
 *           list that holds nothing have no meaning for
 */

/**
 * The one thing a pointer event is read for.
 *
 * @typedef {object} At
 * @property {number} clientX
 * @property {number} clientY
 */

/* How far the pointer travels before this is a drag rather than a press. A
   tap on the handle would otherwise pick the row up and put it back down
   somewhere else by a pixel of hand tremor. */
const SLACK = 4;

/**
 * Where a drag would land, out of what it is carrying and what is under it.
 *
 * @param {Grip} grip
 * @param {Spot | null} spot
 * @param {number} y where the pointer is, so that the upper half of a row
 *        means in front of it and the lower half behind it
 * @param {string} selected which bouquet the middle list is showing
 * @returns {Aim | null}
 */
export function aimFor(grip, spot, y, selected) {
	if (!spot)
		return null;
	const after = y > spot.top + spot.height / 2;

	if (grip.kind === 'bouquet') {
		// A bouquet moves in its own list and nowhere else.
		if (spot.zone !== 'bouquets' || spot.id === grip.id)
			return null;
		return { zone: 'bouquets', id: spot.id, after: after };
	}

	if (spot.zone === 'bouquets') {
		/* Onto a bouquet, which means into it. The one it is already showing
		   is not a destination: the middle list is that bouquet, and a channel
		   dropped on its own name would be asked to move to where it is. */
		if (spot.id === selected)
			return null;
		return { zone: 'handover', id: spot.id, after: false };
	}

	if (spot.id === grip.id)
		return null;
	/* A list with no row in it answers as one place rather than as a row, and
	   names nothing, so there is no in front of it and no behind it either. */
	return { zone: 'members', id: spot.id, after: spot.id === '' ? false : after };
}

/**
 * One list with an entry put somewhere else in it, as a new list.
 *
 * An entry the list does not hold is added, which is what dragging out of the pool
 * means, and one it does hold is moved. A rearrangement that changes nothing hands
 * the list back as it is.
 *
 * @param {readonly string[]} list
 * @param {string} id
 * @param {string} target the entry it is being put beside
 * @param {boolean} after
 * @returns {readonly string[]}
 */
export function placed(list, id, target, after) {
	if (id === target)
		return list;
	/** @type {string[]} */
	const out = [];
	for (const one of list) {
		if (one !== id)
			out.push(one);
	}
	const at = out.indexOf(target);
	// A target that is not in the list any more, which is what a list changing
	// under a gesture looks like. The end is the one place that is always
	// there.
	out.splice(at === -1 ? out.length : (after ? at + 1 : at), 0, id);

	if (out.length === list.length) {
		let same = true;
		for (let i = 0; i < out.length && same; i++)
			same = out[i] === list[i];
		if (same)
			return list;
	}
	return out;
}

/**
 * The single moves that carry one order into another.
 *
 * The box moves a bouquet one place at a time and refuses at either end
 * (src/httpd/ep/ep_channels.cpp), so an order somebody arranged in one gesture
 * reaches it as a sequence of those. Worked out here rather than sent as the
 * gesture happens, because what is on screen is a draft and a draft that had
 * already been sent is not one.
 *
 * Only upward moves are produced, and that is not a simplification: taking
 * each name in turn to the place it is wanted at, from the front, moves
 * everything below it down by exactly as much as it needs. A run of downward
 * moves would be the same list read backwards.
 *
 * @param {readonly string[]} from the order the box holds
 * @param {readonly string[]} to the order that is wanted
 * @returns {Array<{ name: string, direction: 'up' | 'down' }>}
 */
export function stepsFor(from, to) {
	const cur = from.slice();
	/** @type {Array<{ name: string, direction: 'up' | 'down' }>} */
	const steps = [];
	for (let want = 0; want < to.length; want++) {
		const name = to[want];
		if (name === undefined)
			continue;
		let at = cur.indexOf(name);
		if (at === -1)
			continue;
		while (at > want) {
			steps.push({ name: name, direction: 'up' });
			const above = cur[at - 1];
			if (above === undefined)
				break;
			cur[at] = above;
			cur[at - 1] = name;
			at--;
		}
	}
	return steps;
}

/**
 * What is being carried and where it is, for as long as a pointer is down.
 *
 * @typedef {object} Carry
 * @property {Grip} grip
 * @property {number} x where the pointer is now, so the label follows it
 * @property {number} y
 * @property {boolean} moved whether the pointer has travelled far enough for
 *           this to be a drag rather than a press
 * @property {Aim | null} aim
 */

/**
 * The gesture, as a machine that is handed pointer events.
 *
 * @param {(x: number, y: number) => Spot | null} find what is under a point
 * @returns {{
 *   start: (grip: Grip, at: At) => Carry,
 *   to: (at: At, selected: string) => Carry | null,
 *   held: () => Carry | null,
 *   drop: () => { grip: Grip, aim: Aim } | null,
 *   cancel: () => void
 * }}
 */
export function dragging(find) {
	/** @type {Carry | null} */
	let live = null;
	/** @type {{ x: number, y: number }} */
	let from = { x: 0, y: 0 };

	return {
		start: function (grip, at) {
			from = { x: at.clientX, y: at.clientY };
			live = { grip: grip, x: at.clientX, y: at.clientY, moved: false, aim: null };
			return live;
		},

		to: function (at, selected) {
			if (!live)
				return null;
			const far = Math.abs(at.clientX - from.x) > SLACK || Math.abs(at.clientY - from.y) > SLACK;
			const moved = live.moved || far;
			live = {
				grip: live.grip,
				x: at.clientX,
				y: at.clientY,
				moved: moved,
				// Nothing is aimed at until it is a drag, so that a press that
				// happens to sit over another row does not draw a drop line.
				aim: moved ? aimFor(live.grip, find(at.clientX, at.clientY), at.clientY, selected) : null
			};
			return live;
		},

		held: function () {
			return live;
		},

		drop: function () {
			const done = live;
			live = null;
			if (!done || !done.moved || !done.aim)
				return null;
			return { grip: done.grip, aim: done.aim };
		},

		cancel: function () {
			live = null;
		}
	};
}

/**
 * The row under a point, read off the document.
 *
 * The rows say what they are with two attributes rather than being found by
 * their classes, so that what this reads and what the screen writes are one
 * pair of names and a class renamed for the look of it cannot break the
 * gesture.
 *
 * @param {number} x
 * @param {number} y
 * @returns {Spot | null}
 */
export function spotFromPoint(x, y) {
	const under = document.elementFromPoint(x, y);
	if (!under)
		return null;
	const row = under.closest('[data-drop-zone]');
	if (!row)
		return null;
	const zone = row.getAttribute('data-drop-zone');
	if (zone !== 'bouquets' && zone !== 'members')
		return null;
	const box = row.getBoundingClientRect();
	return { zone: zone, id: row.getAttribute('data-drop-id') || '', top: box.top, height: box.height };
}
