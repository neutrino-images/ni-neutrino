/* Playing a channel, and the one thing that is different about two of the four kinds.

   A channel this box tunes has no address a browser could fetch: the picture and the
   sound leave the box on its own outputs, and this screen can do nothing for it but
   ask the box to switch to it.

   A web channel does have one, and it is the channel's own. So the browser fetches it
   directly, past the box: no tuner is taken, nothing is transcoded, and the box is not
   even asked. That is the whole reason the document's policy allows media from
   anywhere and nothing else from anywhere, and why a source that will not play is
   reported here as the source failing and never as the box failing.

   Nothing plays without a click, and there is no element at all until somebody has
   pressed one, because a screen that started fetching a stream while it was being
   drawn would spend bandwidth nobody asked for, on a box whose uplink is somebody's
   home line.

   And the element is not this screen's. Both halves of the split below draw an empty
   place and let ui/playing.js put the element in it, so that a channel started here
   goes on playing while somebody walks to another screen. */

import { html, useState, useEffect, useRef } from '../../runtime.js';
import { api } from '../../api.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { isAborted } from '../../problem.js';
import { t } from '../../i18n.js';
import { channelId } from '../../fmt.js';
import { hrefFor } from '../../nav.js';
import { State } from '../../ui/state.js';
import { Button } from '../../ui/button.js';
import { toast } from '../../ui/toast.js';
import text from './playback.text.js';
import { playlistHref, playlistFile } from './stream.js';
/* The other half of playing in the browser. A channel the box tunes reaches a
   page as a transport stream with sound no browser decodes, so the box copies
   the picture, converts the sound and answers a stream of its own; ui/player.js
   is what asks for that and hands it to the demuxer. */
import { Player } from '../../ui/player.js';
/* What holds the element and the stream for both halves of the split below.
   A channel goes on playing while somebody walks to another screen, so the
   element cannot belong to this one. */
import * as playing from '../../ui/playing.js';
import { emptyWalk, beginPage, failPage, extend, wants, isWeb } from './list.paging.js';

export const css = '/app/screens/channels/playback.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'play.pick'); }


// The widest page the channel route hands out (src/httpd/ep/ep_channels.cpp).
// The search below reads the whole list once and nothing of it twice, so it
// asks for as much per round trip as the box will give.
const kSearchPage = 500;

/**
 * @param {{ param: string, ctx: Web.Context, entry: Web.NavEntry }} props
 * @returns {Web.Drawn}
 */
export default function Playback(props) {
	const wanted = channelId(props.param);

	const [channel, setChannel] = useState(/** @type {Api.Channel | null} */ (null));
	const [problem, setProblem] = useState(/** @type {Web.Failure | null} */ (null));
	const [loading, setLoading] = useState(false);
	const [current, setCurrent] = useState(/** @type {Api.Channel | null} */ (null));

	// The one channel this screen is about, when the address names one.
	useEffect(function () {
		if (wanted === '') {
			setChannel(null);
			setProblem(null);
			setLoading(false);
			return undefined;
		}
		const control = new AbortController();
		setLoading(true);
		setProblem(null);
		api('GET', '/api/v1/channels/{id}', {
			params: { id: wanted },
			signal: control.signal,
		}).then(function (one) {
			setChannel(one);
			setLoading(false);
		}, function (error) {
			if (isAborted(error)) {
				return;
			}
			setChannel(null);
			setProblem(error);
			setLoading(false);
		});
		return function () { control.abort(); };
	}, [wanted]);

	useEffect(function () {
		return store.watch('GET', '/api/v1/channels/current', {}, function (seen) {
			setCurrent(seen.state === store.READY ? seen.data : null);
		});
	}, []);

	/* Nothing here says in advance that it may not be used: the sign in is
	   asked for when the button is pressed and the write goes on after it.
	   Closing the sheet leaves the box as it was and says nothing. */
	/**
	 * @param {Api.Channel} one
	 */
	function zapTo(one) {
		session.requireWrite().then(function () {
			store.write('POST', '/api/v1/zap', {
				touches: ['/api/v1/channels/current', '/api/v1/epg/current'],
				body: { channel_id: one.id },
			}).then(function () {
				toast(t(text, 'play.zap.done', { name: one.name }));
			}, function (error) {
				toast(error && error.problem ? error.problem.title : t(text, 'play.failed'), 'bad');
			});
		}, function () {});
	}

	if (wanted !== '' && loading) {
		return html`<${State} phase="first" />`;
	}

	if (wanted !== '' && problem) {
		return html`<div class="pb-screen">
			<${State} problem=${{ title: problem.problem.title, detail: problem.problem.detail || t(text, 'play.unknown') }} />
		</div>`;
	}

	if (channel) {
		return html`<div class="pb-screen">
			<h2 class="pb-head">${channel.name}</h2>
			<p class="pb-kind">${t(text, 'play.kind.' + (channel.kind || 'unknown'))}</p>
			<${Elsewhere} key=${'where-' + channel.id} channel=${channel} />
			${isWeb(channel)
				? html`<${WebPlayer} key=${channel.id} channel=${channel} />`
				: html`<${Player} key=${channel.id} channel=${channel} />
					<${Tuned} channel=${channel} onZap=${zapTo} />`}
		</div>`;
	}

	return html`<div class="pb-screen">
		${current
			? html`<p class="pb-current">${t(text, 'play.current')}:
				${' '}<a href=${hrefFor('channels', 'playback', current.id)}>${current.name}</a></p>`
			: html`<p class="pb-current">${t(text, 'play.current.none')}</p>`}
		<p class="pb-away">
			<a class="btn" href="/api/v1/stream/playlist" download="neutrino.m3u">${t(text, 'play.m3u.all')}</a>
		</p>
		<p class="pb-why">${t(text, 'play.m3u.all.why')}</p>
		<${WebChannels} />
	</div>`;
}

/**
 * The three ways this channel leaves the browser, and the one sentence that
 * says why the first of them is first.
 *
 * A NEW TAB IS NOT A REQUEST OF THIS PAGE. A document may not pull a stream
 * off another origin into its own video element unless that origin says it
 * may, and a broadcast server has no reason to say so. A new tab is its own
 * address with its own document, and none of that applies there: whatever the
 * browser can play, it plays. So the tab is the first button and not a link
 * hidden under the player, and the player below it is the second way and not
 * the only one.
 *
 * The address is the box's own for a tuned channel and the channel's own for
 * one the browser fetches for itself, and the difference is worth nothing to
 * whoever presses the button, so it is settled here and not on screen.
 *
 * @param {{ channel: Api.Channel }} props
 * @returns {Web.Drawn}
 */
function Elsewhere(props) {
	const channel = props.channel;
	const web = isWeb(channel);
	const [where, setWhere] = useState(web ? channel.url : '');

	useEffect(function () {
		if (web) {
			setWhere(channel.url);
			return undefined;
		}
		const control = new AbortController();
		api('GET', '/api/v1/stream/{id}', {
			params: { id: channel.id },
			signal: control.signal,
		}).then(function (address) {
			setWhere(address.url);
		}, function () {
			// A box that will not say where it streams leaves the two
			// buttons that need an address dead, and the playlist beside
			// them still works: that one is a route and not an address.
			setWhere('');
		});
		return function () { control.abort(); };
	}, [channel.id, web]);

	/**
	 * @returns {void}
	 */
	function copy() {
		const board = navigator.clipboard;
		/* The clipboard is refused in more places than it is granted: an
		   insecure origin, a window that is not in front, a setting. So the
		   address is said out loud when it cannot be put anywhere, which
		   leaves it where somebody can select it. */
		if (!board || typeof board.writeText !== 'function') {
			toast(where, '');
			return;
		}
		board.writeText(where).then(function () {
			toast(t(text, 'play.copied'));
		}, function () {
			toast(where, '');
		});
	}

	return html`<div class="pb-card">
		<p class="pb-away">
			${where === ''
				? html`<${Button} primary=${true} disabled=${true}>${t(text, 'play.newtab')}<//>`
				: html`<a class="btn primary" href=${where} target="_blank" rel="noopener noreferrer">${t(text, 'play.newtab')}</a>`}
			<a class="btn" href=${playlistHref(channel)} download=${playlistFile(channel)}>${t(text, 'play.m3u')}</a>
			<${Button} disabled=${where === ''} onClick=${copy}>${t(text, 'play.copy')}<//>
		</p>
		<p class="pb-why">${t(text, 'play.newtab.why')}</p>
		${where === '' ? null : html`<p class="pb-source">${t(text, 'play.source')}: <span class="pb-url mono">${where}</span></p>`}
	</div>`;
}

/**
 * A channel the box tunes.
 *
 * @param {{ channel: Api.Channel, onZap: (one: Api.Channel) => void }} props
 * @returns {Web.Drawn}
 */
function Tuned(props) {
	const channel = props.channel;
	return html`<div class="pb-card">
		<h2>${t(text, 'play.tuned.title')}</h2>
		<p>${t(text, 'play.tuned.body', { name: channel.name })}</p>
		<p><${Button} primary=${true}
			onClick=${function () { props.onZap(channel); }}>${t(text, 'play.zap')}<//></p>
	</div>`;
}

/**
 * A channel the browser fetches for itself.
 *
 * THROUGH THE BOX AND NOT PAST IT, and through the library where the browser
 * needs one. Almost every one of these channels is a playlist, which no
 * browser but Safari decodes on its own, and the address in the channel list
 * belongs to a server that never agreed to be fetched by this page. The box
 * answers both: it serves the playlist under its own name with every part
 * rewritten to itself, and ui/webtv.js is the piece that walks that and hands
 * it to the demuxer, on the element ui/playing.js holds. Putting the far
 * address on the element instead left a player that never started and said
 * nothing.
 *
 * @param {{ channel: Api.Channel }} props
 * @returns {Web.Drawn}
 */
function WebPlayer(props) {
	const channel = props.channel;
	const sound = channel.kind === 'webradio';
	const slot = useRef(/** @type {HTMLDivElement | null} */ (null));
	const [now, setNow] = useState(playing.current());

	useEffect(function () {
		/* Read once as well as subscribed to, because something may have
		   started between this being drawn and this effect running. */
		setNow(playing.current());
		return playing.subscribe(setNow);
	}, []);

	const mine = now && now.source.of === 'channel' && now.source.id === channel.id ? now : null;
	const started = mine !== null;
	const phase = mine ? mine.phase : 'idle';
	const why = mine ? mine.failure : '';
	const said = mine ? mine.said : '';
	const failed = phase === 'failed';
	/* This screen offers the element a place while the channel playing is its
	   own, and while nothing plays at all, which is the state the button below
	   starts from. Another source drawn in this card under this channel's name
	   is a lie about what the box is doing. */
	const ours = mine !== null || now === null;

	/* Nothing is torn down when the offer is taken back: letting go of the
	   element is what ends the request the box still has open, and the whole
	   point of the element belonging elsewhere is that walking to another
	   screen does not end it. The button below does. */
	useEffect(function () {
		if (!ours) {
			return undefined;
		}
		playing.setSlot('page', slot.current);
		return function () { playing.setSlot('page', null); };
	}, [ours]);

	return html`<div class="pb-card">
		<h2>${t(text, 'play.web.title')}</h2>
		<p>${t(text, 'play.web.body')}</p>
		<p class="pb-controls">
			<${Button} primary=${true} disabled=${started}
				onClick=${function () { playing.start(playing.ofChannel(channel)); }}>${t(text, 'play.start')}<//>
			${' '}
			<${Button} disabled=${!started}
				onClick=${function () { playing.stop(); }}>${t(text, 'play.stop')}<//>
		</p>
		<p class="pb-state">${t(text, 'play.state')}:
			${' '}<span class=${failed ? 'pb-phase bad' : 'pb-phase'} id="pb-phase"
				role="status">${t(text, 'play.state.' + phase)}</span></p>
		${failed
			? html`<p class="pb-failed" role="alert">
				${t(text, 'play.failed.' + (why !== '' ? why : 'unknown'), { url: channel.url })}
				${said !== ''
					? html`${' '}<span class="pb-said mono">${said}</span>`
					: null}
			</p>`
			: null}
		<div class=${sound ? 'pb-media pb-sound' : 'pb-media'} ref=${slot}></div>
	</div>`;
}

/**
 * The channels of this box the browser can fetch for itself, found by walking
 * the channel list once.
 *
 * A walk and not a query, because there is no query: the route filters by
 * television and radio and by bouquet and by nothing else. So this is offered
 * rather than done, the reason is said, and a box holding two thousand
 * channels is read only when somebody asks for it to be.
 *
 * @returns {Web.Drawn}
 */
function WebChannels() {
	const [walk, setWalk] = useState(emptyWalk());
	const [asked, setAsked] = useState(false);
	/* Across both lists and not per page: the walk below begins again for the
	   second one, and between the two there is a moment where no page is in
	   flight and the search is nonetheless not over. */
	const [searching, setSearching] = useState(false);
	const held = useRef(emptyWalk());
	const running = useRef(/** @type {AbortController | null} */ (null));
	const mode = useRef(/** @type {'tv' | 'radio'} */ ('tv'));
	const [found, setFound] = useState(/** @type {Api.Channel[]} */ ([]));
	// Over both lists, because the walk below starts again for the second one
	// and what a person is told is how much of the box has been read.
	const [read, setRead] = useState(0);

	useEffect(function () {
		return function () {
			if (running.current) {
				running.current.abort();
				running.current = null;
			}
		};
	}, []);

	/**
	 * @param {import('./list.paging.js').Walk<Api.Channel>} next
	 */
	function apply(next) {
		held.current = next;
		setWalk(next);
	}

	function step() {
		const was = held.current;
		if (!wants(was)) {
			/* Television first and radio after it, because the route answers
			   one of the two lists and a web channel can be in either. */
			if (mode.current === 'tv') {
				mode.current = 'radio';
				apply(emptyWalk());
				window.setTimeout(step, 0);
				return;
			}
			setSearching(false);
			return;
		}
		const control = new AbortController();
		running.current = control;
		apply(beginPage(was));
		api('GET', '/api/v1/channels', {
			query: {
				mode: mode.current,
				cursor: was.cursor !== '' ? was.cursor : undefined,
				limit: kSearchPage,
			},
			signal: control.signal,
		}).then(function (page) {
			if (running.current !== control) {
				return;
			}
			running.current = null;
			const items = page.items || [];
			/** @type {Api.Channel[]} */
			const web = [];
			for (const one of items) {
				if (isWeb(one)) {
					web.push(one);
				}
			}
			if (web.length > 0) {
				setFound(function (sofar) { return sofar.concat(web); });
			}
			setRead(function (sofar) { return sofar + items.length; });
			apply(extend(held.current, items, page));
			step();
		}, function (error) {
			if (running.current !== control || isAborted(error)) {
				return;
			}
			running.current = null;
			setSearching(false);
			apply(failPage(held.current, error));
		});
	}

	function start() {
		if (running.current) {
			running.current.abort();
			running.current = null;
		}
		mode.current = 'tv';
		held.current = emptyWalk();
		setWalk(held.current);
		setFound([]);
		setRead(0);
		setAsked(true);
		setSearching(true);
		step();
	}

	const done = asked && !searching;

	return html`<div class="pb-card">
		<h2 id="pb-find">${t(text, 'play.find')}</h2>
		<p>${t(text, 'play.find.why')}</p>
		<p><${Button} disabled=${searching}
			onClick=${start}>${asked ? t(text, 'play.find.again') : t(text, 'play.find')}<//></p>
		${asked
			? html`<p class=${searching ? 'pb-found pb-busy' : 'pb-found'} role="status">${searching
				? t(text, 'play.find.busy', { loaded: read })
				: t(text, 'play.find.found', { count: found.length, loaded: read })}</p>`
			: null}
		${walk.error
			? html`<${State} problem=${{ title: walk.error.problem.title, detail: walk.error.problem.detail }} />`
			: null}
		${done && found.length === 0
			? html`<p class="pb-none">${t(text, 'play.find.none')}</p>`
			: null}
		${found.length > 0
			? html`<ul class="pb-list">
				${found.map(function (one) {
					return html`<li key=${one.id}>
						<a class="pb-list-link" href=${hrefFor('channels', 'playback', one.id)}>${one.name}</a>
						${' '}<span class="pb-kind-tag">${t(text, 'play.kind.' + (one.kind || 'unknown'))}</span>
					</li>`;
				})}
			</ul>`
			: null}
	</div>`;
}
