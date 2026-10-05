// What the page does when a call is refused while it believes somebody is signed in.
import { api } from '../../data/ni-web/app/api.js';
import * as session from '../../data/ni-web/app/session.js';

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
		process.stderr.write('session: ' + what + '\n');
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

// the box

/** @type {string[]} */
let asked = [];
/** @type {Map<string, Array<{ status: number, body: string }>>} */
const says = new Map();

/**
 * @param {string} key
 * @param {...[number, object]} answers the last one repeats
 */
function answer(key, ...answers) {
	says.set(key, answers.map(function (one) {
		return { status: one[0], body: JSON.stringify(one[1]) };
	}));
}

globalThis.fetch = /** @type {any} */ (function (/** @type {string} */ url, /** @type {RequestInit} */ init) {
	const key = (init && init.method ? init.method : 'GET') + ' ' + url;
	asked.push(key);
	const queue = says.get(key) || [{ status: 200, body: '{}' }];
	const one = queue.length > 1 ? /** @type {{ status: number, body: string }} */ (queue.shift()) : queue[0];
	return Promise.resolve({
		ok: one.status >= 200 && one.status < 300,
		status: one.status,
		text: function () { return Promise.resolve(one.body); },
		json: function () { return Promise.resolve(JSON.parse(one.body)); },
	});
});

/** @type {Record<string, string>} */
const kept = {};
globalThis.window = /** @type {any} */ ({
	sessionStorage: {
		getItem: function (/** @type {string} */ k) { return Object.prototype.hasOwnProperty.call(kept, k) ? kept[k] : null; },
		setItem: function (/** @type {string} */ k, /** @type {string} */ v) { kept[k] = v; },
		removeItem: function (/** @type {string} */ k) { delete kept[k]; },
	},
	setTimeout: function () { return 1; },
	clearTimeout: function () {},
});

function settle() {
	return new Promise(function (resolve) {
		setTimeout(function () { setTimeout(resolve, 0); }, 0);
	});
}

/** @param {string} key */
function timesAsked(key) {
	return asked.filter(function (one) { return one === key; }).length;
}

const kSession = 'GET /api/v1/session';
const kRead = 'GET /api/v1/system/webserver';
const kSignedIn = { authenticated: true, level: 'system', user: 'root', csrf: 'c1', csrf_header: 'X-CSRF-Token', expires_in: 2592000 };
const kGone = { authenticated: false, level: 'read', user: '', csrf: '', csrf_header: 'X-CSRF-Token', expires_in: 0 };
const kRefused = { type: '/errors/not-permitted', title: 'Forbidden', status: 403, detail: 'this endpoint is not open to this caller' };

/** @param {unknown} error */
function titleOf(error) {
	const e = /** @type {{ problem?: { title?: string } } | null} */ (error);
	return e && e.problem ? e.problem.title : undefined;
}

async function signedIn() {
	answer(kSession, [200, kSignedIn]);
	await session.refresh();
	asked = [];
}

// the box restarted under a signed in page

await signedIn();
answer(kRead, [403, kRefused], [200, { port: 8081 }]);
answer(kSession, [200, kGone], [200, kSignedIn]);
answer('POST /api/v1/login', [200, { csrf: 'c2', user: 'root', csrf_header: 'X-CSRF-Token' }]);
/** @type {{ done: boolean, value: unknown, error: unknown }} */
const reading = { done: false, value: null, error: null };
api('GET', '/api/v1/system/webserver').then(function (value) {
	reading.done = true;
	reading.value = value;
}, function (error) {
	reading.done = true;
	reading.error = error;
});
await settle();
same(timesAsked(kSession), 1, 'a read refused while signed in asks the box about the session once');
is(!session.state().authenticated, 'and a session the box no longer has is dropped from the page');
is(session.state().prompting, 'and the sign in is offered');
is(!reading.done, 'while the read waits for it rather than failing');
await session.login('root', 'ni');
await settle();
is(reading.done && reading.error === null, 'signing in sends the read again and it succeeds: ' + titleOf(reading.error));
same(reading.value, { port: 8081 }, 'with what the box answered the second time');
same(timesAsked(kRead), 2, 'the read went out twice');

// the person closes the sign in

await signedIn();
answer(kRead, [403, kRefused]);
answer(kSession, [200, kGone]);
/** @type {unknown} */
let closedWith = null;
const closing = api('GET', '/api/v1/system/webserver').catch(function (error) {
	closedWith = error;
});
await settle();
is(session.state().prompting, 'the sign in is offered again');
session.cancelLogin();
await closing;
is(closedWith !== null, 'closing it fails the read');
is(titleOf(closedWith) !== 'Forbidden', 'with what happened and not the box\'s bare refusal, got ' + titleOf(closedWith));

// a refusal about the level and not about the session

await signedIn();
answer(kRead, [403, kRefused]);
answer(kSession, [200, kSignedIn]);
/** @type {unknown} */
let levelWith = null;
await api('GET', '/api/v1/system/webserver').catch(function (error) {
	levelWith = error;
});
same(timesAsked(kSession), 1, 'a session that is still there is asked about once');
same(timesAsked(kRead), 1, 'and the read is not sent again');
is(!session.state().prompting, 'and no sign in is offered');
same(titleOf(levelWith), 'Forbidden', 'the refusal goes back as the box wrote it');

// a page that never believed it was in

answer(kSession, [200, kGone]);
await session.refresh();
asked = [];
answer(kRead, [403, kRefused]);
await api('GET', '/api/v1/system/webserver').catch(function () {});
same(timesAsked(kSession), 0, 'a read refused to a page nobody signed in to asks nothing more');
is(!session.state().prompting, 'and offers no sign in by itself');

if (failed > 0) {
	process.stderr.write('session: ' + failed + ' of ' + checked + ' failed\n');
	process.exit(1);
}
console.log('session: ' + checked + ' cases');
