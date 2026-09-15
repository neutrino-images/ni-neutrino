/* The shape every refusal from this box arrives in, and the one place that turns it into
   something a screen can draw.

   The server answers application/problem+json for everything it turns down, with four
   members: type, title, status and detail. A screen draws title as the heading and detail
   as the text, and neither is written a second time anywhere in this application. What a
   screen must not do is read the status and make up a sentence of its own. */

import { word } from './fmt.js';

/* One value, however it arrived, as the four members a screen draws.

   Every member is always there, and all but the status are strings. A shape whose members
   came and went would make every screen test each one before drawing it.

   A body that is not a problem document at all, which is what a proxy in the way or a page
   of HTML answers, is drawn like any other refusal rather than dropped. */
/**
 * @param {unknown} value whatever came back, which may be no problem document
 *        at all: a proxy in the way answers a page of HTML
 * @param {number} status
 * @returns {Api.Problem}
 */
export function toProblem(value, status) {
	const code = Number.isFinite(status) ? status : 0;
	if (!value || typeof value !== 'object') {
		return {
			type: '',
			title: word('problem.unknown.title'),
			status: code,
			detail: word('problem.unknown.detail'),
		};
	}
	/* Read member by member rather than trusted whole. This is the one place
	   in the page where bytes off the network are turned into a shape, and
	   every member is tested for what it is before it is taken. */
	const body = /** @type {Record<string, unknown>} */ (value);
	return {
		type: typeof body['type'] === 'string' ? body['type'] : '',
		title: (typeof body['title'] === 'string' && body['title'] !== '')
			? body['title'] : word('problem.unknown.title'),
		status: typeof body['status'] === 'number' && Number.isFinite(body['status'])
			? body['status'] : code,
		detail: typeof body['detail'] === 'string' ? body['detail'] : '',
	};
}

/* Where a problem may be looked up, and the empty string for one that may not.

   type is a member of a document that came off the network, so a page putting it into an
   href unread would let an answer choose a destination: javascript: runs in this page,
   data: draws a page of somebody else's making inside this origin, and mailto: opens a
   program. A value is therefore linked only when it is this box's own absolute path
   ("/errors/<code>", src/httpd/status.cpp) or an https address. The pair of slashes is
   refused although it begins with one: "//example.com" is another origin wearing the
   leading slash of a local path. */
/**
 * @param {Api.Problem | null | undefined} problem
 * @returns {string} where it may be looked up, and empty for one that may not
 */
export function problemHref(problem) {
	const type = (problem && typeof problem.type === 'string') ? problem.type : '';
	if (type.startsWith('//')) {
		return '';
	}
	if (type.startsWith('/') || type.startsWith('https://')) {
		return type;
	}
	return '';
}

/* What is thrown out of every call, so one catch in a screen sees every way a call
   can fail: a refusal the box wrote, a box that did not answer at all, and a call
   the page itself stopped. The message is the detail and not the title, because the
   message is what ends up in the browser's console when nobody caught it, and the
   title is the same four words for every refusal carrying the same status. */
export class ApiError extends Error {
	/**
	 * @param {Api.Problem} problem
	 * @param {'aborted' | 'offline' | ''} kind
	 */
	constructor(problem, kind) {
		super(problem.detail || problem.title);
		this.name = 'ApiError';
		this.problem = problem;
		this.status = problem.status;
		/* Being stopped is not a fault, and it is told apart from one here
		   rather than by every caller reading a name off a DOMException. A
		   screen that has been left behind throws this on its way out and
		   draws nothing. */
		this.aborted = kind === 'aborted';
		/* A box that did not answer is not a box that refused. The first
		   belongs under the same bar the event stream raises when the stream
		   drops, the second is drawn as what the box said. */
		this.offline = kind === 'offline';
	}
}

/**
 * @param {unknown} error
 * @returns {boolean}
 */
export function isAborted(error) {
	return !!(error && /** @type {{ aborted?: unknown }} */ (error).aborted);
}

/**
 * @param {unknown} error
 * @returns {boolean}
 */
export function isOffline(error) {
	return !!(error && /** @type {{ offline?: unknown }} */ (error).offline);
}

// A call this page stopped itself, in the shape everything else is in.
/** @returns {ApiError} */
export function abortError() {
	return new ApiError({
		type: '',
		title: word('problem.aborted.title'),
		status: 0,
		detail: word('problem.aborted.detail'),
	}, 'aborted');
}

/* A box that did not answer. Status nought and not a code invented here: no answer came
   back, so there is no status, and writing one would put a number on the screen that nothing
   ever sent. */
/**
 * @param {string} detail
 * @returns {ApiError}
 */
export function offlineError(detail) {
	return new ApiError({
		type: '',
		title: word('problem.offline.title'),
		status: 0,
		detail: detail || word('problem.offline.detail'),
	}, 'offline');
}

// A refusal this page states for itself, in the same shape, for the one case
// where nothing was sent at all: the person closed the sign in sheet.
/**
 * @param {string} titleKey
 * @param {string} detailKey
 * @param {number} status
 * @returns {ApiError}
 */
export function localError(titleKey, detailKey, status) {
	return new ApiError({
		type: '',
		title: word(titleKey),
		status: Number.isFinite(status) ? status : 0,
		detail: word(detailKey),
	}, '');
}
