// The remote control, with the picture of what it is driving beside it.
//
// ONE SCREEN AND NOT TWO. The keys and the television picture were two entries of
// this destination, and pressing a key on one while the other was not on screen
// is flying blind.
//
// AND ON A TELEPHONE THE PICTURE IS PINNED AND NOTHING ELSE IS. Seeing what the
// box is drawing is half of what this screen was joined up for, so over the keys
// the picture stays at the head of the reading and every key passes under it.
// Only the picture, and under it the one line that answers a press: a heading, a
// card's edge, a button and a block of five keys pinned with it came to two
// thirds of the reading, and what is under the block is the whole of what
// somebody came here to press. Measured at 390 by 874: pinned was 429 of 652 and
// is 162.
//
// THE KEYS ARE ALL IN ONE PLACE AND THE CROSS IS ONE OF THEM. Taking five of them
// out of the list and standing them over the rest bought a cross without a scroll
// and paid for it in every other key, and it left the cross reading as a second
// way in rather than as the group the list already has.
//
// WHEN THE PICTURE IS FETCHED AGAIN, which is the whole of what this had to
// decide. A capture is real work for a box with an ARM11, so a fresh one after
// every press would punish somebody walking down a menu. A press asks for one and
// the ask is held back until the presses stop, so a run of five is one capture;
// and two captures never come closer together than the floor below, so holding a
// key down cannot turn into a stream of them. One is spent on opening.
//
// The keys are the ones this box names and no others: the route that lists them
// is the truth about what the box will act on, and a layout copied from somebody
// else's handset is a promise about keys that may not exist here. The box names a
// few of them twice, so the list is read as a set.
//
// The keys are also worked with a keyboard. The arrows reach the direction keys,
// the digits reach the digits and the enter key reaches OK.
import { html, useState, useEffect, useLayoutEffect, useRef, route } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import text from './now.text.js';
import icons from './remote.icons.js';
import { hrefFor } from '../../nav.js';
import { Switch } from '../../ui/switch.js';
import { toast } from '../../ui/toast.js';
import { refusalLine } from '../../problem.js';
import { fold } from '../../ui/kept.js';
import { Card, useResource } from './parts.js';
import { useCapture } from './screenshot.js';
import { Dialog } from '../../ui/dialog.js';
import shell from '../../shell.text.js';

export const css = '/app/screens/now/now.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'now.remote.head'); }

/* The order the keys are laid out in, and the only thing on this screen that is
   written down rather than read off the box. A group names keys it would like;
   what is drawn is the ones the box also names, so a box without colour keys has
   no colour row and a box with a key nobody here thought of still gets it, at the
   end, under the last heading.

   A shape is the arrangement the group is worth having. Empty means the row
   everything else falls into. */
const GROUPS = [
	{ id: 'top', shape: 'now-keys-top',
	  keys: ['KEY_POWER', 'KEY_MUTE'] },
	{ id: 'control', shape: 'now-keys-control',
	  keys: ['KEY_TV', 'KEY_RADIO', 'KEY_PVR', 'KEY_FAVORITES', 'KEY_TIME', 'KEY_SLEEP'] },
	{ id: 'digits', shape: 'now-keys-digits',
	  keys: ['KEY_VOLUMEUP'  , 'KEY_1', 'KEY_2', 'KEY_3', 'KEY_PAGEUP',
		                   'KEY_4', 'KEY_5', 'KEY_6',
		 'KEY_VOLUMEDOWN', 'KEY_7', 'KEY_8', 'KEY_9', 'KEY_PAGEDOWN',
		            'KEY_PREVIOUS', 'KEY_0', 'KEY_NEXT'] },
	{ id: 'cross', shape: 'now-keys-cross',
	  keys: ['KEY_MENU', 'KEY_UP', 'KEY_HOME',
		 'KEY_LEFT', 'KEY_OK', 'KEY_RIGHT',
		 'KEY_EPG', 'KEY_DOWN', 'KEY_INFO'] },
	{ id: 'colour', shape: 'now-keys-colour',
	  keys: ['KEY_RED', 'KEY_GREEN', 'KEY_YELLOW', 'KEY_BLUE'] },
	{ id: 'transport', shape: 'now-keys-transport',
	  keys: ['KEY_PLAY', 'KEY_PAUSE', 'KEY_STOP', 'KEY_RECORD',
		 'KEY_REWIND', 'KEY_FORWARD', 'KEY_PREVIOUSSONG', 'KEY_NEXTSONG'] },
	{ id: 'bottom', shape: 'now-keys-bottom',
	  keys: ['KEY_SUBTITLE', 'KEY_TEXT', 'KEY_AUDIO', 'KEY_VIDEO', 'KEY_SAT', 'KEY_WWW', 'KEY_HELP'] },
];

/* What a key on the keyboard stands for. Only the three the plan of this
   screen rests on: the cross, the digits and the one that means OK. Anything
   else typed here belongs to the browser, and a name that is not in here reads
   as nothing rather than as a key. */
/** @type {Record<string, string | undefined>} */
const FROM_KEYBOARD = {
	ArrowUp: 'KEY_UP',
	ArrowDown: 'KEY_DOWN',
	ArrowLeft: 'KEY_LEFT',
	ArrowRight: 'KEY_RIGHT',
	Enter: 'KEY_OK',
	0: 'KEY_0', 1: 'KEY_1', 2: 'KEY_2', 3: 'KEY_3', 4: 'KEY_4',
	5: 'KEY_5', 6: 'KEY_6', 7: 'KEY_7', 8: 'KEY_8', 9: 'KEY_9',
};

/* THE WIDTH AT WHICH THE PICTURE STANDS BESIDE THE KEYS RATHER THAN OVER THEM.

   Nine hundred and not the width at which the frame moves its destinations into
   the bar along the bottom. That one is 1152, and a tablet held sideways is under
   it with room for both columns. Nor 600, which is where this page starts drawing
   for a finger: a tablet held upright is over that and has one column.

   Stated as a media query and read by the stylesheet as the same number, which is
   the one duplication here worth having: the layout has to know it in CSS and the
   drawing has to know it in JavaScript. */
const BESIDE = '(min-width: 900px)';

/**
 * Whether the picture stands beside the keys rather than over them.
 *
 * Watched and not read once. A telephone turned on its side crosses this in the
 * middle of a press.
 *
 * @returns {boolean}
 */
function useBeside() {
	const [beside, setBeside] = useState(window.matchMedia(BESIDE).matches);

	useEffect(function () {
		const query = window.matchMedia(BESIDE);
		function seen() {
			setBeside(query.matches);
		}
		query.addEventListener('change', seen);
		return function () { query.removeEventListener('change', seen); };
	}, []);

	return beside;
}

/**
 * The keys the box named, in the order this screen lays them out, with everything
 * it did not expect gathered at the end.
 *
 * Every name the box gave appears exactly once. That is the property worth
 * having: a name dropped because no group claimed it is a key nobody can press
 * from here and nothing would report it.
 *
 * @param {string[]} named
 * @returns {{ id: string, shape: string, keys: string[] }[]}
 */
export function groupsFor(named) {
	const left = new Set(named);
	const out = [];

	for (const group of GROUPS) {
		const keys = [];
		for (const key of group.keys) {
			if (left.has(key)) {
				keys.push(key);
				left.delete(key);
			}
		}
		if (keys.length > 0) {
			out.push({ id: group.id, shape: group.shape, keys: keys });
		}
	}

	const rest = [];
	for (const key of named) {
		if (left.has(key)) {
			rest.push(key);
			left.delete(key);
		}
	}
	if (rest.length > 0) {
		out.push({ id: 'rest', shape: '', keys: rest });
	}

	return out;
}

/**
 * What a key is called on screen: the word this page has for it, and the name
 * the box gave with its prefix taken off where it has none.
 *
 * @param {string} name
 * @returns {string}
 */
export function labelFor(name) {
	const key = 'now.key.' + name;
	const said = t(text, key);
	if (said !== key) {
		return said;
	}
	return name.indexOf('KEY_') === 0 ? name.slice(4) : name;
}

/**
 * What a key is displayed on screen: the icon this page has for it.
 *
 * @param {string} name
 * @returns {string}
 */
function iconFor(name) {
	return icons[name] || null;
}

/**
 * One group of keys, as the buttons that press them.
 *
 * @param {{ id: string, shape: string, keys: string[] }} group
 * @param {(key: string) => void} onPress
 * @param {Web.Drawn} [also] a control of this page's own that stands in the
 *   group's row without being a key of the box
 * @returns {Web.Drawn}
 */
function keysOf(group, onPress, also) {
	return html`<div class=${group.shape === '' ? 'now-keyset' : 'now-keyset ' + group.shape}>
		${also || null}
		${group.keys.map(function (name) {
			return html`<button
				key=${name}
				type="button"
				class=${'now-key now-key-' + name}
				title=${name}
				aria-label=${labelFor(name)}
				onClick=${function () { onPress(name); }}>${iconFor(name) || html`<span class="now-key-label">${labelFor(name)}</span>`}</button>`;
		})}
	</div>`;
}

/* THE LETTERING ON THE KEYS IS ONE SIZE, AND IT IS THE SIZE THE LONGEST NAME
   FITS AT. The keys are as wide as their column and the columns are as wide as
   the screen allows, so a name that fits on a desk is cut off on a telephone:
   AUDIO in a row of seven, at 390 wide, is a pixel wider than its key. Each key
   shrinking its own name until it fits was the other way, and it makes a row
   of keys in four sizes, which reads as four kinds of key.

   So the names are measured where they stand, the one that has least room sets
   the scale, and every key is given the same size. Measured and not worked out
   from the number of columns and the number of letters, because the font is
   whatever monospace the machine has and the names are whatever the language
   calls them.

   The air is what is left between a name and the edge of its key, and the
   floor is the size below which a name is no longer read; under it the name
   is allowed to touch the edges rather than become a smudge. The keys no group
   claimed are not measured: they lie in a row that wraps, as wide as their
   names, so they always fit and would only ever say "smaller". */
const KEY_AIR_PX = 4;
const KEY_LEAST_PX = 8;

/**
 * Sets the one size of lettering every key on the handset is drawn at.
 *
 * Starts from the stylesheet's own size every time, so a window that grows
 * gives the lettering back as well as taking it away. The size of a name is in
 * proportion to its width, so one measurement is the answer and nothing is
 * tried twice.
 *
 * @param {HTMLElement} box
 * @returns {void}
 */
function fitLettering(box) {
	box.style.removeProperty('--now-key-fs');
	let scale = 1;
	let size = 0;
	for (const label of box.querySelectorAll('.now-keygroup:not(.now-keyrest) span.now-key-label')) {
		const key = label.parentElement;
		const wide = label.getBoundingClientRect().width;
		if (key === null || key.clientWidth === 0 || wide === 0) {
			continue;
		}
		if (size === 0) {
			size = parseFloat(window.getComputedStyle(label).fontSize);
		}
		scale = Math.min(scale, (key.clientWidth - KEY_AIR_PX) / wide);
	}
	if (scale < 1 && size > 0) {
		// Rounded down to a quarter pixel, so rounding never puts it back over.
		const fitted = Math.floor(size * scale * 4) / 4;
		box.style.setProperty('--now-key-fs', Math.max(KEY_LEAST_PX, fitted) + 'px');
	}
}

/* How long the presses have to stop before the picture is fetched again, and
   how close together two captures may ever come. The first is what makes a run
   of presses one capture; the second is what keeps a held key from becoming a
   stream of them. Both are guesses at what a hand does and neither is measured
   off anything, which is why they are here under names rather than inside the
   arithmetic. */
const SETTLE_MS = 700;
const LEAST_GAP_MS = 1500;

/**
 * @param {{ param?: string }} props
 * @returns {Web.Drawn}
 */
export default function Remote(props) {
	const keys = useResource('GET', '/api/v1/osd/remote/keys');
	const lock = useResource('GET', '/api/v1/osd/remote');
	/* Asked so that the power key knows which way it is about to send the box.
	   The frame asks for the same address, so this is answered from the store
	   and costs no second request. */
	const mode = useResource('GET', '/api/v1/system/standby');
	const [last, setLast] = useState('');
	const [askingPower, setAskingPower] = useState(false);
	// The address is the truth: /now/remote/control is the handset full screen.
	const full = props.param === 'control';
	const pushed = useRef(false);
	const beside = useBeside();
	/* One on opening, and one more for every run of presses that has come to
	   an end. The picture reads this and nothing else. */
	const [bump, setBump] = useState(1);
	const waiting = useRef(0);
	const fetched = useRef(0);
	const shot = useCapture(bump);
	const named = (keys.data && keys.data.items) || [];
	const groups = groupsFor(named);
	const pad = useRef(/** @type {HTMLDivElement | null} */ (null));
	/* What the lettering is fitted to: the names as they are written, so a
	   box that names other keys, or a page switched to another language, is
	   measured again. */
	const lettering = named.map(labelFor).join(' ');

	/* Fitted before the keys are first painted, and again whenever the width
	   they are drawn in changes: a telephone turned, a window dragged, the
	   handset taken full screen. Only the width. The lettering decides the
	   height of the keys and not their width, so a change of height is this
	   fitting's own doing and answering it would be chasing itself. */
	useLayoutEffect(function () {
		const box = pad.current;
		if (box === null) {
			return undefined;
		}
		let width = -1;
		function measure() {
			if (box === null || box.clientWidth === width) {
				return;
			}
			width = box.clientWidth;
			fitLettering(box);
		}
		measure();
		const watch = new ResizeObserver(measure);
		watch.observe(box);
		return function () { watch.disconnect(); };
	}, [lettering]);

	useEffect(function () {
		return function () {
			if (waiting.current) {
				window.clearTimeout(waiting.current);
				waiting.current = 0;
			}
		};
	}, []);

	/* Held back until the presses stop, and then no sooner than the floor
	   allows. Restarting the wait on every press is what turns a run of them
	   into one capture. */
	function askForPicture() {
		if (waiting.current) {
			window.clearTimeout(waiting.current);
		}
		const since = Date.now() - fetched.current;
		const wait = Math.max(SETTLE_MS, LEAST_GAP_MS - since);
		waiting.current = window.setTimeout(function () {
			waiting.current = 0;
			fetched.current = Date.now();
			setBump(function (n) { return n + 1; });
		}, wait);
	}

	/**
	 * @param {string} name
	 * @returns {void}
	 */
	function send(name) {
		/* Said once the box took it. What the key did is the box's business and
		   there is no answer that describes it; what this can honestly report is
		   that the key went, and a refused key never went, whether the refusal
		   is the sign in sheet or a line of its own. */
		/* No picture is drawn while the keys are alone on the screen, so none
		   is fetched for them either. */
		if (!full) {
			askForPicture();
		}
		store.write('POST', '/api/v1/osd/remote/key', { body: { name: name } })
			.then(function () {
				setLast(name);
			}, function (failed) {
				setLast('');
				if (failed && failed.aborted === true) {
					return;
				}
				toast(refusalLine(failed && failed.problem) || t(text, 'now.failed'), 'bad');
			});
	}

	/* THE POWER KEY ASKS FIRST WHILE THE BOX IS ON, and so does the header's
	   key and the switch on the box card: it takes the picture off the
	   television, and on a screen of sixty keys it is the one a slip costs
	   most. Every way of pressing comes through here, the drawn keys and the
	   keyboard alike, so neither of them gets round it.
	   Waking does not ask. A box that has not yet said which it is in is taken
	   to be on, because asking once too often costs a press and not asking
	   once too often costs the evening's film.
	   What is sent on yes is still the key and not the standby address: the
	   box decides what its power key does, and this screen presses keys. */
	/**
	 * @param {string} name
	 * @returns {void}
	 */
	function press(name) {
		if (name === 'KEY_POWER' && !(mode.data && mode.data.on === true)) {
			setAskingPower(true);
			return;
		}
		send(name);
	}

	/**
	 * A key on the keyboard, as the key of the handset it stands for.
	 *
	 * Taken on the grid and not on the document, because the arrows belong to
	 * whoever is reading a page and only here do they mean the cross. The grid
	 * itself takes focus, so one tab reaches it and the arrows work from then on.
	 *
	 * @param {KeyboardEvent} event
	 * @returns {void}
	 */
	function onKey(event) {
		const target = /** @type {Element | null} */ (event.target);
		const tag = target && target.tagName ? target.tagName : '';
		if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') {
			return;
		}
		/* The enter key on a focused button is that button being pressed, and
		   the browser has already done it. Taking it here as well would send
		   two keys for one press, which on a channel number is a channel
		   nobody asked for. */
		if (event.key === 'Enter' && tag === 'BUTTON') {
			return;
		}

		const wanted = FROM_KEYBOARD[event.key];
		if (!wanted || named.indexOf(wanted) === -1) {
			return;
		}
		event.preventDefault();
		press(wanted);
	}

	/**
	 * @param {boolean} locked
	 * @returns {void}
	 */
	function writeLock(locked) {
		if (lock.data) {
			store.put('GET', '/api/v1/osd/remote', null, { locked: locked });
		}
		store.write('PUT', '/api/v1/osd/remote', {
			body: { locked: locked },
			touches: ['/api/v1/osd/remote'],
		}).catch(function (failed) {
			store.reload('GET', '/api/v1/osd/remote');
			if (failed && failed.aborted === true) {
				return;
			}
			toast(refusalLine(failed && failed.problem) || t(text, 'now.failed'), 'bad');
		});
	}

	/* THE TELEPHONE AS THE HANDSET. Pressed, this screen lies over the whole
	   window and shows the keys and nothing else: no frame, no picture of the
	   television, no line about the last press, no heading, no hint. The one
	   thing beside them is the way back, a small key in the corner above, and
	   on a telephone the browser's own back gesture as well. The keys are
	   drawn as they always are.

	   Where the browser can, the page also asks for the whole screen, which
	   takes the browser's own bars away as well. A telephone that cannot
	   (Safari on an iPhone gives an element no full screen) still gets the
	   overlay, which is most of the gain. Leaving the browser's full screen
	   by its own gesture leaves this too, so the two never disagree, and so
	   does the escape key where there is one. */
	/**
	 * @param {boolean} next
	 * @returns {void}
	 */
	function goFull(next) {
		const root = document.documentElement;
		if (next) {
			// Has to happen inside the click itself, the browser wants a gesture.
			if (root.requestFullscreen && !document.fullscreenElement) {
				root.requestFullscreen({ navigationUI: 'hide' }).catch(function () {});
			}
			pushed.current = true;
			route(hrefFor('now', 'remote', 'control'));
			return;
		}
		if (document.fullscreenElement && document.exitFullscreen) {
			document.exitFullscreen().catch(function () {});
		}
		if (pushed.current) {
			// Entered by the button: step back instead of stacking another entry.
			pushed.current = false;
			history.back();
		} else {
			// Arrived by address: replace, so Back does not lead into full screen again.
			route(hrefFor('now', 'remote'), true);
		}
	}

	useEffect(function () {
		if (!full) {
			return undefined;
		}
		function onChange() {
			if (!document.fullscreenElement) {
				goFull(false);
			}
		}
		/** @param {KeyboardEvent} event */
		function onEscape(event) {
			/* A question over the keys has the escape key first, and
			   answers it by closing itself. */
			if (event.key === 'Escape' && !event.defaultPrevented) {
				goFull(false);
			}
		}
		document.addEventListener('fullscreenchange', onChange);
		document.addEventListener('keydown', onEscape);
		return function () {
			document.removeEventListener('fullscreenchange', onChange);
			document.removeEventListener('keydown', onEscape);
		};
	}, [full]);

	/* AND THE TELEPHONE STAYS AWAKE WHILE IT IS THE HANDSET. A handset whose
	   screen goes dark between two presses is one nobody holds for a film, so
	   for as long as this screen lies over the window it asks the browser to
	   keep the display on, and gives that back the moment it is left.

	   The browser drops the lock by itself whenever the page is out of sight
	   (another app, the screen turned off by hand), so it is taken again when
	   the page comes back. And it is only offered where the browser has it,
	   which is a secure page: over plain http on the home network there is no
	   wake lock, nothing is said about it, and the full screen is the same
	   full screen without it. Nothing on the screen says so either way: the
	   keys and nothing else. */
	useEffect(function () {
		if (!full || !navigator.wakeLock) {
			return undefined;
		}
		const wake = navigator.wakeLock;
		/** @type {WakeLockSentinel | null} */
		let held = null;
		let left = false;
		function take() {
			if (document.visibilityState !== 'visible' || held !== null) {
				return;
			}
			wake.request('screen').then(function (got) {
				if (left) {
					got.release().catch(function () {});
					return;
				}
				held = got;
				got.addEventListener('release', function () {
					if (held === got) {
						held = null;
					}
				});
			}, function () {});
		}
		take();
		document.addEventListener('visibilitychange', take);
		return function () {
			left = true;
			document.removeEventListener('visibilitychange', take);
			if (held !== null) {
				const was = held;
				held = null;
				was.release().catch(function () {});
			}
		};
	}, [full]);

	/* Left the screen while it lay over the window: the browser's full
	   screen goes with it, or the next screen would open without its bars. */
	useEffect(function () {
		return function () {
			if (document.fullscreenElement && document.exitFullscreen) {
				document.exitFullscreen().catch(function () {});
			}
		};
	}, []);

	/* The address left full screen (Back gesture, a link, escape): this visit's
	   bookkeeping and the browser's own full screen go with it. */
	useEffect(function () {
		if (full)
			return;
		pushed.current = false;
		if (document.fullscreenElement && document.exitFullscreen)
			document.exitFullscreen().catch(function () {});
	}, [full]);

	/* THE LINE THAT ANSWERS A PRESS STANDS UNDER THE PICTURE. Under the keys it
	   is under the block that is pinned, so the one press somebody makes
	   without scrolling was the one press nothing answered on screen; beside
	   the keys it took a third of the width off them and left the cross
	   leaning into one half of the screen. Under the picture it takes a line
	   and no width from anything. */
	/* THE WAY OUT OF FULL SCREEN IS PART OF THE HANDSET. It had a line of
	   its own over the keys, a framed cross at the far end of it, and on a
	   monitor that cross was the loudest thing on the screen after the
	   handset itself. Now it stands in the first row, in the place between
	   power and mute that a handset leaves free, drawn like a key with no
	   key cap: the sign a browser uses for leaving full screen, dimmed.
	   Next to power is not a risk worth avoiding, because power asks first.

	   The first row is drawn for it even where the box names neither power
	   nor mute. And where no key is drawn at all (the list still on its way,
	   refused, or empty) the old line over the card comes back, because
	   then there is no row to stand in and a full screen with no way out of
	   it on a telephone without an escape key is a trap. */
	const exit = html`<button type="button" class="now-key now-full-exit" data-act="full-close"
		title=${t(text, 'now.remote.full.close')}
		aria-label=${t(text, 'now.remote.full.close')}
		onClick=${function () { goFull(false); }}>
		<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960" width="24px" height="24px" fill="currentColor" aria-hidden="true" focusable="false"><path d="M240-120v-120H120v-80h200v200h-80Zm400 0v-200h200v80H720v120h-80ZM120-640v-80h120v-120h80v200H120Zm520 0v-200h80v120h120v80H640Z"/></svg>
	</button>`;
	const keysDrawn = !keys.error && keys.phase !== 'first' && named.length > 0;
	const rows = full && keysDrawn && !groups.some(function (group) { return group.id === 'top'; })
		? [{ id: 'top', shape: 'now-keys-top', keys: /** @type {string[]} */ ([]) }].concat(groups)
		: groups;

	const sent = html`<p class="now-remote-sent" role="status">
		${last === '' ? '' : t(text, 'now.remote.sent', { key: labelFor(last) })}
	</p>`;

	return html`<div class=${full ? 'now-remote is-full' : 'now-remote'}>
		${full && !keysDrawn
			? html`<div class="now-full-back">
				<button type="button" class="btn now-full-close" data-act="full-close"
					title=${t(text, 'now.remote.full.close')}
					aria-label=${t(text, 'now.remote.full.close')}
					onClick=${function () { goFull(false); }}>
					<svg viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="M6 6l12 12M18 6 6 18" /></svg>
				</button>
			</div>`
			: null}
		${/* THE PICTURE, AND OVER THE KEYS THE ONE THING THAT IS PINNED. Beside
		     them it is a card like any other, with its heading and what it is
		     fetched with inside it. Over them the stylesheet pins this block and
		     lets every key pass under it, so nothing is in it but the picture
		     and the line under the picture: a heading, a card's edge, a button
		     and five keys pinned as well came to two thirds of the reading, and
		     what is under the block is the whole of what somebody came here to
		     press. */''}
		${/* KEPT IN THE DOCUMENT IN FULL SCREEN, AND HIDDEN. The picture's
		     watcher (useOnScreen) is tied to the element it was first given;
		     taken out and drawn anew, it went on watching the old one, which
		     never came back on screen, so a picture set to refresh itself
		     stayed still after full screen was left. Hidden, it is the same
		     element, out of sight is not on screen, and nothing is fetched
		     for it until it is back. */''}
		<div class="now-remote-head">
			${beside
				? html`<${Card} title=${t(text, 'now.shot.card.screen')}>
					${shot.seen}
					${sent}
					${shot.options}
				<//>`
				: html`${shot.seen}${sent}`}
		</div>
		<${Card}
			title=${t(text, 'now.remote.card.keys')}
			snapshot=${keys}
			empty=${keys.data && named.length === 0 ? t(text, 'now.remote.none') : undefined}>
			${/* Offered where the picture stands over the keys, which is a
			     telephone or a tablet held upright, and gone once it is taken. */''}
			${full
				? null
				: html`<p class="now-full-open">
					<button type="button" class="btn" data-act="full-open" onClick=${function () { goFull(true); }}>
						${t(text, 'now.remote.full')}
					</button>
				</p>`}
			<div
				ref=${pad}
				role="group"
				tabindex="0"
				aria-label=${t(text, 'now.remote.card.keys')}
				onKeyDown=${onKey}>
				${rows.map(function (group) {
					const drawn = keysOf(group, press, full && group.id === 'top' ? exit : null);
					/* THE ONES NO GROUP CLAIMED ARE FOLDED AWAY AND NOT
					   DROPPED. A kernel names a hundred keys and a handset
					   has forty of them. Dropping them would make a key
					   unreachable that the box would act on; leaving them
					   out in the open made the screen a catalogue rather
					   than a handset. */
					if (group.id === 'rest') {
						return html`<details key=${group.id} class="now-keygroup now-keyrest" ...${fold('now.remote:rest')}>
							<summary class="now-sub">
								${t(text, 'now.remote.count', { count: group.keys.length })}
							</summary>
							${drawn}
						</details>`;
					}
					return html`<section key=${group.id} class="now-keygroup">
						${drawn}
					</section>`;
				})}
			</div>
			<p class="now-hint">${t(text, 'now.remote.hint')}</p>
			${/* A card section in a card section to separate */''}
			<${Card}
				title=${t(text, 'now.remote.card.lock')}
				snapshot=${lock}>
				<${Switch}
					label=${t(text, 'now.remote.lock')}
					checked=${!!(lock.data && lock.data.locked)}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { writeLock(e.currentTarget.checked); }} />
			<p class="now-hint">${t(text, 'now.remote.lock.hint')}</p>
			<//>
		<//>
		${/* Over the keys, what the picture is fetched with is a card of its own
		     and it comes after them. It is not pinned, and a card standing
		     between the pinned picture and the first key is scrolling done
		     before reaching anything anybody came here to press. */''}
		${beside
			? null
			: html`<${Card} title=${t(text, 'now.shot.card.screen')}>${shot.options}<//>`}
		<${Dialog}
			open=${askingPower}
			title=${t(shell, 'shell.power.standby')}
			confirmLabel=${t(shell, 'shell.power.standby')}
			onCancel=${function () { setAskingPower(false); }}
			onConfirm=${function () { setAskingPower(false); send('KEY_POWER'); }}>
			<p>${t(shell, 'shell.power.ask')}</p>
		<//>
	</div>`;
}
