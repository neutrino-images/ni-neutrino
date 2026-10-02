// Where the page starts. It draws the frame, asks the box which language it is in,
// and keeps the one line of state the frame itself shows: what is playing, what is
// recording, and who is signed in. Everything else belongs to a screen.
//
// This and the sign in sheet are the only files that reach into the layer that talks
// to the box; the sheet is the second because the control that opens it is in the
// corner of the bar. What this file expects of that layer is four things:
//
//   api(method, path, options)  one call, spelled the way the document spells it
//   session.start()             the one ask at start for what this request was
//                               granted, since a caller from a private network is
//                               granted reading with no session at all
//   session.subscribe(fn)       told when that changes
//   events.start()              the one stream for the whole page,
//   events.subscribe(fn)        what came over it, so the frame can read again, and
//   events.watchStatus(fn)      whether that stream is carrying, which is the one
//                               thing nothing else can show: every screen refreshes
//                               itself off it, and a page whose stream has fallen
//                               looks right while it goes stale
//
// Everything the frame draws is handed down as a plain value.
import { html, render, Fragment, useState, useEffect } from './runtime.js';
import { Shell } from './router.js';
import { SignIn } from './ui/signin.js';
import { WakeQuestion } from './ui/wake.js';
import { setLanguage, pickLanguage, onLanguage } from './i18n.js';
import { api } from './api.js';
import { channelId } from './fmt.js';
import { isTimeshift } from './fmt.js';
import { restore as restoreTextSize } from './ui/textsize.js';
/* Whole modules and not a named value out of each: neither of these exports
   one. An import of a name a module does not export is refused by the browser
   at load time, before a line of either file runs, and what the page then
   shows is nothing at all. */
import * as session from './session.js';
import * as events from './events.js';
import * as store from './store.js';

// The header's progress share moves with the clock and not with any event.
const CLOCK_MS = 60000;

// What the box calls itself in, as one of the two this page has words for.
// A box that has never been given a language answers with an empty name, which
// is the box picking, and then the browser is asked.
/** @returns {Promise<string>} */
function readLanguage() {
	/* The template and the value apart, not the address already put together.
	   A page that glues addresses loses the template, and then nothing can
	   hold its calls to the document this server writes about itself. */
	return api("GET", "/api/v1/settings/{section}", { section: "general" }).then(function (answer) {
		const rows = (answer && answer.items) || [];
		for (const row of rows) {
			if (row.id === 'language')
				return row.value;
		}
		return '';
	}, function () {
		// A build without that section is a build that cannot say, not a
		// failure worth showing anybody.
		return '';
	});
}


/* What this build carries, which the page cannot see for itself: the API
   documentation is a switch at compile time, and the destination that leads
   into it is left out of the bars where the answer says no. Asked once, at
   start, because a build does not change under a page that is open. */
/** @returns {Promise<{ apiDoc: boolean }>} */
function readBuild() {
	return api("GET", "/api/v1/system/info").then(function (box) {
		nameTab(box && box.hostname ? box.hostname : '');
		return { apiDoc: !!(box && box.api_doc) };
	}, function () {
		/* A box that would not answer is not a box without documentation. What
		   is offered then is everything, and the destination says for itself
		   what it finds when it is opened. */
		return { apiDoc: true };
	});
}

/* Whose box this tab is on. Somebody with three of them has three tabs that
   said the same word, and the browser gives a tab little more than that word
   to tell them apart. The name the box answers to is what its owner typed to
   get here, so it is the half that distinguishes.

   A box that will not say keeps the plain name rather than showing an empty
   half, and the name goes after the product so that a narrowed tab keeps the
   part every tab shares and drops the part only this one has. That is the
   wrong way round for telling them apart, and it is still right: a tab
   narrowed that far shows too few characters for either. */
/**
 * @param {string} host
 * @returns {void}
 */
function nameTab(host) {
	document.title = '';
	const trimmed = host.replace(/^\s+|\s+$/g, '');
	if (trimmed !== '')
		document.title = trimmed + ' - ';
	document.title += 'NI-Web';
}

/** @returns {Web.Drawn} */
function App() {
	/* Said and not left to the first value: every member of this starts empty,
	   so what it would settle on is a frame that can only ever be empty. */
	const [status, setStatus] = useState(/** @type {Web.ShellStatus} */ ({
		channel: null, event: null, recordings: [], standby: null, session: null, build: null, carrying: false,
	}));
	// The words on screen change when the language does, and nothing else does,
	// so the frame is drawn again and the catalogue answers differently.
	const redraw = useState(0)[1];

	useEffect(function () {
		let alive = true;

		function bump() {
			redraw(function (n) { return n + 1; });
		}

		const stopLanguage = onLanguage(bump);

		readLanguage().then(function (named) {
			if (alive)
				setLanguage(pickLanguage(named, navigator.languages || [navigator.language]));
		});

		/**
		 * @param {import('./session.js').Session} granted
		 * @returns {void}
		 */
		function told(granted) {
			if (alive)
				setStatus(function (was) {
					return {
						channel: was.channel, event: was.event, recordings: was.recordings, standby: was.standby,
						session: granted, build: was.build, carrying: was.carrying,
					};
				});
		}

		Promise.resolve(session.start()).then(told, function () { /* Public route or not, a page that cannot ask still draws. */ });

		const stopSession = session.subscribe(told);

		readBuild().then(function (build) {
			if (alive)
				setStatus(function (was) {
					return {
						channel: was.channel, event: was.event, recordings: was.recordings, standby: was.standby,
						session: was.session, build: build, carrying: was.carrying,
					};
				});
		});

		/* Watch channel through the store, read from the same source the card uses. */
		const stopChannel = store.watch('GET', '/api/v1/channels/current', null, function (snapshot) {
			if (alive) {
				const channel = snapshot.state === store.READY ? snapshot.data : null;
				setStatus(function (was) {
					return {
						channel: channel, event: was.event, recordings: was.recordings, standby: was.standby,
						session: was.session, build: was.build, carrying: was.carrying,
					};
				});
			}
		});

		/* Watch recordings through the store, filtering out timeshifts. */
		const stopRecordings = store.watch('GET', '/api/v1/recordings', null, function (snapshot) {
			if (alive) {
				const answer = snapshot.state === store.READY ? snapshot.data : null;
				const rows = (answer && answer.items) || [];
				const recordings = rows.filter(function (row) { return !isTimeshift(row); });
				setStatus(function (was) {
					return {
						channel: was.channel, event: was.event, recordings: recordings, standby: was.standby,
						session: was.session, build: was.build, carrying: was.carrying,
					};
				});
			}
		});

		/* Watch standby state through the store. */
		const stopStandby = store.watch('GET', '/api/v1/system/standby', null, function (snapshot) {
			if (alive) {
				const mode = snapshot.state === store.READY ? snapshot.data : null;
				setStatus(function (was) {
					return {
						channel: was.channel, event: was.event, recordings: was.recordings, standby: mode,
						session: was.session, build: was.build, carrying: was.carrying,
					};
				});
			}
		});

		/* Watch current epg event. This depends on having a channel, so only watch
		   when the channel changes. */
		let stopEvent = function () { };
		const stopChannelForEvent = store.watch('GET', '/api/v1/channels/current', null, function (snapshot) {
			if (!alive) return;
			stopEvent();
			const channel = snapshot.state === store.READY ? snapshot.data : null;
			if (!channel) {
				setStatus(function (was) {
					return {
						channel: was.channel, event: null, recordings: was.recordings, standby: was.standby,
						session: was.session, build: was.build, carrying: was.carrying,
					};
				});
				return;
			}
			const guideId = channelId(channel.epg_id) || channelId(channel.id);
			if (!guideId) {
				return;
			}
			stopEvent = store.watch('GET', '/api/v1/epg/current', { query: { channel: guideId } }, function (snapshot) {
				if (alive) {
					const event = snapshot.state === store.READY ? snapshot.data : null;
					setStatus(function (was) {
						return {
							channel: was.channel, event: event, recordings: was.recordings, standby: was.standby,
							session: was.session, build: was.build, carrying: was.carrying,
						};
					});
				}
			});
		});

		/* Whether that stream is carrying, which the mark in the corner is
		   coloured by. Watched here rather than where it is drawn, because the
		   header is the frame, and the frame reaches the box through this file
		   and the sign in sheet and nowhere else.

		   The state is handed back unchanged where the answer has not changed:
		   this is told on every open and every close, and a new object each time
		   would draw the whole frame again for a value that stayed put. */
		const stopWatching = events.watchStatus(function (/** @type {import('./events.js').StreamStatus} */ stream) {
			if (alive)
				setStatus(function (was) {
					if (was.carrying === stream.carrying)
						return was;
					return {
						channel: was.channel, event: was.event, recordings: was.recordings, standby: was.standby,
						session: was.session, build: was.build, carrying: stream.carrying,
					};
				});
		});

		events.start();
		const clock = setInterval(bump, CLOCK_MS);

		return function () {
			alive = false;
			clearInterval(clock);
			stopLanguage();
			if (typeof stopSession === 'function')
				stopSession();
			stopChannel();
			stopRecordings();
			stopStandby();
			stopChannelForEvent();
			stopEvent();
			stopWatching();
		};
	}, []);

	/* The sheet is a sibling of the frame and not a part of it. It lies over
	   everything, it has to outlive whichever screen asked for the write that
	   raised it, and the frame is drawn from what the box is doing, which is
	   not what a sign in is about. */
	return html`<${Fragment}>
		<${Shell} status=${status} ctx=${{ api: api, session: session, events: events }} />
		<${SignIn} />
		<${WakeQuestion} />
	<//>`;
}

/* Before the first line is drawn and not inside a screen: a reader who needs
   the larger writing would otherwise be shown the smaller one first and see it
   jump. It touches one property of the root element and asks the box nothing,
   so there is no answer to wait for. */
restoreTextSize();

/* The one element the document carries, and the page cannot be drawn without
   it. Said rather than assumed, because a document that lost it would
   otherwise fail inside the runtime with a sentence about a property of null. */
const root = document.getElementById('app');
if (!root) {
	throw new Error('main: the document carries no element with the identifier app');
}
render(html`<${App} />`, root);
