// Signing in, which is one sheet and one place in the whole application.
//
// Why it is mounted beside the frame and not inside a screen. A write is asked for from
// wherever somebody pressed something, and the answer to being below the level a route
// wants is a promise that settles later. Three cards on one screen can all be waiting on
// that one promise, so the thing they wait for has to outlive any one of them.
//
// Why it opens in two ways. The box refusing something is one, and it arrives as
// session.state().prompting. Somebody deciding to sign in before they are refused
// anything is the other, and there is nothing in a session to read for that.
//
// A wrong password does not close this. It is a 401 about the pair that was just typed,
// it belongs in the sheet the person is still looking at, and the layer below
// deliberately does not answer it by opening this sheet again.
import { html, useState, useEffect } from '../runtime.js';
/* The one part of the frame that reaches the layer talking to the box, and the
   reason main.js is no longer the only one. What opens this sheet is a control
   in the corner of the bar, and the bar is handed nothing but what the box is
   doing; threading a fourth value down through the router so that one button
   could reach it would put the wiring in three files to keep it out of one. */
import * as session from '../session.js';
import { t } from '../i18n.js';
/* The words for a session that has run out are the data layer's, beside the
   words for one the person cancelled, because both are things this page says
   about a session rather than anything the box sent. */
import { word } from '../fmt.js';
import text from '../shell.text.js';
import { Sheet } from './sheet.js';
import { Field } from './field.js';
import { Button } from './button.js';
import { State } from './state.js';
import { toast } from './toast.js';

// Somebody asked to sign in without having been refused anything. Module state
// and not a member of the session, because it is a fact about this page and
// not about what the request carries.
let asked = false;

/* Whether the session this page had ran out, which is why the sheet is open when it
   opens by itself. Held here because by the time the sheet is asked for there is
   nothing left to read it off: a session that has run out is refused 403 exactly like a
   caller that never had one. So it is noticed on the way past, and somebody who signed
   out has an expiry still ahead of the clock, which is what tells the two apart.

   Measured on a running box with the session set to a minute: expires_in comes back as
   the lifetime, a read still answers afterwards because the network grants that much,
   and the write that follows is answered 403. */
let lastExpiry = 0;
let ranOut = false;

/**
 * @param {import('../session.js').Session} granted
 * @returns {void}
 */
function notice(granted) {
	if (granted.authenticated) {
		lastExpiry = granted.expiresAt;
		ranOut = false;
		return;
	}
	if (lastExpiry !== 0 && lastExpiry <= Math.floor(Date.now() / 1000)) {
		ranOut = true;
	}
	lastExpiry = 0;
}

/** @type {Set<() => void>} */
const watchers = new Set();

/** @returns {void} */
function announce() {
	for (const watcher of watchers) {
		watcher();
	}
}

// Opens the sheet. Exported for the one control that offers it, which is the
// line in the corner of the frame that says who is signed in.
/** @returns {void} */
export function askToSignIn() {
	if (asked)
		return;
	asked = true;
	announce();
}

/* Signing out, kept here rather than beside the control that presses it, so this
   file is the only one in the frame reaching the layer that talks to the box. It
   changes something, so it carries the second token like every other write.

   A refusal is said and not swallowed: a person who pressed sign out and was left
   signed in with nothing on screen would press it again. */
/** @returns {void} */
export function signOut() {
	session.logout().then(function () {
		// Nothing to draw. What the frame shows about the session is read from
		// the box and arrives through the watcher every part of it already has.
	}, function (/** @type {unknown} */ caught) {
		const failed = /** @type {{ problem?: { detail?: string, title?: string } } | null} */ (caught);
		const problem = failed ? failed.problem : null;
		toast((problem && (problem.detail || problem.title)) || t(text, 'shell.failed'), 'bad');
	});
}

/** @returns {boolean} whether the sheet belongs on screen */
function up() {
	return asked || session.state().prompting;
}

/** @returns {Web.Drawn} */
export function SignIn() {
	const [shown, setShown] = useState(up());
	const [user, setUser] = useState('');
	const [password, setPassword] = useState('');
	const [busy, setBusy] = useState(false);
	const [expired, setExpired] = useState(ranOut);
	const [problem, setProblem] = useState(/** @type {Web.Shown | null} */ (null));

	useEffect(function () {
		function look() {
			setShown(up());
			setExpired(ranOut);
		}
		function told(/** @type {import('../session.js').Session} */ granted) {
			notice(granted);
			look();
		}
		watchers.add(look);
		const stop = session.subscribe(told);
		return function () {
			watchers.delete(look);
			stop();
		};
	}, []);

	/** @returns {void} */
	function close() {
		asked = false;
		setPassword('');
		setProblem(null);
		setBusy(false);
		/* The layer below is told before this sheet goes, because everything
		   that was waiting on a write is waiting on this and is told in the
		   shape every other refusal arrives in. It announces, so the line that
		   follows is what takes the sheet off the screen. */
		session.cancelLogin();
		announce();
	}

	/** @returns {void} */
	function submit() {
		if (busy)
			return;
		setBusy(true);
		setProblem(null);
		session.login(user, password).then(function () {
			asked = false;
			setBusy(false);
			// Not kept for the life of the tab. The name is left standing
			// because a session that ran out is signed in again by the same
			// person; the password is not.
			setPassword('');
			announce();
		}, function (/** @type {unknown} */ caught) {
			/* Read member by member and never trusted whole: this arrives from
			   a rejected promise, which carries whatever was thrown. The box's
			   own sentence is what goes on screen, because a sentence invented
			   here says what this page guessed was refused. */
			const failed = /** @type {{ problem?: { title?: string, detail?: string } } | null} */ (caught);
			const said = failed ? failed.problem : null;
			setBusy(false);
			setProblem({
				title: (said && said.title) || t(text, 'shell.failed'),
				detail: (said && said.detail) || ''
			});
		});
	}

	/**
	 * @param {KeyboardEvent} event
	 * @returns {void}
	 */
	function onKey(event) {
		// The document states form-action 'none', so there is no form here to
		// press return in and this is what return does instead.
		if (event.key === 'Enter')
			submit();
	}

	/**
	 * @param {(value: string) => void} take
	 * @returns {(event: Event) => void}
	 */
	function reading(take) {
		return function (event) {
			const field = /** @type {HTMLInputElement | null} */ (event.target);
			take(field ? field.value : '');
		};
	}

	return html`<${Sheet}
		open=${shown}
		centred=${true}
		onClose=${close}
		label=${t(text, 'shell.signin.title')}>
		<h2>${t(text, 'shell.signin.title')}</h2>
		${expired
			? html`<p class="hint">
				<strong>${word('session.expired.title')}</strong> ${word('session.expired.detail')}
			</p>`
			: html`<p class="hint">${t(text, 'shell.signin.why')}</p>`}
		${problem ? html`<${State} problem=${problem} />` : null}
		<${Field}
			id="signin-user"
			label=${t(text, 'shell.signin.user')}
			value=${user}
			autocomplete="username"
			onInput=${reading(setUser)}
			onKeyDown=${onKey} />
		<${Field}
			id="signin-password"
			type="password"
			label=${t(text, 'shell.signin.password')}
			value=${password}
			autocomplete="current-password"
			onInput=${reading(setPassword)}
			onKeyDown=${onKey} />
		<p>
			<${Button} class="signin-cancel" onClick=${close}>${t(text, 'shell.cancel')}<//>
			${' '}
			<${Button}
				primary=${true}
				class="signin-go"
				disabled=${busy}
				onClick=${submit}>${busy ? t(text, 'shell.signin.busy') : t(text, 'shell.signin.submit')}<//>
		</p>
	<//>`;
}
