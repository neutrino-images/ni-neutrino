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
// is the truth about what the box will act on, and a picture of somebody else's
// handset is a promise about keys that may not exist here. The box names a few of
// them twice, so the list is read as a set.
//
// THE PHOTOGRAPH IS THE ONE SOMEBODY IS HOLDING, or there is none. Which handset a
// box is sold with is worked out from what the box calls itself, and only the
// pictures a build's own model can want are installed, so a picture that is not
// there is not a fault: the switch is simply not offered.
//
// AND IT IS NOT OFFERED AT ALL WHERE THE PICTURE STANDS OVER THE KEYS RATHER THAN
// BESIDE THEM. The photographs are about 169 by 716, one to four and a fifth, and
// the stylesheet holds them to 320 wide: that is 1355 tall, longer than any window
// this page is read in. Over the picture, every key of it is a scroll away from
// the thing it changed, and at 320 a key is some 38 pixels across, under the 44 a
// thumb lands on. So below the width at which the two stand side by side there is
// no photograph and no switch for one either: a switch that would draw nothing
// reads as a thing this screen can do and will not.
//
// WHAT THE SWITCH REMEMBERS IS THIS BROWSER'S AND NOT THE BOX'S. Which of the two
// forms somebody reads the handset in is a convenience of the machine in their
// hand, so it is a thing for local storage rather than for a setting on the box.
//
// Drawn keys are also worked with a keyboard. The arrows reach the direction keys,
// the digits reach the digits and the enter key reaches OK, which is the one thing
// hot spots on a photograph could never do.
import { html, useState, useEffect, useRef, route } from '../../runtime.js';
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
import { HANDSETS, PICTURE_FOR, PICTURE_FOR_HARDWARE, keySpots } from './handsets.js';
import { useCapture } from './screenshot.js';
import { Dialog } from '../../ui/dialog.js';
import shell from '../../shell.text.js';

export const css = '/app/screens/now/now.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'now.remote.head'); }


/* Where the pictures are installed. Written once here, because it is a
   directory of this page's own and not a route of the API, and a path built at
   three call sites is a path that gets renamed at two of them. */
const PICTURE_DIR = '/rc/';

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

/* THE WIDTH AT WHICH THE PICTURE STANDS BESIDE THE KEYS RATHER THAN OVER THEM,
   and with it whether a photograph of the handset is worth offering at all. One
   number for both: side by side the photograph is a column somebody scrolls past,
   stacked it is a thousand pixels between the picture and the key that changed
   it.

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

/* Where the answer to the switch is kept. This browser's and nobody else's,
   and it survives a reload rather than a session, which is what the switch is
   worth: somebody who prefers the drawn keys prefers them tomorrow as well. */
const PHOTO_KEY = 'ni-web.remote.photo';

/**
 * Whether the photograph is wanted, as this browser last said.
 *
 * Absent means yes, which is what this screen did before anything was remembered.
 * Read inside a try, because storage throws rather than answering nothing in a
 * browser told to keep no site data.
 *
 * @returns {boolean}
 */
function photoWanted() {
	try {
		return window.localStorage.getItem(PHOTO_KEY) !== 'no';
	} catch (e) {
		return true;
	}
}

/**
 * @param {boolean} wanted
 * @returns {void}
 */
function rememberPhoto(wanted) {
	try {
		window.localStorage.setItem(PHOTO_KEY, wanted ? 'yes' : 'no');
	} catch (e) {
		// The switch still works for as long as the page is open; the next
		// visit simply starts from the default again.
	}
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
 * Which photograph belongs to this box.
 *
 * The setting comes first, because somebody who told the box it is being driven by
 * an old dbox handset is holding that handset. Nought there means the box's own,
 * looked up under the name the box gives itself: the vendor and the model joined
 * by a space, which is what the old interface matched on as well.
 *
 * @param {string} vendor
 * @param {string} model
 * @param {number} hardware keybindings.remote_control_hardware
 * @returns {string} the name of the picture, empty for a box with none
 */
export function pictureFor(vendor, model, hardware) {
	const byHand = PICTURE_FOR_HARDWARE[hardware];
	if (byHand) {
		return byHand;
	}
	const said = vendor === '' ? model : vendor + ' ' + model;
	return PICTURE_FOR[said] || '';
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
 * The handset, with a shape over every key of it that this box also names.
 *
 * Drawn as one picture and one overlay in the picture's own pixels, so the whole
 * thing scales with the column it sits in. That is also why it is not an image
 * map: the browser measures those against the picture's natural size and not
 * against the size it was drawn at, so every hot spot moves as soon as the
 * picture is scaled.
 *
 * @param {{ name: string, named: readonly string[], onPress: (key: string) => void }} props
 * @returns {Web.Drawn}
 */
function Photograph(props) {
	const handset = HANDSETS[props.name];
	if (!handset) {
		return html`<p class="now-empty">${t(text, 'now.remote.photo.gone')}</p>`;
	}

	const spots = keySpots(handset.spots);

	return html`<div class="now-handset">
		<img
			class="now-handset-picture"
			src=${PICTURE_DIR + props.name + '.webp'}
			width=${handset.w}
			height=${handset.h}
			alt=${t(text, 'now.remote.photo.alt')} />
		<svg
			class="now-handset-map"
			viewBox=${'0 0 ' + handset.w + ' ' + handset.h}
			preserveAspectRatio="none"
			role="group"
			aria-label=${t(text, 'now.remote.card.keys')}>
			${spots.map(function (spot, index) {
				/* A key the box does not name is part of the photograph and
				   nothing else: no focus, no pointer, nothing to press. */
				const live = props.named.indexOf(spot.key) !== -1;
				const common = {
					class: live ? 'now-spot' : 'now-spot now-spot-dead',
					role: live ? 'button' : undefined,
					tabindex: live ? '0' : undefined,
					'aria-label': live ? labelFor(spot.key) : undefined,
					onClick: live ? function () { props.onPress(spot.key); } : undefined,
					onKeyDown: live
						? function (/** @type {KeyboardEvent} */ e) {
							if (e.key === 'Enter' || e.key === ' ') {
								e.preventDefault();
								props.onPress(spot.key);
							}
						}
						: undefined,
				};
				const key = spot.key + '-' + index;
				if (spot.shape === 'c') {
					return html`<circle key=${key} ...${common}
						cx=${spot.at[0]} cy=${spot.at[1]} r=${spot.at[2]} />`;
				}
				if (spot.shape === 'p') {
					const points = [];
					for (let i = 0; i + 1 < spot.at.length; i += 2) {
						points.push(spot.at[i] + ',' + spot.at[i + 1]);
					}
					return html`<polygon key=${key} ...${common} points=${points.join(' ')} />`;
				}
				return html`<rect key=${key} ...${common}
					x=${spot.at[0]} y=${spot.at[1]} width=${spot.at[2]} height=${spot.at[3]} />`;
			})}
		</svg>
	</div>`;
}

/**
 * One group of keys, as the buttons that press them.
 *
 * @param {{ id: string, shape: string, keys: string[] }} group
 * @param {(key: string) => void} onPress
 * @returns {Web.Drawn}
 */
function keysOf(group, onPress) {
	return html`<div class=${group.shape === '' ? 'now-keyset' : 'now-keyset ' + group.shape}>
		${group.keys.map(function (name) {
			return html`<button
				key=${name}
				type="button"
				class=${'now-key now-key-' + name}
				title=${name}
				aria-label=${labelFor(name)}
				onClick=${function () { onPress(name); }}>${iconFor(name) || labelFor(name)}</button>`;
		})}
	</div>`;
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
	const info = useResource('GET', '/api/v1/system/info');
	const binding = useResource('GET', '/api/v1/settings/{section}', { params: { section: 'keybindings' } });
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
	const [wanted, setWanted] = useState(photoWanted);
	/* One on opening, and one more for every run of presses that has come to
	   an end. The picture reads this and nothing else. */
	const [bump, setBump] = useState(1);
	const waiting = useRef(0);
	const fetched = useRef(0);
	const shot = useCapture(bump);
	const named = (keys.data && keys.data.items) || [];
	const groups = groupsFor(named);

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

	let hardware = 0;
	for (const row of (binding.data && binding.data.items) || []) {
		if (row.id === 'remote_control_hardware') {
			hardware = Number(row.value) || 0;
		}
	}
	const box = info.data;
	const picture = box ? pictureFor(box.vendor, box.model, hardware) : '';
	/* Offered where there is a photograph and room to stand it beside the
	   television picture, and drawn where it is also wanted. Two words for two
	   things: a window narrowed past the threshold stops drawing the
	   photograph without forgetting that somebody asked for one. */
	const offer = picture !== '' && beside;
	const showing = offer && wanted;

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
	   most. Every way of pressing comes through here, the drawn keys, the
	   photograph and the keyboard alike, so none of them gets round it.
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
	 * @param {boolean} next
	 * @returns {void}
	 */
	function choosePhoto(next) {
		setWanted(next);
		rememberPhoto(next);
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
	const sent = html`<p class="now-remote-sent" role="status">
		${last === '' ? '' : t(text, 'now.remote.sent', { key: labelFor(last) })}
	</p>`;

	return html`<div class=${full ? 'now-remote is-full' : 'now-remote'}>
		${full
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
			${/* Which of the two forms the keys are drawn in, read before them
			     rather than at the foot of the screen under a card about
			     something else. */''}
			${showing
				? html`<${Photograph} name=${picture} named=${named} onPress=${press} />`
				: html`<div
					role="group"
					tabindex="0"
					aria-label=${t(text, 'now.remote.card.keys')}
					onKeyDown=${onKey}>
					${groups.map(function (group) {
						const drawn = keysOf(group, press);
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
				</div>`}
			<p class="now-hint">${t(text, 'now.remote.hint')}</p>
			${offer
				? html`<${Switch}
					label=${t(text, 'now.remote.photo')}
					checked=${wanted}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { choosePhoto(e.currentTarget.checked); }} />`
				: null}
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
