/* Playing a channel of this box in the browser, and saying so when it cannot be
   done.

   WHAT A BOX SENDS is a transport stream, which no browser opens on its own, and
   inside it are codecs a browser mostly does not decode. German satellite
   television is H.264 with Layer II or AC-3 sound, and every browser plays the
   picture and none of them plays the sound. So the box converts the sound and
   copies the picture, and this fetches the result.

   FOUR ROADS AND THE PAGE PICKS ONE, out of what the channel route says the
   channel carries. Straight through, sound converted, opened without knowing, or
   not offered at all. The last is the honest one and the reason the codecs are
   published: a standard definition channel is MPEG-2, no browser has decoded that
   for years, and the box cannot encode a picture. So there is no player, there is
   a sentence saying why and a button that opens the channel outside the browser.

   THE THIRD ROAD IS THE ORDINARY ONE and it is why nothing is decided before
   somebody presses play. A box learns what a channel carries by tuning it; a scan
   writes down neither the sound nor, in practice, the picture. So for every
   channel nobody has been on the answer is that nothing is known. What this does
   with it is open the channel anyway, which tunes it, and then ask again; a
   channel that turns out to be MPEG-2 is stopped at that point and explained.
   Greying it out beforehand would grey out almost every channel on the box.

   THE BOX SAYS WHAT IT CAN DELIVER AND THE BROWSER SAYS WHAT IT CAN PLAY, and
   the two are different questions. A table of browsers written into this page is
   wrong by the time somebody reads it, so the browser is asked rather than looked
   up: HEVC in Firefox fails here and not in a list. The sound is never one of
   these questions, because whatever a channel carries the browser is asked about
   the AAC it would be handed. */

import { html, useState, useEffect, useRef } from '../runtime.js';
import { api, buildUrl } from '../api.js';
import { ensureCss } from '../css.js';
import { isAborted } from '../problem.js';
import { t } from '../i18n.js';
import { Button } from './button.js';
/* What actually holds the element and the stream, for a channel and for a file
   on a disk alike. This file works out what the box would offer for a channel
   and says it in words; the playing itself outlives this screen and therefore
   cannot be owned by it. */
import * as playing from './playing.js';
import text from './player.text.js';

/**
 * What this needs to know about a channel, which is what the channel route
 * writes. Every member is optional because a box that has never tuned the channel
 * knows none of them and says so rather than guessing.
 *
 * @typedef {object} Playable
 * @property {string} id
 * @property {string} name
 * @property {string} [kind]
 * @property {{ codec?: string, pid?: number }} [video]
 * @property {Array<{ pid?: number, codec?: string, description?: string, selected?: boolean }>} [audio]
 * @property {boolean} [streams_known]
 */

/**
 * @typedef {object} Decision
 * @property {'copy' | 'convert-sound' | 'blind' | 'not-offered'} path what the box would do
 * @property {'' | 'video' | 'audio' | 'browser'} why empty unless the path is not offered
 * @property {boolean} radio
 * @property {string} video the codec name, as the channel route spells it
 * @property {string} audio the codec name of the chosen sound
 * @property {number} apid the pid of the chosen sound, zero when none is named
 * @property {string} src the address the media element is given, empty when there is none
 */

const kCss = '/app/ui/player.css';

/* What the picture becomes once the browser has it, as the browser is asked
   about it. A profile a browser refuses while decoding the same codec in
   practice would be a channel refused for nothing, so these are the mildest
   spellings of each: a browser that plays no baseline H.264 at all plays no
   broadcast either. */
/** @type {Record<string, string>} */
const kPictureTypes = {
	h264: 'avc1.42E01E',
	hevc: 'hvc1.1.6.L93.B0',
};

// What the sound becomes. Plain AAC either way: converted sound is encoded to
// it, and sound that was already AAC is copied.
const kSoundType = 'mp4a.40.2';

/* What each way of failing is called on screen. A table rather than a chain of
   conditions, because every one of these is a different thing to do next and a
   reader has to be able to see all of them at once. */
/** @type {Record<string, string>} */
const kFailures = {
	load: 'player.failed.load',
	browser: 'player.no.browser',
	busy: 'player.busy',
	converter: 'player.no.converter',
	stream: 'player.failed',
};

/* Which sounds this box has a decoder for. THE SAME TABLE IS IN THE BOX
   (src/coreapi/browserplay.cpp), and it has to be, because the box refuses
   what it cannot do whatever this page asks for. What this copy buys is a
   sentence before the request rather than a failure after it, on the day a
   codec is added to the channel route and to only one of the two tables. */
const kConvertible = ['mp2', 'ac3', 'eac3', 'aac', 'aacplus', 'dts', 'dtshd', 'lpcm'];

/**
 * Which sound to ask for.
 *
 * NOT SIMPLY THE ONE THE BOX IS PLAYING. What the box plays is what a television
 * wants, and a German channel carries the same commentary twice: once as Layer II
 * and once as AC-3, neither of which a browser decodes. Where one of the tracks
 * is already AAC, that one costs the box nothing at all.
 *
 * The language comes first even so: the AAC track has to be one described the
 * same way as the one the box chose. Where there is none, the box's own track is
 * converted rather than a stranger's handed over.
 *
 * @param {Playable} channel
 * @returns {{ codec: string, pid: number }}
 */
export function chosenSound(channel) {
	const list = channel.audio || [];
	let selected = null;
	for (const one of list) {
		if (one && one.selected) {
			selected = one;
			break;
		}
	}
	if (!selected && list.length > 0) {
		selected = list[0];
	}
	if (!selected) {
		return { codec: '', pid: 0 };
	}
	if (selected.codec === 'aac') {
		return { codec: 'aac', pid: selected.pid || 0 };
	}

	let sameWords = null;
	for (const one of list) {
		if (!one || one.codec !== 'aac') {
			continue;
		}
		if (one.description === selected.description) {
			sameWords = one;
			break;
		}
	}
	if (sameWords) {
		return { codec: 'aac', pid: sameWords.pid || 0 };
	}
	return { codec: selected.codec || '', pid: selected.pid || 0 };
}

/**
 * Whether this browser can play what the box would deliver. Asked of the
 * browser's own media stack, because that is the only reader that knows.
 *
 * @param {string} video the codec name, or empty for radio
 * @returns {boolean}
 */
export function browserPlays(video) {
	if (video === '') {
		/* Radio arrives as ADTS on an ordinary audio element, which has its own
		   question to answer and does not go through the demuxer at all. */
		const probe = document.createElement('audio');
		return probe.canPlayType('audio/aac') !== '';
	}

	const picture = kPictureTypes[video];
	if (!picture) {
		return false;
	}
	/* The demuxer hands the browser fragmented MP4 and not the transport stream
	   it read, so this is the question the browser is really being asked. */
	const wanted = 'video/mp4; codecs="' + picture + ',' + kSoundType + '"';
	const source = /** @type {any} */ (window).MediaSource;
	if (!source || typeof source.isTypeSupported !== 'function') {
		return false;
	}
	return source.isTypeSupported(wanted) === true;
}

/**
 * Which of the three roads this channel takes, and the address for the two
 * that have one.
 *
 * @param {Playable} channel
 * @returns {Decision}
 */
export function decide(channel) {
	const radio = channel.kind === 'radio' || channel.kind === 'webradio';
	const video = radio ? 'none' : ((channel.video && channel.video.codec) || 'unknown');
	const sound = chosenSound(channel);
	const audio = sound.codec || 'unknown';

	/** @type {Decision} */
	const out = {
		path: 'not-offered',
		why: '',
		radio: radio,
		video: video,
		audio: audio,
		apid: sound.pid,
		src: '',
	};

	/** @returns {string} */
	function address() {
		return buildUrl('/api/v1/stream/browser/{id}', { id: channel.id }, {
			video: video,
			audio: audio,
			apid: sound.pid > 0 ? sound.pid : undefined,
		});
	}

	/* Nothing known about one half is nothing known, and that is the ordinary
	   state of a channel nobody has been on. It is opened rather than refused,
	   because opening it is what makes the box read it. */
	if (channel.streams_known === false || video === 'unknown' || audio === 'unknown') {
		out.path = 'blind';
		out.src = address();
		return out;
	}

	if (kConvertible.indexOf(audio) < 0) {
		out.why = 'audio';
		return out;
	}

	if (!radio && !kPictureTypes[video]) {
		out.why = 'video';
		return out;
	}

	if (!browserPlays(radio ? '' : video)) {
		out.why = 'browser';
		return out;
	}

	out.path = audio === 'aac' ? 'copy' : 'convert-sound';
	out.src = address();
	return out;
}

/**
 * What the element says about itself, read off the element and never kept beside
 * it: the browser owns this state, and a second copy of it goes stale the moment
 * somebody uses the element's own controls.
 *
 * @param {HTMLMediaElement} media
 * @returns {string}
 */
export function phaseOf(media) {
	if (media.error) {
		return 'failed';
	}
	if (media.ended) {
		return 'ended';
	}
	if (media.paused) {
		return media.currentSrc === '' ? 'idle' : 'paused';
	}
	if (media.readyState >= 3) {
		return 'playing';
	}
	return 'loading';
}

/**
 * What the element said went wrong, in its own words.
 *
 * The sentence under a failure used to name two causes at once, that the source
 * was unreachable or served something unplayable, because the screen did not know
 * which. The element does know: MediaError tells a fetch that failed apart from a
 * format this browser will not play, and those two send somebody looking in
 * completely different places.
 *
 * The element's own message is carried through where it has one. It is not
 * translated, because it is not ours to write.
 *
 * Here beside phaseOf, because the two are one question asked twice.
 *
 * @param {HTMLMediaElement} media
 * @returns {{ why: string, said: string } | null}
 */
export function faultOf(media) {
	const error = media.error;
	if (!error) {
		return null;
	}
	/* The four names the standard gives, spelled out rather than indexed,
	   because the code is a number from the browser and an unknown one has to
	   land on the last line here and not on a lookup that quietly yields
	   nothing. */
	let why = 'unknown';
	if (error.code === 1) why = 'aborted';
	else if (error.code === 2) why = 'network';
	else if (error.code === 3) why = 'decode';
	else if (error.code === 4) why = 'format';
	return {
		why: why,
		said: typeof error.message === 'string' ? error.message : '',
	};
}

/**
 * One channel of this box, played or explained.
 *
 * NOTHING PLAYS WITHOUT A CLICK. A screen that started a stream while it was
 * being drawn would put a tuner and a converter on the box for somebody who only
 * opened a page, over an uplink that is somebody's home line.
 *
 * AND NOTHING STOPS WHEN THE SCREEN GOES. What plays is app/ui/playing.js, which
 * outlives every screen; this one offers it a place in the page while it is on
 * and never ends the stream on its own. Ending it is a button.
 *
 * @param {{ channel: Playable }} props
 * @returns {Web.Drawn}
 */
export function Player(props) {
	/* What the box has learned since this was drawn, which for a channel
	   nobody had been on is everything. Kept here as well as where the playing
	   is, because it is still the answer once the playing has stopped: a
	   channel that turns out to carry something nothing here can play is
	   stopped on the strength of that reading, and the sentence that says why
	   is written from it. */
	const [warm, setWarm] = useState(/** @type {Playable | null} */ (null));
	const [now, setNow] = useState(playing.current());
	const slot = useRef(/** @type {HTMLDivElement | null} */ (null));

	useEffect(function () { ensureCss(kCss); }, []);

	useEffect(function () {
		setWarm(null);
		/** @param {import('./playing.js').Playing | null} next */
		function seen(next) {
			setNow(next);
			if (next && next.source.of === 'channel' && next.source.id === props.channel.id) {
				setWarm(next.source.channel);
			}
		}
		/* Read once as well as subscribed to, because something may have
		   started between this being drawn and this effect running. */
		seen(playing.current());
		return playing.subscribe(seen);
	}, [props.channel.id]);

	const channel = warm || props.channel;
	const decision = decide(channel);
	const mine = now && now.source.of === 'channel' && now.source.id === props.channel.id ? now : null;
	const started = mine !== null;
	const phase = mine ? mine.phase : 'idle';
	const failure = mine ? mine.failure : '';
	const offered = decision.path !== 'not-offered';
	/* A screen offers the element a place while the channel playing is its
	   own, and while nothing plays at all, which is the state the button below
	   starts from. It does not offer one for somebody else's source: this
	   screen is about one channel, and another channel's picture, or a
	   recording, drawn in it under this channel's name is a lie about what the
	   box is doing. */
	const ours = mine !== null || now === null;

	/* Taking the offer back is all the cleanup there is: a channel is meant to
	   go on playing while somebody walks to another screen, so this must not
	   stop it. Run again rather than once, because the slot below is drawn
	   only under the two conditions above and an offer left standing would
	   name a node that is no longer in the page. */
	useEffect(function () {
		if (!offered || !ours) {
			return undefined;
		}
		playing.setSlot('page', slot.current);
		return function () { playing.setSlot('page', null); };
	}, [offered, ours]);

	if (!offered) {
		return html`<${Unplayable} channel=${channel} decision=${decision} />`;
	}

	const how = decision.path === 'blind' ? 'player.blind'
		: decision.radio
			? (decision.path === 'copy' ? 'player.radio.direct' : 'player.radio.convert')
			: (decision.path === 'copy' ? 'player.direct' : 'player.convert');

	return html`<div class="pl">
		<p class="pl-how">${t(text, how, { codec: decision.audio })}</p>
		<p class="pl-acts">
			<${Button} primary=${true} disabled=${started}
				onClick=${function () { playing.start(playing.ofChannel(channel)); }}>${t(text, 'player.play')}<//>
			${' '}
			<${Button} disabled=${!started}
				onClick=${function () { playing.stop(); }}>${t(text, 'player.stop')}<//>
		</p>
		<p class="pl-state">${t(text, 'player.state')}:
			${' '}<span class=${phase === 'failed' ? 'pl-phase bad' : 'pl-phase'}
				role="status">${t(text, 'player.state.' + phase)}</span></p>
		${failure !== ''
			? html`<p class="pl-bad" role="alert">${t(text, kFailures[failure] || 'player.failed',
				{ codec: decision.video })}</p>`
			: null}
		<div class=${decision.radio ? 'pl-media pl-sound' : 'pl-media'} ref=${slot}></div>
		<${Elsewhere} channel=${channel} />
	</div>`;
}

/**
 * A channel that will not play here, why, and the way to watch it anyway.
 *
 * @param {{ channel: Playable, decision: Decision }} props
 * @returns {Web.Drawn}
 */
function Unplayable(props) {
	const d = props.decision;
	useEffect(function () { ensureCss(kCss); }, []);

	const said = d.why === 'video' ? t(text, 'player.no.video', { codec: d.video })
		: d.why === 'audio' ? t(text, 'player.no.audio', { codec: d.audio })
		/* Not the sound the channel carries. Radio is converted whatever it
		   arrives as, so what this browser turned down is the AAC the box
		   would have sent, and naming the broadcast codec blames the wrong
		   half. */
		: t(text, 'player.no.browser', { codec: d.radio ? 'aac' : d.video });

	return html`<div class="pl pl-no">
		<h3 class="pl-no-head">${t(text, 'player.no.title')}</h3>
		<p>${said}</p>
		<${Elsewhere} channel=${props.channel} />
	</div>`;
}

/**
 * The way to watch a channel outside the browser: the playlist a player opens,
 * and the address it names, for whoever would rather paste it.
 *
 * The address is asked for and never built here. It is the name this request
 * reached the box by with the streaming port on it, and only the box knows which
 * port that is.
 *
 * @param {{ channel: Playable }} props
 * @returns {Web.Drawn}
 */
function Elsewhere(props) {
	const [address, setAddress] = useState('');

	useEffect(function () {
		const control = new AbortController();
		api('GET', '/api/v1/stream/{id}', {
			params: { id: props.channel.id },
			signal: control.signal,
		}).then(function (where) {
			setAddress(where.url);
		}, function (error) {
			// The button below still works without it: what it fetches is a
			// playlist the box writes, and that needs no address from here.
			if (!isAborted(error)) {
				setAddress('');
			}
		});
		return function () { control.abort(); };
	}, [props.channel.id]);

	const playlist = buildUrl('/api/v1/stream/playlist/{id}', { id: props.channel.id });

	return html`<div class="pl-vlc">
		<p><a class="btn" href=${playlist}>${t(text, 'player.vlc')}</a></p>
		<p class="pl-vlc-hint">${t(text, 'player.vlc.hint')}</p>
		${address !== ''
			? html`<p class="pl-address">${t(text, 'player.address')}:
				${' '}<span class="mono">${address}</span></p>`
			: null}
	</div>`;
}
