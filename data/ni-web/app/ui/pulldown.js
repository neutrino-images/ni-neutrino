/* Pulling the one scrolling element of the page down to refresh it.

   THE BROWSER'S OWN PULL TO REFRESH CANNOT FIRE HERE. body carries overflow: hidden
   (app/css/shell.css) so that the bars stay put, and .content is what scrolls instead
   of the window. A gesture the browser owns needs the window to be the one scrolling,
   so what is below is a gesture of this page's own.

   ONLY AT THE TOP, ONLY DOWN, ONLY A FINGER. Touch events and not pointer events: a
   mouse never starts this, and only a non-passive touchmove can keep the browser from
   taking the drag as its own scroll. The drag has to begin with .content already at its own
   top: pulling down while there is more above to scroll to is scrolling, not this.

   AND NOT WHERE A GESTURE ALREADY BELONGS TO SOMETHING ELSE. A form control takes its
   own drag (a range input's thumb, dragged by the browser). touch-action: none on an
   element is the same page saying a gesture starting there is spoken for (the bouquet
   handle in screens/channels/bouquets.css, the float window's head and corner in
   ui/float.css), so this reads that property back off the element rather than naming
   those places again. A nested list scrolled away from its own top (the windowed list a
   bouquet's members are drawn in) is left alone the same way: what moves under a
   downward drag there is that list and not the page.

   PASSIVE UNTIL IT DECIDES. touchstart and touchend cost this nothing to leave
   passive. touchmove has to be able to call preventDefault, but only once a few
   pixels have settled which way the drag runs: a horizontal drag, or an upward one,
   is let go untouched and costs the browser nothing more than reading that one
   handler. */
import { html, useRef, useState, useCallback } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { refreshAll } from '../refresh.js';

// Elements that answer a touch themselves, whatever the CSS around them says.
const kOwnControls = 'input, select, textarea, button, a[href], [contenteditable="true"]';

// Pixels of travel before a direction is read at all, so a press with a
// finger's tremor in it is not mistaken for a drag one way or the other.
const kSlack = 10;
// How far, in the pixels actually shown, the pull has to reach before letting
// go refreshes rather than cancels.
const kThreshold = 48;
// The most the indicator is ever shown at, so a long drag does not pull the
// page open forever.
const kMaxShown = 84;
// What the raw finger travel is multiplied by below the maximum: a pull reads
// as lighter than the hand that makes it, the way every such gesture does.
const kResistance = 0.5;

/**
 * @param {Element} target where the drag began
 * @param {Element} content the element this gesture is attached to; the walk
 *        stops here without looking further, since content itself is what
 *        scrolls and owns nothing that would refuse the gesture
 * @returns {boolean} whether target or something between it and content has
 *          already claimed the gesture
 */
function spokenFor(target, content) {
	/** @type {Element | null} */
	let node = target;
	while (node && node !== content) {
		if (typeof node.matches === 'function' && node.matches(kOwnControls))
			return true;
		if (getComputedStyle(node).touchAction === 'none')
			return true;
		node = node.parentElement;
	}
	return false;
}

/**
 * @param {Element} target
 * @param {Element} content
 * @returns {boolean} whether a scrollable element between target and content
 *          is scrolled away from its own top
 */
function scrolledElsewhere(target, content) {
	/** @type {Element | null} */
	let node = target;
	while (node && node !== content) {
		if (node.scrollHeight > node.clientHeight + 1 && node.scrollTop > 0)
			return true;
		node = node.parentElement;
	}
	return false;
}

/**
 * @param {number} raw pixels of downward travel
 * @returns {number} pixels to show, resisted past the point a straight 1:1
 *          follow would feel like the page had come loose
 */
function shownFor(raw) {
	if (raw <= 0)
		return 0;
	const eased = raw * kResistance;
	return eased < kMaxShown ? eased : kMaxShown;
}

/**
 * The gesture, attached to the one element that scrolls. Returns what the
 * caller draws it with: a place to hang it (in the ref this hands the
 * element, in the style float.js hands its own slot a function rather than a
 * plain reference, so the listeners are bound the moment the node exists and
 * never again for a node that stays the same), how far it is pulled right
 * now, and whether letting go this instant would refresh.
 *
 * @returns {{ ref: (node: Element | null) => void, shown: number, armed: boolean, busy: boolean }}
 */
export function usePullToRefresh() {
	const [shown, setShown] = useState(0);
	const [armed, setArmed] = useState(false);
	const [busy, setBusy] = useState(false);
	/* What a live gesture is doing, and the two flags the handlers below read
	   at the moment a finger lifts, all in references and not in state: a
	   touchmove fires many times a second and none of those has to wait for
	   a draw before the next one is read, and armed and busy are set by one
	   handler and read by another that may run renders later. */
	const held = useRef(/** @type {{ id: number, y: number, x: number, tracking: boolean } | null} */ (null));
	const busyRef = useRef(false);
	const armedRef = useRef(false);
	const bound = useRef(/** @type {Element | null} */ (null));

	/** @param {boolean} value */
	function markArmed(value) {
		armedRef.current = value;
		setArmed(value);
	}

	/** @param {boolean} value */
	function markBusy(value) {
		busyRef.current = value;
		setBusy(value);
	}

	/**
	 * @param {Event} raw
	 * @returns {void}
	 */
	function down(raw) {
		const event = /** @type {TouchEvent} */ (raw);
		const content = bound.current;
		if (!content || busyRef.current || event.touches.length !== 1)
			return;
		if (content.scrollTop > 0)
			return;
		const target = /** @type {Element} */ (event.target);
		if (spokenFor(target, content) || scrolledElsewhere(target, content))
			return;
		const touch = event.touches[0];
		if (!touch)
			return;
		held.current = { id: touch.identifier, y: touch.clientY, x: touch.clientX, tracking: false };
	}

	/**
	 * @param {Event} raw
	 * @returns {void}
	 */
	function move(raw) {
		const event = /** @type {TouchEvent} */ (raw);
		const content = bound.current;
		const gesture = held.current;
		if (!content || !gesture)
			return;
		if (event.touches.length !== 1) {
			cancel();
			return;
		}
		const touch = event.touches[0];
		if (!touch)
			return;
		const dy = touch.clientY - gesture.y;
		const dx = touch.clientX - gesture.x;
		if (!gesture.tracking) {
			if (Math.abs(dy) < kSlack && Math.abs(dx) < kSlack)
				return;
			if (dy <= 0 || Math.abs(dx) > Math.abs(dy)) {
				held.current = null;
				return;
			}
			gesture.tracking = true;
		}
		if (content.scrollTop > 0) {
			cancel();
			return;
		}
		event.preventDefault();
		const next = shownFor(dy);
		setShown(next);
		markArmed(next >= kThreshold);
	}

	/** @returns {void} */
	function up() {
		const gesture = held.current;
		if (!gesture)
			return;
		held.current = null;
		if (gesture.tracking && armedRef.current) {
			markBusy(true);
			setShown(kThreshold);
			markArmed(false);
			refreshAll();
			// The indicator only has to say the pull was taken, not wait for
			// everything the refresh woke.
			setTimeout(function () {
				markBusy(false);
				setShown(0);
			}, 600);
		} else {
			setShown(0);
			markArmed(false);
		}
	}

	/** @returns {void} */
	function cancel() {
		held.current = null;
		markArmed(false);
		if (!busyRef.current)
			setShown(0);
	}

	/* Stable across every draw, so the listeners are bound once per node. */
	const attach = useCallback(function (/** @type {Element | null} */ node) {
		const previous = bound.current;
		if (previous) {
			previous.removeEventListener('touchstart', down);
			previous.removeEventListener('touchmove', move);
			previous.removeEventListener('touchend', up);
			previous.removeEventListener('touchcancel', cancel);
		}
		bound.current = node;
		if (node) {
			node.addEventListener('touchstart', down, { passive: true });
			node.addEventListener('touchmove', move, { passive: false });
			node.addEventListener('touchend', up, { passive: true });
			node.addEventListener('touchcancel', cancel, { passive: true });
		}
	}, []);

	return { ref: attach, shown: shown, armed: armed, busy: busy };
}

/**
 * The indicator itself: how far it has grown is the only thing drawn from a
 * gesture in progress, and no width or height of its own beyond that, so the
 * caller can place it as the first thing inside the element it is pulling.
 *
 * @param {{ shown: number, armed: boolean, busy: boolean }} props
 * @returns {Web.Drawn}
 */
export function PullIndicator(props) {
	if (props.shown <= 0 && !props.busy)
		return null;
	const label = props.busy ? t(text, 'shell.reloading')
		: props.armed ? t(text, 'shell.pull.release')
			: t(text, 'shell.pull.hint');
	return html`<div
		class="pull-indicator"
		data-armed=${props.armed ? '' : null}
		data-busy=${props.busy ? '' : null}
		style=${{ height: Math.max(props.shown, props.busy ? kThreshold : 0) + 'px' }}
		aria-hidden="true">
		<span class="pull-mark"></span>
		<span class="pull-label">${label}</span>
	</div>`;
}
