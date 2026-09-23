// One channel the box plays from an address somewhere else, in the page.
//
// WHY THE ADDRESSES ARE THE BOX'S OWN. A page may not put a far server's
// stream into its own video element unless that server says it may, and the
// servers these channel lists point at do not say it. So the box fetches the
// stream and answers it under its own name, and the page talks to one server
// instead of two. Every address in here is a route of this box; not one of
// them is the address written in the channel list.
//
// TWO ADDRESSES AND NOT ONE, tried in order. Most of these channels are a
// playlist, which is what the first names; a few are a stream on their own,
// which is what the second names. Nothing here asks the box which of the two a
// channel is: the first address answers a refusal for a channel that is not a
// playlist, and a refusal is the same event as a stream that would not play,
// so one path handles both and no request is spent finding out.
//
// AND THE LIBRARY IS FETCHED ONLY WHERE IT IS NEEDED. Safari decodes the
// playlist format itself and is handed the address directly. Every other
// browser is handed it through the library the runtime fetches on demand, and
// a browser where neither works is told so rather than left with a black
// rectangle.
import { html, useEffect, useRef, useState, loadHls } from '../runtime.js';
import { t } from '../i18n.js';
import text from './webtv.text.js';
import { ensureCss } from '../css.js';

/* The two routes, spelled as the box's own document spells them, so that the
   check which holds every address on these pages to that document can read
   them. What goes on the wire is the same string with the channel put in. */
const kCss = '/app/ui/webtv.css';

const PLAYLIST = '/api/v1/webtv/{id}/playlist.m3u8';
const STREAM = '/api/v1/webtv/{id}/stream';

/**
 * @param {string} template
 * @param {string} id
 * @returns {string}
 */
function addressFor(template, id) {
	return template.replace('{id}', encodeURIComponent(id));
}

/* Whether this browser decodes the playlist format without help.

   NOT canPlayType, WHICH LIES HERE. Chromium answers "maybe" for a playlist
   and then plays nothing at all: the element reaches readyState four with a
   manifest it never demuxed and simply stays paused, which is the one failure
   nothing on the page can report. Measured on Chromium 152.

   So the question asked is the one with an answer: whether this browser has
   the interface the library needs. Where it has, the library is used even by
   a browser that would also manage alone, which is what the library's own
   authors advise. Where it has not, the element's own decoder is all there
   is, and that is Safari, where it is enough. */
/** @returns {boolean} */
export function playsPlaylistsItself() {
	const source = window.MediaSource;
	if (source && typeof source.isTypeSupported === 'function') {
		return false;
	}
	const probe = document.createElement('video');
	return probe.canPlayType('application/vnd.apple.mpegurl') !== '';
}

/**
 * What the block is doing, handed to whoever drew it so that a screen can put
 * a mark beside its own heading without reaching inside here.
 *
 * @typedef {'starting' | 'playing' | 'unplayable'} WebTvPhase
 */

/**
 * Puts one channel of this box on a media element and keeps it there.
 *
 * Exported because two screens play a channel and only one of them draws this
 * file's own block: the other has a card, two buttons and a line of its own
 * around the element, and what it was missing was this, not the frame.
 *
 * @param {HTMLMediaElement} el
 * @param {string} id
 * @param {(phase: WebTvPhase) => void} [told]
 * @returns {() => void} what ends it, to be called when the element goes away
 */
export function attachWebTv(el, id, told) {
	/* Everything below asks this before it touches the element or the
	   state, because the two things that end this block, a channel change
	   and the screen going away, both happen while a fetch of the library
	   or of the first bytes is still in the air. */
	let dropped = false;
	/** @type {{ destroy: () => void } | null} */
	let engine = null;
	let at = 0;

	const tries = [addressFor(PLAYLIST, id), addressFor(STREAM, id)];

	/** @param {WebTvPhase} next */
	function say(next) {
		if (dropped)
			return;
		if (told)
			told(next);
	}

	function letGo() {
		if (engine) {
			engine.destroy();
			engine = null;
		}
	}

	/* The next address, or the answer that there is none.

	   A failure is a failure whichever end it came from: the box refusing
	   a channel that is not a playlist, the far server not answering, and
	   this browser not decoding what arrived all reach here, and what the
	   page does about each is the same. */
	function next() {
		if (dropped)
			return;
		letGo();
		at += 1;
		if (at >= tries.length) {
			say('unplayable');
			return;
		}
		start();
	}

	/** @param {string} address */
	function throughLibrary(address) {
		loadHls().then(function (module) {
			if (dropped)
				return;
			const Hls = module.default;
			if (!Hls || !Hls.isSupported()) {
				next();
				return;
			}
			/* No worker. A worker is loaded from an address this page
			   makes up, and the policy this page states allows scripts
			   from this box and from nowhere else, so one would be blocked
			   and the library would fall back to the main thread anyway,
			   having spent a failure on it first. */
			const made = new Hls({ enableWorker: false });
			made.on(Hls.Events.ERROR, function (_event, data) {
				if (data && data.fatal)
					next();
			});
			engine = made;
			made.loadSource(address);
			made.attachMedia(el);
		}, function () {
			next();
		});
	}

	function start() {
		const address = tries[at];
		if (!address) {
			say('unplayable');
			return;
		}
		say('starting');
		if (at === 0 && !playsPlaylistsItself()) {
			throughLibrary(address);
			return;
		}
		el.src = address;
		el.load();
	}

	function onPlaying() { say('playing'); }
	function onFailure() { next(); }

	el.addEventListener('playing', onPlaying);
	el.addEventListener('error', onFailure);
	start();

	return function () {
		dropped = true;
		el.removeEventListener('playing', onPlaying);
		el.removeEventListener('error', onFailure);
		letGo();
		/* Taking the address away and loading again is what ends the
		   request that is still running, and that request is the box
		   fetching this channel off a far server. Left alone, the box goes
		   on fetching a stream nobody is watching until it notices the
		   socket has gone. */
		el.removeAttribute('src');
		el.load();
	};
}

/**
 * @param {{
 *   channelId: string,
 *   name?: string,
 *   poster?: string,
 *   muted?: boolean,
 *   onPhase?: (phase: WebTvPhase) => void
 * }} props
 * @returns {Web.Drawn}
 */
export function WebTvPlayer(props) {
	/** @type {{ current: HTMLVideoElement | null }} */
	const frame = useRef(null);
	const [phase, setPhase] = useState(/** @type {WebTvPhase} */ ('starting'));
	// Bumped by the button on the refusal, which is the whole of what starting
	// again means: the effect below runs from the top.
	const [attempt, setAttempt] = useState(0);

	const id = props.channelId;
	const told = props.onPhase;

	useEffect(function () { ensureCss(kCss); }, []);

	useEffect(function () {
		if (!frame.current)
			return undefined;
		return attachWebTv(frame.current, id, function (next) {
			setPhase(next);
			if (told)
				told(next);
		});
	}, [id, attempt, told]);

	const name = props.name || t(text, 'webtv.name.unknown');
	const address = addressFor(PLAYLIST, id);

	return html`<div class="webtv" data-phase=${phase}>
		<video
			class="webtv-frame"
			ref=${frame}
			poster=${props.poster || ''}
			aria-label=${t(text, 'webtv.label', { name: name })}
			controls
			autoplay
			playsinline
			muted=${props.muted !== false}></video>
		${phase === 'starting'
			? html`<p class="webtv-say">${t(text, 'webtv.starting')}</p>`
			: null}
		${phase === 'playing'
			? html`<p class="webtv-say sr">${t(text, 'webtv.playing')}</p>`
			: null}
		${phase === 'unplayable'
			? html`<div class="note webtv-refused">
				<h3>${t(text, 'webtv.unplayable.title')}</h3>
				<p>${t(text, 'webtv.unplayable.body')}</p>
				<p class="webtv-address">
					<span>${t(text, 'webtv.address.label')}</span>
					<code>${address}</code>
				</p>
				<button
					class="btn"
					type="button"
					onClick=${function () { setAttempt(attempt + 1); }}>
					${t(text, 'webtv.retry')}
				</button>
			</div>`
			: null}
	</div>`;
}
