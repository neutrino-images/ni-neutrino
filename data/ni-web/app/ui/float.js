/* The source that goes on playing over whatever screen somebody walked to.

   WHAT THIS DRAWS IS A PLACE AND NEVER THE ELEMENT. The element belongs to
   app/ui/playing.js and outlives every screen; this offers it the slot named
   float and takes the offer back when it goes away. It draws nothing at all
   while the screen that plays the source has a slot of its own.

   AND IT SAYS WHICH OF THE TWO IT IS HOLDING. A channel of the box and a file on
   a disk of it are not equally expensive to forget: a channel holds a tuner and
   one of the two sounds the box can convert, and a window left standing takes
   both from whoever wants them next, while a file costs one open request. The
   name alone does not separate them, because a recording of this box is named
   after the channel it came off, so a window saying nothing else would read the
   same in both cases. One word does separate them, and it is written for both
   and not only for the expensive one: a mark that appears half the time is a
   mark nobody can read.

   TWO FORMS AND ONE THRESHOLD. Wide it is a window somebody can carry about and
   pull open; narrow it is a bar across the foot of the frame with no gesture on
   it. The threshold is the width at which the destinations leave the header for
   the bar along the bottom (app/css/shell.css), and it is the same number
   because it is the same judgement: above it there is room beside the reading,
   below it the whole width is the reading.

   Stated here as a media query and nowhere else. The form goes into an attribute
   and the stylesheet follows the attribute.

   AND THE BAR IS A ROW OF THE FRAME AND NOT AN OVERLAY. A bar fixed to the foot
   of the window would lie over the destinations, and holding it clear of them
   would mean writing down the height of that bar a second time. In the flow it
   stands above them and the reading shortens by what it takes.

   NOTHING HERE IS REMEMBERED PAST A RELOAD. */

import { html, useState, useEffect, useRef } from '../runtime.js';
import { ensureCss } from '../css.js';
import { t } from '../i18n.js';
import text from './float.text.js';
import * as playing from './playing.js';

const kCss = '/app/ui/float.css';

/* The width at which the frame moves its destinations into the bar along the
   bottom. Above it this floats, below it this is a bar of its own. */
const kFloats = '(min-width: 1152px)';

/* How small it may be pulled and how big it opens. Two of each, because a
   window around a picture and a window around a row of sound controls are not
   the same shape, and one least height would leave one of the two mostly
   empty. Both are handed to the arithmetic in playing.js as the least, so
   there is one rule about rooms and two sets of numbers for it. */
const kLeastPicture = { w: 240, h: 176 };
const kLeastSound = { w: 240, h: 92 };
const kFirstPicture = { w: 448, h: 284 };
const kFirstSound = { w: 340, h: 92 };

// How far in from the corner it opens, and how far one press of an arrow key
// moves or pulls it.
const kEdge = 16;
const kStep = 16;

/**
 * @returns {{w: number, h: number}} the room a window fixed to the viewport has
 */
function roomNow() {
	return { w: window.innerWidth, h: window.innerHeight };
}

/**
 * @param {boolean} sound
 * @returns {{w: number, h: number}}
 */
function leastFor(sound) {
	return sound ? kLeastSound : kLeastPicture;
}

/**
 * Where the window stands before anybody has moved it: against the far corner
 * with a gap, which is out of the way of the header and of the reading both.
 *
 * @param {boolean} sound
 * @param {{w: number, h: number}} room
 * @returns {{x:number,y:number,w:number,h:number}}
 */
function corner(sound, room) {
	const first = sound ? kFirstSound : kFirstPicture;
	return playing.clamped({
		x: room.w - first.w - kEdge,
		y: room.h - first.h - kEdge,
		w: first.w,
		h: first.h,
	}, room, leastFor(sound));
}

/* THE SLOT IS OFFERED BY A FUNCTION THAT DOES NOT CHANGE FROM DRAW TO DRAW,
   which is why this stands outside the component. A reference the runtime takes
   for a new one is one it withdraws and hands out again on every draw, and
   withdrawing it moves the element into the waiting room and back for nothing. */
/**
 * @param {Element | null} node
 * @returns {void}
 */
function hold(node) {
	playing.setSlot('float', node);
}

/**
 * What a gesture is holding: which of the two it is, whose pointer, where it
 * began and what the window looked like then. Measured from the beginning of the
 * gesture and never from the last event, so a window held against an edge does
 * not creep away from the hand that is still pushing at it.
 *
 * @typedef {object} Gesture
 * @property {'move' | 'size'} how
 * @property {number} id
 * @property {number} x
 * @property {number} y
 * @property {{x:number,y:number,w:number,h:number}} from
 */

/**
 * @returns {Web.Drawn}
 */
export function Float() {
	const [now, setNow] = useState(playing.current());
	const [wide, setWide] = useState(window.matchMedia(kFloats).matches);
	const [room, setRoom] = useState(roomNow());
	/* Null until somebody has taken hold of it, which is what keeps a window
	   nobody has touched in the corner as the room around it changes. */
	const [box, setBox] = useState(/** @type {{x:number,y:number,w:number,h:number} | null} */ (null));
	const [styled, setStyled] = useState(false);
	/* In a reference and not in state: what a gesture began with changes
	   nothing on screen, and a draw between the press and the first move would
	   lose the press. */
	const held = useRef(/** @type {Gesture | null} */ (null));

	useEffect(function () {
		/* Read once as well as subscribed to, because something may have
		   started between this being drawn and this effect running. */
		setNow(playing.current());
		return playing.subscribe(setNow);
	}, []);

	useEffect(function () {
		const query = window.matchMedia(kFloats);
		function seen() {
			setWide(query.matches);
		}
		query.addEventListener('change', seen);
		return function () { query.removeEventListener('change', seen); };
	}, []);

	const sound = !!(now && now.source.sound);

	useEffect(function () {
		function measured() {
			const next = roomNow();
			setRoom(next);
			/* A window somebody placed is held inside what room is left, and
			   one nobody has touched stays null so that it goes on standing in
			   the corner wherever the corner has moved to. A telephone turned
			   on its side and a window dragged narrow are the same thing from
			   here. */
			setBox(function (was) {
				return was === null ? null : playing.clamped(was, next, leastFor(sound));
			});
		}
		window.addEventListener('resize', measured);
		return function () { window.removeEventListener('resize', measured); };
	}, [sound]);

	/* Fetched when something starts playing and not when the page loads, and
	   waited for before anything is drawn: a window drawn before its
	   stylesheet has arrived is a window drawn twice, once in the wrong place. */
	useEffect(function () {
		if (now === null || styled) {
			return undefined;
		}
		let alive = true;
		ensureCss(kCss).then(function () {
			if (alive) {
				setStyled(true);
			}
		});
		return function () { alive = false; };
	}, [now === null, styled]);

	/* Out of what was delivered and not asked of the module here: this draws
	   again when what is playing changes, and which slot carries the element
	   is part of that. Asked here instead, the answer would be read once and
	   never again on the draw that never happened. */
	if (now === null || !styled || now.slot === 'page') {
		return null;
	}

	const at = box || corner(sound, room);
	const least = leastFor(sound);
	const name = now.source.name;

	/**
	 * @param {PointerEvent} event
	 * @param {'move' | 'size'} how
	 * @returns {void}
	 */
	function grab(event, how) {
		if (!wide || event.button > 0) {
			return;
		}
		/* The close button sits on the surface the window is carried by, so a
		   press on it is a press and not the beginning of a drag. */
		const on = /** @type {Element | null} */ (event.target);
		if (how === 'move' && on && typeof on.closest === 'function' && on.closest('.fp-shut')) {
			return;
		}
		const surface = /** @type {Element & { setPointerCapture?: (id: number) => void }} */ (event.currentTarget);
		if (surface && typeof surface.setPointerCapture === 'function') {
			surface.setPointerCapture(event.pointerId);
		}
		held.current = { how: how, id: event.pointerId, x: event.clientX, y: event.clientY, from: at };
		/* Or a finger scrolls the page under the window instead of moving it,
		   and a mouse selects the words in the head. The stylesheet says the
		   same thing to the touch machinery with touch-action, which decides
		   before any of this runs. */
		event.preventDefault();
	}

	/**
	 * @param {PointerEvent} event
	 * @returns {void}
	 */
	function slide(event) {
		const gesture = held.current;
		if (!gesture || gesture.id !== event.pointerId) {
			return;
		}
		const dx = event.clientX - gesture.x;
		const dy = event.clientY - gesture.y;
		const from = gesture.from;
		if (gesture.how === 'move') {
			setBox(playing.clamped({ x: from.x + dx, y: from.y + dy, w: from.w, h: from.h }, room, least));
			return;
		}
		/* The far corner is what the hand has hold of, so the near one is held
		   still and what the room refuses comes off the size. A different sum
		   from the one above, and it lives beside it in playing.js. */
		setBox(playing.sized({ x: from.x, y: from.y, w: from.w + dx, h: from.h + dy }, room, least));
	}

	/**
	 * @param {PointerEvent} event
	 * @returns {void}
	 */
	function letGo(event) {
		const gesture = held.current;
		if (!gesture || gesture.id !== event.pointerId) {
			return;
		}
		held.current = null;
		const surface = /** @type {Element & {
			releasePointerCapture?: (id: number) => void,
			hasPointerCapture?: (id: number) => boolean
		}} */ (event.currentTarget);
		if (surface && typeof surface.releasePointerCapture === 'function'
			&& typeof surface.hasPointerCapture === 'function'
			&& surface.hasPointerCapture(event.pointerId)) {
			surface.releasePointerCapture(event.pointerId);
		}
	}

	/* THE SAME TWO GESTURES WITHOUT A POINTER. Both go through the same two
	   sums, so a window driven from a keyboard cannot come to rest anywhere a
	   dragged one could not. */
	/**
	 * @param {KeyboardEvent} event
	 * @param {'move' | 'size'} how what the arrows do here, which is what the
	 *        control they are pressed on is for
	 * @returns {void}
	 */
	function bumped(event, how) {
		let dx = 0;
		let dy = 0;
		if (event.key === 'ArrowLeft') dx = -kStep;
		else if (event.key === 'ArrowRight') dx = kStep;
		else if (event.key === 'ArrowUp') dy = -kStep;
		else if (event.key === 'ArrowDown') dy = kStep;
		else return;
		event.preventDefault();
		if (how === 'size') {
			setBox(playing.sized({ x: at.x, y: at.y, w: at.w + dx, h: at.h + dy }, room, least));
			return;
		}
		setBox(playing.clamped({ x: at.x + dx, y: at.y + dy, w: at.w, h: at.h }, room, least));
	}

	const said = now.phase === 'failed' ? t(text, 'float.failed')
		: now.phase === 'loading' ? t(text, 'float.loading')
			: '';

	/* Left, top, width and height, and not a corner and a size: the window is
	   placed from the near corner, which is the corner both sums hold still. */
	const placed = wide
		? { left: at.x + 'px', top: at.y + 'px', width: at.w + 'px', height: at.h + 'px' }
		: null;

	/* The place for the element comes first in the markup at both widths and
	   the head follows it, so that the node the element lives in is the same
	   node before and after the threshold is crossed. Which of the two stands
	   first on screen is the stylesheet's business and not this one's. */
	return html`<div
		class="fp"
		data-form=${wide ? 'window' : 'bar'}
		data-sound=${sound ? 'one' : null}
		data-phase=${now.phase}
		style=${placed}
		role="region"
		aria-label=${t(text, 'float.label', { name: name })}>
		<div class="fp-media" ref=${hold}></div>
		<div
			class="fp-head"
			onPointerDown=${function (/** @type {PointerEvent} */ e) { grab(e, 'move'); }}
			onPointerMove=${slide}
			onPointerUp=${letGo}
			onPointerCancel=${letGo}>
			${wide ? html`<button
				type="button"
				class="fp-grip"
				aria-label=${t(text, 'float.move')}
				title=${t(text, 'float.keys')}
				onKeyDown=${function (/** @type {KeyboardEvent} */ e) { bumped(e, 'move'); }}>⠿</button>` : null}
			<span class="fp-kind">${t(text, 'float.kind.' + now.source.of)}</span>
			<span class="fp-name">${name}</span>
			${said === '' ? null : html`<span class="fp-said" role="status">${said}</span>`}
			<button
				type="button"
				class="fp-shut"
				aria-label=${t(text, 'float.close')}
				title=${t(text, 'float.close')}
				onClick=${function () { playing.stop(); }}>✕</button>
		</div>
		${wide ? html`<button
			type="button"
			class="fp-grab"
			aria-label=${t(text, 'float.size')}
			title=${t(text, 'float.keys')}
			onPointerDown=${function (/** @type {PointerEvent} */ e) { grab(e, 'size'); }}
			onPointerMove=${slide}
			onPointerUp=${letGo}
			onPointerCancel=${letGo}
			onKeyDown=${function (/** @type {KeyboardEvent} */ e) { bumped(e, 'size'); }}></button>` : null}
	</div>`;
}
