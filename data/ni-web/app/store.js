/* One answer per question, and one question at a time.

   A hundred and fifty lines instead of a library, because what this page needs out of one
   is four things, and all four are things this box in particular makes necessary.

   One request per key. The server runs four worker threads and takes sixty four
   connections, and the channel list is the one answer that costs it real work.

   Invalidation by prefix. An event says the timers changed. It does not say which page of
   which listing, and it could not: what the stream carries is a type and a channel.

   Three named loads: first load, reload, and write. A reload draws no empty screen, which
   is the difference between a page that flickers on every event and one that does not.

   A write makes stale what it says it touches. That statement is the caller's, because
   nothing here could work it out, and it is also the answer to what has to be read again.
   Except where the box only took the message: then it has not changed yet, and what
   corrects the page is the event. */

import { api, buildUrl, valuesFor } from './api.js';
import { isAborted } from './problem.js';

// What an entry is. A screen draws by state and never by whether data happens
// to be there.
export const EMPTY = 'empty';
export const LOADING = 'loading';
export const READY = 'ready';
export const FAILED = 'error';

// And, while it is loading, which of the three loads this is.
export const FIRST = 'first';
export const AGAIN = 'again';
export const WRITING = 'writing';

// The one status this file reads: the box has the change and has not made it yet.
const kTaken = 202;

/**
 * One address this store holds, and everything it knows about it.
 *
 * data is unknown here and is given its shape at the door: load, reload and watch are each
 * generic over the address they are called with.
 *
 * @typedef {object} Entry
 * @property {string} key
 * @property {string} method
 * @property {string} path
 * @property {Record<string, Api.Scalar> | null} params
 * @property {Record<string, Api.Scalar> | undefined} query
 * @property {string} url
 * @property {Web.State} state
 * @property {Web.Phase} phase
 * @property {unknown} data
 * @property {Web.Failure | null} error
 * @property {number} at
 * @property {Array<(snapshot: Web.Snapshot<unknown>) => void>} watchers
 * @property {Promise<unknown> | null} pending
 * @property {AbortController | null} control
 */

/** @type {Map<string, Entry>} */
const entries = new Map();

/**
 * @param {string} method
 * @param {string} url
 * @returns {string}
 */
function keyOf(method, url) {
	return method + ' ' + url;
}

/**
 * @param {string} method
 * @param {string} path
 * @param {Record<string, unknown> | null | undefined} options
 * @returns {Entry}
 */
function entryFor(method, path, options) {
	/** @type {Record<string, unknown>} */
	const opts = options || {};
	const verb = String(method).toUpperCase();
	const values = valuesFor(path, opts);
	const query = /** @type {Record<string, Api.Scalar> | undefined} */ (opts.query);
	const url = buildUrl(path, values, query);
	const key = keyOf(verb, url);
	let entry = entries.get(key);
	if (entry === undefined) {
		entry = {
			key: key,
			method: verb,
			path: path,
			params: values,
			query: query,
			url: url,
			state: EMPTY,
			phase: '',
			data: null,
			error: null,
			at: 0,
			watchers: [],
			pending: null,
			control: null,
		};
		entries.set(key, entry);
	}
	return entry;
}

/**
 * @param {Entry} entry
 * @returns {Web.Snapshot<unknown>}
 */
function snapshot(entry) {
	return {
		state: entry.state,
		phase: entry.phase,
		data: entry.data,
		error: entry.error,
		at: entry.at,
		url: entry.url,
	};
}

/**
 * @param {Entry} entry
 * @returns {void}
 */
function announce(entry) {
	const now = snapshot(entry);
	// A copy of the list, because a watcher may stop watching from inside its
	// own call and splicing the list this loop walks would skip its neighbour.
	for (const watcher of entry.watchers.slice()) {
		watcher(now);
	}
}

/**
 * @param {Entry} entry
 * @param {Web.Phase} phase
 * @returns {void}
 */
function begin(entry, phase) {
	entry.state = LOADING;
	entry.phase = phase;
	announce(entry);
}

/**
 * @param {Entry} entry
 * @param {unknown} data
 * @returns {void}
 */
function finish(entry, data) {
	entry.state = READY;
	entry.phase = '';
	entry.data = data;
	entry.error = null;
	entry.at = Date.now();
	entry.pending = null;
	entry.control = null;
	announce(entry);
}

/**
 * @param {Entry} entry
 * @param {unknown} error
 * @returns {void}
 */
function fail(entry, error) {
	entry.pending = null;
	entry.control = null;
	if (isAborted(error)) {
		/* A load this store stopped itself leaves the entry as it was. It is stopped
		   because the whole store is being emptied or because the answer is no longer
		   wanted, and in neither case is there a fault to draw. */
		entry.state = entry.data === null ? EMPTY : READY;
		entry.phase = '';
		announce(entry);
		return;
	}
	entry.state = FAILED;
	entry.phase = '';
	entry.error = /** @type {Web.Failure} */ (error);
	announce(entry);
}

/* One load, or the one that is already running. The controller belongs to the store and
   not to the caller: two screens share this request, so one of them leaving must not stop
   the answer the other is waiting for. */
/**
 * @param {Entry} entry
 * @param {Web.Phase} phase
 * @returns {Promise<unknown>}
 */
function start(entry, phase) {
	if (entry.pending !== null) {
		return entry.pending;
	}
	const control = new AbortController();
	entry.control = control;
	begin(entry, phase);
	/* The store asks with a method and a template it was handed, so the one call
	   cannot tell it what the answer looks like and hands back unknown. Every door
	   out of this file is generic over the address its caller wrote. */
	const running = api(entry.method, entry.path, {
		params: entry.params,
		query: entry.query,
		signal: control.signal,
	}).then(function (data) {
		if (entry.control === control) {
			finish(entry, data);
		}
		return data;
	}, function (error) {
		if (entry.control === control) {
			fail(entry, error);
		}
		throw error;
	});
	entry.pending = running;
	return running;
}

/**
 * What is known right now, without asking for anything.
 *
 * @template {string} M
 * @template {string} P
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, 'json'> | null} [options]
 * @returns {Web.Snapshot<Api.Result<`${Uppercase<M>} ${P}`>>}
 */
export function read(method, path, options) {
	return /** @type {Web.Snapshot<Api.Result<`${Uppercase<M>} ${P}`>>} */ (
		snapshot(entryFor(method, path, /** @type {Record<string, unknown> | null | undefined} */ (options))));
}

/* The answer, asking for it if it is not there. A caller that arrives while somebody else
   is asking waits on that one request. */
/**
 * @template {string} M
 * @template {string} P
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, 'json'> | null} [options]
 * @returns {Promise<Api.Result<`${Uppercase<M>} ${P}`>>}
 */
export function load(method, path, options) {
	const entry = entryFor(method, path, /** @type {Record<string, unknown> | null | undefined} */ (options));
	/** @type {Promise<unknown>} */
	const answer = (entry.state === READY && entry.pending === null)
		? Promise.resolve(entry.data)
		: start(entry, entry.data === null ? FIRST : AGAIN);
	return /** @type {Promise<Api.Result<`${Uppercase<M>} ${P}`>>} */ (answer);
}

// The same question asked again on purpose, whatever is held. The phase says
// which of the two loads this is, so a reload keeps what is on the screen.
/**
 * @template {string} M
 * @template {string} P
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, 'json'> | null} [options]
 * @returns {Promise<Api.Result<`${Uppercase<M>} ${P}`>>}
 */
export function reload(method, path, options) {
	const entry = entryFor(method, path, /** @type {Record<string, unknown> | null | undefined} */ (options));
	/** @type {Promise<unknown>} */
	const answer = entry.pending !== null
		? entry.pending
		: start(entry, entry.data === null ? FIRST : AGAIN);
	return /** @type {Promise<Api.Result<`${Uppercase<M>} ${P}`>>} */ (answer);
}

/* Watching one key. The watcher is called with every change of that entry, beginning with
   what is known at the moment it starts watching. The function it hands back stops the
   watching; a screen that does not call it leaves a watcher behind that draws into a screen
   nobody is looking at. */
/**
 * @template {string} M
 * @template {string} P
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, 'json'> | null} options
 * @param {(snapshot: Web.Snapshot<Api.Result<`${Uppercase<M>} ${P}`>>) => void} watcher
 * @returns {() => void} stops the watching
 */
export function watch(method, path, options, watcher) {
	const entry = entryFor(method, path, /** @type {Record<string, unknown> | null | undefined} */ (options));
	const listener = /** @type {(snapshot: Web.Snapshot<unknown>) => void} */ (watcher);
	entry.watchers.push(listener);
	listener(snapshot(entry));
	if (entry.state === EMPTY && entry.pending === null) {
		start(entry, FIRST).catch(function () {
			// The entry carries the fault and every watcher has been told
			// about it. Nothing here is left to do with it, and an unhandled
			// rejection in the console would say otherwise.
		});
	}
	return function () {
		const at = entry.watchers.indexOf(listener);
		if (at !== -1) {
			entry.watchers.splice(at, 1);
		}
	};
}

/* Everything under any of these pieces of an address is no longer to be trusted. What
   nobody is looking at is dropped rather than reloaded, because it costs the box four
   worker threads to answer a question nobody asked.

   ask is what separates the two callers. An event says the box has changed, so what is on
   screen is asked again at once. A write the box has only taken says it is about to, and a
   read sent there is answered about the box before it acted. */
/**
 * @param {readonly string[]} prefixes
 * @param {boolean} ask whether what somebody is watching is asked again now
 * @returns {void}
 */
function stale(prefixes, ask) {
	/** @type {string[]} */
	const keys = [];
	entries.forEach(function (entry) {
		for (const prefix of prefixes) {
			if (entry.url.indexOf(prefix) === 0) {
				keys.push(entry.key);
				return;
			}
		}
	});
	for (const key of keys) {
		const entry = entries.get(key);
		if (entry === undefined) {
			continue;
		}
		if (entry.watchers.length === 0) {
			entries.delete(entry.key);
			continue;
		}
		if (ask && entry.pending === null) {
			start(entry, entry.data === null ? FIRST : AGAIN).catch(function () {
				// As above: the entry holds the fault and the watchers know.
			});
		}
	}
}

/**
 * @param {string} prefix
 * @returns {void}
 */
export function invalidate(prefix) {
	stale([prefix], true);
}

/* Everything, forgotten. What the stream missed while it was down is everything this store
   holds, and there is no identifier to replay from (src/httpd/events.cpp sends no id), so
   the honest answer to a stream coming back is that nothing here is known any more. */
/** @returns {void} */
export function clear() {
	/** @type {string[]} */
	const keys = [];
	entries.forEach(function (entry) {
		keys.push(entry.key);
	});
	for (const key of keys) {
		const entry = entries.get(key);
		if (entry === undefined) {
			continue;
		}
		if (entry.control !== null) {
			entry.control.abort();
			entry.control = null;
			entry.pending = null;
		}
		if (entry.watchers.length === 0) {
			entries.delete(entry.key);
			continue;
		}
		entry.data = null;
		entry.state = EMPTY;
		start(entry, FIRST).catch(function () {
			// As above.
		});
	}
}

/* What a screen believes, written into the store without asking the box. This is how a
   page stays still after a write it knows the outcome of: it states the new value here,
   draws it, and lets the event correct it. Nothing is sent, and an entry nobody has ever
   loaded is left alone rather than invented. */
/**
 * @template {string} M
 * @template {string} P
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, 'json'> | null} options
 * @param {Api.Result<`${Uppercase<M>} ${P}`>} data
 * @returns {void}
 */
export function put(method, path, options, data) {
	const entry = entryFor(method, path, /** @type {Record<string, unknown> | null | undefined} */ (options));
	if (entry.state === EMPTY) {
		return;
	}
	entry.data = data;
	entry.state = READY;
	entry.phase = '';
	entry.at = Date.now();
	announce(entry);
}

/* A call that changes something. The entries the caller says it touches are marked while
   it is in flight, so a screen can grey the thing it is changing without inventing a
   loading state of its own, and they stop being true when it lands.

   touches is the caller's statement about its own call. There is nothing here that could
   work it out: a POST to /api/v1/zap changes what /api/v1/channels/current answers, and
   only the caller knows that. It is also the answer to what has to be read again, so the
   one list is sent through the door an event uses.

   Asked again only where the box says it carried the change out. 202 is the box saying it
   has the message and no more (src/httpd/endpoints.h, accepted): a read sent then answers
   about the box before it acted, and for the one that reads the channel lists again it
   would read a list being rebuilt under it. Those reach the page on the stream. */
/**
 * @template {string} M
 * @template {string} P
 * @template {Api.Accept} [A='json']
 * @param {M} method
 * @param {P} path
 * @param {Api.Options<`${Uppercase<M>} ${P}`, A>} [options]
 * @returns {Promise<Api.Answer<`${Uppercase<M>} ${P}`, A>>}
 */
export async function write(method, path, options) {
	/** @type {Api.Options<string, Api.Accept>} */
	const opts = /** @type {Api.Options<string, Api.Accept>} */ (options || {});
	/** @type {Entry[]} */
	const touched = [];
	const prefixes = opts.touches || [];
	entries.forEach(function (entry) {
		for (const prefix of prefixes) {
			if (entry.url.indexOf(prefix) === 0) {
				touched.push(entry);
				return;
			}
		}
	});
	for (const entry of touched) {
		if (entry.state === READY) {
			entry.phase = WRITING;
			announce(entry);
		}
	}
	/** @type {{ code: number }} */
	const seen = { code: 0 };
	try {
		return /** @type {Api.Answer<`${Uppercase<M>} ${P}`, A>} */ (await api(method, path, opts, seen));
	} finally {
		for (const entry of touched) {
			if (entry.phase === WRITING) {
				entry.phase = '';
				announce(entry);
			}
		}
		/* From the prefixes and not from the entries marked above, a screen that arrived
		   while the call was in flight watching one of them too. A refusal, and a call that
		   never reached the box, leave everything alone: nothing changed. */
		if (seen.code >= 200 && seen.code < 300) {
			stale(prefixes, seen.code !== kTaken);
		}
	}
}
