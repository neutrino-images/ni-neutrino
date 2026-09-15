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
import { html, useState, useEffect, useRef } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import text from './now.text.js';
import { Switch } from '../../ui/switch.js';
import { toast } from '../../ui/toast.js';
import { Card, useResource } from './parts.js';
import { HANDSETS, PICTURE_FOR, PICTURE_FOR_HARDWARE, keySpots } from './handsets.js';
import { useCapture, Display } from './screenshot.js';

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
	{ id: 'digits', shape: 'now-keys-digits',
	  keys: ['KEY_1', 'KEY_2', 'KEY_3', 'KEY_4', 'KEY_5', 'KEY_6', 'KEY_7', 'KEY_8', 'KEY_9', 'KEY_0'] },
	{ id: 'cross', shape: 'now-keys-cross',
	  keys: ['KEY_UP', 'KEY_LEFT', 'KEY_OK', 'KEY_RIGHT', 'KEY_DOWN'] },
	{ id: 'volume', shape: '',
	  keys: ['KEY_VOLUMEUP', 'KEY_VOLUMEDOWN', 'KEY_MUTE', 'KEY_PAGEUP', 'KEY_PAGEDOWN'] },
	{ id: 'colour', shape: 'now-keys-colour',
	  keys: ['KEY_RED', 'KEY_GREEN', 'KEY_YELLOW', 'KEY_BLUE'] },
	{ id: 'transport', shape: '',
	  keys: ['KEY_PLAY', 'KEY_PAUSE', 'KEY_PLAYPAUSE', 'KEY_STOP', 'KEY_RECORD',
		 'KEY_REWIND', 'KEY_FORWARD', 'KEY_PREVIOUS', 'KEY_NEXT',
		 'KEY_PREVIOUSSONG', 'KEY_NEXTSONG'] },
	{ id: 'screen', shape: '',
	  keys: ['KEY_MENU', 'KEY_SETUP', 'KEY_HOME', 'KEY_BACK', 'KEY_BACKSPACE', 'KEY_INFO',
		 'KEY_EPG', 'KEY_HELP', 'KEY_TEXT', 'KEY_LAST', 'KEY_FAVORITES', 'KEY_SUBTITLE',
		 'KEY_TIME', 'KEY_PROGRAM', 'KEY_BOOKMARKS', 'KEY_MOVE', 'KEY_MODE',
		 'KEY_WWW', 'KEY_GAMES'] },
	{ id: 'source', shape: '',
	  keys: ['KEY_TV', 'KEY_TV2', 'KEY_RADIO', 'KEY_AUDIO', 'KEY_VIDEO', 'KEY_SAT', 'KEY_PVR'] },
	{ id: 'power', shape: '',
	  keys: ['KEY_POWER', 'KEY_SLEEP', 'KEY_STANDBYON', 'KEY_STANDBYOFF'] },
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
				onClick=${function () { onPress(name); }}>${labelFor(name)}</button>`;
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
 * @returns {Web.Drawn}
 */
export default function Remote() {
	const keys = useResource('GET', '/api/v1/osd/remote/keys');
	const lock = useResource('GET', '/api/v1/osd/remote');
	const info = useResource('GET', '/api/v1/system/info');
	const binding = useResource('GET', '/api/v1/settings/{section}', { params: { section: 'keybindings' } });
	const [last, setLast] = useState('');
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
	function press(name) {
		/* Said before the box answers and left standing after it. What the key
		   did is the box's business and there is no answer that describes it;
		   what this can honestly report is that the key went, and a refusal
		   replaces the line with what the box said. */
		setLast(name);
		askForPicture();
		store.write('POST', '/api/v1/osd/remote/key', { body: { name: name } })
			.catch(function (failed) {
				setLast('');
				if (failed && failed.aborted === true) {
					return;
				}
				toast(failed && failed.problem ? failed.problem.title : t(text, 'now.failed'), 'bad');
			});
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
			toast(failed && failed.problem ? failed.problem.title : t(text, 'now.failed'), 'bad');
		});
	}

	/* THE LINE THAT ANSWERS A PRESS STANDS UNDER THE PICTURE. Under the keys it
	   is under the block that is pinned, so the one press somebody makes
	   without scrolling was the one press nothing answered on screen; beside
	   the keys it took a third of the width off them and left the cross
	   leaning into one half of the screen. Under the picture it takes a line
	   and no width from anything. */
	const sent = html`<p class="now-remote-sent" role="status">
		${last === '' ? '' : t(text, 'now.remote.sent', { key: labelFor(last) })}
	</p>`;

	return html`<div class="now-remote">
		${/* THE PICTURE, AND OVER THE KEYS THE ONE THING THAT IS PINNED. Beside
		     them it is a card like any other, with its heading and what it is
		     fetched with inside it. Over them the stylesheet pins this block and
		     lets every key pass under it, so nothing is in it but the picture
		     and the line under the picture: a heading, a card's edge, a button
		     and five keys pinned as well came to two thirds of the reading, and
		     what is under the block is the whole of what somebody came here to
		     press. */''}
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
			${/* Which of the two forms the keys are drawn in, read before them
			     rather than at the foot of the screen under a card about
			     something else. */''}
			${offer
				? html`<${Switch}
					label=${t(text, 'now.remote.photo')}
					checked=${wanted}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { choosePhoto(e.currentTarget.checked); }} />`
				: null}
			<p class="now-hint">${t(text, 'now.remote.lead')}</p>
			${showing
				? html`<p class="now-hint">${t(text, 'now.remote.photo.lead')}</p>
					<${Photograph} name=${picture} named=${named} onPress=${press} />`
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
							return html`<details key=${group.id} class="now-keygroup now-keyrest">
								<summary class="now-sub">
									${t(text, 'now.group.rest')}
									${' '}<span class="now-hint">${t(text, 'now.remote.count', { count: group.keys.length })}</span>
								</summary>
								${drawn}
							</details>`;
						}
						return html`<section key=${group.id} class="now-keygroup">
							<h3 class="now-sub">${t(text, 'now.group.' + group.id)}</h3>
							${drawn}
						</section>`;
					})}
				</div>`}
		<//>
		${/* Over the keys, what the picture is fetched with is a card of its own
		     and it comes after them. It is not pinned, and a card standing
		     between the pinned picture and the first key is scrolling done
		     before reaching anything anybody came here to press. */''}
		${beside
			? null
			: html`<${Card} title=${t(text, 'now.shot.card.screen')}>${shot.options}<//>`}
		${/* The order is the layout. Two columns fill left, right, left, right,
		     so the front panel lands under the picture and the switch about the
		     box's own handset lands in the card of the keys it stands for,
		     without a rule naming a position. */''}
		<${Display} />
		<${Card}
			title=${t(text, 'now.remote.card.lock')}
			snapshot=${lock}>
			<${Switch}
				label=${t(text, 'now.remote.lock')}
				checked=${!!(lock.data && lock.data.locked)}
				onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { writeLock(e.currentTarget.checked); }} />
		<//>
	</div>`;
}
