/* One file on a disk of this box, played in the row it stands in, or refused in
   words.

   WHAT PLAYS IT IS NOT THIS SCREEN. app/ui/playing.js holds the element and the
   stream for everything this page plays, a channel of the box and a file alike,
   and it outlives every screen. So a recording goes on playing in the window on
   shell level while somebody walks to the guide, the way a channel does, and the
   rule that only one thing plays at a time is a property of that module rather
   than a line each screen has to remember. This screen offers the element a place
   while the file playing is its own, says what road the name takes, and explains
   a refusal.

   THE ROAD IS DECIDED ON THE NAME AND THE REFUSAL IS NOT. A name ending in .ts
   says the container and nothing about the picture inside it, and the box does
   not read a recording to find out; what the demuxer turns out to be handing the
   browser is compared with what this browser will decode the moment it says so
   (app/ui/disk.js), and the answer arrives while the file is already playing. So
   a refusal is kept here after the playing has stopped on the strength of it,
   which is the only way the sentence explaining it can still be on screen.

   JUMPING IS THE ONE THING THAT DOES NOT WORK, and the sentence under the player
   says so. A recording is a bare transport stream with no index in it, so a jump
   past what has arrived is a jump to nowhere; app/ui/playing.js holds every jump
   inside what has arrived, and the reason is written there beside the listener
   that does it. */

import { html, useState, useEffect, useRef } from '../../runtime.js';
import { t } from '../../i18n.js';
import { Button } from '../../ui/button.js';
/* The one place that holds a stream, so a recording and live television are the
   same element in the same two slots rather than two players side by side. */
import * as playing from '../../ui/playing.js';
import { fileRoad } from './paths.js';
import text from './files.text.js';

/**
 * One file, played or explained.
 *
 * NOTHING IS FETCHED WITHOUT A PRESS. A row opened is a row somebody wanted to
 * look at; a recording is gigabytes over somebody's home line, and a screen that
 * began fetching one because a finger landed on the wrong row would be spending
 * that line for nothing.
 *
 * @param {{ name: string, href: string, outside: string }} props name is the leaf, href the
 *        address the box hands the bytes over at for this browser, and outside the same
 *        file as an address another program can be given, empty while there is none
 * @returns {Web.Drawn}
 */
export function FilePlay(props) {
	const road = fileRoad(props.name);
	const slot = useRef(/** @type {HTMLDivElement | null} */ (null));
	const [now, setNow] = useState(playing.current());
	/** A refusal, which is either the name itself or something the file turned
	    out to be. Empty while there is none. */
	const [bad, setBad] = useState(/** @type {{ why: string, codec: string }} */ (
		{ why: road.how === 'none' ? road.why : '', codec: '' }));

	useEffect(function () {
		/** @param {import('../../ui/playing.js').Playing | null} next */
		function seen(next) {
			setNow(next);
			if (!next || next.source.of !== 'file' || next.source.id !== props.href) {
				return;
			}
			/* Kept here because it is still the answer once the playing has
			   stopped, and it is stopped the moment the module names one: the
			   sentence under this control is written from what was learned too
			   late to have been said beforehand. */
			if (next.failure !== '') {
				setBad({ why: next.failure, codec: next.said });
			}
		}
		/* Read once as well as subscribed to, because something may have
		   started between this being drawn and this effect running. */
		seen(playing.current());
		return playing.subscribe(seen);
	}, [props.href]);

	const mine = now && now.source.of === 'file' && now.source.id === props.href ? now : null;
	const started = mine !== null;
	const phase = mine ? mine.phase : 'idle';
	/* A screen offers the element a place while the file playing is its own, and
	   while nothing plays at all, which is the state the button below starts
	   from. It does not offer one for somebody else's source: this control is
	   about one file, and another file's picture drawn in it under this file's
	   name is a lie about what is playing. */
	const ours = mine !== null || now === null;
	const offered = bad.why === '' && ours;

	/* Taking the offer back is all the cleanup there is: a file is meant to go
	   on playing while somebody walks to another screen, so this must not stop
	   it. Run again rather than once, because the slot below is drawn only under
	   the two conditions above and an offer left standing would name a node that
	   is no longer in the page. */
	useEffect(function () {
		if (!offered) {
			return undefined;
		}
		playing.setSlot('page', slot.current);
		return function () { playing.setSlot('page', null); };
	}, [offered]);

	if (bad.why !== '') {
		/* Spoken as well as drawn, because half of these refusals take the place
		   of a player somebody pressed and a reader who cannot see that happen
		   is left with a control that went quiet. A region already in the page
		   when it is first drawn is not announced, which is the other half. */
		return html`<div class="files-play files-play-no">
			<h3>${t(text, 'files.play.no.title')}</h3>
			<p role="status">${t(text, 'files.play.no.' + bad.why, { codec: bad.codec })}</p>
			<${Elsewhere} href=${props.outside} />
		</div>`;
	}

	/* The place for the element is in the markup at all times and draws nothing
	   at all while nothing stands in it, which files.css says on :empty. In the
	   markup, so that the offer above names a node that is already in the page
	   by the time the module has an element to put there; drawing nothing,
	   because an empty rectangle with a border is a player somebody has not
	   pressed, and offering one is what this whole screen is about not doing. */
	return html`<div class="files-play">
		<p class="files-play-how">${t(text, 'files.play.how.' + road.how)}</p>
		<p class="files-play-acts">
			<${Button} primary=${true} disabled=${started}
				onClick=${function () {
					playing.start(playing.ofFile(props.name, props.href, road));
				}}>${t(text, 'files.play.start')}<//>
			${' '}
			<${Button} disabled=${!started}
				onClick=${function () { playing.stop(); }}>${t(text, 'files.play.stop')}<//>
			${phase === 'loading'
				? html`<span class="files-play-state" role="status">${t(text, 'files.play.loading')}</span>`
				: null}
		</p>
		<div class=${road.sound ? 'files-play-media files-play-sound' : 'files-play-media'}
			ref=${slot}></div>
		${road.how === 'demuxed'
			? html`<p class="files-play-how">${t(text, 'files.play.seek')}</p>`
			: null}
		<${Elsewhere} href=${props.outside} />
	</div>`;
}

/**
 * The way to watch it outside the browser.
 *
 * The address and not a playlist: the box writes one for a channel and none for a
 * file, and a link to the file itself is a download and not a player. So what is
 * offered is the address to paste, which is what every player asks for.
 *
 * WHAT THE ADDRESS CARRIES, and why this waits for it. A player sets no header and has
 * no cookie, and the route that hands over the bytes asks for the level a signed in
 * person holds. So the address carries a credential of its own, drawn by the screen that
 * lists the files, worth the media of this box and nothing else, and lasting about
 * as long as a session. Until it has arrived, and if the box will not draw one, nothing
 * is offered at all: an address that answers 403 to whoever pastes it is worse than no
 * address, which is the whole reason this was rewritten.
 *
 * @param {{ href: string }} props the address to paste, empty while there is none
 * @returns {Web.Drawn}
 */
function Elsewhere(props) {
	if (props.href === '') {
		return null;
	}
	/* Absolute, because this is meant to be carried to another program. The name
	   this page was reached under is the name that program has to use too. */
	const address = new URL(props.href, window.location.href).href;
	return html`<p class="files-play-vlc">${t(text, 'files.play.vlc')}
		${' '}<span class="mono">${address}</span></p>`;
}
