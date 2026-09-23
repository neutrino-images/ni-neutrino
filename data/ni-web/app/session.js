/* The page starts by asking what this request was granted rather than by asking whether
   somebody signed in: a caller from a private network is granted Read with no session at
   all (src/httpd/netmatch.cpp), so signed in is not the normal case.

   Two facts that cost time to learn:

   The login answer carries no level. It answers csrf, user and csrf_header and nothing
   else, so a page that renders what it may do out of that answer renders a guess; the
   documentation page made exactly that mistake. What a request was granted is the answer
   of GET /api/v1/session.

   A session that has run out is answered 403 and not 401. A dead cookie grants what the
   network grants, which on a box is Read, and Read below Write is a refusal to permit and
   not a demand to authenticate. 401 is what a wrong name or password answers. */

import { api, buildUrl } from './api.js';
import { localError, isAborted } from './problem.js';

// Per tab and per session, which is what this token belongs to. A second tab
// asks the same route for its own, and a token left in localStorage would
// outlive the session it names and be presented for nothing.
const kCsrfKey = 'ni-web.csrf';
const kCsrfHeaderKey = 'ni-web.csrf-header';

/** @type {readonly Web.Level[]} */
const kLevels = ['public', 'read', 'write', 'system'];

/* What the box last said about this request, and nothing this page decided for itself. */
/**
 * What the page holds about this request, and what a watcher is handed.
 *
 * @typedef {object} Session
 * @property {boolean} checked whether the box has been asked at all, so that
 *           "nobody is signed in" and "not asked yet" are not drawn alike
 * @property {boolean} authenticated
 * @property {Web.Level} level
 * @property {string} user
 * @property {number} expiresAt seconds since the epoch, nought for no session
 * @property {boolean} prompting whether the sign in sheet is open
 */

/** @type {Session & { csrf: string, csrfHeader: string }} */
const current = {
	checked: false,
	authenticated: false,
	level: 'public',
	user: '',
	csrf: '',
	csrfHeader: '',
	expiresAt: 0,
	prompting: false,
};

/** @type {Array<(session: Session) => void>} */
const watchers = [];
/** @type {Array<{ resolve: (session: Session) => void, reject: (error: unknown) => void }>} */
let waiting = [];
let alarm = 0;
/** @type {Promise<Session> | null} */
let asking = null;

/* The token for a program outside this browser, drawn once and handed to everything that
   asks.

   ONE FOR THE PAGE AND NOT ONE PER FILE. What it stands for is a part of this box and not
   a name: the box resolves that part afresh on every request, so the same token serves
   every recording. Drawing one per row would be one derivation of a password hash per row
   opened, on a box with a hundred and twenty eight megabytes and no hurry.

   A promise and not a string, so two rows opened at once wait on one call rather than
   drawing two tokens. Dropped when the session goes, because the box gives it no longer
   than a session and a page holding a dead one would show an address that answers
   nothing.

   Never stored. A token in an address is already the weakest credential this server
   hands out, and one kept in the browser between visits would outlive every reason it was
   drawn for. */
/** @type {Promise<string> | null} */
let outsideToken = null;

/* Storage is wrapped because reading it throws rather than answering nothing in a browser
   told to keep no site data, and the whole of what is lost when it does is one round trip
   to the route that hands the token out again. */
/**
 * @param {string} key
 * @returns {string}
 */
function stored(key) {
	try {
		return window.sessionStorage.getItem(key) || '';
	} catch (e) {
		return '';
	}
}

/**
 * @param {string} key
 * @param {string} value
 * @returns {void}
 */
function store(key, value) {
	try {
		if (value === '') {
			window.sessionStorage.removeItem(key);
		} else {
			window.sessionStorage.setItem(key, value);
		}
	} catch (e) {
		// A tab that cannot remember its token asks for it again on the next
		// load. Nothing else here depends on it.
	}
}

current.csrf = stored(kCsrfKey);
current.csrfHeader = stored(kCsrfHeaderKey);

/** @returns {Session} */
export function state() {
	return {
		checked: current.checked,
		authenticated: current.authenticated,
		level: current.level,
		user: current.user,
		expiresAt: current.expiresAt,
		prompting: current.prompting,
	};
}

/**
 * @param {(session: Session) => void} watcher
 * @returns {() => void} stops the watching
 */
export function subscribe(watcher) {
	watchers.push(watcher);
	return function () {
		const at = watchers.indexOf(watcher);
		if (at !== -1) {
			watchers.splice(at, 1);
		}
	};
}

/** @returns {void} */
function announce() {
	const snapshot = state();
	for (const watcher of watchers) {
		watcher(snapshot);
	}
}

// The token and the name of the header it travels in, for the one caller that
// needs them, which is the client.
/** @returns {string} */
export function csrf() {
	return current.csrf;
}

/** @returns {string} */
export function csrfHeader() {
	return current.csrfHeader;
}

/**
 * @param {Web.Level} level
 * @returns {boolean}
 */
export function atLeast(level) {
	const want = kLevels.indexOf(level);
	const have = kLevels.indexOf(current.level);
	return want !== -1 && have >= want;
}

/** @returns {boolean} */
export function canWrite() {
	return atLeast('write');
}

/** @returns {boolean} */
export function canSystem() {
	return atLeast('system');
}

/* The alarm that goes off when the session does.

   A session is absolute and not sliding: expires is written once when it is opened and
   never moved (src/httpd/auth.cpp), and the lifetime is a day (src/httpd/webconfig.cpp).
   So a tab left open overnight is refused in the middle of whatever it is doing, once, the
   next morning.

   What it does is ask the box again rather than decide what is true. */
/**
 * @param {number} seconds
 * @returns {void}
 */
function setAlarm(seconds) {
	if (alarm !== 0) {
		window.clearTimeout(alarm);
		alarm = 0;
	}
	if (!Number.isFinite(seconds) || seconds <= 0) {
		return;
	}
	alarm = window.setTimeout(function () {
		alarm = 0;
		refresh();
	}, seconds * 1000);
}

/**
 * @param {Api.Result<'GET /api/v1/session'> | null} answer
 * @returns {void}
 */
function take(answer) {
	/* Partial and not the shape whole: the answer is null where the box sent no body
	   at all, and every member below is tested for what it is before it is taken. */
		/** @type {Partial<Api.Result<'GET /api/v1/session'>>} */
	const body = answer || {};
	current.checked = true;
	current.authenticated = body.authenticated === true;
	/* A session that has gone takes the token for another program with it. The box gives
	   one no longer than a session, so what is held here after that is at best about to
	   stop working, and a page showing an address that answers nothing is the defect this
	   token was drawn to close. The next caller draws a fresh one. */
	if (!current.authenticated) {
		outsideToken = null;
	}
	const level = /** @type {Web.Level} */ (body.level);
	current.level = kLevels.indexOf(level) === -1 ? 'public' : level;
	current.user = typeof body.user === 'string' ? body.user : '';
	/* The header name is kept even when no token came with it. The box answers it to a
	   caller holding no session precisely so a page about to sign in already has it, and
	   gating the whole reading on authenticated would leave the name written out a second
	   time in this application. */
	if (typeof body.csrf_header === 'string' && body.csrf_header !== '') {
		current.csrfHeader = body.csrf_header;
		store(kCsrfHeaderKey, current.csrfHeader);
	}
	current.csrf = typeof body.csrf === 'string' ? body.csrf : '';
	store(kCsrfKey, current.csrf);
	const left = (typeof body.expires_in === 'number' && Number.isFinite(body.expires_in))
		? body.expires_in : 0;
	current.expiresAt = left > 0 ? Math.floor(Date.now() / 1000) + left : 0;
	setAlarm(left);
	announce();
}

/* What this request carries, asked of the box.

   Public, so it answers a caller holding nothing as readily as one holding a session
   (src/httpd/ep/ep_auth.cpp). A box that does not answer leaves what was known standing: the
   page is then wrong about the level until the next answer, and the worst it does with
   that is offer a control whose refusal says so. */
/**
 * @param {AbortSignal} [signal]
 * @returns {Promise<Session>}
 */
export async function refresh(signal) {
	try {
		/* Not recovered, ever. This route is what recovery is made of: a refusal to it
		   that sent the client back here would be this function calling itself for as long
		   as the box goes on refusing. The route is Public and answers a caller holding
		   nothing, so a refusal to it is never something a second attempt would answer. */
		const answer = await api('GET', '/api/v1/session', { signal: signal, recover: false });
		take(answer);
		return state();
	} catch (error) {
		if (!isAborted(error)) {
			current.checked = true;
			announce();
		}
		return state();
	}
}

// The one reading on the way in. A reload throws away everything this module holds and the
// cookie survives it, so without this the page comes up signed out while the browser is
// still signed in.
/** @returns {Promise<Session>} */
export function start() {
	return refresh();
}

/* Opening a session. The answer hands over the second token and says who, and says nothing
   about what that is worth, so the route that does say is asked straight after: two round
   trips at a sign in, once, for a fact this page would otherwise be guessing at.

   The token is asked for and the call sent again if the box refuses it for want of one.
   The sheet is not opened from in here: a wrong password is a 401 that belongs in the
   sheet the person is already looking at. */
/**
 * @param {string} user
 * @param {string} password
 * @returns {Promise<Session>}
 */
export async function login(user, password) {
	const answer = await api('POST', '/api/v1/login', {
		body: { user: user, password: password },
		recover: 'token',
	});
	if (answer && typeof answer.csrf === 'string') {
		current.csrf = answer.csrf;
		store(kCsrfKey, current.csrf);
	}
	if (answer && typeof answer.csrf_header === 'string' && answer.csrf_header !== '') {
		current.csrfHeader = answer.csrf_header;
		store(kCsrfHeaderKey, current.csrfHeader);
	}
	await refresh();
	current.prompting = false;
	asking = null;
	const done = waiting;
	waiting = [];
	for (const one of done) {
		one.resolve(state());
	}
	announce();
	return state();
}

/* Closing it. This needs the second token like everything else that changes something,
   which is the whole reason the route that hands the token back out exists: without it a
   reloaded page could neither write, nor sign in again, nor sign out. */
/** @returns {Promise<Session>} */
export async function logout() {
	try {
		await api('POST', '/api/v1/logout', { recover: 'token' });
	} finally {
		current.csrf = '';
		store(kCsrfKey, '');
		outsideToken = null;
		await refresh();
	}
	return state();
}

/**
 * The address of a recording, for a program that is not this browser.
 *
 * WHY THIS EXISTS AT ALL. A media player is handed an address and nothing else: it sets no
 * header and carries no cookie, and the route that hands over the bytes asks for System.
 * So the address this page shows answered 403 to everybody who pasted it. What it carries
 * now is a token, and the token is worth the media of this box and nothing else, for
 * as long as a session lasts.
 *
 * The address is built by the one file that builds addresses, from the path this was
 * handed and the token this draws, and this is the only caller that adds the second. Every
 * other address the page asks for travels with the cookie the browser attaches by itself,
 * and a token written into one of those would be a secret in a history list for nothing.
 *
 * @param {string} path the file on the box
 * @returns {Promise<string>} the address to paste elsewhere
 */
export async function outsideAddress(path) {
	if (outsideToken === null) {
		/* Kept before it settles, so two rows opened at once wait on one call. Cleared
		   again on a refusal, or the first failure would be the answer for the rest of
		   the visit. */
		outsideToken = api('POST', '/api/v1/token/media', { recover: 'token' })
			.then(function (answer) {
				return answer && typeof answer.token === 'string' ? answer.token : '';
			})
			.catch(function (error) {
				outsideToken = null;
				throw error;
			});
	}
	const token = await outsideToken;
	if (token === '') {
		throw localError('session.token.title', 'session.token.detail', 500);
	}
	return buildUrl('/api/v1/storage/file', null, { path: path, token: token });
}

/* A promise, not an event: the caller writes on, or does not, and the intention behind the
   first attempt is never lost in between. Kept in a list rather than one at a time, because
   three cards on one screen can all want to write at the same moment. */
/**
 * @param {Web.Level} level
 * @returns {Promise<Session>} settles when the request carries that much, and
 *          is rejected when the person closed the sheet
 */
export function requireLevel(level) {
	if (atLeast(level)) {
		return Promise.resolve(state());
	}
	if (asking === null) {
		asking = new Promise(function (resolve, reject) {
			waiting.push({ resolve: resolve, reject: reject });
		});
		current.prompting = true;
		announce();
	}
	return asking;
}

/** @returns {Promise<Session>} */
export function requireWrite() {
	return requireLevel('write');
}

/** @returns {Promise<Session>} */
export function requireSystem() {
	return requireLevel('system');
}

// The person closed the sheet. Everything that was waiting on it is told so, in the shape
// everything else is told in, so a caller has one catch and not two.
/** @returns {void} */
export function cancelLogin() {
	current.prompting = false;
	asking = null;
	const done = waiting;
	waiting = [];
	for (const one of done) {
		one.reject(localError('session.cancelled.title', 'session.cancelled.detail', 401));
	}
	announce();
}

/* What the client does with a refusal that may not be one: ask what this request carries
   now, and say whether there is anything new to send.

   Three ways out, and the third is the one that is easy to get wrong. Asking produced a
   token this call has not sent yet: send it again. Asking says nobody is signed in: open
   the sheet, and when the sign in is through, send it again. Asking says somebody is signed
   in and hands back the same token that was just refused: the refusal is about the level
   and not about the token, so it goes back to the caller as the box wrote it. */
/**
 * @param {number} status the status the box refused with
 * @param {boolean} mayPrompt whether the sign in sheet may be opened
 * @returns {Promise<boolean>} whether there is anything new to send
 */
export async function recover(status, mayPrompt) {
	const before = current.csrf;
	await refresh();
	if (current.authenticated && current.csrf !== '' && current.csrf !== before) {
		return true;
	}
	if (current.authenticated) {
		return false;
	}
	if (mayPrompt === false) {
		return false;
	}
	if (status !== 401 && status !== 403) {
		return false;
	}
	try {
		await requireWrite();
		return true;
	} catch (e) {
		return false;
	}
}
