/* Every call this page places goes through here, spelled the way the document spells it,
   so a check can read the calls out of the source and hold them to the document the box
   itself writes (test/web/check-web-paths.sh against test/web/openapi.json). That is also
   why a path is a literal at every call site and a value in it is handed over beside it:

       api('DELETE', '/api/v1/timers/{id}', { params: { id: timer.id } })

   A path assembled out of pieces is one the check cannot see.

   This is the only file in the application that calls fetch. */

import { ApiError, toProblem, abortError, offlineError } from './problem.js';
import * as session from './session.js';

/* The methods that ask for something rather than change it, named the way the server names
   them (src/httpd/auth.cpp, safeMethod). The second token is asked for on everything else. */
/** @type {Record<string, boolean>} */
const kSafeMethods = { GET: true, HEAD: true, OPTIONS: true };

/**
 * @param {string} method
 * @returns {boolean}
 */
function isSafe(method) {
	return kSafeMethods[method] === true;
}

/* One value in a path, escaped. encodeURIComponent and not the looser encodeURI: a bouquet
   name can carry a slash, a question mark or a hash, and any of the three left as itself
   turns one segment into two, or into a query the server then refuses. */
/**
 * @param {string} path the document's own template
 * @param {Record<string, Api.Scalar> | null | undefined} params
 * @returns {string}
 */
function fillPath(path, params) {
	/** @type {Record<string, Api.Scalar>} */
	const given = params || {};
	/** @type {Record<string, boolean>} */
	const taken = {};
	let out = '';
	let at = 0;
	while (at < path.length) {
		const open = path.indexOf('{', at);
		if (open === -1) {
			out += path.slice(at);
			break;
		}
		const close = path.indexOf('}', open);
		if (close === -1) {
			throw new Error('api: unterminated placeholder in ' + path);
		}
		const name = path.slice(open + 1, close);
		if (!Object.prototype.hasOwnProperty.call(given, name)) {
			throw new Error('api: ' + path + ' wants ' + name);
		}
		const value = given[name];
		if (value === undefined || value === null || value === '') {
			throw new Error('api: ' + path + ' wants a value for ' + name);
		}
		out += path.slice(at, open) + encodeURIComponent(String(value));
		taken[name] = true;
		at = close + 1;
	}
	/* A value handed over for a name the path does not carry is a mistake at
	   the call site and is said so here. Silently dropping it would send a
	   request that is short of what the caller meant and answer 200 to it. */
	for (const name of Object.keys(given)) {
		if (taken[name] !== true) {
			throw new Error('api: ' + path + ' has no ' + name);
		}
	}
	return out;
}

/* The query, in the order of its names and not in the order they were written. The store
   keys an entry by the address it asks for, so two screens asking for one thing in two
   spellings have to arrive at one key, and an assertion over this function then holds the
   address and not the order of the object literal at the call site.

   A name whose value is undefined or null is left out: handing it over would spell the word
   "undefined" into the address, and the server refuses a value it cannot read rather than
   ignoring it (src/httpd/router.cpp). */
/**
 * @param {Record<string, Api.Scalar | null | undefined> | null | undefined} query
 * @returns {string}
 */
function buildQuery(query) {
	if (!query) {
		return '';
	}
	const parts = [];
	for (const name of Object.keys(query).sort()) {
		const value = query[name];
		if (value === undefined || value === null) {
			continue;
		}
		if (typeof value === 'object') {
			// No route here takes a list or a nested value in its query
			// (src/httpd/endpoint.h), so there is no spelling for one that would
			// be right and this says so rather than sending [object Object].
			throw new Error('api: ' + name + ' is not a plain value');
		}
		parts.push(encodeURIComponent(name) + '=' + encodeURIComponent(String(value)));
	}
	return parts.join('&');
}

// The whole address one call asks for. Exported because it is the one piece of
// this file that can be asserted without a box behind it.
/**
 * @param {string} path
 * @param {Record<string, Api.Scalar> | null | undefined} params
 * @param {Record<string, Api.Scalar | null | undefined> | null | undefined} [query]
 * @returns {string}
 */
export function buildUrl(path, params, query) {
	const filled = fillPath(path, params);
	const search = buildQuery(query);
	return search === '' ? filled : filled + '?' + search;
}

/* Everything a call may say about itself besides the values for the template.
   Named here because the short spelling below is told apart from the long one
   by exactly this list. */
const kOptionNames = ['params', 'query', 'body', 'signal', 'accept', 'recover', 'touches'];

/* The values for the template, out of what a call handed over. Two spellings, the short one
   being what most calls are:

       api('GET', '/api/v1/settings/{section}', { section: 'general' })
       api('GET', '/api/v1/epg', { query: { channel: id } })

   A call whose options say nothing but the names the template asks for is the first;
   anything naming one of the options above is the second. The two cannot be confused, no
   template in this API carrying a piece named after one of those options. */
/**
 * @param {string} path
 * @param {Record<string, unknown> | null | undefined} options
 * @returns {Record<string, Api.Scalar> | null}
 */
export function valuesFor(path, options) {
	/** @type {Record<string, unknown>} */
	const opts = options || {};
	if (opts.params !== undefined) {
		return /** @type {Record<string, Api.Scalar>} */ (opts.params);
	}
	if (path.indexOf('{') === -1) {
		return null;
	}
	for (const option of kOptionNames) {
		if (opts[option] !== undefined) {
			return null;
		}
	}
	/* The short spelling: what is left is the values for the template, and the
	   template is the one thing that says what they are. A value that is not a
	   plain one reaches fillPath and is refused there by name. */
	return /** @type {Record<string, Api.Scalar>} */ (opts);
}

/* What the answer is turned into. json is the answer for almost everything; blob is what a
   screenshot and a file download are, text is what a playlist is, and response hands the
   answer over untouched for the one caller that needs a header off it. */
/**
 * @param {Api.Accept | undefined} kind
 * @returns {string}
 */
function acceptHeader(kind) {
	return kind === 'json' || kind === undefined ? 'application/json' : '*/*';
}

/**
 * The answer, read the way the caller asked for it. What comes back is the bytes of a
 * document off the network, so it is unknown here and is given its shape at the one call.
 *
 * @param {Response} response
 * @param {Api.Accept | undefined} kind
 * @returns {Promise<unknown>}
 */
async function readAnswer(response, kind) {
	if (kind === 'response') {
		return response;
	}
	if (response.status === 204 || response.status === 205) {
		return null;
	}
	if (kind === 'blob') {
		return response.blob();
	}
	if (kind === 'text') {
		return response.text();
	}
	/* An empty body under a 200 is not an error and is not JSON either: the answer to
	   a route that states no shape is nothing at all. Read as text first so this tells
	   the two apart rather than throwing a parser's sentence about position nought. */
	const body = await response.text();
	if (body === '') {
		return null;
	}
	try {
		return JSON.parse(body);
	} catch (e) {
		throw new ApiError(toProblem(null, response.status), '');
	}
}

// The refusal the box wrote, read out of the answer. A refusal whose body is not a problem
// document still comes back as one, which is what toProblem is for.
/**
 * @param {Response} response
 * @returns {Promise<Api.Problem>}
 */
async function readProblem(response) {
	/** @type {unknown} */
	let body = null;
	try {
		body = await response.json();
	} catch (e) {
		body = null;
	}
	return toProblem(body, response.status);
}

/**
 * @param {string} method
 * @param {string} url
 * @param {Api.Options<string, Api.Accept>} options
 * @param {boolean} mayRecover whether a refusal may be answered by asking for
 *        a fresh token and sending once more
 * @param {{ code: number }} seen filled in with what the box answered
 * @returns {Promise<unknown>}
 */
async function send(method, url, options, mayRecover, seen) {
	/** @type {RequestInit & { headers: Record<string, string> }} */
	const init = {
		method: method,
		/* Said rather than left to the default it happens to be: the whole of
		   what signs this page in is a cookie, and a request sent without it is
		   an anonymous request that gets a refusal nobody can act on. */
		credentials: 'same-origin',
		headers: { 'Accept': acceptHeader(options.accept) },
		signal: options.signal,
	};

	if (options.body !== undefined && options.body !== null) {
		/* A plain object is this server's own body shape: one flat JSON object
		   (src/httpd/endpoint.h). Anything else is bytes somebody means to put on the box
		   as they are, which is what PUT /api/v1/storage/file takes. */
		if (typeof options.body === 'object' && options.body.constructor === Object) {
			init.headers['Content-Type'] = 'application/json';
			init.body = JSON.stringify(options.body);
		} else {
			// Bytes the caller means to put on the box as they are, which is
			// the branch the comment above is about. Nothing here knows or
			// wants to know which of the forms fetch takes they arrived in.
			init.body = /** @type {BodyInit} */ (options.body);
		}
	}

	/* Only on what changes something, and under the name the session itself states
	   (csrf_header, src/httpd/ep/ep_auth.cpp). The name is not written a second time
	   in this application: the box says what it is, on every session answer. */
	if (!isSafe(method)) {
		const header = session.csrfHeader();
		const token = session.csrf();
		if (header !== '' && token !== '') {
			init.headers[header] = token;
		}
	}

	let response;
	try {
		response = await fetch(url, init);
	} catch (caught) {
		// fetch rejects for two things that are nothing alike: the page stopped
		// the call, and the box did not answer. What it rejects with is whatever
		// the browser threw, which is why it arrives unknown and is read here.
		const error = /** @type {{ name?: unknown, message?: unknown } | null} */ (caught);
		if (error && error.name === 'AbortError') {
			throw abortError();
		}
		throw offlineError(error && typeof error.message === 'string' ? error.message : '');
	}

	/* Before the answer is read, so it is there whichever way this ends. 202 and 204 both
	   come back as nothing at all and are opposite statements: the box has done it, or the
	   box has the message in front of it. */
	seen.code = response.status;

	if (response.ok) {
		return readAnswer(response, options.accept);
	}

	const problem = await readProblem(response);

	/* The one retry, and why it is one.

	   A request that changes something and arrives with a live cookie but without the matching
	   second token is refused 403 (auth.cpp, allowed), and so is one whose session has run out,
	   because a caller holding a dead cookie is granted what the network grants it, which is
	   below Write. Both are the ordinary state of a tab left open a while.

	   Exactly once. A second round would be a loop nobody sees, and the state that would cause
	   it is a refusal that has nothing to do with the token: a bearer credential below the
	   level a route asks for is refused with the same status and the same code. */
	const recoverable = response.status === 401 || (response.status === 403 && !isSafe(method));
	if (mayRecover && options.recover !== false && recoverable) {
		/* recover: 'token' is the middle setting, and the two calls that use it are the
		   two this whole mechanism is made of. Signing in and signing out are themselves
		   requests that change something, so a page holding a live cookie and no token is
		   refused both. What must not happen there is the sheet: a wrong password answered
		   by opening the sheet that just sent it is a circle with a person inside it. */
		const again = await session.recover(response.status, options.recover !== 'token');
		if (again) {
			return send(method, url, options, false, seen);
		}
	}

	throw new ApiError(problem, '');
}

/* One call to the box.

   method and path are literals at the call site, always, and the path is the document's own
   template. The values for it are handed over beside it, query is the query, body is a flat
   object for everything this server declares a body for and raw bytes for the one route that
   takes those. signal comes from the screen's own AbortController, so a screen that is left
   behind stops drawing rather than drawing over the one that replaced it.

   The answer is typed off the address: the method and the template together are how the
   document names a route, and the shapes generated out of that document are keyed the same
   way. An address this API does not have answers unknown. Api.Answer and not Api.Result,
   because a caller that asked for bytes gets bytes whatever the document says. */
/**
 * @template {string} M
 * @template {string} P
 * @template {Api.Accept} [A='json']
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, A>} [options]
 * @param {{ code: number }} [seen] filled in with the status the box answered
 *        with, for the one caller that has to tell a change the box carried out
 *        from one it has only taken
 * @returns {Promise<Api.Answer<`${Uppercase<M>} ${P}`, A>>}
 */
export async function api(method, path, options, seen) {
	const verb = String(method).toUpperCase();
	/** @type {Record<string, unknown>} */
	const opts = options || {};
	/* Only this box's own API, and nothing else. The old surface is not spoken here, not even
	   as a fallback, and neither is another host: the second address this page learned to speak
	   to would be the beginning of two of them. Stated as what is allowed. */
	if (path.indexOf('/api/') !== 0) {
		throw new Error('api: ' + path + ' is not this API');
	}
	const url = buildUrl(path, valuesFor(path, opts), /** @type {Record<string, Api.Scalar> | undefined} */ (opts.query));
	/* The one cast in this application, and where it belongs. What came back is bytes off the
	   network and is unknown until something says otherwise, and the thing that says otherwise
	   is the document the server writes about itself. test/check-web-paths.sh holds every
	   address written in this tree to that same document. */
	return /** @type {Promise<Api.Answer<`${Uppercase<M>} ${P}`, A>>} */ (
		send(verb, url, /** @type {Api.Options<string, Api.Accept>} */ (opts), true, seen || { code: 0 }));
}
